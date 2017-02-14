/*
 * This file contains routines to perform character device operations.
 * Character drivers may suspend I/O requests on their devices (read, write,
 * ioctl), as well as select requests.  These requests will therefore suspend
 * their calling process, freeing up the associated VFS worker thread for other
 * tasks.  The I/O requests may later be cancelled as a result of the suspended
 * process receiving a signal (which it either catches or dies from), in which
 * case there will be a worker thread associated with the cancellation.  Open
 * and close requests may not suspend and will thus block the calling thread.
 *
 * The entry points in this file are:
 *   cdev_map:    map a character device to its actual device number
 *   cdev_open:   open a character device
 *   cdev_close:  close a character device
 *   cdev_io:     initiate a read, write, or ioctl to a character device
 *   cdev_select: initiate a select call on a device
 *   cdev_cancel: cancel an I/O request, blocking until it has been cancelled
 *   cdev_reply:  process the result of a character driver request
 */

#include "fs.h"
#include "vnode.h"
#include "file.h"
#include <string.h>
#include <fcntl.h>
#include <sys/ttycom.h>
#include <assert.h>

/*
 * Map the given device number to a real device number, remapping /dev/tty to
 * the given process's controlling terminal if it has one.  Perform a bounds
 * check on the resulting device's major number, and return NO_DEV on failure.
 * This function is idempotent but not used that way.
 */
dev_t
cdev_map(dev_t dev, struct fproc * rfp)
{
	devmajor_t major;

	/*
	 * First cover one special case: /dev/tty, the magic device that
	 * translates to the controlling TTY.
	 */
	if ((major = major(dev)) == CTTY_MAJOR) {
		/* No controlling terminal?  Fail the request. */
		if (rfp->fp_tty == NO_DEV) return NO_DEV;

		/* Substitute the controlling terminal device. */
		dev = rfp->fp_tty;
		major = major(dev);
	}

	if (major < 0 || major >= NR_DEVICES) return NO_DEV;

	return dev;
}

/*
 * Obtain the dmap structure for the given device, if a valid driver exists for
 * the major device.  Perform redirection for CTTY_MAJOR.
 */
static struct dmap *
cdev_get(dev_t dev, devminor_t * minor_dev)
{
	struct dmap *dp;
	int slot;

	/*
	 * Remap /dev/tty as needed.  Perform a bounds check on the major
	 * number.
	 */
	if ((dev = cdev_map(dev, fp)) == NO_DEV)
		return NULL;

	/* Determine the driver endpoint. */
	dp = &dmap[major(dev)];

	/* See if driver is roughly valid. */
	if (dp->dmap_driver == NONE) return NULL;

	if (isokendpt(dp->dmap_driver, &slot) != OK) {
		printf("VFS: cdev_get: old driver for major %x (%d)\n",
		    major(dev), dp->dmap_driver);
		return NULL;
	}

	/* Also return the (possibly redirected) minor number. */
	*minor_dev = minor(dev);
	return dp;
}

/*
 * A new minor device number has been returned.  Request PFS to create a
 * temporary device file to hold it.
 */
static int
cdev_clone(int fd, dev_t dev, devminor_t new_minor)
{
	struct vnode *vp;
	struct node_details res;
	int r;

	assert(fd != -1);

	/* Device number of the new device. */
	dev = makedev(major(dev), new_minor);

	/* Create a new file system node on PFS for the cloned device. */
	r = req_newnode(PFS_PROC_NR, fp->fp_effuid, fp->fp_effgid,
	    RWX_MODES | I_CHAR_SPECIAL, dev, &res);
	if (r != OK) {
		(void)cdev_close(dev);
		return r;
	}

	/* Drop the old node and use the new values. */
	if ((vp = get_free_vnode()) == NULL) {
		req_putnode(PFS_PROC_NR, res.inode_nr, 1); /* is this right? */
		(void)cdev_close(dev);
		return err_code;
	}
	lock_vnode(vp, VNODE_OPCL);

	assert(fp->fp_filp[fd] != NULL);
	unlock_vnode(fp->fp_filp[fd]->filp_vno);
	put_vnode(fp->fp_filp[fd]->filp_vno);

	vp->v_fs_e = res.fs_e;
	vp->v_vmnt = NULL;
	vp->v_dev = NO_DEV;
	vp->v_inode_nr = res.inode_nr;
	vp->v_mode = res.fmode;
	vp->v_sdev = dev;
	vp->v_fs_count = 1;
	vp->v_ref_count = 1;
	fp->fp_filp[fd]->filp_vno = vp;

	return OK;
}

/*
 * Open or close a character device.  The given operation must be either
 * CDEV_OPEN or CDEV_CLOSE.  For CDEV_OPEN, 'fd' must be the file descriptor
 * for the file being opened; for CDEV_CLOSE, it is ignored.  For CDEV_OPEN,
 * 'flags' identifies a bitwise combination of R_BIT, W_BIT, and/or O_NOCTTY;
 * for CDEV_CLOSE, it too is ignored.
 */
static int
cdev_opcl(int op, dev_t dev, int fd, int flags)
{
	devminor_t minor_dev, new_minor;
	struct dmap *dp;
	struct fproc *rfp;
	message dev_mess;
	int r, r2, acc;

	/*
	 * We need the a descriptor for CDEV_OPEN, because if the driver
	 * returns a cloned device, we need to replace what the fd points to.
	 * For CDEV_CLOSE however, we may be closing a device for which the
	 * calling process has no file descriptor, and thus we expect no
	 * meaningful fd value in that case.
	 */
	assert(op == CDEV_OPEN || op == CDEV_CLOSE);
	assert(fd != -1 || op == CDEV_CLOSE);

	/* Determine task dmap. */
	if ((dp = cdev_get(dev, &minor_dev)) == NULL)
		return ENXIO;

	/*
	 * CTTY exception: do not actually send the open/close request for
	 * /dev/tty to the driver.  This avoids the case that the actual device
	 * will remain open forever if the process calls setsid() after opening
	 * /dev/tty.
	 */
	if (major(dev) == CTTY_MAJOR) return OK;

	/*
	 * Add O_NOCTTY to the access flags if this process is not a session
	 * leader, or if it already has a controlling tty, or if it is someone
	 * else's controlling tty.  For performance reasons, only search the
	 * full process table if this driver has set controlling TTYs before.
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
		acc = 0;
		if (flags & R_BIT) acc |= CDEV_R_BIT;
		if (flags & W_BIT) acc |= CDEV_W_BIT;
		if (flags & O_NOCTTY) acc |= CDEV_NOCTTY;
		dev_mess.m_vfs_lchardriver_openclose.user = who_e;
		dev_mess.m_vfs_lchardriver_openclose.access = acc;
	}

	/* Send the request to the driver. */
	if ((r = asynsend3(dp->dmap_driver, &dev_mess, AMF_NOREPLY)) != OK)
		panic("VFS: asynsend in cdev_opcl failed: %d", r);

	/* Block the thread waiting for a reply. */
	self->w_task = dp->dmap_driver;
	self->w_drv_sendrec = &dev_mess;

	worker_wait();

	self->w_task = NONE;
	assert(self->w_drv_sendrec == NULL);

	/* Process the reply. */
	r = dev_mess.m_lchardriver_vfs_reply.status;

	if (op == CDEV_OPEN && r >= 0) {
		/*
		 * Some devices need special processing upon open.  Such a
		 * device is "cloned", i.e., on a succesful open it is replaced
		 * by a new device with a new unique minor device number.  This
		 * new device number identifies a new object that has been
		 * allocated within a driver.
		 */
		if (r & CDEV_CLONED) {
			new_minor = r & ~(CDEV_CLONED | CDEV_CTTY);
			if ((r2 = cdev_clone(fd, dev, new_minor)) < 0)
				return r2;
		}

		/* Did this call make the TTY the controlling TTY? */
		if (r & CDEV_CTTY) {
			fp->fp_tty = dev;
			dp->dmap_seen_tty = TRUE;
		}

		r = OK;
	}

	/* Return the result from the driver. */
	return r;
}

/*
 * Open a character device.
 */
int
cdev_open(int fd, dev_t dev, int flags)
{

	return cdev_opcl(CDEV_OPEN, dev, fd, flags);
}

/*
 * Close a character device.
 */
int
cdev_close(dev_t dev)
{

	return cdev_opcl(CDEV_CLOSE, dev, -1, 0);
}

/*
 * Initiate a read, write, or ioctl to a character device.  The given operation
 * must be CDEV_READ, CDEV_WRITE, or CDEV_IOCTL.  The call is made on behalf of
 * user process 'proc_e'.  For read/write requests, 'bytes' is the number of
 * bytes to read into 'buf' at file position 'pos'.  For ioctl requests,
 * 'bytes' is actually an IOCTL request code, which implies the size of the
 * buffer 'buf' if needed for the request at all ('pos' is ignored here).  The
 * 'flags' field contains file pointer flags, from which O_NONBLOCK is tested.
 */
int
cdev_io(int op, dev_t dev, endpoint_t proc_e, vir_bytes buf, off_t pos,
	unsigned long bytes, int flags)
{
	devminor_t minor_dev;
	struct dmap *dp;
	message dev_mess;
	cp_grant_id_t gid;
	int r;

	assert(op == CDEV_READ || op == CDEV_WRITE || op == CDEV_IOCTL);

	/* Determine task map. */
	if ((dp = cdev_get(dev, &minor_dev)) == NULL)
		return EIO;

	/*
	 * Handle TIOCSCTTY ioctl: set controlling TTY.  FIXME: this should not
	 * hardcode major device numbers, and not assume that the IOCTL request
	 * succeeds!
	 */
	if (op == CDEV_IOCTL && bytes == TIOCSCTTY &&
	    (major(dev) == TTY_MAJOR || major(dev) == PTY_MAJOR)) {
		fp->fp_tty = dev;
	}

	/* Create a grant for the buffer provided by the user process. */
	if (op != CDEV_IOCTL) {
		gid = cpf_grant_magic(dp->dmap_driver, proc_e, buf,
		    (size_t)bytes, (op == CDEV_READ) ? CPF_WRITE : CPF_READ);
		if (!GRANT_VALID(gid))
			panic("VFS: cpf_grant_magic failed");
	} else
		gid = make_ioctl_grant(dp->dmap_driver, proc_e, buf, bytes);

	/* Set up the message that will be sent to the driver. */
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
	fp->fp_cdev.dev = dev;
	fp->fp_cdev.endpt = dp->dmap_driver;
	fp->fp_cdev.grant = gid;	/* revoke this when unsuspended */
	suspend(FP_BLOCKED_ON_CDEV);

	return SUSPEND;
}

/*
 * Initiate a select call on a device.  Return OK iff the request was sent.
 * This function explicitly bypasses cdev_get() since it must not do CTTY
 * mapping, because a) the caller already has done that, b) "fp" may be wrong.
 */
int
cdev_select(dev_t dev, int ops)
{
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

	return OK;
}

/*
 * Cancel an I/O request, blocking until it has been cancelled.
 */
int
cdev_cancel(dev_t dev, endpoint_t endpt __unused, cp_grant_id_t grant)
{
	devminor_t minor_dev;
	message dev_mess;
	struct dmap *dp;
	int r;

	/* Determine task dmap. */
	if ((dp = cdev_get(dev, &minor_dev)) == NULL)
		return EIO;

	/* Prepare the request message. */
	memset(&dev_mess, 0, sizeof(dev_mess));
	dev_mess.m_type = CDEV_CANCEL;
	dev_mess.m_vfs_lchardriver_cancel.minor = minor_dev;
	dev_mess.m_vfs_lchardriver_cancel.id = fp->fp_endpoint;

	/* Send the request to the driver. */
	if ((r = asynsend3(dp->dmap_driver, &dev_mess, AMF_NOREPLY)) != OK)
		panic("VFS: asynsend in cdev_cancel failed: %d", r);

	/* Suspend this thread until we have received the response. */
	self->w_task = dp->dmap_driver;
	self->w_drv_sendrec = &dev_mess;

	worker_wait();

	self->w_task = NONE;
	assert(self->w_drv_sendrec == NULL);

	/* Clean up. */
	if (GRANT_VALID(grant))
		(void)cpf_revoke(grant);

	/* Return the result.  Note that the request may have completed. */
	r = dev_mess.m_lchardriver_vfs_reply.status;

	return (r == EAGAIN) ? EINTR : r; /* see below regarding error codes */
}

/*
 * A character driver has results for an open, close, read, write, or ioctl
 * call (i.e., everything except select).  There may be a thread waiting for
 * these results as part of an ongoing open, close, or (for read/write/ioctl)
 * cancel call.  If so, wake up that thread; if not, send a reply to the
 * requesting process. This function MUST NOT block its calling thread.
 */
static void
cdev_generic_reply(message * m_ptr)
{
	struct fproc *rfp;
	struct worker_thread *wp;
	endpoint_t proc_e;
	int r, slot;

	proc_e = m_ptr->m_lchardriver_vfs_reply.id;

	if (m_ptr->m_lchardriver_vfs_reply.status == SUSPEND) {
		printf("VFS: ignoring SUSPEND status from %d\n",
		    m_ptr->m_source);
		return;
	}

	if (isokendpt(proc_e, &slot) != OK) {
		printf("VFS: proc %d from %d not found\n",
		    proc_e, m_ptr->m_source);
		return;
	}
	rfp = &fproc[slot];
	wp = rfp->fp_worker;
	if (wp != NULL && wp->w_task == who_e && wp->w_drv_sendrec != NULL) {
		assert(!fp_is_blocked(rfp));
		*wp->w_drv_sendrec = *m_ptr;
		wp->w_drv_sendrec = NULL;
		worker_signal(wp);	/* continue open/close/cancel */
	} else if (rfp->fp_blocked_on != FP_BLOCKED_ON_CDEV ||
	    rfp->fp_cdev.endpt != m_ptr->m_source) {
		/*
		 * This would typically be caused by a protocol error, i.e., a
		 * driver not properly following the character driver protocol.
		 */
		printf("VFS: proc %d not blocked on %d\n",
		    proc_e, m_ptr->m_source);
	} else {
		/*
		 * Some services use the same infrastructure for nonblocking
		 * and cancelled requests, resulting in one of EINTR or EAGAIN
		 * when the other is really the appropriate code.  Thus,
		 * cdev_cancel converts EAGAIN into EINTR, and we convert EINTR
		 * into EAGAIN here.  TODO: this may be obsolete by now..?
		 */
		r = m_ptr->m_lchardriver_vfs_reply.status;
		revive(proc_e, (r == EINTR) ? EAGAIN : r);
	}
}

/*
 * A character driver has results for us.
 */
void
cdev_reply(void)
{

	if (get_dmap_by_endpt(who_e) == NULL) {
		printf("VFS: ignoring char dev reply from unknown driver %d\n",
		    who_e);
		return;
	}

	switch (call_nr) {
	case CDEV_REPLY:
		cdev_generic_reply(&m_in);
		break;
	case CDEV_SEL1_REPLY:
		select_cdev_reply1(m_in.m_source,
		    m_in.m_lchardriver_vfs_sel1.minor,
		    m_in.m_lchardriver_vfs_sel1.status);
		break;
	case CDEV_SEL2_REPLY:
		select_cdev_reply2(m_in.m_source,
		    m_in.m_lchardriver_vfs_sel2.minor,
		    m_in.m_lchardriver_vfs_sel2.status);
		break;
	default:
		printf("VFS: char driver %u sent unknown reply %x\n",
		    who_e, call_nr);
	}
}
