/* This file contains the singlethreaded device driver interface.
 *
 * Changes:
 *   Aug 27, 2011   extracted from driver.c (A. Welzel)
 *
 * The entry points into this file are:
 *   blockdriver_task:		the main message loop of the driver
 *   blockdriver_terminate:	break out of the main message loop
 *   blockdriver_handle_msg:	handle a single received message
 *   blockdriver_receive_mq:	message receive interface with message queueing
 *   blockdriver_mq_queue:	queue an incoming message for later processing
 */

#include <minix/drivers.h>
#include <minix/blockdriver.h>

#include "const.h"
#include "driver.h"
#include "mq.h"

static int running;

/*===========================================================================*
 *			       blockdriver_receive_mq			     *
 *===========================================================================*/
int blockdriver_receive_mq(message *m_ptr, int *status_ptr)
{
/* receive() interface for drivers with message queueing. */

  /* Any queued messages? */
  if (mq_dequeue(SINGLE_THREAD, m_ptr, status_ptr))
	return OK;

  /* Fall back to standard receive() interface otherwise. */
  return driver_receive(ANY, m_ptr, status_ptr);
}

/*===========================================================================*
 *				blockdriver_terminate			     *
 *===========================================================================*/
void blockdriver_terminate(void)
{
/* Break out of the main driver loop after finishing the current request. */

  running = FALSE;
}

/*===========================================================================*
 *				blockdriver_task			     *
 *===========================================================================*/
void blockdriver_task(struct blockdriver *bdp)
{
/* Main program of any block device driver task. */
  int r, ipc_status;
  message mess;

  running = TRUE;

  /* Here is the main loop of the disk task.  It waits for a message, carries
   * it out, and sends a reply.
   */
  while (running) {
	if ((r = blockdriver_receive_mq(&mess, &ipc_status)) != OK)
		panic("blockdriver_receive_mq failed: %d", r);

	blockdriver_process(bdp, &mess, ipc_status);
  }
}

/*===========================================================================*
 *				blockdriver_process			     *
 *===========================================================================*/
void blockdriver_process(struct blockdriver *bdp, message *m_ptr,
  int ipc_status)
{
/* Handle the given received message. */
  int r;

  /* Process the notification or request. */
  if (is_ipc_notify(ipc_status)) {
	blockdriver_handle_notify(bdp, m_ptr);

	/* Do not reply to notifications. */
  } else {
	r = blockdriver_handle_request(bdp, m_ptr, SINGLE_THREAD);

	blockdriver_reply(m_ptr, ipc_status, r);
  }
}

/*===========================================================================*
 *				blockdriver_mq_queue			     *
 *===========================================================================*/
int blockdriver_mq_queue(message *m, int status)
{
/* Queue a message for later processing. */

  return mq_enqueue(SINGLE_THREAD, m, status);
}
