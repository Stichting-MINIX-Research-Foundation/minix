/* This file contains a simple message queue implementation to support both
 * the singlethread and the multithreaded driver implementation.
 *
 * Changes:
 *   Oct 27, 2011   rewritten to use sys/queue.h (D.C. van Moolenbroek)
 *   Aug 27, 2011   integrated into libblockdriver (A. Welzel)
 */

#include <minix/blockdriver_mt.h>
#include <sys/queue.h>
#include <assert.h>

#include "const.h"
#include "mq.h"

#define MQ_SIZE		128

struct mq_cell {
  message mess;
  int ipc_status;
  STAILQ_ENTRY(mq_cell) next;
};

static struct mq_cell pool[MQ_SIZE];
static STAILQ_HEAD(queue, mq_cell) queue[MAX_DEVICES];
static STAILQ_HEAD(free_list, mq_cell) free_list;

/*===========================================================================*
 *				mq_init					     *
 *===========================================================================*/
void mq_init(void)
{
/* Initialize the message queues and message cells.
 */
  int i;

  STAILQ_INIT(&free_list);

  for (i = 0; i < MAX_DEVICES; i++)
	STAILQ_INIT(&queue[i]);

  for (i = 0; i < MQ_SIZE; i++)
	STAILQ_INSERT_HEAD(&free_list, &pool[i], next);
}

/*===========================================================================*
 *				mq_enqueue				     *
 *===========================================================================*/
int mq_enqueue(device_id_t device_id, const message *mess,
  int ipc_status)
{
/* Add a message, including its IPC status, to the message queue of a device.
 * Return TRUE iff the message was added successfully.
 */
  struct mq_cell *cell;

  assert(device_id >= 0 && device_id < MAX_DEVICES);

  if (STAILQ_EMPTY(&free_list))
	return FALSE;

  cell = STAILQ_FIRST(&free_list);
  STAILQ_REMOVE_HEAD(&free_list, next);

  cell->mess = *mess;
  cell->ipc_status = ipc_status;

  STAILQ_INSERT_TAIL(&queue[device_id], cell, next);

  return TRUE;
}

/*===========================================================================*
 *				mq_isempty				     *
 *===========================================================================*/
int mq_isempty(device_id_t device_id)
{
/* Return whether the message queue for the given device is empty.
 */

  assert(device_id >= 0 && device_id < MAX_DEVICES);

  return STAILQ_EMPTY(&queue[device_id]);
}

/*===========================================================================*
 *				mq_dequeue				     *
 *===========================================================================*/
int mq_dequeue(device_id_t device_id, message *mess, int *ipc_status)
{
/* Return and remove a message, including its IPC status, from the message
 * queue of a thread. Return TRUE iff a message was available.
 */
  struct mq_cell *cell;

  if (mq_isempty(device_id))
	return FALSE;

  cell = STAILQ_FIRST(&queue[device_id]);
  STAILQ_REMOVE_HEAD(&queue[device_id], next);

  *mess = cell->mess;
  *ipc_status = cell->ipc_status;

  STAILQ_INSERT_HEAD(&free_list, cell, next);

  return TRUE;
}
