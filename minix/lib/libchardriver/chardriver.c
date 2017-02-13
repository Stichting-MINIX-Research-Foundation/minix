/* This file contains the device independent character driver interface.
 *
 * Character drivers support the following requests. Message format m10 is
 * used. Field names are prefixed with CDEV_. Separate field names are used for
 * the "access", "ops", "user", and "request" fields.
 *
 *     m_type      MINOR    GRANT   COUNT   FLAGS   ID     POS_LO    POS_HI
 * +-------------+-------+--------+-------+-------+------+---------+--------+
 * | CDEV_OPEN   | minor | access | user  |       |  id  |         |        |
 * |-------------+-------+--------+-------+-------+------+---------+--------|
 * | CDEV_CLOSE  | minor |        |       |       |  id  |         |        |
 * |-------------+-------+--------+-------+-------+------+---------+--------|
 * | CDEV_READ   | minor | grant  | bytes | flags |  id  |     position     |
 * |-------------+-------+--------+-------+-------+------+---------+--------|
 * | CDEV_WRITE  | minor | grant  | bytes | flags |  id  |     position     |
 * |-------------+-------+--------+-------+-------+------+---------+--------|
 * | CDEV_IOCTL  | minor | grant  | user  | flags |  id  | request |        |
 * |-------------+-------+--------+-------+-------+------+---------+--------|
 * | CDEV_CANCEL | minor |        |       |       |  id  |         |        |
 * |-------------+-------+--------+-------+-------+------+---------+--------|
 * | CDEV_SELECT | minor |  ops   |       |       |      |         |        |
 * --------------------------------------------------------------------------
 *
 * The following reply messages are used.
 *
 *       m_type        MINOR   STATUS                ID
 * +-----------------+-------+--------+-----+-----+------+---------+--------+
 * | CDEV_REPLY      |       | status |     |     |  id  |         |        |
 * |-----------------+-------+--------+-----+-----+------+---------+--------|
 * | CDEV_SEL1_REPLY | minor | status |     |     |      |         |        |
 * |-----------------+-------+--------+-----+-----+------+---------+--------|
 * | CDEV_SEL2_REPLY | minor | status |     |     |      |         |        |
 * --------------------------------------------------------------------------
 *
 * Changes:
 *   Sep 01, 2013   complete rewrite of the API  (D.C. van Moolenboek)
 *   Aug 20, 2013   retire synchronous protocol  (D.C. van Moolenbroek)
 *   Oct 16, 2011   split character and block protocol  (D.C. van Moolenbroek)
 *   Aug 27, 2011   move common functions into driver.c  (A. Welzel)
 *   Jul 25, 2005   added SYS_SIG type for signals  (Jorrit N. Herder)
 *   Sep 15, 2004   added SYN_ALARM type for timeouts  (Jorrit N. Herder)
 *   Jul 23, 2004   removed kernel dependencies  (Jorrit N. Herder)
 *   Apr 02, 1992   constructed from AT wini and floppy driver  (Kees J. Bot)
 */

#include <assert.h>

#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <minix/ds.h>

static int running;

/* Management data for opened devices. */
static devminor_t open_devs[MAX_NR_OPEN_DEVICES];
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
static int is_open_dev(devminor_t minor)
{
/* Check whether the given minor device has previously been opened. */
  int i;

  for (i = 0; i < next_open_devs_slot; i++)
	if (open_devs[i] == minor)
		return TRUE;

  return FALSE;
}

/*===========================================================================*
 *				set_open_dev				     *
 *===========================================================================*/
static void set_open_dev(devminor_t minor)
{
/* Mark the given minor device as having been opened. */

  if (next_open_devs_slot >= MAX_NR_OPEN_DEVICES)
	panic("out of slots for open devices");

  open_devs[next_open_devs_slot] = minor;
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
  const char *driver_prefix = "drv.chr.";

  /* Callers are allowed to use ipc_sendrec to communicate with drivers.
   * For this reason, there may blocked callers when a driver restarts.
   * Ask the kernel to unblock them (if any).
   */
  if ((r = sys_statectl(SYS_STATE_CLEAR_IPC_REFS, 0, 0)) != OK)
	panic("chardriver_announce: sys_statectl failed: %d", r);

  /* Publish a driver up event. */
  if ((r = ds_retrieve_label_name(label, sef_self())) != OK)
	panic("chardriver_announce: unable to get own label: %d", r);

  snprintf(key, DS_MAX_KEYLEN, "%s%s", driver_prefix, label);
  if ((r = ds_publish_u32(key, DS_DRIVER_UP, DSF_OVERWRITE)) != OK)
	panic("chardriver_announce: unable to publish driver up event: %d", r);

  /* Expect an open for any device before serving regular driver requests. */
  clear_open_devs();
}

/*===========================================================================*
 *				chardriver_reply_task			     *
 *===========================================================================*/
void chardriver_reply_task(endpoint_t endpt, cdev_id_t id, int r)
{
/* Reply to a (read, write, ioctl) task request that was suspended earlier.
 * Not-so-well-written drivers may use this function to send a reply to a
 * request that is being processed right now, and then return EDONTREPLY later.
 */
  message m_reply;

  if (r == EDONTREPLY || r == SUSPEND)
	panic("chardriver: bad task reply: %d", r);

  memset(&m_reply, 0, sizeof(m_reply));

  m_reply.m_type = CDEV_REPLY;
  m_reply.m_lchardriver_vfs_reply.status = r;
  m_reply.m_lchardriver_vfs_reply.id = id;

  if ((r = asynsend3(endpt, &m_reply, AMF_NOREPLY)) != OK)
	printf("chardriver_reply_task: send to %d failed: %d\n", endpt, r);
}

/*===========================================================================*
 *				chardriver_reply_select			     *
 *===========================================================================*/
void chardriver_reply_select(endpoint_t endpt, devminor_t minor, int r)
{
/* Reply to a select request with a status update. This must not be used to
 * reply to a select request that is being processed right now.
 */
  message m_reply;

  /* Replying with an error is allowed (if unusual). */
  if (r == EDONTREPLY || r == SUSPEND)
	panic("chardriver: bad select reply: %d", r);

  memset(&m_reply, 0, sizeof(m_reply));

  m_reply.m_type = CDEV_SEL2_REPLY;
  m_reply.m_lchardriver_vfs_sel2.minor = minor;
  m_reply.m_lchardriver_vfs_sel2.status = r;

  if ((r = asynsend3(endpt, &m_reply, AMF_NOREPLY)) != OK)
	printf("chardriver_reply_select: send to %d failed: %d\n", endpt, r);
}

/*===========================================================================*
 *				send_reply				     *
 *===========================================================================*/
static void send_reply(endpoint_t endpt, message *m_ptr, int ipc_status)
{
/* Send a reply message to a request. */
  int r;

  /* If we would block sending the message, send it asynchronously. */
  if (IPC_STATUS_CALL(ipc_status) == SENDREC)
	r = ipc_sendnb(endpt, m_ptr);
  else
	r = asynsend3(endpt, m_ptr, AMF_NOREPLY);

  if (r != OK)
	printf("chardriver: unable to send reply to %d: %d\n", endpt, r);
}

/*===========================================================================*
 *				chardriver_reply			     *
 *===========================================================================*/
static void chardriver_reply(message *mess, int ipc_status, int r)
{
/* Prepare and send a reply message. */
  message reply_mess;

  /* If the EDONTREPLY pseudo-reply is given, we do not reply. This is however
   * allowed only for blocking task calls. Perform a sanity check.
   */
  if (r == EDONTREPLY) {
	switch (mess->m_type) {
	case CDEV_READ:
	case CDEV_WRITE:
	case CDEV_IOCTL:
		/* FIXME: we should be able to check FLAGS against
		 * CDEV_NONBLOCK here, but in practice, several drivers do not
		 * send a reply through this path (eg TTY) or simply do not
		 * implement nonblocking calls properly (eg audio).
		 */
#if 0
		if (mess->m_vfs_lchardriver_readwrite.flags & CDEV_NONBLOCK)
			panic("chardriver: cannot suspend nonblocking I/O");
#endif
		/*fall-through*/
	case CDEV_CANCEL:
		return;	/* alright */
	default:
		panic("chardriver: cannot suspend request %d", mess->m_type);
	}
  }

  if (r == SUSPEND)
	panic("chardriver: SUSPEND should not be used anymore");

  /* Do not reply with ERESTART. The only possible caller, VFS, will find out
   * through other means when we have restarted, and is not (fully) ready to
   * deal with ERESTART errors.
   */
  if (r == ERESTART)
	return;

  memset(&reply_mess, 0, sizeof(reply_mess));

  switch (mess->m_type) {
  case CDEV_OPEN:
  case CDEV_CLOSE:
	reply_mess.m_type = CDEV_REPLY;
	reply_mess.m_lchardriver_vfs_reply.status = r;
	reply_mess.m_lchardriver_vfs_reply.id =
		mess->m_vfs_lchardriver_openclose.id;
	break;

  case CDEV_READ:
  case CDEV_WRITE:
  case CDEV_IOCTL:
	reply_mess.m_type = CDEV_REPLY;
	reply_mess.m_lchardriver_vfs_reply.status = r;
	reply_mess.m_lchardriver_vfs_reply.id =
		mess->m_vfs_lchardriver_readwrite.id;
	break;

  case CDEV_CANCEL: /* For cancel, this is a reply to the original request! */
	reply_mess.m_type = CDEV_REPLY;
	reply_mess.m_lchardriver_vfs_reply.status = r;
	reply_mess.m_lchardriver_vfs_reply.id =
		mess->m_vfs_lchardriver_cancel.id;
	break;

  case CDEV_SELECT:
	reply_mess.m_type = CDEV_SEL1_REPLY;
	reply_mess.m_lchardriver_vfs_sel1.status = r;
	reply_mess.m_lchardriver_vfs_sel1.minor =
		mess->m_vfs_lchardriver_select.minor;
	break;

  default:
	panic("chardriver: unknown request %d", mess->m_type);
  }

  send_reply(mess->m_source, &reply_mess, ipc_status);
}

/*===========================================================================*
 *				do_open					     *
 *===========================================================================*/
static int do_open(const struct chardriver *cdp, message *m_ptr)
{
/* Open a minor device. */
  endpoint_t user_endpt;
  devminor_t minor;
  int r, bits;

  /* Default action if no open hook is in place. */
  if (cdp->cdr_open == NULL)
	return OK;

  /* Call the open hook. */
  minor = m_ptr->m_vfs_lchardriver_openclose.minor;
  bits = m_ptr->m_vfs_lchardriver_openclose.access;
  user_endpt = m_ptr->m_vfs_lchardriver_openclose.user;

  r = cdp->cdr_open(minor, bits, user_endpt);

  /* If the device has been cloned, mark the new minor as open too. */
  if (r >= 0 && (r & CDEV_CLONED)) {
	minor = r & ~(CDEV_CLONED | CDEV_CTTY);
	if (!is_open_dev(minor))
		set_open_dev(minor);
  }

  return r;
}

/*===========================================================================*
 *				do_close				     *
 *===========================================================================*/
static int do_close(const struct chardriver *cdp, message *m_ptr)
{
/* Close a minor device. */
  devminor_t minor;

  /* Default action if no close hook is in place. */
  if (cdp->cdr_close == NULL)
	return OK;

  /* Call the close hook. */
  minor = m_ptr->m_vfs_lchardriver_openclose.minor;

  return cdp->cdr_close(minor);
}

/*===========================================================================*
 *				do_trasnfer				     *
 *===========================================================================*/
static int do_transfer(const struct chardriver *cdp, message *m_ptr,
	int do_write)
{
/* Carry out a read or write task request. */
  devminor_t minor;
  off_t position;
  endpoint_t endpt;
  cp_grant_id_t grant;
  size_t size;
  int flags;
  cdev_id_t id;
  ssize_t r;

  minor = m_ptr->m_vfs_lchardriver_readwrite.minor;
  position = m_ptr->m_vfs_lchardriver_readwrite.pos;
  endpt = m_ptr->m_source;
  grant = m_ptr->m_vfs_lchardriver_readwrite.grant;
  size = m_ptr->m_vfs_lchardriver_readwrite.count;
  flags = m_ptr->m_vfs_lchardriver_readwrite.flags;
  id = m_ptr->m_vfs_lchardriver_readwrite.id;

  /* Call the read/write hook, if the appropriate one is in place. */
  if (!do_write && cdp->cdr_read != NULL)
	r = cdp->cdr_read(minor, position, endpt, grant, size, flags, id);
  else if (do_write && cdp->cdr_write != NULL)
	r = cdp->cdr_write(minor, position, endpt, grant, size, flags, id);
  else
	r = EIO; /* Default action if no read/write hook is in place. */

  return r;
}

/*===========================================================================*
 *				do_ioctl				     *
 *===========================================================================*/
static int do_ioctl(const struct chardriver *cdp, message *m_ptr)
{
/* Carry out an I/O control task request. */
  devminor_t minor;
  unsigned long request;
  cp_grant_id_t grant;
  endpoint_t endpt, user_endpt;
  int flags;
  cdev_id_t id;

  /* Default action if no ioctl hook is in place. */
  if (cdp->cdr_ioctl == NULL)
	return ENOTTY;

  /* Call the ioctl hook. */
  minor = m_ptr->m_vfs_lchardriver_readwrite.minor;
  request = m_ptr->m_vfs_lchardriver_readwrite.request;
  endpt = m_ptr->m_source;
  grant = m_ptr->m_vfs_lchardriver_readwrite.grant;
  flags = m_ptr->m_vfs_lchardriver_readwrite.flags;
  user_endpt = m_ptr->m_vfs_lchardriver_readwrite.user;
  id = m_ptr->m_vfs_lchardriver_readwrite.id;

  return cdp->cdr_ioctl(minor, request, endpt, grant, flags, user_endpt, id);
}

/*===========================================================================*
 *				do_cancel				     *
 *===========================================================================*/
static int do_cancel(const struct chardriver *cdp, message *m_ptr)
{
/* Cancel a suspended (read, write, ioctl) task request. The original request
 * may already have finished, in which case no reply should be sent.
 */
  devminor_t minor;
  endpoint_t endpt;
  cdev_id_t id;

  /* Default action if no cancel hook is in place: let the request finish. */
  if (cdp->cdr_cancel == NULL)
	return EDONTREPLY;

  /* Call the cancel hook. */
  minor = m_ptr->m_vfs_lchardriver_cancel.minor;
  endpt = m_ptr->m_source;
  id = m_ptr->m_vfs_lchardriver_cancel.id;

  return cdp->cdr_cancel(minor, endpt, id);
}

/*===========================================================================*
 *				do_select				     *
 *===========================================================================*/
static int do_select(const struct chardriver *cdp, message *m_ptr)
{
/* Perform a select query on a minor device. */
  devminor_t minor;
  unsigned int ops;
  endpoint_t endpt;

  /* Default action if no select hook is in place. */
  if (cdp->cdr_select == NULL)
	return EBADF;

  /* Call the select hook. */
  minor = m_ptr->m_vfs_lchardriver_select.minor;
  ops = m_ptr->m_vfs_lchardriver_select.ops;
  endpt = m_ptr->m_source;

  return cdp->cdr_select(minor, ops, endpt);
}

/*===========================================================================*
 *				do_block_open				     *
 *===========================================================================*/
static void do_block_open(message *m_ptr, int ipc_status)
{
/* Reply to a block driver open request stating there is no such device. */
  message m_reply;

  memset(&m_reply, 0, sizeof(m_reply));

  m_reply.m_type = BDEV_REPLY;
  m_reply.m_lblockdriver_lbdev_reply.status = ENXIO;
  m_reply.m_lblockdriver_lbdev_reply.id = m_ptr->m_lbdev_lblockdriver_msg.id;

  send_reply(m_ptr->m_source, &m_reply, ipc_status);
}

/*===========================================================================*
 *				chardriver_process			     *
 *===========================================================================*/
void chardriver_process(const struct chardriver *cdp, message *m_ptr,
	int ipc_status)
{
/* Call the appropiate driver function, based on the type of request. Send a
 * reply to the caller if necessary.
 */
  int r;

  /* Check for notifications first. We never reply to notifications. */
  if (is_ipc_notify(ipc_status)) {
	switch (_ENDPOINT_P(m_ptr->m_source)) {
	case HARDWARE:
		if (cdp->cdr_intr)
			cdp->cdr_intr(m_ptr->m_notify.interrupts);
		break;

	case CLOCK:
		if (cdp->cdr_alarm)
			cdp->cdr_alarm(m_ptr->m_notify.timestamp);
		break;

	default:
		if (cdp->cdr_other)
			cdp->cdr_other(m_ptr, ipc_status);
	}

	return; /* do not send a reply */
  }

  /* Reply to block driver open requests with an error code. Otherwise, if
   * someone creates a block device node for a character driver, opening that
   * device node will cause the corresponding VFS thread to block forever.
   */
  if (m_ptr->m_type == BDEV_OPEN) {
	do_block_open(m_ptr, ipc_status);

	return;
  }

  if (IS_CDEV_RQ(m_ptr->m_type)) {
	int minor;

	/* Try to retrieve minor device number */
	r = chardriver_get_minor(m_ptr, &minor);

	if (OK != r)
		return;

	/* We might get spurious requests if the driver has been restarted.
	 * Deny any requests on devices that have not previously been opened.
	 */
	if (!is_open_dev(minor)) {
		/* Ignore spurious requests for unopened devices. */
		if (m_ptr->m_type != CDEV_OPEN)
			return; /* do not send a reply */

		/* Mark the device as opened otherwise. */
		set_open_dev(minor);
	}
  }

  /* Call the appropriate function(s) for this request. */
  switch (m_ptr->m_type) {
  case CDEV_OPEN:	r = do_open(cdp, m_ptr);		break;
  case CDEV_CLOSE:	r = do_close(cdp, m_ptr);		break;
  case CDEV_READ:	r = do_transfer(cdp, m_ptr, FALSE);	break;
  case CDEV_WRITE:	r = do_transfer(cdp, m_ptr, TRUE);	break;
  case CDEV_IOCTL:	r = do_ioctl(cdp, m_ptr);		break;
  case CDEV_CANCEL:	r = do_cancel(cdp, m_ptr);		break;
  case CDEV_SELECT:	r = do_select(cdp, m_ptr);		break;
  default:
	if (cdp->cdr_other)
		cdp->cdr_other(m_ptr, ipc_status);
	return; /* do not send a reply */
  }

  chardriver_reply(m_ptr, ipc_status, r);
}

/*===========================================================================*
 *				chardriver_terminate			     *
 *===========================================================================*/
void chardriver_terminate(void)
{
/* Break out of the main loop after finishing the current request. */

  running = FALSE;

  sef_cancel();
}

/*===========================================================================*
 *				chardriver_task				     *
 *===========================================================================*/
void chardriver_task(const struct chardriver *cdp)
{
/* Main program of any character device driver task. */
  int r, ipc_status;
  message mess;

  running = TRUE;

  /* Here is the main loop of the character driver task.  It waits for a
   * message, carries it out, and sends a reply.
   */
  while (running) {
	if ((r = sef_receive_status(ANY, &mess, &ipc_status)) != OK) {
		if (r == EINTR && !running)
			break;

		panic("chardriver: sef_receive_status failed: %d", r);
	}

	chardriver_process(cdp, &mess, ipc_status);
  }
}

/*===========================================================================*
 *				chardriver_get_minor			     *
 *===========================================================================*/
int chardriver_get_minor(const message *m, devminor_t *minor)
{
  assert(NULL != m);
  assert(NULL != minor);

  switch(m->m_type)
  {
	case CDEV_OPEN:
	case CDEV_CLOSE:
	    *minor = m->m_vfs_lchardriver_openclose.minor;
	    return OK;
	case CDEV_CANCEL:
	    *minor = m->m_vfs_lchardriver_cancel.minor;
	    return OK;
	case CDEV_SELECT:
	    *minor = m->m_vfs_lchardriver_select.minor;
	    return OK;
	case CDEV_READ:
	case CDEV_WRITE:
	case CDEV_IOCTL:
	    *minor = m->m_vfs_lchardriver_readwrite.minor;
	    return OK;
	default:
	    return EINVAL;
  }
}
