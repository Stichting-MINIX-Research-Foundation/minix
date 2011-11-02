/* This file contains the common part of the device driver interface.
 * In addition, callers get to choose between the singlethreaded API
 * (in driver_st.c) and the multithreaded API (in driver_mt.c).
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
 * Changes:
 *   Aug 27, 2011   split common functions into driver_common.c (A. Welzel)
 *   Jul 25, 2005   added SYS_SIG type for signals  (Jorrit N. Herder)
 *   Sep 15, 2004   added SYN_ALARM type for timeouts  (Jorrit N. Herder)
 *   Jul 23, 2004   removed kernel dependencies  (Jorrit N. Herder)
 *   Apr 02, 1992   constructed from AT wini and floppy driver  (Kees J. Bot)
 */

#include <minix/drivers.h>
#include <sys/ioc_disk.h>
#include <minix/driver.h>
#include <minix/ds.h>

#include "driver.h"
#include "mq.h"

/* Management data for opened devices. */
PRIVATE int open_devs[MAX_NR_OPEN_DEVICES];
PRIVATE int next_open_devs_slot = 0;

/*===========================================================================*
 *				clear_open_devs				     *
 *===========================================================================*/
PRIVATE void clear_open_devs(void)
{
/* Reset the set of previously opened minor devices. */
  next_open_devs_slot = 0;
}

/*===========================================================================*
 *				is_open_dev				     *
 *===========================================================================*/
PRIVATE int is_open_dev(int device)
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
PRIVATE void set_open_dev(int device)
{
/* Mark the given minor device as having been opened. */

  if (next_open_devs_slot >= MAX_NR_OPEN_DEVICES)
	panic("out of slots for open devices");

  open_devs[next_open_devs_slot] = device;
  next_open_devs_slot++;
}

/*===========================================================================*
 *				asyn_reply				     *
 *===========================================================================*/
PRIVATE void asyn_reply(message *mess, int r)
{
/* Send a reply using the asynchronous character device protocol. */
  message reply_mess;

  /* Do not reply with ERESTART in this protocol. The only possible caller,
   * VFS, will find out through other means when we have restarted, and is not
   * (fully) ready to deal with ERESTART errors.
   */
  if (r == ERESTART)
	return;

  switch (mess->m_type) {
  case DEV_OPEN:
	reply_mess.m_type = DEV_REVIVE;
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
 *				standard_reply				     *
 *===========================================================================*/
PRIVATE void standard_reply(message *m_ptr, int ipc_status, int reply)
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
 *				driver_reply				     *
 *===========================================================================*/
PUBLIC void driver_reply(int driver_type, message *m_ptr, int ipc_status,
  int reply)
{
/* Prepare and send a reply message. */

  if (reply == EDONTREPLY)
	return;

  switch (driver_type) {
  case DRIVER_STD:
	standard_reply(m_ptr, ipc_status, reply);

	break;

  case DRIVER_ASYN:
	asyn_reply(m_ptr, reply);

	break;

  default:
	panic("unknown driver type: %d", driver_type);
  }
}

/*===========================================================================*
 *				driver_announce				     *
 *===========================================================================*/
PUBLIC void driver_announce(void)
{
/* Announce we are up after a fresh start or restart. */
  int r;
  char key[DS_MAX_KEYLEN];
  char label[DS_MAX_KEYLEN];
  char *driver_prefix = "drv.vfs.";

  /* Callers are allowed to use sendrec to communicate with drivers.
   * For this reason, there may blocked callers when a driver restarts.
   * Ask the kernel to unblock them (if any).
   */
#if USE_STATECTL
  r = sys_statectl(SYS_STATE_CLEAR_IPC_REFS);
  if (r != OK) {
	panic("driver_announce: sys_statectl failed: %d\n", r);
  }
#endif

  /* Publish a driver up event. */
  r = ds_retrieve_label_name(label, getprocnr());
  if (r != OK) {
	panic("driver_announce: unable to get own label: %d\n", r);
  }
  snprintf(key, DS_MAX_KEYLEN, "%s%s", driver_prefix, label);
  r = ds_publish_u32(key, DS_DRIVER_UP, DSF_OVERWRITE);
  if (r != OK) {
	panic("driver_announce: unable to publish driver up event: %d\n", r);
  }

  /* Expect a DEV_OPEN for any device before serving regular driver requests. */
  clear_open_devs();

  driver_mq_init();
}

/*===========================================================================*
 *				do_rdwt					     *
 *===========================================================================*/
PRIVATE int do_rdwt(struct driver *dp, message *mp)
{
/* Carry out a single read or write request. */
  iovec_t iovec1;
  int r, opcode;
  u64_t position;

  /* Disk address?  Address and length of the user buffer? */
  if (mp->COUNT < 0) return(EINVAL);

  /* Prepare for I/O. */
  if ((*dp->dr_prepare)(mp->DEVICE) == NULL) return(ENXIO);

  /* Create a one element scatter/gather vector for the buffer. */
  if(mp->m_type == DEV_READ_S) opcode = DEV_GATHER_S;
  else	opcode =  DEV_SCATTER_S;

  iovec1.iov_addr = (vir_bytes) mp->IO_GRANT;
  iovec1.iov_size = mp->COUNT;

  /* Transfer bytes from/to the device. */
  position= make64(mp->POSITION, mp->HIGHPOS);
  r = (*dp->dr_transfer)(mp->m_source, opcode, position, &iovec1, 1);

  /* Return the number of bytes transferred or an error code. */
  return(r == OK ? (int) (mp->COUNT - iovec1.iov_size) : r);
}

/*===========================================================================*
 *				do_vrdwt				     *
 *===========================================================================*/
PRIVATE int do_vrdwt(struct driver *dp, message *mp)
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
		0, (vir_bytes) iovec, iovec_size, D)) {
	printf("bad I/O vector by: %d\n", mp->m_source);
	return(EINVAL);
  }

  /* Prepare for I/O. */
  if ((*dp->dr_prepare)(mp->DEVICE) == NULL) return(ENXIO);

  /* Transfer bytes from/to the device. */
  opcode = mp->m_type;
  position= make64(mp->POSITION, mp->HIGHPOS);
  r = (*dp->dr_transfer)(mp->m_source, opcode, position, iovec, nr_req);

  /* Copy the I/O vector back to the caller. */
  if (OK != sys_safecopyto(mp->m_source, (vir_bytes) mp->IO_GRANT, 
		0, (vir_bytes) iovec, iovec_size, D)) {
	printf("couldn't return I/O vector: %d\n", mp->m_source);
	return(EINVAL);
  }

  return(r);
}

/*===========================================================================*
 *				driver_handle_notify			     *
 *===========================================================================*/
PUBLIC void driver_handle_notify(struct driver *dp, message *m_ptr)
{
/* Take care of the notifications (interrupt and clock messages) by calling the
 * appropiate callback functions. Never send a reply.
 */

  /* Call the appropriate function for this notification. */
  switch (_ENDPOINT_P(m_ptr->m_source)) {
  case HARDWARE:
	if (dp->dr_hw_int)
		dp->dr_hw_int(dp, m_ptr);
	break;

  case CLOCK:
	if (dp->dr_alarm)
		dp->dr_alarm(dp, m_ptr);
	break;

  default:
	if (dp->dr_other)
		(void) dp->dr_other(dp, m_ptr);
  }
}

/*===========================================================================*
 *				driver_handle_request			     *
 *===========================================================================*/
PUBLIC int driver_handle_request(struct driver *dp, message *m_ptr)
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
  if (IS_DEV_MINOR_RQ(m_ptr->m_type) && !is_open_dev(m_ptr->DEVICE)) {
	/* Reply ERESTART to spurious requests for unopened devices. */
	if (m_ptr->m_type != DEV_OPEN)
		return ERESTART;

	/* Mark the device as opened otherwise. */
	set_open_dev(m_ptr->DEVICE);
  }

  /* Call the appropriate function(s) for this request. */
  switch (m_ptr->m_type) {
  case DEV_OPEN:	r = (*dp->dr_open)(dp, m_ptr);		break;	
  case DEV_CLOSE:	r = (*dp->dr_close)(dp, m_ptr);		break;
  case DEV_IOCTL_S:	r = (*dp->dr_ioctl)(dp, m_ptr);		break;
  case CANCEL:		r = (*dp->dr_cancel)(dp, m_ptr);	break;
  case DEV_SELECT:	r = (*dp->dr_select)(dp, m_ptr);	break;
  case DEV_READ_S:
  case DEV_WRITE_S:	r = do_rdwt(dp, m_ptr);			break;
  case DEV_GATHER_S:
  case DEV_SCATTER_S:	r = do_vrdwt(dp, m_ptr);		break;
  default:
	if (dp->dr_other)
		r = dp->dr_other(dp, m_ptr);
	else
		r = EINVAL;
  }

  /* Let the driver perform any cleanup. */
  (*dp->dr_cleanup)();

  return r;
}

/*===========================================================================*
 *				no_name					     *
 *===========================================================================*/
PUBLIC char *no_name(void)
{
/* Use this default name if there is no specific name for the device. This was
 * originally done by fetching the name from the task table for this process: 
 * "return(tasktab[proc_number(proc_ptr) + NR_TASKS].name);", but currently a
 * real "noname" is returned. Perhaps, some system information service can be
 * queried for a name at a later time.
 */
  static char name[] = "noname";
  return name;
}

/*===========================================================================*
 *				do_nop					     *
 *===========================================================================*/
PUBLIC int do_nop(struct driver *UNUSED(dp), message *mp)
{
/* Nothing there, or nothing to do. */

  switch (mp->m_type) {
  case DEV_OPEN:	return(ENODEV);
  case DEV_CLOSE:	return(OK);
  case DEV_IOCTL_S:	
  default:		printf("nop: ignoring code %d\n", mp->m_type);
			return(EIO);
  }
}

/*===========================================================================*
 *				nop_ioctl				     *
 *===========================================================================*/
PUBLIC int nop_ioctl(struct driver *UNUSED(dp), message *UNUSED(mp))
{
  return(ENOTTY);
}

/*===========================================================================*
 *				nop_alarm				     *
 *===========================================================================*/
PUBLIC void nop_alarm(struct driver *UNUSED(dp), message *UNUSED(mp))
{
/* Ignore the leftover alarm. */
}

/*===========================================================================*
 *				nop_prepare				     *
 *===========================================================================*/
PUBLIC struct device *nop_prepare(int UNUSED(device))
{
/* Nothing to prepare for. */
  return(NULL);
}

/*===========================================================================*
 *				nop_cleanup				     *
 *===========================================================================*/
PUBLIC void nop_cleanup(void)
{
/* Nothing to clean up. */
}

/*===========================================================================*
 *				nop_cancel				     *
 *===========================================================================*/
PUBLIC int nop_cancel(struct driver *UNUSED(dr), message *UNUSED(m))
{
/* Nothing to do for cancel. */
   return(OK);
}

/*===========================================================================*
 *				nop_select				     *
 *===========================================================================*/
PUBLIC int nop_select(struct driver *UNUSED(dr), message *UNUSED(m))
{
/* Nothing to do for select. */
   return(OK);
}

/*===========================================================================*
 *				do_diocntl				     *
 *===========================================================================*/
PUBLIC int do_diocntl(struct driver *dp, message *mp)
{
/* Carry out a partition setting/getting request. */
  struct device *dv;
  struct partition entry;
  unsigned int request;
  int s;

  request = mp->REQUEST;

  if (request != DIOCSETP && request != DIOCGETP) {
  	if(dp->dr_other)
		return dp->dr_other(dp, mp);
  	else
		return(ENOTTY);
  }

  /* Decode the message parameters. */
  if ((dv = (*dp->dr_prepare)(mp->DEVICE)) == NULL) return(ENXIO);

  if (request == DIOCSETP) {
	/* Copy just this one partition table entry. */
	s=sys_safecopyfrom(mp->m_source, (vir_bytes) mp->IO_GRANT, 
		0, (vir_bytes) &entry, sizeof(entry), D);
	if(s != OK)
	    return s;
	dv->dv_base = entry.base;
	dv->dv_size = entry.size;
  } else {
	/* Return a partition table entry and the geometry of the drive. */
	entry.base = dv->dv_base;
	entry.size = dv->dv_size;
	(*dp->dr_geometry)(&entry);
	s=sys_safecopyto(mp->m_source, (vir_bytes) mp->IO_GRANT, 
		0, (vir_bytes) &entry, sizeof(entry), D);
        if (OK != s) 
	    return s;
  }
  return(OK);
}
