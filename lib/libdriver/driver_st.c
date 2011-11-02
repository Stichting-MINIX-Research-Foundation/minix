/* This file contains the singlethreaded device independent driver interface.
 *
 * Changes:
 *   Aug 27, 2011   extracted from driver.c (A. Welzel)
 *
 * The entry points into this file are:
 *   driver_task:	the main message loop of the driver
 *   driver_terminate:	break out of the main message loop
 *   driver_handle_msg:	handle a single received message
 *   driver_receive:	message receive interface for drivers
 *   driver_receive_mq:	message receive interface; try the message queue first
 *   driver_mq_queue:	queue an incoming message for later processing
 */

#include <minix/drivers.h>
#include <minix/driver.h>

#include "driver.h"
#include "mq.h"

PUBLIC endpoint_t device_endpt;		/* used externally by log driver */
PRIVATE int driver_running;

/*===========================================================================*
 *				driver_receive				     *
 *===========================================================================*/
PUBLIC int driver_receive(endpoint_t src, message *m_ptr, int *status_ptr)
{
/* receive() interface for drivers. */

  return sef_receive_status(src, m_ptr, status_ptr);
}

/*===========================================================================*
 *			       driver_receive_mq			     *
 *===========================================================================*/
PUBLIC int driver_receive_mq(message *m_ptr, int *status_ptr)
{
/* receive() interface for drivers with message queueing. */

  /* Any queued messages? */
  if (driver_mq_dequeue(DRIVER_MQ_SINGLE, m_ptr, status_ptr))
  	return OK;

  /* Fall back to standard receive() interface otherwise. */
  return driver_receive(ANY, m_ptr, status_ptr);
}

/*===========================================================================*
 *				driver_terminate			     *
 *===========================================================================*/
PUBLIC void driver_terminate(void)
{
/* Break out of the main driver loop after finishing the current request. */

  driver_running = FALSE;
}

/*===========================================================================*
 *				driver_task				     *
 *===========================================================================*/
PUBLIC void driver_task(dp, type)
struct driver *dp;	/* Device dependent entry points. */
int type;		/* Driver type (DRIVER_STD or DRIVER_ASYN) */
{
/* Main program of any device driver task. */
  int r, ipc_status;
  message mess;

  driver_running = TRUE;

  /* Here is the main loop of the disk task.  It waits for a message, carries
   * it out, and sends a reply.
   */
  while (driver_running) {
	if ((r = driver_receive_mq(&mess, &ipc_status)) != OK)
		panic("driver_receive_mq failed: %d", r);

	driver_handle_msg(dp, type, &mess, ipc_status);
  }
}

/*===========================================================================*
 *				driver_handle_msg			     *
 *===========================================================================*/
PUBLIC void driver_handle_msg(struct driver *dp, int driver_type,
  message *m_ptr, int ipc_status)
{
/* Handle the given received message. */
  int r;

  /* Dirty hack: set a global variable for the log driver. */
  device_endpt = m_ptr->USER_ENDPT;

  /* Process the notification or request. */
  if (is_ipc_notify(ipc_status)) {
	driver_handle_notify(dp, m_ptr);

	/* Do not reply to notifications. */
  } else {
	r = driver_handle_request(dp, m_ptr);

	driver_reply(driver_type, m_ptr, ipc_status, r);
  }
}

/*===========================================================================*
 *				driver_mq_queue				     *
 *===========================================================================*/
PUBLIC int driver_mq_queue(message *m, int status)
{
/* Queue a message for later processing. */

  return driver_mq_enqueue(DRIVER_MQ_SINGLE, m, status);
}
