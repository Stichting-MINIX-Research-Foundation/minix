/* This file contains a simple message queue implementation to support both
 * the singlethread and the multithreaded driver implementation.
 *
 * Changes:
 *   Oct 27, 2011   rewritten to use sys/queue.h (D.C. van Moolenbroek)
 *   Aug 27, 2011   integrated into libdriver (A. Welzel)
 */

#include <minix/driver_mt.h>
#include <sys/queue.h>
#include <assert.h>

#include "mq.h"

#define MQ_SIZE		128

struct mq_cell {
  message mess;
  int ipc_status;
  STAILQ_ENTRY(mq_cell) next;
};

PRIVATE struct mq_cell pool[MQ_SIZE];

PRIVATE STAILQ_HEAD(queue, mq_cell) queue[DRIVER_MT_MAX_WORKERS];
PRIVATE STAILQ_HEAD(free_list, mq_cell) free_list;

/*===========================================================================*
 *				driver_mq_init				     *
 *===========================================================================*/
PUBLIC void driver_mq_init(void)
{
/* Initialize the message queues and message cells. 
 */
  int i;

  STAILQ_INIT(&free_list);

  for (i = 0; i < DRIVER_MT_MAX_WORKERS; i++)
	STAILQ_INIT(&queue[i]);

  for (i = 0; i < MQ_SIZE; i++)
	STAILQ_INSERT_HEAD(&free_list, &pool[i], next);
}

/*===========================================================================*
 *				driver_mq_enqueue			     *
 *===========================================================================*/
PUBLIC int driver_mq_enqueue(thread_id_t thread_id, const message *mess,
  int ipc_status)
{
/* Add a message, including its IPC status, to the message queue of a thread.
 * Return TRUE iff the message was added successfully.
 */
  struct mq_cell *cell;

  assert(thread_id >= 0 && thread_id < DRIVER_MT_MAX_WORKERS);

  if (STAILQ_EMPTY(&free_list))
	return FALSE;

  cell = STAILQ_FIRST(&free_list);
  STAILQ_REMOVE_HEAD(&free_list, next);

  cell->mess = *mess;
  cell->ipc_status = ipc_status;

  STAILQ_INSERT_TAIL(&queue[thread_id], cell, next);

  return TRUE;
}

/*===========================================================================*
 *				driver_mq_dequeue			     *
 *===========================================================================*/
PUBLIC int driver_mq_dequeue(thread_id_t thread_id, message *mess,
  int *ipc_status)
{
/* Return and remove a message, including its IPC status, from the message
 * queue of a thread. Return TRUE iff a message was available.
 */
  struct mq_cell *cell;

  assert(thread_id >= 0 && thread_id < DRIVER_MT_MAX_WORKERS);

  if (STAILQ_EMPTY(&queue[thread_id]))
	return FALSE;

  cell = STAILQ_FIRST(&queue[thread_id]);
  STAILQ_REMOVE_HEAD(&queue[thread_id], next);

  *mess = cell->mess;
  *ipc_status = cell->ipc_status;

  STAILQ_INSERT_HEAD(&free_list, cell, next);

  return TRUE;
}
