/* This file contains the device independent block driver interface.
 *
 * Block drivers support the following requests. Message format m10 is used.
 * Field names are prefixed with BDEV_. Separate field names are used for the
 * "access", "request", and "user" fields.
 *
 *    m_type        MINOR     COUNT     GRANT   FLAGS    ID    REQUEST  POS
 * +--------------+--------+----------+-------+-------+------+---------+------+
 * | BDEV_OPEN    | minor  |  access  |       |       |  id  |         |      |
 * |--------------+--------+----------+-------+-------+------+---------+------|
 * | BDEV_CLOSE   | minor  |          |       |       |  id  |         |      |
 * |--------------+--------+----------+-------+-------+------+---------+------|
 * | BDEV_READ    | minor  |  bytes   | grant | flags |  id  |         | pos. |
 * |--------------+--------+----------+-------+-------+------+---------+------|
 * | BDEV_WRITE   | minor  |  bytes   | grant | flags |  id  |         | pos. |
 * |--------------+--------+----------+-------+-------+------+---------+------|
 * | BDEV_GATHER  | minor  | elements | grant | flags |  id  |         | pos. |
 * |--------------+--------+----------+-------+-------+------+---------+------|
 * | BDEV_SCATTER | minor  | elements | grant | flags |  id  |         | pos. |
 * |--------------+--------+----------+-------+-------+------+---------+------|
 * | BDEV_IOCTL   | minor  |          | grant | user  |  id  | request |      |
 * ----------------------------------------------------------------------------
 *
 * The following reply message is used for all requests.
 *
 *    m_type        STATUS				 ID
 * +--------------+--------+----------+-------+-------+------+---------+------+
 * | BDEV_REPLY   | status |          |       |       |  id  |         |      |
 * ----------------------------------------------------------------------------
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
#include <minix/blockdriver.h>
#include <minix/ds.h>
#include <sys/ioc_block.h>
#include <sys/ioc_disk.h>

#include "driver.h"
#include "mq.h"
#include "trace.h"

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
 *				blockdriver_announce			     *
 *===========================================================================*/
void blockdriver_announce(int type)
{
/* Announce we are up after a fresh start or a restart. */
  int r;
  char key[DS_MAX_KEYLEN];
  char label[DS_MAX_KEYLEN];
  char *driver_prefix = "drv.blk.";

  /* Callers are allowed to use ipc_sendrec to communicate with drivers.
   * For this reason, there may blocked callers when a driver restarts.
   * Ask the kernel to unblock them (if any). Note that most block drivers
   * will not restart statefully, and thus will skip this code.
   */
  if (type == SEF_INIT_RESTART) {
	if ((r = sys_statectl(SYS_STATE_CLEAR_IPC_REFS)) != OK)
		panic("blockdriver_init: sys_statectl failed: %d", r);
  }

  /* Publish a driver up event. */
  if ((r = ds_retrieve_label_name(label, sef_self())) != OK)
	panic("blockdriver_init: unable to get own label: %d", r);

  snprintf(key, DS_MAX_KEYLEN, "%s%s", driver_prefix, label);
  if ((r = ds_publish_u32(key, DS_DRIVER_UP, DSF_OVERWRITE)) != OK)
	panic("blockdriver_init: unable to publish driver up event: %d", r);

  /* Expect an open for any device before serving regular driver requests. */
  clear_open_devs();

  /* Initialize or reset the message queue. */
  mq_init();
}

/*===========================================================================*
 *				send_reply				     *
 *===========================================================================*/
static void send_reply(endpoint_t endpt, message *m_ptr, int ipc_status)
{
/* Send a reply message to a request. */
  int r;

  /* If we would block sending the message, send it asynchronously. The NOREPLY
   * flag is set because the caller may also issue a SENDREC (mixing sync and
   * async comm), and the asynchronous reply could otherwise end up satisfying
   * the SENDREC's receive part, after which our next SENDNB call would fail.
   */
  if (IPC_STATUS_CALL(ipc_status) == SENDREC)
	r = ipc_sendnb(endpt, m_ptr);
  else
	r = asynsend3(endpt, m_ptr, AMF_NOREPLY);

  if (r != OK)
	printf("blockdriver: unable to send reply to %d: %d\n", endpt, r);
}

/*===========================================================================*
 *				blockdriver_reply			     *
 *===========================================================================*/
void blockdriver_reply(message *m_ptr, int ipc_status, int reply)
{
/* Reply to a block request sent to the driver. */
  message m_reply;

  if (reply == EDONTREPLY)
	return;

  memset(&m_reply, 0, sizeof(m_reply));

  m_reply.m_type = BDEV_REPLY;
  m_reply.m_lblockdriver_lbdev_reply.status = reply;
  m_reply.m_lblockdriver_lbdev_reply.id = m_ptr->m_lbdev_lblockdriver_msg.id;

  send_reply(m_ptr->m_source, &m_reply, ipc_status);
}

/*===========================================================================*
 *				do_open					     *
 *===========================================================================*/
static int do_open(struct blockdriver *bdp, message *mp)
{
/* Open a minor device. */

  return (*bdp->bdr_open)(mp->m_lbdev_lblockdriver_msg.minor, mp->m_lbdev_lblockdriver_msg.access);
}

/*===========================================================================*
 *				do_close				     *
 *===========================================================================*/
static int do_close(struct blockdriver *bdp, message *mp)
{
/* Close a minor device. */

  return (*bdp->bdr_close)(mp->m_lbdev_lblockdriver_msg.minor);
}

/*===========================================================================*
 *				do_rdwt					     *
 *===========================================================================*/
static int do_rdwt(struct blockdriver *bdp, message *mp)
{
/* Carry out a single read or write request. */
  iovec_t iovec1;
  u64_t position;
  int do_write;
  ssize_t r;

  /* Disk address?  Address and length of the user buffer? */
  if (mp->m_lbdev_lblockdriver_msg.count < 0) return EINVAL;

  /* Create a one element scatter/gather vector for the buffer. */
  iovec1.iov_addr = mp->m_lbdev_lblockdriver_msg.grant;
  iovec1.iov_size = mp->m_lbdev_lblockdriver_msg.count;

  /* Transfer bytes from/to the device. */
  do_write = (mp->m_type == BDEV_WRITE);
  position = mp->m_lbdev_lblockdriver_msg.pos;

  r = (*bdp->bdr_transfer)(mp->m_lbdev_lblockdriver_msg.minor, do_write, position, mp->m_source,
	&iovec1, 1, mp->m_lbdev_lblockdriver_msg.flags);

  /* Return the number of bytes transferred or an error code. */
  return r;
}

/*===========================================================================*
 *				do_vrdwt				     *
 *===========================================================================*/
static int do_vrdwt(struct blockdriver *bdp, message *mp, thread_id_t id)
{
/* Carry out an device read or write to/from a vector of buffers. */
  iovec_t iovec[NR_IOREQS];
  unsigned int nr_req;
  u64_t position;
  int i, do_write;
  ssize_t r, size;

  /* Copy the vector from the caller to kernel space. */
  nr_req = mp->m_lbdev_lblockdriver_msg.count;	/* Length of I/O vector */
  if (nr_req > NR_IOREQS) nr_req = NR_IOREQS;

  if (OK != sys_safecopyfrom(mp->m_source, (vir_bytes) mp->m_lbdev_lblockdriver_msg.grant,
		0, (vir_bytes) iovec, nr_req * sizeof(iovec[0]))) {
	printf("blockdriver: bad I/O vector by: %d\n", mp->m_source);
	return EINVAL;
  }

  /* Check for overflow condition, and update the size for block tracing. */
  for (i = size = 0; i < nr_req; i++) {
	if ((ssize_t) (size + iovec[i].iov_size) < size) return EINVAL;
	size += iovec[i].iov_size;
  }

  trace_setsize(id, size);

  /* Transfer bytes from/to the device. */
  do_write = (mp->m_type == BDEV_SCATTER);
  position = mp->m_lbdev_lblockdriver_msg.pos;

  r = (*bdp->bdr_transfer)(mp->m_lbdev_lblockdriver_msg.minor, do_write, position, mp->m_source,
	iovec, nr_req, mp->m_lbdev_lblockdriver_msg.flags);

  /* Return the number of bytes transferred or an error code. */
  return r;
}

/*===========================================================================*
 *				do_dioctl				     *
 *===========================================================================*/
static int do_dioctl(struct blockdriver *bdp, dev_t minor,
  unsigned long request, endpoint_t endpt, cp_grant_id_t grant)
{
/* Carry out a disk-specific I/O control request. */
  struct device *dv;
  struct part_geom entry;
  int r = EINVAL;

  switch (request) {
  case DIOCSETP:
	/* Copy just this one partition table entry. */
	r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &entry,
		sizeof(entry));
	if (r != OK)
		return r;

	if ((dv = (*bdp->bdr_part)(minor)) == NULL)
		return ENXIO;
	dv->dv_base = entry.base;
	dv->dv_size = entry.size;

	break;

  case DIOCGETP:
	/* Return a partition table entry and the geometry of the drive. */
	if ((dv = (*bdp->bdr_part)(minor)) == NULL)
		return ENXIO;
	entry.base = dv->dv_base;
	entry.size = dv->dv_size;
	if (bdp->bdr_geometry) {
		(*bdp->bdr_geometry)(minor, &entry);
	} else {
		/* The driver doesn't care -- make up fake geometry. */
		entry.cylinders = (unsigned long)(entry.size / SECTOR_SIZE) / 
								  (64 * 32);
		entry.heads = 64;
		entry.sectors = 32;
	}

	r = sys_safecopyto(endpt, grant, 0, (vir_bytes) &entry, sizeof(entry));

	break;
  }

  return r;
}

/*===========================================================================*
 *				do_ioctl				     *
 *===========================================================================*/
static int do_ioctl(struct blockdriver *bdp, message *mp)
{
/* Carry out an I/O control request. We forward block trace control requests
 * to the tracing module, and handle setting/getting partitions when the driver
 * has specified that it is a disk driver.
 */
  dev_t minor;
  unsigned long request;
  cp_grant_id_t grant;
  endpoint_t user_endpt;
  int r;

  minor = mp->m_lbdev_lblockdriver_msg.minor;
  request = mp->m_lbdev_lblockdriver_msg.request;
  grant = mp->m_lbdev_lblockdriver_msg.grant;
  user_endpt = mp->m_lbdev_lblockdriver_msg.user;

  switch (request) {
  case BIOCTRACEBUF:
  case BIOCTRACECTL:
  case BIOCTRACEGET:
	/* Block trace control. */
	r = trace_ctl(minor, request, mp->m_source, grant);

	break;

  case DIOCSETP:
  case DIOCGETP:
	/* Handle disk-specific IOCTLs only for disk-type drivers. */
	if (bdp->bdr_type == BLOCKDRIVER_TYPE_DISK) {
		/* Disk partition control. */
		r = do_dioctl(bdp, minor, request, mp->m_source, grant);

		break;
	}

	/* fall-through */
  default:
	if (bdp->bdr_ioctl)
		r = (*bdp->bdr_ioctl)(minor, request, mp->m_source, grant,
			user_endpt);
	else
		r = ENOTTY;
  }

  return r;
}

/*===========================================================================*
 *				do_char_open				     *
 *===========================================================================*/
static void do_char_open(message *m_ptr, int ipc_status)
{
/* Reply to a character driver open request stating there is no such device. */
  message m_reply;

  memset(&m_reply, 0, sizeof(m_reply));

  m_reply.m_type = CDEV_REPLY;
  m_reply.m_lchardriver_vfs_reply.status = ENXIO;
  m_reply.m_lchardriver_vfs_reply.id = m_ptr->m_vfs_lchardriver_openclose.id;

  send_reply(m_ptr->m_source, &m_reply, ipc_status);
}

/*===========================================================================*
 *				blockdriver_process_on_thread		     *
 *===========================================================================*/
void blockdriver_process_on_thread(struct blockdriver *bdp, message *m_ptr,
	int ipc_status, thread_id_t id)
{
/* Call the appropiate driver function, based on the type of request. Send
 * a result code to the caller. The call is processed in the context of the
 * given thread ID, which may be SINGLE_THREAD for single-threaded callers.
 */
  int r;

  /* Check for notifications first. We never reply to notifications. */
  if (is_ipc_notify(ipc_status)) {
	switch (_ENDPOINT_P(m_ptr->m_source)) {
	case HARDWARE:
		if (bdp->bdr_intr)
			(*bdp->bdr_intr)(m_ptr->m_notify.interrupts);
		break;

	case CLOCK:
		if (bdp->bdr_alarm)
			(*bdp->bdr_alarm)(m_ptr->m_notify.timestamp);
		break;

	default:
		if (bdp->bdr_other)
			(*bdp->bdr_other)(m_ptr, ipc_status);
	}

	return; /* do not send a reply */
  }

  /* Reply to character driver open requests with an error code. Otherwise, if
   * someone creates a character device node for a block driver, opening that
   * device node will cause the corresponding VFS thread to block forever.
   */
  if (m_ptr->m_type == CDEV_OPEN) {
	do_char_open(m_ptr, ipc_status);

	return;
  }

  /* We might get spurious requests if the driver has been restarted. Deny any
   * requests on devices that have not previously been opened, signaling the
   * caller that something went wrong.
   */
  if (IS_BDEV_RQ(m_ptr->m_type) && !is_open_dev(m_ptr->m_lbdev_lblockdriver_msg.minor)) {
	/* Reply ERESTART to spurious requests for unopened devices. */
	if (m_ptr->m_type != BDEV_OPEN) {
		blockdriver_reply(m_ptr, ipc_status, ERESTART);

		return;
	}

	/* Mark the device as opened otherwise. */
	set_open_dev(m_ptr->m_lbdev_lblockdriver_msg.minor);
  }

  trace_start(id, m_ptr);

  /* Call the appropriate function(s) for this request. */
  switch (m_ptr->m_type) {
  case BDEV_OPEN:	r = do_open(bdp, m_ptr);	break;
  case BDEV_CLOSE:	r = do_close(bdp, m_ptr);	break;
  case BDEV_READ:
  case BDEV_WRITE:	r = do_rdwt(bdp, m_ptr);	break;
  case BDEV_GATHER:
  case BDEV_SCATTER:	r = do_vrdwt(bdp, m_ptr, id);	break;
  case BDEV_IOCTL:	r = do_ioctl(bdp, m_ptr);	break;
  default:
	if (bdp->bdr_other != NULL)
		(*bdp->bdr_other)(m_ptr, ipc_status);

	return;	/* do not send a reply */
  }

  /* Let the driver perform any cleanup. */
  if (bdp->bdr_cleanup != NULL)
	(*bdp->bdr_cleanup)();

  trace_finish(id, r);

  blockdriver_reply(m_ptr, ipc_status, r);
}
