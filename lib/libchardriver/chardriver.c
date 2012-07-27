/* This file contains the device independent character driver interface.
 *
 * The drivers support the following operations (using message format m2):
 *
 *    m_type         DEVICE  USER_ENDPT  COUNT   POSITION  HIGHPOS   IO_GRANT
 * ----------------------------------------------------------------------------
 * | DEV_OPEN      | device | proc nr |  mode   |        |        |           |
 * |---------------+--------+---------+---------+--------+--------+-----------|
 * | DEV_CLOSE     | device | proc nr |         |        |        |           |
 * |---------------+--------+---------+---------+--------+--------+-----------|
 * | DEV_READ_S    | device | proc nr |  bytes  | off lo | off hi i buf grant |
 * |---------------+--------+---------+---------+--------+--------+-----------|
 * | DEV_WRITE_S   | device | proc nr |  bytes  | off lo | off hi | buf grant |
 * |---------------+--------+---------+---------+--------+--------+-----------|
 * | DEV_GATHER_S  | device | proc nr | iov len | off lo | off hi | iov grant |
 * |---------------+--------+---------+---------+--------+--------+-----------|
 * | DEV_SCATTER_S | device | proc nr | iov len | off lo | off hi | iov grant |
 * |---------------+--------+---------+---------+--------+--------+-----------|
 * | DEV_IOCTL_S   | device | proc nr | request |        |        | buf grant |
 * |---------------+--------+---------+---------+--------+--------+-----------|
 * | CANCEL        | device | proc nr |   r/w   |        |        |   grant   |
 * |---------------+--------+---------+---------+--------+--------+-----------|
 * | DEV_SELECT    | device |   ops   |         |        |        |           |
 * ----------------------------------------------------------------------------
 *
 * The entry points into this file are:
 *   driver_task:	the main message loop of the driver
 *   driver_receive:	message receive interface for drivers
 *
 * Changes:
 *   Oct 16, 2011   split character and block protocol  (D.C. van Moolenbroek)
 *   Aug 27, 2011   move common functions into driver.c  (A. Welzel)
 *   Jul 25, 2005   added SYS_SIG type for signals  (Jorrit N. Herder)
 *   Sep 15, 2004   added SYN_ALARM type for timeouts  (Jorrit N. Herder)
 *   Jul 23, 2004   removed kernel dependencies  (Jorrit N. Herder)
 *   Apr 02, 1992   constructed from AT wini and floppy driver  (Kees J. Bot)
 */

#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <minix/ds.h>

static int running;

/* Management data for opened devices. */
static int open_devs[MAX_NR_OPEN_DEVICES];
static int next_open_devs_slot = 0;

/*===========================================================================*
 *				clear_open_devs				     *
 *===========================================================================*/
static void clear_open_devs(void)
{
/* Reset the set of previously opened minor devices. */
  next_open_devs_slot = 0;
}

/*===========================================================================*
 *				is_open_dev				     *
 *===========================================================================*/
static int is_open_dev(int device)
{
/* Check whether the given minor device has previously been opened. */
  int i;

  for (i = 0; i < next_open_devs_slot; i++)
	if (open_devs[i] == device)
		return TRUE;

  return FALSE;
}

/*===========================================================================*
 *				set_open_dev				     *
 *===========================================================================*/
static void set_open_dev(int device)
{
/* Mark the given minor device as having been opened. */

  if (next_open_devs_slot >= MAX_NR_OPEN_DEVICES)
	panic("out of slots for open devices");

  open_devs[next_open_devs_slot] = device;
  next_open_devs_slot++;
}

/*===========================================================================*
 *				chardriver_announce			     *
 *===========================================================================*/
void chardriver_announce(void)
{
/* Announce we are up after a fresh start or restart. */
  int r;
  char key[DS_MAX_KEYLEN];
  char label[DS_MAX_KEYLEN];
  char *driver_prefix = "drv.chr.";

  /* Callers are allowed to use sendrec to communicate with drivers.
   * For this reason, there may blocked callers when a driver restarts.
   * Ask the kernel to unblock them (if any).
   */
#if USE_STATECTL
  if ((r = sys_statectl(SYS_STATE_CLEAR_IPC_REFS)) != OK)
	panic("chardriver_init: sys_statectl failed: %d", r);
#endif

  /* Publish a driver up event. */
  if ((r = ds_retrieve_label_name(label, getprocnr())) != OK)
	panic("chardriver_init: unable to get own label: %d", r);

  snprintf(key, DS_MAX_KEYLEN, "%s%s", driver_prefix, label);
  if ((r = ds_publish_u32(key, DS_DRIVER_UP, DSF_OVERWRITE)) != OK)
	panic("chardriver_init: unable to publish driver up event: %d", r);

  /* Expect a DEV_OPEN for any device before serving regular driver requests. */
  clear_open_devs();
}

/*===========================================================================*
 *				async_reply				     *
 *===========================================================================*/
static void async_reply(message *mess, int r)
{
/* Send a reply using the asynchronous character device protocol. */
  message reply_mess;

  /* Do not reply with ERESTART in this protocol. The only possible caller,
   * VFS, will find out through other means when we have restarted, and is not
   * (fully) ready to deal with ERESTART errors.
   */
  if (r == ERESTART)
	return;

  memset(&reply_mess, 0, sizeof(reply_mess));

  switch (mess->m_type) {
  case DEV_OPEN:
	reply_mess.m_type = DEV_OPEN_REPL;
	reply_mess.REP_ENDPT = mess->USER_ENDPT;
	reply_mess.REP_STATUS = r;
	break;

  case DEV_CLOSE:
	reply_mess.m_type = DEV_CLOSE_REPL;
	reply_mess.REP_ENDPT = mess->USER_ENDPT;
	reply_mess.REP_STATUS = r;
	break;

  case DEV_READ_S:
  case DEV_WRITE_S:
  case DEV_IOCTL_S:
	if (r == SUSPEND)
		printf("driver_task: reviving %d (%d) with SUSPEND\n",
			mess->m_source, mess->USER_ENDPT);

	reply_mess.m_type = DEV_REVIVE;
	reply_mess.REP_ENDPT = mess->USER_ENDPT;
	reply_mess.REP_IO_GRANT = (cp_grant_id_t) mess->IO_GRANT;
	reply_mess.REP_STATUS = r;
	break;

  case CANCEL:
	/* The original request should send a reply. */
	return;

  case DEV_SELECT:
	reply_mess.m_type = DEV_SEL_REPL1;
	reply_mess.DEV_MINOR = mess->DEVICE;
	reply_mess.DEV_SEL_OPS = r;
	break;

  default:
	reply_mess.m_type = TASK_REPLY;
	reply_mess.REP_ENDPT = mess->USER_ENDPT;
	/* Status is # of bytes transferred or error code. */
	reply_mess.REP_STATUS = r;
	break;
  }

  r = asynsend(mess->m_source, &reply_mess);

  if (r != OK)
	printf("asyn_reply: unable to asynsend reply to %d: %d\n",
		mess->m_source, r);
}

/*===========================================================================*
 *				sync_reply				     *
 *===========================================================================*/
static void sync_reply(message *m_ptr, int ipc_status, int reply)
{
/* Reply to a message sent to the driver. */
  endpoint_t caller_e, user_e;
  int r;

  caller_e = m_ptr->m_source;
  user_e = m_ptr->USER_ENDPT;

  m_ptr->m_type = TASK_REPLY;
  m_ptr->REP_ENDPT = user_e;
  m_ptr->REP_STATUS = reply;

  /* If we would block sending the message, send it asynchronously. */
  if (IPC_STATUS_CALL(ipc_status) == SENDREC)
	r = sendnb(caller_e, m_ptr);
  else
	r = asynsend(caller_e, m_ptr);

  if (r != OK)
	printf("driver_reply: unable to send reply to %d: %d\n", caller_e, r);
}

/*===========================================================================*
 *				send_reply				     *
 *===========================================================================*/
static void send_reply(int type, message *m_ptr, int ipc_status, int reply)
{
/* Prepare and send a reply message. */

  if (reply == EDONTREPLY)
	return;

  if (type == CHARDRIVER_ASYNC)
	async_reply(m_ptr, reply);
  else
	sync_reply(m_ptr, ipc_status, reply);
}

/*===========================================================================*
 *				do_rdwt					     *
 *===========================================================================*/
static int do_rdwt(struct chardriver *cdp, message *mp)
{
/* Carry out a single read or write request. */
  iovec_t iovec1;
  int r, opcode;
  u64_t position;

  /* Disk address?  Address and length of the user buffer? */
  if (mp->COUNT < 0) return(EINVAL);

  /* Prepare for I/O. */
  if ((*cdp->cdr_prepare)(mp->DEVICE) == NULL) return(ENXIO);

  /* Create a one element scatter/gather vector for the buffer. */
  if(mp->m_type == DEV_READ_S)
	  opcode = DEV_GATHER_S;
  else
	  opcode =  DEV_SCATTER_S;

  iovec1.iov_addr = (vir_bytes) mp->IO_GRANT;
  iovec1.iov_size = mp->COUNT;

  /* Transfer bytes from/to the device. */
  position= make64(mp->POSITION, mp->HIGHPOS);
  r = (*cdp->cdr_transfer)(mp->m_source, opcode, position, &iovec1, 1,
	mp->USER_ENDPT, mp->FLAGS);

  /* Return the number of bytes transferred or an error code. */
  return(r == OK ? (int) (mp->COUNT - iovec1.iov_size) : r);
}

/*===========================================================================*
 *				do_vrdwt				     *
 *===========================================================================*/
static int do_vrdwt(struct chardriver *cdp, message *mp)
{
/* Carry out an device read or write to/from a vector of user addresses.
 * The "user addresses" are assumed to be safe, i.e. FS transferring to/from
 * its own buffers, so they are not checked.
 */
  iovec_t iovec[NR_IOREQS];
  phys_bytes iovec_size;
  unsigned nr_req;
  int r, opcode;
  u64_t position;

  nr_req = mp->COUNT;	/* Length of I/O vector */

  /* Copy the vector from the caller to kernel space. */
  if (nr_req > NR_IOREQS) nr_req = NR_IOREQS;
  iovec_size = (phys_bytes) (nr_req * sizeof(iovec[0]));

  if (OK != sys_safecopyfrom(mp->m_source, (vir_bytes) mp->IO_GRANT,
		0, (vir_bytes) iovec, iovec_size)) {
	printf("bad I/O vector by: %d\n", mp->m_source);
	return(EINVAL);
  }

  /* Prepare for I/O. */
  if ((*cdp->cdr_prepare)(mp->DEVICE) == NULL) return(ENXIO);

  /* Transfer bytes from/to the device. */
  opcode = mp->m_type;
  position= make64(mp->POSITION, mp->HIGHPOS);
  r = (*cdp->cdr_transfer)(mp->m_source, opcode, position, iovec, nr_req,
	mp->USER_ENDPT, mp->FLAGS);

  /* Copy the I/O vector back to the caller. */
  if (OK != sys_safecopyto(mp->m_source, (vir_bytes) mp->IO_GRANT,
		0, (vir_bytes) iovec, iovec_size)) {
	printf("couldn't return I/O vector: %d\n", mp->m_source);
	return(EINVAL);
  }

  return(r);
}

/*===========================================================================*
 *				handle_notify				     *
 *===========================================================================*/
static void handle_notify(struct chardriver *cdp, message *m_ptr)
{
/* Take care of the notifications (interrupt and clock messages) by calling the
 * appropiate callback functions. Never send a reply.
 */

  /* Call the appropriate function for this notification. */
  switch (_ENDPOINT_P(m_ptr->m_source)) {
  case CLOCK:
	if (cdp->cdr_alarm)
		cdp->cdr_alarm(m_ptr);
	break;

  default:
	if (cdp->cdr_other)
		(void) cdp->cdr_other(m_ptr);
  }
}

/*===========================================================================*
 *				handle_request				     *
 *===========================================================================*/
static int handle_request(struct chardriver *cdp, message *m_ptr)
{
/* Call the appropiate driver function, based on the type of request. Return
 * the result code that is to be sent back to the caller, or EDONTREPLY if no
 * reply is to be sent.
 */
  int r;

  /* We might get spurious requests if the driver has been restarted. Deny any
   * requests on devices that have not previously been opened, signaling the
   * caller that something went wrong.
   */
  if (IS_CDEV_MINOR_RQ(m_ptr->m_type) && !is_open_dev(m_ptr->DEVICE)) {
	/* Reply ERESTART to spurious requests for unopened devices. */
	if (m_ptr->m_type != DEV_OPEN)
		return ERESTART;

	/* Mark the device as opened otherwise. */
	set_open_dev(m_ptr->DEVICE);
  }

  /* Call the appropriate function(s) for this request. */
  switch (m_ptr->m_type) {
  case DEV_OPEN:	r = (*cdp->cdr_open)(m_ptr);	break;
  case DEV_CLOSE:	r = (*cdp->cdr_close)(m_ptr);	break;
  case DEV_IOCTL_S:	r = (*cdp->cdr_ioctl)(m_ptr);	break;
  case CANCEL:		r = (*cdp->cdr_cancel)(m_ptr);	break;
  case DEV_SELECT:	r = (*cdp->cdr_select)(m_ptr);	break;
  case DEV_READ_S:
  case DEV_WRITE_S:	r = do_rdwt(cdp, m_ptr);	break;
  case DEV_GATHER_S:
  case DEV_SCATTER_S:	r = do_vrdwt(cdp, m_ptr);	break;
  default:
	if (cdp->cdr_other)
		r = cdp->cdr_other(m_ptr);
	else
		r = EINVAL;
  }

  /* Let the driver perform any cleanup. */
  if (cdp->cdr_cleanup)
	(*cdp->cdr_cleanup)();

  return r;
}

/*===========================================================================*
 *				chardriver_process			     *
 *===========================================================================*/
void chardriver_process(struct chardriver *cdp, int driver_type,
  message *m_ptr, int ipc_status)
{
/* Handle the given received message. */
  int r;

  /* Process the notification or request. */
  if (is_ipc_notify(ipc_status)) {
	handle_notify(cdp, m_ptr);

	/* Do not reply to notifications. */
  } else {
	r = handle_request(cdp, m_ptr);

	send_reply(driver_type, m_ptr, ipc_status, r);
  }
}

/*===========================================================================*
 *				chardriver_task				     *
 *===========================================================================*/
void chardriver_task(struct chardriver *cdp, int driver_type)
{
/* Main program of any device driver task. */
  int r, ipc_status;
  message mess;

  running = TRUE;

  /* Here is the main loop of the disk task.  It waits for a message, carries
   * it out, and sends a reply.
   */
  while (running) {
	if ((r = sef_receive_status(ANY, &mess, &ipc_status)) != OK)
		panic("driver_receive failed: %d", r);

	chardriver_process(cdp, driver_type, &mess, ipc_status);
  }
}

/*===========================================================================*
 *				do_nop					     *
 *===========================================================================*/
int do_nop(message *UNUSED(mp))
{
  return(OK);
}

/*===========================================================================*
 *				nop_ioctl				     *
 *===========================================================================*/
int nop_ioctl(message *UNUSED(mp))
{
  return(ENOTTY);
}

/*===========================================================================*
 *				nop_alarm				     *
 *===========================================================================*/
void nop_alarm(message *UNUSED(mp))
{
/* Ignore the leftover alarm. */
}

/*===========================================================================*
 *				nop_cleanup				     *
 *===========================================================================*/
void nop_cleanup(void)
{
/* Nothing to clean up. */
}

/*===========================================================================*
 *				nop_cancel				     *
 *===========================================================================*/
int nop_cancel(message *UNUSED(m))
{
/* Nothing to do for cancel. */
   return(OK);
}

/*===========================================================================*
 *				nop_select				     *
 *===========================================================================*/
int nop_select(message *UNUSED(m))
{
/* Nothing to do for select. */
   return(OK);
}
