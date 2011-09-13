/* When a needed block is not in the cache, it must be fetched from the disk.
 * Special character files also require I/O.  The routines for these are here.
 *
 * The entry points in this file are:
 *   dev_open:   FS opens a device
 *   dev_close:  FS closes a device
 *   dev_io:	 FS does a read or write on a device
 *   dev_status: FS processes callback request alert
 *   gen_opcl:   generic call to a task to perform an open/close
 *   gen_io:     generic call to a task to perform an I/O operation
 *   no_dev:     open/close processing for devices that don't exist
 *   no_dev_io:  i/o processing for devices that don't exist
 *   tty_opcl:   perform tty-specific processing for open/close
 *   ctty_opcl:  perform controlling-tty-specific processing for open/close
 *   ctty_io:    perform controlling-tty-specific processing for I/O
 *   pm_setsid:	 perform VFS's side of setsid system call
 *   do_ioctl:	 perform the IOCTL system call
 */

#include "fs.h"
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/endpoint.h>
#include <minix/ioctl.h>
#include <minix/u64.h>
#include "file.h"
#include "fproc.h"
#include "dmap.h"
#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"
#include "param.h"

FORWARD _PROTOTYPE( void restart_reopen, (int major)			);
FORWARD _PROTOTYPE( int safe_io_conversion, (endpoint_t, cp_grant_id_t *,
					     int *,
					     endpoint_t *, void **,
					     size_t, u32_t *)	);

PRIVATE int dummyproc;


/*===========================================================================*
 *				dev_open				     *
 *===========================================================================*/
PUBLIC int dev_open(
  dev_t dev,			/* device to open */
  endpoint_t proc_e,		/* process to open for */
  int flags			/* mode bits and flags */
)
{
  int major, r;

  /* Determine the major device number call the device class specific
   * open/close routine.  (This is the only routine that must check the
   * device number for being in range.  All others can trust this check.)
   */
  major = major(dev);
  if (major < 0 || major >= NR_DEVICES) major = 0;
  if (dmap[major].dmap_driver == NONE) return(ENXIO);
  r = (*dmap[major].dmap_opcl)(DEV_OPEN, dev, proc_e, flags);
  return(r);
}


/*===========================================================================*
 *				dev_reopen				     *
 *===========================================================================*/
PUBLIC int dev_reopen(
  dev_t dev,			/* device to open */
  int filp_no,			/* filp to reopen for */
  int flags			/* mode bits and flags */
)
{
/* Reopen a device after a failing device driver */

  int major, r;
  struct dmap *dp;

  /* Determine the major device number and call the device class specific
   * open/close routine.  (This is the only routine that must check the device
   * number for being in range.  All others can trust this check.)
   */

  major = major(dev);
  if (major < 0 || major >= NR_DEVICES) major = 0;
  dp = &dmap[major];
  if (dp->dmap_driver == NONE) return(ENXIO);
  r = (*dp->dmap_opcl)(DEV_REOPEN, dev, filp_no, flags);
  if (r == SUSPEND) r = OK;
  return(r);
}


/*===========================================================================*
 *				dev_close				     *
 *===========================================================================*/
PUBLIC int dev_close(
  dev_t dev,			/* device to close */
  int filp_no
)
{
/* Close a device */
  int r, major;

  /* See if driver is roughly valid. */
  major = major(dev);
  if (major < 0 || major >= NR_DEVICES) return(ENXIO);
  if (dmap[major].dmap_driver == NONE) return(ENXIO);
  r = (*dmap[major].dmap_opcl)(DEV_CLOSE, dev, filp_no, 0);
  return(r);
}


/*===========================================================================*
 *				find_suspended_ep			     *
 *===========================================================================*/
endpoint_t find_suspended_ep(endpoint_t driver, cp_grant_id_t g)
{
/* A process is suspended on a driver for which VFS issued a grant. Find out
 * which process it was.
 */
  struct fproc *rfp;
  for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	if(rfp->fp_pid == PID_FREE)
		continue;

	if(rfp->fp_blocked_on == FP_BLOCKED_ON_OTHER &&
	   rfp->fp_task == driver && rfp->fp_grant == g)
		return(rfp->fp_endpoint);
  }

  return(NONE);
}


/*===========================================================================*
 *				dev_status				     *
 *===========================================================================*/
PUBLIC void dev_status(message *m)
{
/* A device sent us a notification it has something for us. Retrieve it. */

  message st;
  int major, get_more = 1;
  endpoint_t endpt;

  for (major = 0; major < NR_DEVICES; major++)
	if (dmap_driver_match(m->m_source, major))
		break; /* 'major' is the device that sent the message */

  if (major >= NR_DEVICES)	/* Device endpoint not found; nothing to do */
	return;

  if (dmap[major].dmap_style == STYLE_DEVA ||
      dmap[major].dmap_style == STYLE_CLONE_A) {
	printf("VFS: not doing dev_status for async driver %d\n", m->m_source);
	return;
  }

  /* Continuously send DEV_STATUS messages until the device has nothing to
   * say to us anymore. */
  do {
	int r;
	st.m_type = DEV_STATUS;
	r = sendrec(m->m_source, &st);
	if (r == OK && st.REP_STATUS == ERESTART) r = EDEADEPT;
	if (r != OK) {
		printf("VFS: DEV_STATUS failed to %d: %d\n", m->m_source, r);
		if (r == EDEADSRCDST || r == EDEADEPT) return;
		panic("VFS: couldn't sendrec for DEV_STATUS: %d", r);
	}

	switch(st.m_type) {
	  case DEV_REVIVE:
		/* We've got results for a read/write/ioctl call to a
		 * synchronous character driver */
		endpt = st.REP_ENDPT;
		if (endpt == VFS_PROC_NR) {
			endpt = find_suspended_ep(m->m_source,st.REP_IO_GRANT);
			if(endpt == NONE) {
			  printf("VFS: proc with grant %d from %d not found\n",
				 st.REP_IO_GRANT, st.m_source);
			  continue;
			}
		}
		revive(endpt, st.REP_STATUS);
		break;
	  case DEV_IO_READY:
		/* Reply to a select request: driver is ready for I/O */
		select_reply2(st.m_source, st.DEV_MINOR, st.DEV_SEL_OPS);
		break;
	  default:
		printf("VFS: unrecognized reply %d to DEV_STATUS\n",st.m_type);
		/* Fall through. */
	  case DEV_NO_STATUS:
		get_more = 0;
		break;
	}
  } while(get_more);
}

/*===========================================================================*
 *				safe_io_conversion			     *
 *===========================================================================*/
PRIVATE int safe_io_conversion(driver, gid, op, io_ept, buf, bytes, pos_lo)
endpoint_t driver;
cp_grant_id_t *gid;
int *op;
endpoint_t *io_ept;
void **buf;
size_t bytes;
u32_t *pos_lo;
{
/* Convert operation to the 'safe' variant (i.e., grant based) if applicable.
 * If no copying of data is involved, there is also no need to convert. */

  int access = 0;
  size_t size;

  *gid = GRANT_INVALID;		/* Grant to buffer */

  switch(*op) {
    case VFS_DEV_READ:
    case VFS_DEV_WRITE:
	/* Change to safe op. */
	*op = (*op == VFS_DEV_READ) ? DEV_READ_S : DEV_WRITE_S;
	*gid = cpf_grant_magic(driver, *io_ept, (vir_bytes) *buf, bytes,
			       *op == DEV_READ_S ? CPF_WRITE : CPF_READ);
	if (*gid < 0)
		panic("VFS: cpf_grant_magic of READ/WRITE buffer failed");
	break;
    case VFS_DEV_IOCTL:
	*pos_lo = *io_ept; /* Old endpoint in POSITION field. */
	*op = DEV_IOCTL_S;
	if(_MINIX_IOCTL_IOR(m_in.REQUEST)) access |= CPF_WRITE;
	if(_MINIX_IOCTL_IOW(m_in.REQUEST)) access |= CPF_READ;
	if(_MINIX_IOCTL_BIG(m_in.REQUEST))
		size = _MINIX_IOCTL_SIZE_BIG(m_in.REQUEST);
	else
		size = _MINIX_IOCTL_SIZE(m_in.REQUEST);

	/* Grant access to the buffer even if no I/O happens with the ioctl, in
	 * order to disambiguate requests with DEV_IOCTL_S.
	 */
	*gid = cpf_grant_magic(driver, *io_ept, (vir_bytes) *buf, size, access);
	if (*gid < 0)
		panic("VFS: cpf_grant_magic IOCTL buffer failed");

	break;
    case VFS_DEV_SELECT:
	*op = DEV_SELECT;
	break;
    default:
	panic("VFS: unknown operation %d for safe I/O conversion", *op);
  }

  /* If we have converted to a safe operation, I/O endpoint becomes VFS if it
   * wasn't already.
   */
  if(GRANT_VALID(*gid)) {
	*io_ept = VFS_PROC_NR;
	return(1);
  }

  /* Not converted to a safe operation (because there is no copying involved in
   * this operation).
   */
  return(0);
}

/*===========================================================================*
 *				dev_io					     *
 *===========================================================================*/
PUBLIC int dev_io(
  int op,			/* DEV_READ, DEV_WRITE, DEV_IOCTL, etc. */
  dev_t dev,			/* major-minor device number */
  int proc_e,			/* in whose address space is buf? */
  void *buf,			/* virtual address of the buffer */
  u64_t pos,			/* byte position */
  size_t bytes,			/* how many bytes to transfer */
  int flags,			/* special flags, like O_NONBLOCK */
  int suspend_reopen		/* Just suspend the process */
)
{
/* Read from or write to a device.  The parameter 'dev' tells which one. */
  struct dmap *dp;
  u32_t pos_lo, pos_high;
  message dev_mess;
  cp_grant_id_t gid = GRANT_INVALID;
  int safe, minor_dev, major_dev;
  void *buf_used;
  endpoint_t ioproc;

  pos_lo = ex64lo(pos);
  pos_high = ex64hi(pos);
  major_dev = major(dev);
  minor_dev = minor(dev);

  /* Determine task dmap. */
  dp = &dmap[major_dev];

  /* See if driver is roughly valid. */
  if (dp->dmap_driver == NONE) {
	printf("VFS: dev_io: no driver for major %d\n", major_dev);
	return(ENXIO);
  }

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

  /* By default, these are right. */
  dev_mess.USER_ENDPT = proc_e;
  dev_mess.ADDRESS  = buf;

  /* Convert DEV_* to DEV_*_S variants. */
  buf_used = buf;
  safe = safe_io_conversion(dp->dmap_driver, &gid, &op,
			    (endpoint_t *) &dev_mess.USER_ENDPT, &buf_used,
			    bytes, &pos_lo);

  /* If the safe conversion was done, set the IO_GRANT to
   * the grant id.
   */
  if(safe) dev_mess.IO_GRANT = (char *) gid;

  /* Set up the rest of the message passed to task. */
  dev_mess.m_type   = op;
  dev_mess.DEVICE   = minor_dev;
  dev_mess.POSITION = pos_lo;
  dev_mess.COUNT    = bytes;
  dev_mess.HIGHPOS  = pos_high;

  /* This will be used if the i/o is suspended. */
  ioproc = dev_mess.USER_ENDPT;

  /* Call the task. */
  (*dp->dmap_io)(dp->dmap_driver, &dev_mess);

  if(dp->dmap_driver == NONE) {
	/* Driver has vanished. */
	printf("VFS: driver gone?!\n");
	if(safe) cpf_revoke(gid);
	return(EIO);
  }

  /* Task has completed.  See if call completed. */
  if (dev_mess.REP_STATUS == SUSPEND) {
	if ((flags & O_NONBLOCK) && !(dp->dmap_style == STYLE_DEVA ||
				      dp->dmap_style == STYLE_CLONE_A)) {
		/* Not supposed to block. */
		dev_mess.m_type = CANCEL;
		dev_mess.USER_ENDPT = ioproc;
		dev_mess.IO_GRANT = (char *) gid;

		/* This R_BIT/W_BIT check taken from suspend()/unpause()
		 * logic. Mode is expected in the COUNT field.
		 */
		dev_mess.COUNT = 0;
		if (call_nr == READ) 		dev_mess.COUNT = R_BIT;
		else if (call_nr == WRITE)	dev_mess.COUNT = W_BIT;
		dev_mess.DEVICE = minor_dev;
		(*dp->dmap_io)(dp->dmap_driver, &dev_mess);
		if (dev_mess.REP_STATUS == EINTR) dev_mess.REP_STATUS = EAGAIN;
	} else {
		/* select() will do suspending itself. */
		if(op != DEV_SELECT) {
			/* Suspend user. */
			wait_for(dp->dmap_driver);
		}
		assert(!GRANT_VALID(fp->fp_grant));
		fp->fp_grant = gid;	/* revoke this when unsuspended. */
		fp->fp_ioproc = ioproc;

		if (flags & O_NONBLOCK) {
			/* Not supposed to block, send cancel message */
			dev_mess.m_type = CANCEL;
			dev_mess.USER_ENDPT = ioproc;
			dev_mess.IO_GRANT = (char *) gid;

			/* This R_BIT/W_BIT check taken from suspend()/unpause()
			 * logic. Mode is expected in the COUNT field.
			 */
			dev_mess.COUNT = 0;
			if(call_nr == READ) 		dev_mess.COUNT = R_BIT;
			else if(call_nr == WRITE)	dev_mess.COUNT = W_BIT;
			dev_mess.DEVICE = minor_dev;
			(*dp->dmap_io)(dp->dmap_driver, &dev_mess);

			/* Should do something about EINTR -> EAGAIN mapping */
		}
		return(SUSPEND);
	}
  }

  /* No suspend, or cancelled suspend, so I/O is over and can be cleaned up. */
  if(safe) cpf_revoke(gid);

  return(dev_mess.REP_STATUS);
}

/*===========================================================================*
 *				gen_opcl				     *
 *===========================================================================*/
PUBLIC int gen_opcl(
  int op,			/* operation, DEV_OPEN or DEV_CLOSE */
  dev_t dev,			/* device to open or close */
  endpoint_t proc_e,		/* process to open/close for */
  int flags			/* mode bits and flags */
)
{
/* Called from the dmap struct on opens & closes of special files.*/
  int r, minor_dev, major_dev;
  struct dmap *dp;
  message dev_mess;

  /* Determine task dmap. */
  major_dev = major(dev);
  minor_dev = minor(dev);
  if (major_dev < 0 || major_dev >= NR_DEVICES) return(ENXIO);
  dp = &dmap[major_dev];
  if (dp->dmap_driver == NONE) {
	printf("VFS: gen_opcl: no driver for major %d\n", major_dev);
	return(ENXIO);
  }

  dev_mess.m_type   = op;
  dev_mess.DEVICE   = minor_dev;
  dev_mess.USER_ENDPT = proc_e;
  dev_mess.COUNT    = flags;

  /* Call the task. */
  r = (*dp->dmap_io)(dp->dmap_driver, &dev_mess);
  if (r != OK) return(r);

  return(dev_mess.REP_STATUS);
}

/*===========================================================================*
 *				tty_opcl				     *
 *===========================================================================*/
PUBLIC int tty_opcl(
  int op,			/* operation, DEV_OPEN or DEV_CLOSE */
  dev_t dev,			/* device to open or close */
  endpoint_t proc_e,		/* process to open/close for */
  int flags			/* mode bits and flags */
)
{
/* This procedure is called from the dmap struct on tty open/close. */

  int r;
  register struct fproc *rfp;

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
PUBLIC int ctty_opcl(
  int op,			/* operation, DEV_OPEN or DEV_CLOSE */
  dev_t dev,			/* device to open or close */
  endpoint_t proc_e,		/* process to open/close for */
  int flags			/* mode bits and flags */
)
{
/* This procedure is called from the dmap struct on opening or closing
 * /dev/tty, the magic device that translates to the controlling tty.
 */

  return(fp->fp_tty == 0 ? ENXIO : OK);
}


/*===========================================================================*
 *				pm_setsid				     *
 *===========================================================================*/
PUBLIC void pm_setsid(proc_e)
int proc_e;
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
PUBLIC int do_ioctl()
{
/* Perform the ioctl(ls_fd, request, argx) system call (uses m2 fmt). */

  int r = OK, suspend_reopen;
  struct filp *f;
  register struct vnode *vp;
  dev_t dev;

  if ((f = get_filp(m_in.ls_fd, VNODE_READ)) == NULL) return(err_code);
  vp = f->filp_vno;		/* get vnode pointer */
  if ((vp->v_mode & I_TYPE) != I_CHAR_SPECIAL &&
      (vp->v_mode & I_TYPE) != I_BLOCK_SPECIAL) {
	r = ENOTTY;
  }

  if (r == OK) {
	suspend_reopen = (f->filp_state != FS_NORMAL);
	dev = (dev_t) vp->v_sdev;

	r = dev_io(VFS_DEV_IOCTL, dev, who_e, m_in.ADDRESS, cvu64(0),
		   m_in.REQUEST, f->filp_flags, suspend_reopen);
  }

  unlock_filp(f);

  return(r);
}


/*===========================================================================*
 *				gen_io					     *
 *===========================================================================*/
PUBLIC int gen_io(driver_e, mess_ptr)
endpoint_t driver_e;		/* which endpoint to call */
message *mess_ptr;		/* pointer to message for task */
{
/* All file system I/O ultimately comes down to I/O on major/minor device
 * pairs.  These lead to calls on the following routines via the dmap table.
 */

  int r, proc_e;

  proc_e = mess_ptr->USER_ENDPT;

  r = sendrec(driver_e, mess_ptr);
  if (r == OK && mess_ptr->REP_STATUS == ERESTART) r = EDEADEPT;
  if (r != OK) {
	if (r == EDEADSRCDST || r == EDEADEPT) {
		printf("VFS: dead driver %d\n", driver_e);
		dmap_unmap_by_endpt(driver_e);
		return(r);
	} else if (r == ELOCKED) {
		printf("VFS: ELOCKED talking to %d\n", driver_e);
		return(r);
	}
	panic("call_task: can't send/receive: %d", r);
  }

  /* Did the process we did the sendrec() for get a result? */
  if (mess_ptr->REP_ENDPT != proc_e) {
	printf("VFS: strange device reply from %d, type = %d, "
		"proc = %d (not %d) (2) ignored\n", mess_ptr->m_source,
		mess_ptr->m_type, proc_e, mess_ptr->REP_ENDPT);

	return(EIO);
  }

  return(OK);
}


/*===========================================================================*
 *				asyn_io					     *
 *===========================================================================*/
PUBLIC int asyn_io(task_nr, mess_ptr)
int task_nr;			/* which task to call */
message *mess_ptr;		/* pointer to message for task */
{
/* All file system I/O ultimately comes down to I/O on major/minor device
 * pairs. These lead to calls on the following routines via the dmap table.
 */

  int r;

  fp->fp_sendrec = mess_ptr;	/* Remember where result should be stored */
  r = asynsend3(task_nr, mess_ptr, AMF_NOREPLY);

  if (r != OK) panic("VFS: asynsend in asyn_io failed: %d", r);

  /* Fake a SUSPEND */
  mess_ptr->REP_STATUS = SUSPEND;
  return(OK);
}


/*===========================================================================*
 *				ctty_io					     *
 *===========================================================================*/
PUBLIC int ctty_io(task_nr, mess_ptr)
int task_nr;			/* not used - for compatibility with dmap_t */
message *mess_ptr;		/* pointer to message for task */
{
/* This routine is only called for one device, namely /dev/tty.  Its job
 * is to change the message to use the controlling terminal, instead of the
 * major/minor pair for /dev/tty itself.
 */

  struct dmap *dp;

  if (fp->fp_tty == 0) {
	/* No controlling tty present anymore, return an I/O error. */
	mess_ptr->REP_STATUS = EIO;
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

	(*dp->dmap_io)(dp->dmap_driver, mess_ptr);
  }

  return(OK);
}


/*===========================================================================*
 *				no_dev					     *
 *===========================================================================*/
PUBLIC int no_dev(
  int UNUSED(op),		/* operation, DEV_OPEN or DEV_CLOSE */
  dev_t UNUSED(dev),		/* device to open or close */
  int UNUSED(proc),		/* process to open/close for */
  int UNUSED(flags)		/* mode bits and flags */
)
{
/* Called when opening a nonexistent device. */
  return(ENODEV);
}

/*===========================================================================*
 *				no_dev_io				     *
 *===========================================================================*/
PUBLIC int no_dev_io(int proc, message *m)
{
/* Called when doing i/o on a nonexistent device. */
  printf("VFS: I/O on unmapped device number\n");
  return(EIO);
}


/*===========================================================================*
 *				clone_opcl				     *
 *===========================================================================*/
PUBLIC int clone_opcl(
  int op,			/* operation, DEV_OPEN or DEV_CLOSE */
  dev_t dev,			/* device to open or close */
  int proc_e,			/* process to open/close for */
  int flags			/* mode bits and flags */
)
{
/* Some devices need special processing upon open.  Such a device is "cloned",
 * i.e. on a succesful open it is replaced by a new device with a new unique
 * minor device number.  This new device number identifies a new object (such
 * as a new network connection) that has been allocated within a task.
 */
  struct dmap *dp;
  int r, minor_dev, major_dev;
  message dev_mess;

  /* Determine task dmap. */
  minor_dev = minor(dev);
  major_dev = major(dev);
  if (major_dev < 0 || major_dev >= NR_DEVICES) return(ENXIO);
  dp = &dmap[major_dev];
  if (dp->dmap_driver == NONE) {
	printf("VFS clone_opcl: no driver for major %d\n", major_dev);
	return(ENXIO);
  }

  dev_mess.m_type   = op;
  dev_mess.DEVICE   = minor_dev;
  dev_mess.USER_ENDPT = proc_e;
  dev_mess.COUNT    = flags;

  if(isokendpt(dp->dmap_driver, &dummyproc) != OK) {
	printf("VFS clone_opcl: bad driver endpoint for major %d (%d)\n",
	       major_dev, dp->dmap_driver);
	return(ENXIO);
  }

  /* Call the task. */
  r = (*dp->dmap_io)(dp->dmap_driver, &dev_mess);
  if (r != OK) return(r);

  if (op == DEV_OPEN && dp->dmap_style == STYLE_CLONE_A) {
	/* Wait for reply when driver is asynchronous */
	worker_wait();
  }

  if (op == DEV_OPEN && dev_mess.REP_STATUS >= 0) {
	if (dev_mess.REP_STATUS != minor_dev) {
                struct vnode *vp;
                struct node_details res;

		/* A new minor device number has been returned.
                 * Request PFS to create a temporary device file to hold it.
                 */

                /* Device number of the new device. */
		dev = (dev & ~(BYTE << MINOR)) | (dev_mess.REP_STATUS << MINOR);

                /* Issue request */
		r = req_newnode(PFS_PROC_NR, fp->fp_effuid, fp->fp_effgid,
			ALL_MODES | I_CHAR_SPECIAL, dev, &res);
                if (r != OK) {
			(void) clone_opcl(DEV_CLOSE, dev, proc_e, 0);
			return r;
                }

                /* Drop old node and use the new values */
                vp = fp->fp_filp[m_in.fd]->filp_vno;

		unlock_vnode(vp);
                put_vnode(vp);
		if ((vp = get_free_vnode()) == NULL)
			return(err_code);

		lock_vnode(vp, VNODE_OPCL);

                vp->v_fs_e = res.fs_e;
                vp->v_vmnt = NULL;
                vp->v_dev = NO_DEV;
		vp->v_fs_e = res.fs_e;
                vp->v_inode_nr = res.inode_nr;
                vp->v_mode = res.fmode;
                vp->v_sdev = dev;
                vp->v_fs_count = 1;
                vp->v_ref_count = 1;
		fp->fp_filp[m_in.fd]->filp_vno = vp;
	}
	dev_mess.REP_STATUS = OK;
  }
  return(dev_mess.REP_STATUS);
}


/*===========================================================================*
 *				dev_up					     *
 *===========================================================================*/
PUBLIC void dev_up(int maj)
{
  /* A new device driver has been mapped in. This function
   * checks if any filesystems are mounted on it, and if so,
   * dev_open()s them so the filesystem can be reused.
  */
  int r, new_driver_e, needs_reopen, fd_nr, found;
  struct filp *rfilp;
  struct vmnt *vmp;
  struct fproc *rfp;
  struct vnode *vp;

  /* First deal with block devices. We need to consider both mounted file
   * systems and open block-special files.
   */
  if (maj < 0 || maj >= NR_DEVICES) panic("VFS: out-of-bound major");
  new_driver_e = dmap[maj].dmap_driver;

  /* Tell each affected mounted file system about the new endpoint. This code
   * is currently useless, as driver endpoints do not change across restarts.
   */
  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp) {
	int minor_dev, major_dev;
	major_dev = major(vmp->m_dev);
	minor_dev = minor(vmp->m_dev);
	if (major_dev != maj) continue;

	/* Send the new driver endpoint to the mounted file system. */
	if (OK != req_newdriver(vmp->m_fs_e, vmp->m_dev, new_driver_e))
		printf("VFS dev_up: error sending new driver endpoint."
		       " FS_e: %d req_nr: %d\n", vmp->m_fs_e, REQ_NEW_DRIVER);
  }

  /* For each block-special file that was previously opened on the affected
   * device, we need to reopen it on the new driver.
   */
  found = 0;
  for (rfilp = filp; rfilp < &filp[NR_FILPS]; rfilp++) {
	if (rfilp->filp_count < 1 || !(vp = rfilp->filp_vno)) continue;
	if (major(vp->v_sdev) != maj) continue;
	if (!S_ISBLK(vp->v_mode)) continue;

	/* Reopen the device on the driver, once per filp. */
	if ((r = dev_open(vp->v_sdev, VFS_PROC_NR, rfilp->filp_mode)) != OK)
		printf("VFS: mounted dev %d/%d re-open failed: %d.\n",
			maj, minor(vp->v_sdev), r);

	found = 1;
  }

  /* If any block-special file was open for this major at all, also inform the
   * root file system about the new endpoint of the driver. We do this even if
   * the block-special file is linked to another mounted file system, merely
   * because it is more work to check for that case.
   */
  if (found) {
	if (OK != req_newdriver(ROOT_FS_E, makedev(maj, 0), new_driver_e))
		printf("VFSdev_up: error sending new driver endpoint."
		       " FS_e: %d req_nr: %d\n", ROOT_FS_E, REQ_NEW_DRIVER);
  }

  /* The rest of the code deals with character-special files. To start with,
   * look for processes that are suspened in an OPEN call. Set FP_SUSP_REOPEN
   * to indicate that this process was suspended before the call to dev_up.
   */
  for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	if(rfp->fp_pid == PID_FREE) continue;
	if(rfp->fp_blocked_on != FP_BLOCKED_ON_DOPEN) continue;

	printf("VFS: dev_up: found process in FP_BLOCKED_ON_DOPEN, fd %d\n",
		rfp->fp_block_fd);
	fd_nr = rfp->fp_block_fd;
	rfilp = rfp->fp_filp[fd_nr];
	vp = rfilp->filp_vno;
	if (!vp) panic("VFS: restart_reopen: no vp");
	if ((vp->v_mode &  I_TYPE) != I_CHAR_SPECIAL) continue;
	if (major(vp->v_sdev) != maj) continue;

	rfp->fp_flags |= FP_SUSP_REOPEN;
  }

  needs_reopen= FALSE;
  for (rfilp = filp; rfilp < &filp[NR_FILPS]; rfilp++) {
	if (rfilp->filp_count < 1 || !(vp = rfilp->filp_vno)) continue;
	if (major(vp->v_sdev) != maj) continue;
	if (!S_ISCHR(vp->v_mode)) continue;

	rfilp->filp_state = FS_NEEDS_REOPEN;
	needs_reopen = TRUE;
  }

  if (needs_reopen)
	restart_reopen(maj);

}

/*===========================================================================*
 *				open_reply				     *
 *===========================================================================*/
PUBLIC void open_reply(void)
{
  struct fproc *rfp;
  endpoint_t proc_e;
  int slot;

  proc_e = m_in.REP_ENDPT;
  if (isokendpt(proc_e, &slot) != OK) return;
  rfp = &fproc[slot];
  *rfp->fp_sendrec = m_in;
  worker_signal(worker_get(rfp->fp_wtid));	/* Continue open */
}

/*===========================================================================*
 *				restart_reopen				     *
 *===========================================================================*/
PRIVATE void restart_reopen(maj)
int maj;
{
  int n, r, minor_dev, major_dev, fd_nr;
  endpoint_t driver_e;
  struct vnode *vp;
  struct filp *rfilp;
  struct fproc *rfp;

  if (maj < 0 || maj >= NR_DEVICES) panic("VFS: out-of-bound major");
  for (rfilp = filp; rfilp < &filp[NR_FILPS]; rfilp++) {
	if (rfilp->filp_count < 1 || !(vp = rfilp->filp_vno)) continue;
	if (rfilp->filp_state != FS_NEEDS_REOPEN) continue;
	if ((vp->v_mode & I_TYPE) != I_CHAR_SPECIAL) continue;

	major_dev = major(vp->v_sdev);
	minor_dev = minor(vp->v_sdev);
	if (major_dev != maj) continue;

	if (!(rfilp->filp_flags & O_REOPEN)) {
		/* File descriptor is to be closed when driver restarts. */
		n = invalidate(rfilp);
		if (n != rfilp->filp_count) {
			printf("VFS: warning: invalidate/count "
			       "discrepancy (%d, %d)\n", n, rfilp->filp_count);
		}
		rfilp->filp_count = 0;
		continue;
	}

	r = dev_reopen(vp->v_sdev, rfilp-filp, vp->v_mode & (R_BIT|W_BIT));
	if (r == OK) return;

	/* Device could not be reopened. Invalidate all filps on that device.*/
	n = invalidate(rfilp);
	if (n != rfilp->filp_count) {
		printf("VFS: warning: invalidate/count "
			"discrepancy (%d, %d)\n", n, rfilp->filp_count);
	}
	rfilp->filp_count = 0;
	printf("VFS: file on dev %d/%d re-open failed: %d; "
		"invalidated %d fd's.\n", major_dev, minor_dev, r, n);
  }

  /* Nothing more to re-open. Restart suspended processes */
  driver_e = dmap[maj].dmap_driver;

  for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	if(rfp->fp_pid == PID_FREE) continue;
	if(rfp->fp_blocked_on == FP_BLOCKED_ON_OTHER &&
	   rfp->fp_task == driver_e && (rfp->fp_flags & FP_SUSP_REOPEN)) {
		rfp->fp_flags &= ~FP_SUSP_REOPEN;
		rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;
		reply(rfp->fp_endpoint, ERESTART);
	}
  }

  /* Look for processes that are suspened in an OPEN call */
  for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	if (rfp->fp_pid == PID_FREE) continue;
	if (rfp->fp_blocked_on == FP_BLOCKED_ON_DOPEN ||
	    !(rfp->fp_flags & FP_SUSP_REOPEN)) continue;

	printf("VFS: restart_reopen: found process in FP_BLOCKED_ON_DOPEN, fd %d\n",
		rfp->fp_block_fd);
	fd_nr =	rfp->fp_block_fd;
	rfilp = rfp->fp_filp[fd_nr];

	if (!rfilp) {
		/* Open failed, and automatic reopen was not requested */
		rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;
		FD_CLR(fd_nr, &rfp->fp_filp_inuse);
		reply(rfp->fp_endpoint, EIO);
		continue;
	}

	vp = rfilp->filp_vno;
	if (!vp) panic("VFS: restart_reopen: no vp");
	if ((vp->v_mode &  I_TYPE) != I_CHAR_SPECIAL) continue;
	if (major(vp->v_sdev) != maj) continue;

	rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;
	reply(rfp->fp_endpoint, fd_nr);
  }
}


/*===========================================================================*
 *				reopen_reply				     *
 *===========================================================================*/
PUBLIC void reopen_reply()
{
  endpoint_t driver_e;
  int filp_no, status, maj;
  struct filp *rfilp;
  struct vnode *vp;
  struct dmap *dp;

  driver_e = m_in.m_source;
  filp_no = m_in.REP_ENDPT;
  status = m_in.REP_STATUS;

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

  if (rfilp->filp_state != FS_NEEDS_REOPEN) {
	printf("VFS: reopen_reply: bad state %d for filp number %d"
	       " (from driver %d)\n", rfilp->filp_state, filp_no, driver_e);
	return;
  }

  if ((vp->v_mode & I_TYPE) != I_CHAR_SPECIAL) {
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
	rfilp->filp_state= FS_NORMAL;
  } else {
	printf("VFS: reopen_reply: should handle error status\n");
	return;
  }

  restart_reopen(maj);
}
