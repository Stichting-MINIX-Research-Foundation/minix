/* When a needed block is not in the cache, it must be fetched from the disk.
 * Special character files also require I/O.  The routines for these are here.
 *
 * The entry points in this file are:
 *   dev_open:   open a character device
 *   dev_reopen: reopen a character device after a driver crash
 *   dev_close:  close a character device
 *   cdev_reply: process the result of a character driver request
 *   bdev_open:  open a block device
 *   bdev_close: close a block device
 *   bdev_reply: process the result of a block driver request
 *   dev_io:	 FS does a read or write on a device
 *   gen_opcl:   generic call to a character driver to perform an open/close
 *   gen_io:     generic call to a character driver to initiate I/O
 *   no_dev:     open/close processing for devices that don't exist
 *   no_dev_io:  i/o processing for devices that don't exist
 *   tty_opcl:   perform tty-specific processing for open/close
 *   ctty_opcl:  perform controlling-tty-specific processing for open/close
 *   ctty_io:    perform controlling-tty-specific processing for I/O
 *   pm_setsid:	 perform VFS's side of setsid system call
 *   do_ioctl:	 perform the IOCTL system call
 *   dev_select: initiate a select call on a device
 *   dev_cancel: cancel an I/O request, blocking until it has been cancelled
 */

#include "fs.h"
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/endpoint.h>
#include <minix/ioctl.h>
#include <minix/u64.h>
#include "file.h"
#include "scratchpad.h"
#include "dmap.h"
#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"
#include "param.h"

static int block_io(endpoint_t driver_e, message *mess_ptr);
static cp_grant_id_t make_grant(endpoint_t driver_e, endpoint_t user_e, int op,
	void *buf, unsigned long size);
static void restart_reopen(devmajor_t major);
static void reopen_reply(message *m_ptr);

static int dummyproc;

/*===========================================================================*
 *				dev_open				     *
 *===========================================================================*/
int dev_open(
  dev_t dev,			/* device to open */
  int flags			/* mode bits and flags */
)
{
/* Open a character device. */
  devmajor_t major_dev;
  int r;

  /* Determine the major device number so as to call the device class specific
   * open/close routine.  (This is the only routine that must check the
   * device number for being in range.  All others can trust this check.)
   */
  major_dev = major(dev);
  if (major_dev < 0 || major_dev >= NR_DEVICES) return(ENXIO);
  if (dmap[major_dev].dmap_driver == NONE) return(ENXIO);
  r = (*dmap[major_dev].dmap_opcl)(DEV_OPEN, dev, fp->fp_endpoint, flags);
  return(r);
}


/*===========================================================================*
 *				dev_reopen				     *
 *===========================================================================*/
int dev_reopen(
  dev_t dev,			/* device to open */
  int filp_no,			/* filp to reopen for */
  int flags			/* mode bits and flags */
)
{
/* Reopen a character device after a failing device driver. */
  devmajor_t major_dev;
  struct dmap *dp;
  int r;

  /* Determine the major device number and call the device class specific
   * open/close routine.  (This is the only routine that must check the device
   * number for being in range.  All others can trust this check.)
   */
  major_dev = major(dev);
  if (major_dev < 0 || major_dev >= NR_DEVICES) return(ENXIO);
  dp = &dmap[major_dev];
  if (dp->dmap_driver == NONE) return(ENXIO);
  r = (*dp->dmap_opcl)(DEV_REOPEN, dev, filp_no, flags);
  if (r == SUSPEND) r = OK;
  return(r);
}


/*===========================================================================*
 *				dev_close				     *
 *===========================================================================*/
int dev_close(
  dev_t dev			/* device to close */
)
{
/* Close a character device. */
  devmajor_t major_dev;
  int r;

  /* See if driver is roughly valid. */
  major_dev = major(dev);
  if (major_dev < 0 || major_dev >= NR_DEVICES) return(ENXIO);
  if (dmap[major_dev].dmap_driver == NONE) return(ENXIO);
  r = (*dmap[major_dev].dmap_opcl)(DEV_CLOSE, dev, fp->fp_endpoint, 0);
  return(r);
}


/*===========================================================================*
 *				bdev_open				     *
 *===========================================================================*/
int bdev_open(dev_t dev, int access)
{
/* Open a block device. */
  devmajor_t major_dev;
  devminor_t minor_dev;
  message dev_mess;
  int r;

  major_dev = major(dev);
  minor_dev = minor(dev);
  if (major_dev < 0 || major_dev >= NR_DEVICES) return ENXIO;
  if (dmap[major_dev].dmap_driver == NONE) return ENXIO;

  memset(&dev_mess, 0, sizeof(dev_mess));
  dev_mess.m_type = BDEV_OPEN;
  dev_mess.BDEV_MINOR = minor_dev;
  dev_mess.BDEV_ACCESS = 0;
  if (access & R_BIT) dev_mess.BDEV_ACCESS |= BDEV_R_BIT;
  if (access & W_BIT) dev_mess.BDEV_ACCESS |= BDEV_W_BIT;
  dev_mess.BDEV_ID = 0;

  /* Call the task. */
  r = block_io(dmap[major_dev].dmap_driver, &dev_mess);
  if (r != OK)
	return r;

  return dev_mess.BDEV_STATUS;
}


/*===========================================================================*
 *				bdev_close				     *
 *===========================================================================*/
int bdev_close(dev_t dev)
{
/* Close a block device. */
  devmajor_t major_dev;
  devminor_t minor_dev;
  message dev_mess;
  int r;

  major_dev = major(dev);
  minor_dev = minor(dev);
  if (major_dev < 0 || major_dev >= NR_DEVICES) return ENXIO;
  if (dmap[major_dev].dmap_driver == NONE) return ENXIO;

  memset(&dev_mess, 0, sizeof(dev_mess));
  dev_mess.m_type = BDEV_CLOSE;
  dev_mess.BDEV_MINOR = minor_dev;
  dev_mess.BDEV_ID = 0;

  r = block_io(dmap[major_dev].dmap_driver, &dev_mess);
  if (r != OK)
	return r;

  return dev_mess.BDEV_STATUS;
}


/*===========================================================================*
 *				bdev_ioctl				     *
 *===========================================================================*/
static int bdev_ioctl(dev_t dev, endpoint_t proc_e, unsigned long req,
	void *buf)
{
/* Perform an I/O control operation on a block device. */
  struct dmap *dp;
  cp_grant_id_t gid;
  message dev_mess;
  devmajor_t major_dev;
  devminor_t minor_dev;

  major_dev = major(dev);
  minor_dev = minor(dev);

  /* Determine task dmap. */
  dp = &dmap[major_dev];
  if (dp->dmap_driver == NONE) {
	printf("VFS: bdev_ioctl: no driver for major %d\n", major_dev);
	return(ENXIO);
  }

  /* Set up a grant if necessary. */
  gid = make_grant(dp->dmap_driver, proc_e, BDEV_IOCTL, buf, req);

  /* Set up the message passed to the task. */
  memset(&dev_mess, 0, sizeof(dev_mess));

  dev_mess.m_type = BDEV_IOCTL;
  dev_mess.BDEV_MINOR = minor_dev;
  dev_mess.BDEV_REQUEST = req;
  dev_mess.BDEV_GRANT = gid;
  dev_mess.BDEV_USER = proc_e;
  dev_mess.BDEV_ID = 0;

  /* Call the task. */
  block_io(dp->dmap_driver, &dev_mess);

  /* Clean up. */
  if (GRANT_VALID(gid)) cpf_revoke(gid);

  if (dp->dmap_driver == NONE) {
	printf("VFS: block driver gone!?\n");
	return(EIO);
  }

  /* Return the result. */
  return(dev_mess.BDEV_STATUS);
}


/*===========================================================================*
 *				find_suspended_ep			     *
 *===========================================================================*/
static endpoint_t find_suspended_ep(endpoint_t driver, cp_grant_id_t g)
{
/* A process is suspended on a driver for which VFS issued a grant. Find out
 * which process it was.
 */
  struct fproc *rfp;
  for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	if(rfp->fp_pid == PID_FREE)
		continue;

	if (rfp->fp_task == driver && rfp->fp_grant == g) {
		assert(!fp_is_blocked(rfp) ||
			rfp->fp_blocked_on == FP_BLOCKED_ON_OTHER);
		return(rfp->fp_endpoint);
	}
  }

  return(NONE);
}


/*===========================================================================*
 *				make_grant				     *
 *===========================================================================*/
static cp_grant_id_t make_grant(endpoint_t driver_e, endpoint_t user_e, int op,
	void *buf, unsigned long bytes)
{
/* Create a magic grant for the given operation and buffer. */
  cp_grant_id_t gid;
  int access;
  size_t size;

  switch (op) {
  case DEV_READ_S:
  case DEV_WRITE_S:
	gid = cpf_grant_magic(driver_e, user_e, (vir_bytes) buf,
		(size_t) bytes, op == DEV_READ_S ? CPF_WRITE : CPF_READ);
	break;

  case DEV_IOCTL_S:
  case BDEV_IOCTL:
	/* For IOCTLs, the bytes parameter contains the IOCTL request.
	 * This request encodes the requested access method and buffer size.
	 */
	access = 0;
	if(_MINIX_IOCTL_IOR(bytes)) access |= CPF_WRITE;
	if(_MINIX_IOCTL_IOW(bytes)) access |= CPF_READ;
	if(_MINIX_IOCTL_BIG(bytes))
		size = _MINIX_IOCTL_SIZE_BIG(bytes);
	else
		size = _MINIX_IOCTL_SIZE(bytes);

	/* Grant access to the buffer even if no I/O happens with the ioctl, in
	 * order to disambiguate requests with DEV_IOCTL_S.
	 */
	gid = cpf_grant_magic(driver_e, user_e, (vir_bytes) buf, size, access);
	break;

  default:
	panic("VFS: unknown operation %d", op);
  }

  if (!GRANT_VALID(gid))
	panic("VFS: cpf_grant_magic failed");

  return gid;
}


/*===========================================================================*
 *				dev_io					     *
 *===========================================================================*/
int dev_io(
  int op,			/* DEV_READ_S, DEV_WRITE_S, or DEV_IOCTL_S */
  dev_t dev,			/* major-minor device number */
  endpoint_t proc_e,		/* in whose address space is buf? */
  void *buf,			/* virtual address of the buffer */
  off_t pos,			/* byte position */
  size_t bytes,			/* how many bytes to transfer */
  int flags,			/* special flags, like O_NONBLOCK */
  int suspend_reopen		/* Just suspend the process */
)
{
/* Initiate a read, write, or ioctl to a device. */
  struct dmap *dp;
  message dev_mess;
  cp_grant_id_t gid;
  devmajor_t major_dev;
  devminor_t minor_dev;
  int r;

  assert(op == DEV_READ_S || op == DEV_WRITE_S || op == DEV_IOCTL_S);

  /* Determine task dmap. */
  major_dev = major(dev);
  minor_dev = minor(dev);
  dp = &dmap[major_dev];

  /* See if driver is roughly valid. */
  if (dp->dmap_driver == NONE) return(ENXIO);

  if (suspend_reopen) {
	/* Suspend user. */
	fp->fp_grant = GRANT_INVALID;
	fp->fp_ioproc = NONE;
	wait_for(dp->dmap_driver);
	fp->fp_flags |= FP_SUSP_REOPEN;
	return(SUSPEND);
  }

  if(isokendpt(dp->dmap_driver, &dummyproc) != OK) {
	printf("VFS: dev_io: old driver for major %x (%d)\n", major_dev,
		dp->dmap_driver);
	return(ENXIO);
  }

  /* Create a grant for the buffer provided by the user process. */
  gid = make_grant(dp->dmap_driver, proc_e, op, buf, bytes);

  /* Set up the rest of the message that will be sent to the driver. */
  memset(&dev_mess, 0, sizeof(dev_mess));
  dev_mess.m_type   = op;
  dev_mess.DEVICE   = minor_dev;
  if (op == DEV_IOCTL_S) {
	dev_mess.REQUEST  = bytes;
	dev_mess.POSITION = proc_e;
  } else {
	dev_mess.POSITION = ex64lo(pos);
	dev_mess.COUNT    = bytes;
  }
  dev_mess.HIGHPOS  = ex64hi(pos);
  dev_mess.USER_ENDPT = VFS_PROC_NR;
  dev_mess.IO_GRANT = (void *) gid;
  dev_mess.FLAGS    = 0;
  if (flags & O_NONBLOCK)
	  dev_mess.FLAGS |= FLG_OP_NONBLOCK;

  /* Send the request to the driver. */
  r = (*dp->dmap_io)(dp->dmap_driver, &dev_mess);

  if (r != OK) {
	cpf_revoke(gid);

	return r;
  }

  /* Suspend the calling process until a reply arrives. */
  wait_for(dp->dmap_driver);
  assert(!GRANT_VALID(fp->fp_grant));
  fp->fp_grant = gid;	/* revoke this when unsuspended. */
  fp->fp_ioproc = VFS_PROC_NR;

  return SUSPEND;
}


/*===========================================================================*
 *				clone_cdev				     *
 *===========================================================================*/
static int cdev_clone(dev_t dev, endpoint_t proc_e, devminor_t new_minor)
{
/* A new minor device number has been returned. Request PFS to create a
 * temporary device file to hold it.
 */
  struct vnode *vp;
  struct node_details res;
  int r;

  assert(proc_e == fp->fp_endpoint);

  /* Device number of the new device. */
  dev = makedev(major(dev), new_minor);

  /* Issue request */
  r = req_newnode(PFS_PROC_NR, fp->fp_effuid, fp->fp_effgid,
      ALL_MODES | I_CHAR_SPECIAL, dev, &res);
  if (r != OK) {
	(void) gen_opcl(DEV_CLOSE, dev, proc_e, 0);
	return r;
  }

  /* Drop old node and use the new values */
  if ((vp = get_free_vnode()) == NULL) {
	req_putnode(PFS_PROC_NR, res.inode_nr, 1); /* is this right? */
	(void) gen_opcl(DEV_CLOSE, dev, proc_e, 0);
	return(err_code);
  }
  lock_vnode(vp, VNODE_OPCL);

  assert(FD_ISSET(scratch(fp).file.fd_nr, &fp->fp_filp_inuse));
  unlock_vnode(fp->fp_filp[scratch(fp).file.fd_nr]->filp_vno);
  put_vnode(fp->fp_filp[scratch(fp).file.fd_nr]->filp_vno);

  vp->v_fs_e = res.fs_e;
  vp->v_vmnt = NULL;
  vp->v_dev = NO_DEV;
  vp->v_fs_e = res.fs_e;
  vp->v_inode_nr = res.inode_nr;
  vp->v_mode = res.fmode;
  vp->v_sdev = dev;
  vp->v_fs_count = 1;
  vp->v_ref_count = 1;
  fp->fp_filp[scratch(fp).file.fd_nr]->filp_vno = vp;

  return OK;
}


/*===========================================================================*
 *				gen_opcl				     *
 *===========================================================================*/
int gen_opcl(
  int op,			/* operation, DEV_OPEN/DEV_REOPEN/DEV_CLOSE */
  dev_t dev,			/* device to open or close */
  endpoint_t proc_e,		/* process to open/close for */
  int flags			/* mode bits and flags */
)
{
/* Called from the dmap struct on opens & closes of special files.*/
  devmajor_t major_dev;
  devminor_t minor_dev;
  struct dmap *dp;
  message dev_mess;
  int r;

  /* Determine task dmap. */
  major_dev = major(dev);
  minor_dev = minor(dev);
  assert(major_dev >= 0 && major_dev < NR_DEVICES);
  dp = &dmap[major_dev];
  assert(dp->dmap_driver != NONE);

  assert(!IS_BDEV_RQ(op));

  /* Prepare the request message. */
  memset(&dev_mess, 0, sizeof(dev_mess));

  dev_mess.m_type = op;
  dev_mess.DEVICE = minor_dev;
  dev_mess.USER_ENDPT = proc_e;
  dev_mess.COUNT = flags;

  /* Call the task. */
  r = (*dp->dmap_io)(dp->dmap_driver, &dev_mess);

  if (r != OK) return(r);

  if (op == DEV_OPEN || op == DEV_CLOSE) {
	/* Block the thread waiting for a reply. */
	fp->fp_task = dp->dmap_driver;
	self->w_task = dp->dmap_driver;
	self->w_drv_sendrec = &dev_mess;

	worker_wait();

	self->w_task = NONE;
	self->w_drv_sendrec = NULL;
  }

  /* Return the result from the driver. */
  return(dev_mess.REP_STATUS);
}

/*===========================================================================*
 *				tty_opcl				     *
 *===========================================================================*/
int tty_opcl(
  int op,			/* operation, DEV_OPEN or DEV_CLOSE */
  dev_t dev,			/* device to open or close */
  endpoint_t proc_e,		/* process to open/close for */
  int flags			/* mode bits and flags */
)
{
/* This procedure is called from the dmap struct on tty open/close. */
  int r;
  register struct fproc *rfp;

  assert(!IS_BDEV_RQ(op));

  /* Add O_NOCTTY to the flags if this process is not a session leader, or
   * if it already has a controlling tty, or if it is someone elses
   * controlling tty.
   */
  if (!(fp->fp_flags & FP_SESLDR) || fp->fp_tty != 0) {
	flags |= O_NOCTTY;
  } else {
	for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
		if(rfp->fp_pid == PID_FREE) continue;
		if (rfp->fp_tty == dev) flags |= O_NOCTTY;
	}
  }

  r = gen_opcl(op, dev, proc_e, flags);

  /* Did this call make the tty the controlling tty? */
  if (r == 1) {
	fp->fp_tty = dev;
	r = OK;
  }

  return(r);
}


/*===========================================================================*
 *				ctty_opcl				     *
 *===========================================================================*/
int ctty_opcl(
  int op,			/* operation, DEV_OPEN or DEV_CLOSE */
  dev_t UNUSED(dev),		/* device to open or close */
  endpoint_t UNUSED(proc_e),	/* process to open/close for */
  int UNUSED(flags)		/* mode bits and flags */
)
{
/* This procedure is called from the dmap struct on opening or closing
 * /dev/tty, the magic device that translates to the controlling tty.
 */

  if (IS_BDEV_RQ(op))
	panic("ctty_opcl() called for block device request?");

  return(fp->fp_tty == 0 ? ENXIO : OK);
}


/*===========================================================================*
 *				pm_setsid				     *
 *===========================================================================*/
void pm_setsid(endpoint_t proc_e)
{
/* Perform the VFS side of the SETSID call, i.e. get rid of the controlling
 * terminal of a process, and make the process a session leader.
 */
  register struct fproc *rfp;
  int slot;

  /* Make the process a session leader with no controlling tty. */
  okendpt(proc_e, &slot);
  rfp = &fproc[slot];
  rfp->fp_flags |= FP_SESLDR;
  rfp->fp_tty = 0;
}


/*===========================================================================*
 *				do_ioctl				     *
 *===========================================================================*/
int do_ioctl(message *UNUSED(m_out))
{
/* Perform the ioctl(ls_fd, request, argx) system call */
  unsigned long ioctlrequest;
  int r = OK, suspend_reopen;
  struct filp *f;
  register struct vnode *vp;
  dev_t dev;
  void *argx;

  scratch(fp).file.fd_nr = job_m_in.VFS_IOCTL_FD;
  ioctlrequest = job_m_in.VFS_IOCTL_REQ;
  argx = job_m_in.VFS_IOCTL_ARG;

  if ((f = get_filp(scratch(fp).file.fd_nr, VNODE_READ)) == NULL)
	return(err_code);
  vp = f->filp_vno;		/* get vnode pointer */
  if (!S_ISCHR(vp->v_mode) && !S_ISBLK(vp->v_mode)) {
	r = ENOTTY;
  }

  if (r == OK) {
	suspend_reopen = (f->filp_state & FS_NEEDS_REOPEN);
	dev = (dev_t) vp->v_sdev;

	if (S_ISBLK(vp->v_mode))
		r = bdev_ioctl(dev, who_e, ioctlrequest, argx);
	else
		r = dev_io(DEV_IOCTL_S, dev, who_e, argx, 0,
			   ioctlrequest, f->filp_flags, suspend_reopen);
  }

  unlock_filp(f);

  return(r);
}


/*===========================================================================*
 *				dev_select				     *
 *===========================================================================*/
int dev_select(dev_t dev, int ops)
{
/* Initiate a select call on a device. Return OK iff the request was sent. */
  devmajor_t major_dev;
  devminor_t minor_dev;
  message dev_mess;
  struct dmap *dp;

  major_dev = major(dev);
  minor_dev = minor(dev);
  dp = &dmap[major_dev];

  if (dp->dmap_driver == NONE) return ENXIO;

  memset(&dev_mess, 0, sizeof(dev_mess));

  dev_mess.m_type = DEV_SELECT;
  dev_mess.DEV_MINOR = minor_dev;
  dev_mess.DEV_SEL_OPS = ops;

  /* Call the task. */
  return (*dp->dmap_io)(dp->dmap_driver, &dev_mess);
}


/*===========================================================================*
 *				dev_cancel				     *
 *===========================================================================*/
int dev_cancel(dev_t dev)
{
/* Cancel an I/O request, blocking until it has been cancelled. */
  devmajor_t major_dev;
  devminor_t minor_dev;
  message dev_mess;
  struct dmap *dp;
  int r;

  major_dev = major(dev);
  minor_dev = minor(dev);
  dp = &dmap[major_dev];

  memset(&dev_mess, 0, sizeof(dev_mess));

  dev_mess.m_type = CANCEL;
  dev_mess.DEVICE = minor_dev;
  dev_mess.USER_ENDPT = fp->fp_ioproc;
  dev_mess.IO_GRANT = (char *) fp->fp_grant;

  /* Tell driver R or W. Mode is from current call, not open. */
  /* FIXME: ioctls also pass through here! */
  dev_mess.COUNT = fp->fp_block_callnr == READ ? R_BIT : W_BIT;

  r = (*dp->dmap_io)(fp->fp_task, &dev_mess);
  if (r != OK) return r; /* ctty_io returned an error? should be impossible */

  /* Suspend this thread until we have received the response. */
  fp->fp_task = dp->dmap_driver;
  self->w_task = dp->dmap_driver;
  self->w_drv_sendrec = &dev_mess;

  worker_wait();

  self->w_task = NONE;
  self->w_drv_sendrec = NULL;

  /* Clean up and return the result (note: the request may have completed). */
  if (GRANT_VALID(fp->fp_grant)) {
	(void) cpf_revoke(fp->fp_grant);
	fp->fp_grant = GRANT_INVALID;
  }

  r = dev_mess.REP_STATUS;
  return (r == EAGAIN) ? EINTR : r;
}


/*===========================================================================*
 *				block_io				     *
 *===========================================================================*/
static int block_io(endpoint_t driver_e, message *mess_ptr)
{
/* Perform I/O on a block device. The current thread is suspended until a reply
 * comes in from the driver.
 */
  int r, status, retry_count;
  message mess_retry;

  assert(IS_BDEV_RQ(mess_ptr->m_type));
  mess_retry = *mess_ptr;
  retry_count = 0;

  do {
	r = drv_sendrec(driver_e, mess_ptr);
	if (r == OK) {
		status = mess_ptr->BDEV_STATUS;
		if (status == ERESTART) {
			r = EDEADEPT;
			*mess_ptr = mess_retry;
			retry_count++;
		}
	}
  } while (status == ERESTART && retry_count < 5);

  /* If we failed to restart the request, return EIO */
  if (status == ERESTART && retry_count >= 5) {
	r = OK;
	mess_ptr->m_type = EIO;
  }

  if (r != OK) {
	if (r == EDEADSRCDST || r == EDEADEPT) {
		printf("VFS: dead driver %d\n", driver_e);
		dmap_unmap_by_endpt(driver_e);
		return(r);
	} else if (r == ELOCKED) {
		printf("VFS: ELOCKED talking to %d\n", driver_e);
		return(r);
	}
	panic("block_io: can't send/receive: %d", r);
  }

  return(OK);
}


/*===========================================================================*
 *				gen_io					     *
 *===========================================================================*/
int gen_io(endpoint_t drv_e, message *mess_ptr)
{
/* Initiate I/O to a character driver. Do not wait for the reply. */
  int r;

  assert(!IS_BDEV_RQ(mess_ptr->m_type));

  r = asynsend3(drv_e, mess_ptr, AMF_NOREPLY);

  if (r != OK) panic("VFS: asynsend in gen_io failed: %d", r);

  /* Fake a SUSPEND */
  mess_ptr->REP_STATUS = SUSPEND;
  return(OK);
}


/*===========================================================================*
 *				ctty_io					     *
 *===========================================================================*/
int ctty_io(
  endpoint_t UNUSED(task_nr),	/* not used - for compatibility with dmap_t */
  message *mess_ptr		/* pointer to message for task */
)
{
/* This routine is only called for one device, namely /dev/tty.  Its job
 * is to change the message to use the controlling terminal, instead of the
 * major/minor pair for /dev/tty itself.
 */
  struct dmap *dp;

  if (fp->fp_tty == 0) {
	/* No controlling tty present anymore, return an I/O error. */
	return(EIO);
  } else {
	/* Substitute the controlling terminal device. */
	dp = &dmap[major(fp->fp_tty)];
	mess_ptr->DEVICE = minor(fp->fp_tty);

	if (dp->dmap_driver == NONE) {
		printf("FS: ctty_io: no driver for dev\n");
		return(EIO);
	}

	if (isokendpt(dp->dmap_driver, &dummyproc) != OK) {
		printf("VFS: ctty_io: old driver %d\n", dp->dmap_driver);
		return(EIO);
	}

	return (*dp->dmap_io)(dp->dmap_driver, mess_ptr);
  }
}


/*===========================================================================*
 *				no_dev					     *
 *===========================================================================*/
int no_dev(
  int UNUSED(op),		/* operation, DEV_OPEN or DEV_CLOSE */
  dev_t UNUSED(dev),		/* device to open or close */
  endpoint_t UNUSED(proc),		/* process to open/close for */
  int UNUSED(flags)		/* mode bits and flags */
)
{
/* Called when opening a nonexistent device. */
  return(ENODEV);
}

/*===========================================================================*
 *				no_dev_io				     *
 *===========================================================================*/
int no_dev_io(endpoint_t UNUSED(proc), message *UNUSED(m))
{
/* Called when doing i/o on a nonexistent device. */
  printf("VFS: I/O on unmapped device number\n");
  return(EIO);
}

/*===========================================================================*
 *				clone_opcl				     *
 *===========================================================================*/
int clone_opcl(
  int op,			/* operation, DEV_OPEN or DEV_CLOSE */
  dev_t dev,			/* device to open or close */
  endpoint_t proc_e,		/* process to open/close for */
  int flags			/* mode bits and flags */
)
{
/* Some devices need special processing upon open.  Such a device is "cloned",
 * i.e. on a succesful open it is replaced by a new device with a new unique
 * minor device number.  This new device number identifies a new object (such
 * as a new network connection) that has been allocated within a task.
 */
  int r;

  r = gen_opcl(op, dev, proc_e, flags);

  if (op == DEV_OPEN && r >= 0) {
	if (r != minor(dev))
		r = cdev_clone(dev, proc_e, r);
	else
		r = OK;
  }

  return r;
}


/*===========================================================================*
 *				bdev_up					     *
 *===========================================================================*/
void bdev_up(devmajor_t maj)
{
  /* A new block device driver has been mapped in. This may affect both mounted
   * file systems and open block-special files.
   */
  int r, found, bits;
  struct filp *rfilp;
  struct vmnt *vmp;
  struct vnode *vp;
  char *label;

  if (maj < 0 || maj >= NR_DEVICES) panic("VFS: out-of-bound major");
  label = dmap[maj].dmap_label;
  found = 0;

  /* For each block-special file that was previously opened on the affected
   * device, we need to reopen it on the new driver.
   */
  for (rfilp = filp; rfilp < &filp[NR_FILPS]; rfilp++) {
	if (rfilp->filp_count < 1 || !(vp = rfilp->filp_vno)) continue;
	if (major(vp->v_sdev) != maj) continue;
	if (!S_ISBLK(vp->v_mode)) continue;

	/* Reopen the device on the driver, once per filp. */
	bits = rfilp->filp_mode & (R_BIT|W_BIT);
	if ((r = bdev_open(vp->v_sdev, bits)) != OK) {
		printf("VFS: mounted dev %d/%d re-open failed: %d.\n",
			maj, minor(vp->v_sdev), r);
		dmap[maj].dmap_recovering = 0;
		return; /* Give up entirely */
	}

	found = 1;
  }

  /* Tell each affected mounted file system about the new endpoint.
   */
  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp) {
	if (major(vmp->m_dev) != maj) continue;

	/* Send the driver label to the mounted file system. */
	if (OK != req_newdriver(vmp->m_fs_e, vmp->m_dev, label))
		printf("VFS dev_up: error sending new driver label to %d\n",
		       vmp->m_fs_e);
  }

  /* If any block-special file was open for this major at all, also inform the
   * root file system about the new driver. We do this even if the
   * block-special file is linked to another mounted file system, merely
   * because it is more work to check for that case.
   */
  if (found) {
	if (OK != req_newdriver(ROOT_FS_E, makedev(maj, 0), label))
		printf("VFSdev_up: error sending new driver label to %d\n",
			ROOT_FS_E);
  }
}


/*===========================================================================*
 *				cdev_up					     *
 *===========================================================================*/
void cdev_up(devmajor_t maj)
{
  /* A new character device driver has been mapped in.
  */
  int needs_reopen;
  struct filp *rfilp;
  struct vnode *vp;

  needs_reopen= FALSE;
  for (rfilp = filp; rfilp < &filp[NR_FILPS]; rfilp++) {
	if (rfilp->filp_count < 1 || !(vp = rfilp->filp_vno)) continue;
	if (major(vp->v_sdev) != maj) continue;
	if (!S_ISCHR(vp->v_mode)) continue;

	rfilp->filp_state |= FS_NEEDS_REOPEN;
	needs_reopen = TRUE;
  }

  if (needs_reopen)
	restart_reopen(maj);
}

/*===========================================================================*
 *				opcl_reply				     *
 *===========================================================================*/
static void opcl_reply(message *m_ptr)
{
/* A character driver has replied to an open or close request. This function
 * MUST NOT block its calling thread.
 */
  struct fproc *rfp;
  struct worker_thread *wp;
  endpoint_t proc_e;
  int slot;

  proc_e = m_ptr->REP_ENDPT;
  if (isokendpt(proc_e, &slot) != OK) return;
  rfp = &fproc[slot];
  wp = rfp->fp_worker;
  if (wp == NULL || wp->w_task != who_e) {
	printf("VFS: no worker thread waiting for a reply from %d\n", who_e);
	return;
  }
  *wp->w_drv_sendrec = *m_ptr;
  worker_signal(wp);	/* Continue open/close */
}


/*===========================================================================*
 *				task_reply				     *
 *===========================================================================*/
static void task_reply(message *m_ptr)
{
/* A character driver has results for a read, write, or ioctl call. There may
 * be a thread waiting for these results as part of an ongoing dev_cancel call.
 * If so, wake up that thread; if not, send a reply to the requesting process.
 * This function MUST NOT block its calling thread.
 */
  struct fproc *rfp;
  struct worker_thread *wp;
  endpoint_t proc_e;
  int slot;

  proc_e = m_ptr->REP_ENDPT;
  if (proc_e == VFS_PROC_NR)
	proc_e = find_suspended_ep(m_ptr->m_source, m_ptr->REP_IO_GRANT);
  else
	printf("VFS: endpoint %u from %u is not VFS\n",
		proc_e, m_ptr->m_source);

  if (proc_e == NONE) {
	printf("VFS: proc with grant %d from %d not found\n",
		m_ptr->REP_IO_GRANT, m_ptr->m_source);
  } else if (m_ptr->REP_STATUS == SUSPEND) {
	printf("VFS: got SUSPEND on DEV_REVIVE: not reviving proc\n");
  } else {
	/* If there is a thread active for this process, we assume that this
	 * thread aims to cancel the ongoing operation. In that case, wake up
	 * the thread to let it finish unpausing the process. Otherwise, revive
	 * the process as usual.
	 */
	if (isokendpt(proc_e, &slot) != OK) return;
	rfp = &fproc[slot];
	wp = rfp->fp_worker;
	if (wp != NULL && wp->w_task == who_e) {
		assert(!fp_is_blocked(rfp));
		*wp->w_drv_sendrec = *m_ptr;
		worker_signal(wp);	/* Continue cancel */
	} else {
		revive(proc_e, m_ptr->REP_STATUS);
	}
  }
}


/*===========================================================================*
 *			       cdev_reply				     *
 *===========================================================================*/
void cdev_reply(void)
{
/* A character driver has results for us. */

  switch (call_nr) {
  case DEV_OPEN_REPL:
  case DEV_CLOSE_REPL:	opcl_reply(&m_in);	break;
  case DEV_REOPEN_REPL:	reopen_reply(&m_in);	break;
  case DEV_REVIVE:	task_reply(&m_in);	break;
  case DEV_SEL_REPL1:
	select_reply1(m_in.m_source, m_in.DEV_MINOR, m_in.DEV_SEL_OPS);
	break;
  case DEV_SEL_REPL2:
	select_reply2(m_in.m_source, m_in.DEV_MINOR, m_in.DEV_SEL_OPS);
	break;
  default:
	printf("VFS: char driver %u sent unknown reply %x\n", who_e, call_nr);
  }
}


/*===========================================================================*
 *				bdev_reply				     *
 *===========================================================================*/
void bdev_reply(struct dmap *dp)
{
/* A block driver has results for a call. There must be a thread waiting for
 * these results - wake it up. This function MUST NOT block its calling thread.
 */
  struct worker_thread *wp;

  assert(dp != NULL);
  assert(dp->dmap_servicing != NONE);

  wp = worker_get(dp->dmap_servicing);
  if (wp == NULL || wp->w_task != who_e) {
	printf("VFS: no worker thread waiting for a reply from %d\n", who_e);
	return;
  }

  assert(wp->w_drv_sendrec != NULL);
  *wp->w_drv_sendrec = m_in;
  wp->w_drv_sendrec = NULL;
  worker_signal(wp);
}

/*===========================================================================*
 *				filp_gc_thread				     *
 *===========================================================================*/
static void filp_gc_thread(void)
{
/* Filp garbage collection thread function. Since new filps may be invalidated
 * while the actual garbage collection procedure is running, we repeat the
 * procedure until it can not find any more work to do.
 */

  while (do_filp_gc())
	/* simply repeat */;
}

/*===========================================================================*
 *				restart_reopen				     *
 *===========================================================================*/
static void restart_reopen(devmajor_t maj)
{
  devmajor_t major_dev;
  devminor_t minor_dev;
  endpoint_t driver_e;
  struct vnode *vp;
  struct filp *rfilp;
  struct fproc *rfp;
  message m_out;
  int n, r;

  memset(&m_out, 0, sizeof(m_out));

  if (maj < 0 || maj >= NR_DEVICES) panic("VFS: out-of-bound major");

  for (rfilp = filp; rfilp < &filp[NR_FILPS]; rfilp++) {
	if (rfilp->filp_count < 1 || !(vp = rfilp->filp_vno)) continue;
	if (!(rfilp->filp_state & FS_NEEDS_REOPEN)) continue;
	if (!S_ISCHR(vp->v_mode)) continue;

	major_dev = major(vp->v_sdev);
	minor_dev = minor(vp->v_sdev);
	if (major_dev != maj) continue;

	if (rfilp->filp_flags & O_REOPEN) {
		/* Try to reopen a file upon driver restart */
		r = dev_reopen(vp->v_sdev, rfilp-filp,
			rfilp->filp_mode & (R_BIT|W_BIT));

		if (r == OK)
			return;

		printf("VFS: file on dev %d/%d re-open failed: %d\n",
			major_dev, minor_dev, r);
	}

	/* File descriptor is to be closed when driver restarts. */
	n = invalidate_filp(rfilp);
	if (n != rfilp->filp_count) {
		printf("VFS: warning: invalidate/count "
		       "discrepancy (%d, %d)\n", n, rfilp->filp_count);
	}
	rfilp->filp_count = 0;

	/* We have to clean up this filp and vnode, but can't do that yet as
	 * it's locked by a worker thread. Start a new job to garbage collect
	 * invalidated filps associated with this device driver. This thread
	 * is associated with a process that we know is idle otherwise: VFS.
	 * Be careful that we don't start two threads or lose work, though.
	 */
	if (worker_can_start(fproc_addr(VFS_PROC_NR))) {
		worker_start(fproc_addr(VFS_PROC_NR), filp_gc_thread,
			&m_out /*unused*/, FALSE /*use_spare*/);
	}
  }

  /* Nothing more to re-open. Restart suspended processes */
  driver_e = dmap[maj].dmap_driver;
  for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	if(rfp->fp_pid == PID_FREE) continue;
	if(rfp->fp_blocked_on == FP_BLOCKED_ON_OTHER &&
	   rfp->fp_task == driver_e && (rfp->fp_flags & FP_SUSP_REOPEN)) {
		rfp->fp_flags &= ~FP_SUSP_REOPEN;
		rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;
		reply(&m_out, rfp->fp_endpoint, ERESTART);
	}
  }
}


/*===========================================================================*
 *				reopen_reply				     *
 *===========================================================================*/
static void reopen_reply(message *m_ptr)
{
  endpoint_t driver_e;
  devmajor_t maj;
  int filp_no, status;
  struct filp *rfilp;
  struct vnode *vp;
  struct dmap *dp;

  driver_e = m_ptr->m_source;
  filp_no = m_ptr->REP_ENDPT;
  status = m_ptr->REP_STATUS;

  if (filp_no < 0 || filp_no >= NR_FILPS) {
	printf("VFS: reopen_reply: bad filp number %d from driver %d\n",
		filp_no, driver_e);
	return;
  }

  rfilp = &filp[filp_no];
  if (rfilp->filp_count < 1) {
	printf("VFS: reopen_reply: filp number %d not inuse (from driver %d)\n",
	       filp_no, driver_e);
	return;
  }

  vp = rfilp->filp_vno;
  if (!vp) {
	printf("VFS: reopen_reply: no vnode for filp number %d (from driver "
		"%d)\n", filp_no, driver_e);
	return;
  }

  if (!(rfilp->filp_state & FS_NEEDS_REOPEN)) {
	printf("VFS: reopen_reply: bad state %d for filp number %d"
	       " (from driver %d)\n", rfilp->filp_state, filp_no, driver_e);
	return;
  }

  if (!S_ISCHR(vp->v_mode)) {
	printf("VFS: reopen_reply: bad mode 0%o for filp number %d"
	       " (from driver %d)\n", vp->v_mode, filp_no, driver_e);
	return;
  }

  maj = major(vp->v_sdev);
  dp = &dmap[maj];
  if (dp->dmap_driver != driver_e) {
	printf("VFS: reopen_reply: bad major %d for filp number %d "
		"(from driver %d, current driver is %d)\n", maj, filp_no,
		driver_e, dp->dmap_driver);
	return;
  }

  if (status == OK) {
	rfilp->filp_state &= ~FS_NEEDS_REOPEN;
  } else {
	printf("VFS: reopen_reply: should handle error status\n");
	return;
  }

  restart_reopen(maj);
}
