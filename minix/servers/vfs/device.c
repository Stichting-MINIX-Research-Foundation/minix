/* When a needed block is not in the cache, it must be fetched from the disk.
 * Special character files also require I/O.  The routines for these are here.
 *
 * The entry points in this file are:
 *   cdev_open:   open a character device
 *   cdev_close:  close a character device
 *   cdev_io:     initiate a read, write, or ioctl to a character device
 *   cdev_select: initiate a select call on a device
 *   cdev_cancel: cancel an I/O request, blocking until it has been cancelled
 *   cdev_reply:  process the result of a character driver request
 *   bdev_open:   open a block device
 *   bdev_close:  close a block device
 *   bdev_reply:  process the result of a block driver request
 *   bdev_up:     a block driver has been mapped in
 *   do_ioctl:    perform the IOCTL system call
 */

#include "fs.h"
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/ttycom.h>
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

static int cdev_opcl(int op, dev_t dev, int flags);
static int block_io(endpoint_t driver_e, message *mess_ptr);
static cp_grant_id_t make_grant(endpoint_t driver_e, endpoint_t user_e, int op,
	vir_bytes buf, unsigned long size);

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
  dev_mess.m_lbdev_lblockdriver_msg.minor = minor_dev;
  dev_mess.m_lbdev_lblockdriver_msg.access = 0;
  if (access & R_BIT) dev_mess.m_lbdev_lblockdriver_msg.access |= BDEV_R_BIT;
  if (access & W_BIT) dev_mess.m_lbdev_lblockdriver_msg.access |= BDEV_W_BIT;
  dev_mess.m_lbdev_lblockdriver_msg.id = 0;

  /* Call the task. */
  r = block_io(dmap[major_dev].dmap_driver, &dev_mess);
  if (r != OK)
	return r;

  return dev_mess.m_lblockdriver_lbdev_reply.status;
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
  dev_mess.m_lbdev_lblockdriver_msg.minor = minor_dev;
  dev_mess.m_lbdev_lblockdriver_msg.id = 0;

  r = block_io(dmap[major_dev].dmap_driver, &dev_mess);
  if (r != OK)
	return r;

  return dev_mess.m_lblockdriver_lbdev_reply.status;
}


/*===========================================================================*
 *				bdev_ioctl				     *
 *===========================================================================*/
static int bdev_ioctl(dev_t dev, endpoint_t proc_e, unsigned long req,
	vir_bytes buf)
{
/* Perform an I/O control operation on a block device. */
  struct dmap *dp;
  cp_grant_id_t gid;
  message dev_mess;
  devmajor_t major_dev;
  devminor_t minor_dev;
  int r;

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
  dev_mess.m_lbdev_lblockdriver_msg.minor = minor_dev;
  dev_mess.m_lbdev_lblockdriver_msg.request = req;
  dev_mess.m_lbdev_lblockdriver_msg.grant = gid;
  dev_mess.m_lbdev_lblockdriver_msg.user = proc_e;
  dev_mess.m_lbdev_lblockdriver_msg.id = 0;

  /* Call the task. */
  r = block_io(dp->dmap_driver, &dev_mess);

  /* Clean up. */
  if (GRANT_VALID(gid)) cpf_revoke(gid);

  /* Return the result. */
  if (r != OK)
	return(r);

  return(dev_mess.m_lblockdriver_lbdev_reply.status);
}


/*===========================================================================*
 *				make_grant				     *
 *===========================================================================*/
static cp_grant_id_t make_grant(endpoint_t driver_e, endpoint_t user_e, int op,
	vir_bytes buf, unsigned long bytes)
{
/* Create a magic grant for the given operation and buffer. */
  cp_grant_id_t gid;
  int access;
  size_t size;

  switch (op) {
  case CDEV_READ:
  case CDEV_WRITE:
	gid = cpf_grant_magic(driver_e, user_e, buf,
		(size_t) bytes, op == CDEV_READ ? CPF_WRITE : CPF_READ);
	break;

  case CDEV_IOCTL:
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

	/* Grant access to the buffer even if no I/O happens with the ioctl,
	 * although now that we no longer identify responses based on grants,
	 * this is not strictly necessary.
	 */
	gid = cpf_grant_magic(driver_e, user_e, buf, size, access);
	break;

  default:
	panic("VFS: unknown operation %d", op);
  }

  if (!GRANT_VALID(gid))
	panic("VFS: cpf_grant_magic failed");

  return gid;
}

/*===========================================================================*
 *				cdev_map				     *
 *===========================================================================*/
dev_t cdev_map(dev_t dev, struct fproc *rfp)
{
/* Map the given device number to a real device number, remapping /dev/tty to
 * the given process's controlling terminal if it has one. Perform a bounds
 * check on the resulting device's major number, and return NO_DEV on failure.
 * This function is idempotent but not used that way.
 */
  devmajor_t major;

  /* First cover one special case: /dev/tty, the magic device that translates
   * to the controlling tty.
   */
  if ((major = major(dev)) == CTTY_MAJOR) {
	/* No controlling terminal? Fail the request. */
	if (rfp->fp_tty == NO_DEV) return NO_DEV;

	/* Substitute the controlling terminal device. */
	dev = rfp->fp_tty;
	major = major(dev);
  }

  if (major < 0 || major >= NR_DEVICES) return NO_DEV;

  return dev;
}

/*===========================================================================*
 *				cdev_get				     *
 *===========================================================================*/
static struct dmap *cdev_get(dev_t dev, devminor_t *minor_dev)
{
/* Obtain the dmap structure for the given device, if a valid driver exists for
 * the major device. Perform redirection for CTTY_MAJOR.
 */
  struct dmap *dp;
  int slot;

  /* Remap /dev/tty as needed. Perform a bounds check on the major number. */
  if ((dev = cdev_map(dev, fp)) == NO_DEV)
	return(NULL);

  /* Determine task dmap. */
  dp = &dmap[major(dev)];

  /* See if driver is roughly valid. */
  if (dp->dmap_driver == NONE) return(NULL);

  if (isokendpt(dp->dmap_driver, &slot) != OK) {
	printf("VFS: cdev_get: old driver for major %x (%d)\n", major(dev),
		dp->dmap_driver);
	return(NULL);
  }

  /* Also return the (possibly redirected) minor number. */
  *minor_dev = minor(dev);
  return dp;
}

/*===========================================================================*
 *				cdev_io					     *
 *===========================================================================*/
int cdev_io(
  int op,			/* CDEV_READ, CDEV_WRITE, or CDEV_IOCTL */
  dev_t dev,			/* major-minor device number */
  endpoint_t proc_e,		/* in whose address space is buf? */
  vir_bytes buf,		/* virtual address of the buffer */
  off_t pos,			/* byte position */
  unsigned long bytes,		/* how many bytes to transfer, or request */
  int flags			/* special flags, like O_NONBLOCK */
)
{
/* Initiate a read, write, or ioctl to a character device. */
  devminor_t minor_dev;
  struct dmap *dp;
  message dev_mess;
  cp_grant_id_t gid;
  int r;

  assert(op == CDEV_READ || op == CDEV_WRITE || op == CDEV_IOCTL);

  /* Determine task map. */
  if ((dp = cdev_get(dev, &minor_dev)) == NULL)
	return(EIO);

  /* Handle TIOCSCTTY ioctl: set controlling tty.
   * TODO: cleaner implementation work in progress.
   */
  if (op == CDEV_IOCTL && bytes == TIOCSCTTY && major(dev) == TTY_MAJOR) {
       fp->fp_tty = dev;
  }

  /* Create a grant for the buffer provided by the user process. */
  gid = make_grant(dp->dmap_driver, proc_e, op, buf, bytes);

  /* Set up the rest of the message that will be sent to the driver. */
  memset(&dev_mess, 0, sizeof(dev_mess));
  dev_mess.m_type = op;
  dev_mess.m_vfs_lchardriver_readwrite.minor = minor_dev;
  if (op == CDEV_IOCTL) {
	dev_mess.m_vfs_lchardriver_readwrite.request = bytes;
	dev_mess.m_vfs_lchardriver_readwrite.user = proc_e;
  } else {
	dev_mess.m_vfs_lchardriver_readwrite.pos = pos;
	dev_mess.m_vfs_lchardriver_readwrite.count = bytes;
  }
  dev_mess.m_vfs_lchardriver_readwrite.id = proc_e;
  dev_mess.m_vfs_lchardriver_readwrite.grant = gid;
  dev_mess.m_vfs_lchardriver_readwrite.flags = 0;
  if (flags & O_NONBLOCK)
	  dev_mess.m_vfs_lchardriver_readwrite.flags |= CDEV_NONBLOCK;

  /* Send the request to the driver. */
  if ((r = asynsend3(dp->dmap_driver, &dev_mess, AMF_NOREPLY)) != OK)
	panic("VFS: asynsend in cdev_io failed: %d", r);

  /* Suspend the calling process until a reply arrives. */
  wait_for(dp->dmap_driver);
  assert(!GRANT_VALID(fp->fp_grant));
  fp->fp_grant = gid;	/* revoke this when unsuspended. */

  return SUSPEND;
}


/*===========================================================================*
 *				cdev_clone				     *
 *===========================================================================*/
static int cdev_clone(dev_t dev, devminor_t new_minor)
{
/* A new minor device number has been returned. Request PFS to create a
 * temporary device file to hold it.
 */
  struct vnode *vp;
  struct node_details res;
  int r;

  /* Device number of the new device. */
  dev = makedev(major(dev), new_minor);

  /* Issue request */
  r = req_newnode(PFS_PROC_NR, fp->fp_effuid, fp->fp_effgid,
      ALL_MODES | I_CHAR_SPECIAL, dev, &res);
  if (r != OK) {
	(void) cdev_opcl(CDEV_CLOSE, dev, 0);
	return r;
  }

  /* Drop old node and use the new values */
  if ((vp = get_free_vnode()) == NULL) {
	req_putnode(PFS_PROC_NR, res.inode_nr, 1); /* is this right? */
	(void) cdev_opcl(CDEV_CLOSE, dev, 0);
	return(err_code);
  }
  lock_vnode(vp, VNODE_OPCL);

  assert(fp->fp_filp[scratch(fp).file.fd_nr] != NULL);
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
 *				cdev_opcl				     *
 *===========================================================================*/
static int cdev_opcl(
  int op,			/* operation, CDEV_OPEN or CDEV_CLOSE */
  dev_t dev,			/* device to open or close */
  int flags			/* mode bits and flags */
)
{
/* Open or close a character device. */
  devminor_t minor_dev, new_minor;
  struct dmap *dp;
  struct fproc *rfp;
  message dev_mess;
  int r, r2;

  assert(op == CDEV_OPEN || op == CDEV_CLOSE);

  /* Determine task dmap. */
  if ((dp = cdev_get(dev, &minor_dev)) == NULL)
	return(ENXIO);

  /* CTTY exception: do not actually send the open/close request for /dev/tty
   * to the driver.  This avoids the case that the actual device will remain
   * open forever if the process calls setsid() after opening /dev/tty.
   */
  if (major(dev) == CTTY_MAJOR) return(OK);

  /* Add O_NOCTTY to the access flags if this process is not a session leader,
   * or if it already has a controlling tty, or if it is someone else's
   * controlling tty.  For performance reasons, only search the full process
   * table if this driver has set controlling ttys before.
   */
  if (!(fp->fp_flags & FP_SESLDR) || fp->fp_tty != 0) {
	flags |= O_NOCTTY;
  } else if (!(flags & O_NOCTTY) && dp->dmap_seen_tty) {
	for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++)
		if (rfp->fp_pid != PID_FREE && rfp->fp_tty == dev)
			flags |= O_NOCTTY;
  }

  /* Prepare the request message. */
  memset(&dev_mess, 0, sizeof(dev_mess));

  dev_mess.m_type = op;
  dev_mess.m_vfs_lchardriver_openclose.minor = minor_dev;
  dev_mess.m_vfs_lchardriver_openclose.id = who_e;
  if (op == CDEV_OPEN) {
	dev_mess.m_vfs_lchardriver_openclose.user = who_e;
	dev_mess.m_vfs_lchardriver_openclose.access = 0;
	if (flags & R_BIT)
		dev_mess.m_vfs_lchardriver_openclose.access |= CDEV_R_BIT;
	if (flags & W_BIT)
		dev_mess.m_vfs_lchardriver_openclose.access |= CDEV_W_BIT;
	if (flags & O_NOCTTY)
		dev_mess.m_vfs_lchardriver_openclose.access |= CDEV_NOCTTY;
  }

  /* Send the request to the driver. */
  if ((r = asynsend3(dp->dmap_driver, &dev_mess, AMF_NOREPLY)) != OK)
	panic("VFS: asynsend in cdev_opcl failed: %d", r);

  /* Block the thread waiting for a reply. */
  fp->fp_task = dp->dmap_driver;
  self->w_task = dp->dmap_driver;
  self->w_drv_sendrec = &dev_mess;

  worker_wait();

  self->w_task = NONE;
  self->w_drv_sendrec = NULL;

  /* Process the reply. */
  r = dev_mess.m_lchardriver_vfs_reply.status;

  if (op == CDEV_OPEN && r >= 0) {
	/* Some devices need special processing upon open. Such a device is
	 * "cloned", i.e. on a succesful open it is replaced by a new device
	 * with a new unique minor device number. This new device number
	 * identifies a new object (such as a new network connection) that has
	 * been allocated within a driver.
	 */
	if (r & CDEV_CLONED) {
		new_minor = r & ~(CDEV_CLONED | CDEV_CTTY);
		if ((r2 = cdev_clone(dev, new_minor)) < 0)
			return(r2);
	}

	/* Did this call make the tty the controlling tty? */
	if (r & CDEV_CTTY) {
		fp->fp_tty = dev;
		dp->dmap_seen_tty = TRUE;
	}

	r = OK;
  }

  /* Return the result from the driver. */
  return(r);
}


/*===========================================================================*
 *				cdev_open				     *
 *===========================================================================*/
int cdev_open(dev_t dev, int flags)
{
/* Open a character device. */

  return cdev_opcl(CDEV_OPEN, dev, flags);
}


/*===========================================================================*
 *				cdev_close				     *
 *===========================================================================*/
int cdev_close(dev_t dev)
{
/* Close a character device. */

  return cdev_opcl(CDEV_CLOSE, dev, 0);
}


/*===========================================================================*
 *				do_ioctl				     *
 *===========================================================================*/
int do_ioctl(void)
{
/* Perform the ioctl(2) system call. */
  unsigned long ioctlrequest;
  int r = OK;
  struct filp *f;
  register struct vnode *vp;
  dev_t dev;
  vir_bytes argx;

  scratch(fp).file.fd_nr = job_m_in.m_lc_vfs_ioctl.fd;
  ioctlrequest = job_m_in.m_lc_vfs_ioctl.req;
  argx = (vir_bytes)job_m_in.m_lc_vfs_ioctl.arg;

  if ((f = get_filp(scratch(fp).file.fd_nr, VNODE_READ)) == NULL)
	return(err_code);
  vp = f->filp_vno;		/* get vnode pointer */
  if (!S_ISCHR(vp->v_mode) && !S_ISBLK(vp->v_mode)) {
	r = ENOTTY;
  }

  if (r == OK) {
	dev = vp->v_sdev;

	if (S_ISBLK(vp->v_mode)) {
		f->filp_ioctl_fp = fp;

		r = bdev_ioctl(dev, who_e, ioctlrequest, argx);

		f->filp_ioctl_fp = NULL;
	} else
		r = cdev_io(CDEV_IOCTL, dev, who_e, argx, 0, ioctlrequest,
			f->filp_flags);
  }

  unlock_filp(f);

  return(r);
}


/*===========================================================================*
 *				cdev_select				     *
 *===========================================================================*/
int cdev_select(dev_t dev, int ops)
{
/* Initiate a select call on a device. Return OK iff the request was sent.
 * This function explicitly bypasses cdev_get() since it must not do CTTY
 * mapping, because a) the caller already has done that, b) "fp" may be wrong.
 */
  devmajor_t major;
  message dev_mess;
  struct dmap *dp;
  int r;

  /* Determine task dmap, without CTTY mapping. */
  assert(dev != NO_DEV);
  major = major(dev);
  assert(major >= 0 && major < NR_DEVICES);
  assert(major != CTTY_MAJOR);
  dp = &dmap[major];

  /* Prepare the request message. */
  memset(&dev_mess, 0, sizeof(dev_mess));

  dev_mess.m_type = CDEV_SELECT;
  dev_mess.m_vfs_lchardriver_select.minor = minor(dev);
  dev_mess.m_vfs_lchardriver_select.ops = ops;

  /* Send the request to the driver. */
  if ((r = asynsend3(dp->dmap_driver, &dev_mess, AMF_NOREPLY)) != OK)
	panic("VFS: asynsend in cdev_select failed: %d", r);

  return(OK);
}


/*===========================================================================*
 *				cdev_cancel				     *
 *===========================================================================*/
int cdev_cancel(dev_t dev)
{
/* Cancel an I/O request, blocking until it has been cancelled. */
  devminor_t minor_dev;
  message dev_mess;
  struct dmap *dp;
  int r;

  /* Determine task dmap. */
  if ((dp = cdev_get(dev, &minor_dev)) == NULL)
	return(EIO);

  /* Prepare the request message. */
  memset(&dev_mess, 0, sizeof(dev_mess));

  dev_mess.m_type = CDEV_CANCEL;
  dev_mess.m_vfs_lchardriver_cancel.minor = minor_dev;
  dev_mess.m_vfs_lchardriver_cancel.id = fp->fp_endpoint;

  /* Send the request to the driver. */
  if ((r = asynsend3(dp->dmap_driver, &dev_mess, AMF_NOREPLY)) != OK)
	panic("VFS: asynsend in cdev_cancel failed: %d", r);

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

  r = dev_mess.m_lchardriver_vfs_reply.status;
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
	if (r != OK)
		return r;

	status = mess_ptr->m_lblockdriver_lbdev_reply.status;
	if (status == ERESTART) {
		r = EDEADEPT;
		*mess_ptr = mess_retry;
		retry_count++;
	}
  } while (status == ERESTART && retry_count < 5);

  /* If we failed to restart the request, return EIO */
  if (status == ERESTART && retry_count >= 5)
	return EIO;

  if (r != OK) {
	if (r == EDEADSRCDST || r == EDEADEPT) {
		printf("VFS: dead driver %d\n", driver_e);
		dmap_unmap_by_endpt(driver_e);
		return(EIO);
	} else if (r == ELOCKED) {
		printf("VFS: ELOCKED talking to %d\n", driver_e);
		return(EIO);
	}
	panic("block_io: can't send/receive: %d", r);
  }

  return(OK);
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
 *				cdev_generic_reply			     *
 *===========================================================================*/
static void cdev_generic_reply(message *m_ptr)
{
/* A character driver has results for an open, close, read, write, or ioctl
 * call (i.e., everything except select). There may be a thread waiting for
 * these results as part of an ongoing open, close, or (for read/write/ioctl)
 * cancel call. If so, wake up that thread; if not, send a reply to the
 * requesting process. This function MUST NOT block its calling thread.
 */
  struct fproc *rfp;
  struct worker_thread *wp;
  endpoint_t proc_e;
  int r, slot;

  proc_e = m_ptr->m_lchardriver_vfs_reply.id;

  if (m_ptr->m_lchardriver_vfs_reply.status == SUSPEND) {
	printf("VFS: got SUSPEND from %d, not reviving\n", m_ptr->m_source);
	return;
  }

  if (isokendpt(proc_e, &slot) != OK) {
	printf("VFS: proc %d from %d not found\n", proc_e, m_ptr->m_source);
	return;
  }
  rfp = &fproc[slot];
  wp = rfp->fp_worker;
  if (wp != NULL && wp->w_task == who_e) {
	assert(!fp_is_blocked(rfp));
	*wp->w_drv_sendrec = *m_ptr;
	worker_signal(wp);	/* Continue open/close/cancel */
  } else if (rfp->fp_blocked_on != FP_BLOCKED_ON_OTHER ||
		rfp->fp_task != m_ptr->m_source) {
	/* This would typically be caused by a protocol error, i.e. a driver
	 * not properly following the character driver protocol rules.
	 */
	printf("VFS: proc %d not blocked on %d\n", proc_e, m_ptr->m_source);
  } else {
	/* Some services (inet) use the same infrastructure for nonblocking
	 * and cancelled requests, resulting in one of EINTR or EAGAIN when the
	 * other is really the appropriate code.  Thus, cdev_cancel converts
	 * EAGAIN into EINTR, and we convert EINTR into EAGAIN here.
	 */
	r = m_ptr->m_lchardriver_vfs_reply.status;
	revive(proc_e, (r == EINTR) ? EAGAIN : r);
  }
}


/*===========================================================================*
 *			       cdev_reply				     *
 *===========================================================================*/
void cdev_reply(void)
{
/* A character driver has results for us. */

  if (get_dmap(who_e) == NULL) {
	printf("VFS: ignoring char dev reply from unknown driver %d\n", who_e);
	return;
  }

  switch (call_nr) {
  case CDEV_REPLY:
	cdev_generic_reply(&m_in);
	break;
  case CDEV_SEL1_REPLY:
	select_reply1(m_in.m_source, m_in.m_lchardriver_vfs_sel1.minor,
		m_in.m_lchardriver_vfs_sel1.status);
	break;
  case CDEV_SEL2_REPLY:
	select_reply2(m_in.m_source, m_in.m_lchardriver_vfs_sel2.minor,
		m_in.m_lchardriver_vfs_sel2.status);
	break;
  default:
	printf("VFS: char driver %u sent unknown reply %x\n", who_e, call_nr);
  }
}


/*===========================================================================*
 *				bdev_reply				     *
 *===========================================================================*/
void bdev_reply(void)
{
/* A block driver has results for a call. There must be a thread waiting for
 * these results - wake it up. This function MUST NOT block its calling thread.
 */
  struct worker_thread *wp;
  struct dmap *dp;

  if ((dp = get_dmap(who_e)) == NULL) {
	printf("VFS: ignoring block dev reply from unknown driver %d\n",
		who_e);
	return;
  }

  if (dp->dmap_servicing == INVALID_THREAD) {
	printf("VFS: ignoring spurious block dev reply from %d\n", who_e);
	return;
  }

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
