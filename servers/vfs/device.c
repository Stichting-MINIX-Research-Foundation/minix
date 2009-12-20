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
 *   do_ioctl:	 perform the IOCTL system call
 *   do_setsid:	 perform the SETSID system call (FS side)
 */

#include "fs.h"
#include <fcntl.h>
#include <assert.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/endpoint.h>
#include <minix/ioctl.h>
#include <minix/u64.h>
#include "file.h"
#include "fproc.h"
#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"
#include "param.h"

#define ELEMENTS(a) (sizeof(a)/sizeof((a)[0]))

FORWARD _PROTOTYPE( int safe_io_conversion, (endpoint_t, cp_grant_id_t *,
					     int *, cp_grant_id_t *, int,
					     endpoint_t *, void **, int *,
					     vir_bytes, u32_t *)	);
FORWARD _PROTOTYPE( void safe_io_cleanup, (cp_grant_id_t, cp_grant_id_t *,
					   int)				);
FORWARD _PROTOTYPE( void restart_reopen, (int maj)			);

extern int dmap_size;
PRIVATE int dummyproc;


/*===========================================================================*
 *				dev_open				     *
 *===========================================================================*/
PUBLIC int dev_open(dev, proc, flags)
dev_t dev;			/* device to open */
int proc;			/* process to open for */
int flags;			/* mode bits and flags */
{
  int major, r;
  struct dmap *dp;

  /* Determine the major device number call the device class specific
   * open/close routine.  (This is the only routine that must check the
   * device number for being in range.  All others can trust this check.)
   */
  major = (dev >> MAJOR) & BYTE;
  if (major >= NR_DEVICES) major = 0;
  dp = &dmap[major];
  if (dp->dmap_driver == NONE) return(ENXIO);
  r = (*dp->dmap_opcl)(DEV_OPEN, dev, proc, flags);
  return(r);
}


/*===========================================================================*
 *				dev_reopen				     *
 *===========================================================================*/
PUBLIC int dev_reopen(dev, filp_no, flags)
dev_t dev;			/* device to open */
int filp_no;			/* filp to reopen for */
int flags;			/* mode bits and flags */
{
  int major, r;
  struct dmap *dp;

  /* Determine the major device number call the device class specific
   * open/close routine.  (This is the only routine that must check the
   * device number for being in range.  All others can trust this check.)
   */
  major = (dev >> MAJOR) & BYTE;
  if (major >= NR_DEVICES) major = 0;
  dp = &dmap[major];
  if (dp->dmap_driver == NONE) return(ENXIO);
  r = (*dp->dmap_opcl)(DEV_REOPEN, dev, filp_no, flags);
  if (r == OK) panic(__FILE__,"OK on reopen from", dp->dmap_driver);
  if (r == SUSPEND) r = OK;
  return(r);
}


/*===========================================================================*
 *				dev_close				     *
 *===========================================================================*/
PUBLIC int dev_close(dev, filp_no)
dev_t dev;			/* device to close */
int filp_no;
{
  int r;

  /* See if driver is roughly valid. */
  if (dmap[(dev >> MAJOR)].dmap_driver == NONE) return(ENXIO);
  r = (*dmap[(dev >> MAJOR) & BYTE].dmap_opcl)(DEV_CLOSE, dev, filp_no, 0);
  return(r);
}


/*===========================================================================*
 *				suspended_ep				     *
 *===========================================================================*/
endpoint_t suspended_ep(endpoint_t driver, cp_grant_id_t g)
{
/* A process is suspended on a driver for which FS issued
 * a grant. Find out which process it was.
 */
  struct fproc *rfp;
  for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	if(rfp->fp_pid == PID_FREE) continue;
        if(rfp->fp_blocked_on == FP_BLOCKED_ON_OTHER &&
	   rfp->fp_task == driver && rfp->fp_grant == g)
		return rfp->fp_endpoint;
    }

    return(NONE);
}


/*===========================================================================*
 *				dev_status				     *
 *===========================================================================*/
PUBLIC void dev_status(message *m)
{
  message st;
  int d, get_more = 1;
  endpoint_t endpt;

  for(d = 0; d < NR_DEVICES; d++)
	if (dmap[d].dmap_driver != NONE && dmap[d].dmap_driver == m->m_source)
		break;

	if (d >= NR_DEVICES) return;
	if (dmap[d].dmap_async_driver) {
		printf("dev_status: not doing dev_status for async driver %d\n",
			m->m_source);
		return;
	}

	do {
		int r;
		st.m_type = DEV_STATUS;
		if ((r = sendrec(m->m_source, &st)) != OK) {
			printf("DEV_STATUS failed to %d: %d\n", m->m_source, r);
			if (r == EDEADSRCDST) return;
			if (r == EDSTDIED) return;
			if (r == ESRCDIED) return;
			panic(__FILE__,"couldn't sendrec for DEV_STATUS", r);
		}

		switch(st.m_type) {
			case DEV_REVIVE:
				endpt = st.REP_ENDPT;
				if(endpt == FS_PROC_NR) {
					endpt = suspended_ep(m->m_source,
						st.REP_IO_GRANT);
					if(endpt == NONE) {
						printf("FS: proc with grant %d"
						" from %d not found (revive)\n",
						st.REP_IO_GRANT, st.m_source);
						continue;
					}
				}
				revive(endpt, st.REP_STATUS);
				break;
			case DEV_IO_READY:
				select_notified(d, st.DEV_MINOR,
					st.DEV_SEL_OPS);
				break;
			default:
				printf("FS: unrecognized reply %d to "
					"DEV_STATUS\n", st.m_type);
				/* Fall through. */
			case DEV_NO_STATUS:
				get_more = 0;
				break;
		}
	} while(get_more);

	return;
}


/*===========================================================================*
 *				safe_io_conversion			     *
 *===========================================================================*/
PRIVATE int safe_io_conversion(driver, gid, op, gids, gids_size,
	io_ept, buf, vec_grants, bytes, pos_lo)
endpoint_t driver;
cp_grant_id_t *gid;
int *op;
cp_grant_id_t *gids;
int gids_size;
endpoint_t *io_ept;
void **buf;
int *vec_grants;
vir_bytes bytes;
u32_t *pos_lo;
{
  int access = 0, size, j;
  iovec_t *v;
  static iovec_t new_iovec[NR_IOREQS];

  /* Number of grants allocated in vector I/O. */
  *vec_grants = 0;

  /* Driver can handle it - change request to a safe one. */
  *gid = GRANT_INVALID;

  switch(*op) {
	case VFS_DEV_READ:
	case VFS_DEV_WRITE:
	   /* Change to safe op. */
	   *op = *op == VFS_DEV_READ ? DEV_READ_S : DEV_WRITE_S;

	   *gid = cpf_grant_magic(driver, *io_ept, (vir_bytes) *buf, bytes,
	   			  *op == DEV_READ_S ? CPF_WRITE : CPF_READ);
	   if (*gid < 0)
		panic(__FILE__, "cpf_grant_magic of buffer failed\n", NO_NUM);
	   break;
	case VFS_DEV_GATHER:
	case VFS_DEV_SCATTER:
	   /* Change to safe op. */
	   *op = *op == VFS_DEV_GATHER ? DEV_GATHER_S : DEV_SCATTER_S;

	   /* Grant access to my new i/o vector. */
	   *gid = cpf_grant_direct(driver, (vir_bytes) new_iovec,
				  bytes * sizeof(iovec_t), CPF_READ|CPF_WRITE);
	   if (*gid < 0) 
		panic(__FILE__, "cpf_grant_direct of vector failed", NO_NUM);
			
	   v = (iovec_t *) *buf;
	   /* Grant access to i/o buffers. */
	   for(j = 0; j < bytes; j++) {
		if(j >= NR_IOREQS) panic(__FILE__, "vec too big", bytes);

		new_iovec[j].iov_addr = 
		gids[j] = 
		cpf_grant_direct(driver, (vir_bytes) v[j].iov_addr, v[j].iov_size,
				 *op == DEV_GATHER_S ? CPF_WRITE : CPF_READ);

		if(!GRANT_VALID(gids[j])) 
			panic(__FILE__, "grant to iovec buf failed", NO_NUM);
			   
		new_iovec[j].iov_size = v[j].iov_size;
		(*vec_grants)++;
	   }

	   /* Set user's vector to the new one. */
	   *buf = new_iovec;
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


	   /* Do this even if no I/O happens with the ioctl, in
 	    * order to disambiguate requests with DEV_IOCTL_S.
	    */
	   *gid = cpf_grant_magic(driver, *io_ept, (vir_bytes) *buf, size,
	   			  access);
	   if (*gid < 0) 
		panic(__FILE__, "cpf_grant_magic failed (ioctl)\n", NO_NUM);
			
	   break;
	case VFS_DEV_SELECT:
	   *op = DEV_SELECT;
	   break;
	default:
	   panic(__FILE__,"safe_io_conversion: unknown operation", *op);
  }

  /* If we have converted to a safe operation, I/O
   * endpoint becomes FS if it wasn't already.
   */
  if(GRANT_VALID(*gid)) {
	*io_ept = FS_PROC_NR;
	return 1;
   }

   /* Not converted to a safe operation (because there is no
    * copying involved in this operation).
    */
  return 0;
}

/*===========================================================================*
 *			safe_io_cleanup					     *
 *===========================================================================*/
PRIVATE void safe_io_cleanup(gid, gids, gids_size)
cp_grant_id_t gid;
cp_grant_id_t *gids;
int gids_size;
{
/* Free resources (specifically, grants) allocated by safe_io_conversion(). */
  int j;

  cpf_revoke(gid);
  for(j = 0; j < gids_size; j++)
	cpf_revoke(gids[j]);
}


/*===========================================================================*
 *				dev_io					     *
 *===========================================================================*/
PUBLIC int dev_io(op, dev, proc_e, buf, pos, bytes, flags, suspend_reopen)
int op;				/* DEV_READ, DEV_WRITE, DEV_IOCTL, etc. */
dev_t dev;			/* major-minor device number */
int proc_e;			/* in whose address space is buf? */
void *buf;			/* virtual address of the buffer */
u64_t pos;			/* byte position */
int bytes;			/* how many bytes to transfer */
int flags;			/* special flags, like O_NONBLOCK */
int suspend_reopen;		/* Just suspend the process */
{
/* Read or write from a device.  The parameter 'dev' tells which one. */
  struct dmap *dp;
  u32_t pos_lo, pos_high;
  message dev_mess;
  cp_grant_id_t gid = GRANT_INVALID;
  static cp_grant_id_t gids[NR_IOREQS];
  int vec_grants = 0, orig_op, safe;
  void *buf_used;
  endpoint_t ioproc;

  pos_lo= ex64lo(pos);
  pos_high= ex64hi(pos);

  /* Determine task dmap. */
  dp = &dmap[(dev >> MAJOR) & BYTE];
  orig_op = op;

  /* See if driver is roughly valid. */
  if (dp->dmap_driver == NONE) {
	printf("FS: dev_io: no driver for dev %x\n", dev);
	return(ENXIO);
  }

  if (suspend_reopen) {
	/* Suspend user. */
	fp->fp_grant = GRANT_INVALID;
	fp->fp_ioproc = NONE;
	wait_for(dp->dmap_driver);
	fp->fp_flags |= SUSP_REOPEN;
	return(SUSPEND);
  }

  if(isokendpt(dp->dmap_driver, &dummyproc) != OK) {
	printf("FS: dev_io: old driver for dev %x (%d)\n",dev,dp->dmap_driver);
	return(ENXIO);
  }

  /* By default, these are right. */
  dev_mess.IO_ENDPT = proc_e;
  dev_mess.ADDRESS  = buf;

  /* Convert DEV_* to DEV_*_S variants. */
  buf_used = buf;
  safe = safe_io_conversion(dp->dmap_driver, &gid, &op, gids, NR_IOREQS,
  			    (endpoint_t*) &dev_mess.IO_ENDPT, &buf_used,
			    &vec_grants, bytes, &pos_lo);

  if(buf != buf_used)
	panic(__FILE__,"dev_io: safe_io_conversion changed buffer", NO_NUM);

  /* If the safe conversion was done, set the ADDRESS to
   * the grant id.
   */
  if(safe) dev_mess.IO_GRANT = (char *) gid;

  /* Set up the rest of the message passed to task. */
  dev_mess.m_type   = op;
  dev_mess.DEVICE   = (dev >> MINOR) & BYTE;
  dev_mess.POSITION = pos_lo;
  dev_mess.COUNT    = bytes;
  dev_mess.HIGHPOS  = pos_high;

  /* This will be used if the i/o is suspended. */
  ioproc = dev_mess.IO_ENDPT;

  /* Call the task. */
  (*dp->dmap_io)(dp->dmap_driver, &dev_mess);

  if(dp->dmap_driver == NONE) {
  	/* Driver has vanished. */
	printf("Driver gone?\n");
	if(safe) safe_io_cleanup(gid, gids, vec_grants);
	return(EIO);
  }

  /* Task has completed.  See if call completed. */
  if (dev_mess.REP_STATUS == SUSPEND) {
	if(vec_grants > 0) panic(__FILE__,"SUSPEND on vectored i/o", NO_NUM);
	
	/* fp is uninitialized at init time. */
	if(!fp) panic(__FILE__,"SUSPEND on NULL fp", NO_NUM);

	if ((flags & O_NONBLOCK) && !dp->dmap_async_driver) {
		/* Not supposed to block. */
		dev_mess.m_type = CANCEL;
		dev_mess.IO_ENDPT = ioproc;
		dev_mess.IO_GRANT = (char *) gid;

		/* This R_BIT/W_BIT check taken from suspend()/unpause()
		 * logic. Mode is expected in the COUNT field.
		 */
		dev_mess.COUNT = 0;
		if (call_nr == READ) 		dev_mess.COUNT = R_BIT;
		else if (call_nr == WRITE)	dev_mess.COUNT = W_BIT;
		dev_mess.DEVICE = (dev >> MINOR) & BYTE;
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
			dev_mess.IO_ENDPT = ioproc;
			dev_mess.IO_GRANT = (char *) gid;

			/* This R_BIT/W_BIT check taken from suspend()/unpause()
			 * logic. Mode is expected in the COUNT field.
			 */
			dev_mess.COUNT = 0;
			if(call_nr == READ) 		dev_mess.COUNT = R_BIT;
			else if(call_nr == WRITE)	dev_mess.COUNT = W_BIT;
			dev_mess.DEVICE = (dev >> MINOR) & BYTE;
			(*dp->dmap_io)(dp->dmap_driver, &dev_mess);

			/* Should do something about EINTR -> EAGAIN mapping */
		}
		return(SUSPEND);
	}
  }

  /* No suspend, or cancelled suspend, so I/O is over and can be cleaned up. */
  if(safe) safe_io_cleanup(gid, gids, vec_grants);

  return(dev_mess.REP_STATUS);
}

/*===========================================================================*
 *				gen_opcl				     *
 *===========================================================================*/
PUBLIC int gen_opcl(op, dev, proc_e, flags)
int op;				/* operation, DEV_OPEN or DEV_CLOSE */
dev_t dev;			/* device to open or close */
int proc_e;			/* process to open/close for */
int flags;			/* mode bits and flags */
{
/* Called from the dmap struct in table.c on opens & closes of special files.*/
  int r;
  struct dmap *dp;
  message dev_mess;

  /* Determine task dmap. */
  dp = &dmap[(dev >> MAJOR) & BYTE];

  dev_mess.m_type   = op;
  dev_mess.DEVICE   = (dev >> MINOR) & BYTE;
  dev_mess.IO_ENDPT = proc_e;
  dev_mess.COUNT    = flags;

  if (dp->dmap_driver == NONE) {
	printf("FS: gen_opcl: no driver for dev %x\n", dev);
	return(ENXIO);
  }

  /* Call the task. */
  r= (*dp->dmap_io)(dp->dmap_driver, &dev_mess);
  if (r != OK) return(r);

  return(dev_mess.REP_STATUS);
}

/*===========================================================================*
 *				tty_opcl				     *
 *===========================================================================*/
PUBLIC int tty_opcl(op, dev, proc_e, flags)
int op;				/* operation, DEV_OPEN or DEV_CLOSE */
dev_t dev;			/* device to open or close */
int proc_e;			/* process to open/close for */
int flags;			/* mode bits and flags */
{
/* This procedure is called from the dmap struct on tty open/close. */
 
  int r;
  register struct fproc *rfp;

  /* Add O_NOCTTY to the flags if this process is not a session leader, or
   * if it already has a controlling tty, or if it is someone elses
   * controlling tty.
   */
  if (!fp->fp_sesldr || fp->fp_tty != 0) {
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
PUBLIC int ctty_opcl(op, dev, proc_e, flags)
int op;				/* operation, DEV_OPEN or DEV_CLOSE */
dev_t dev;			/* device to open or close */
int proc_e;			/* process to open/close for */
int flags;			/* mode bits and flags */
{
/* This procedure is called from the dmap struct in table.c on opening/closing
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
/* Perform the FS side of the SETSID call, i.e. get rid of the controlling
 * terminal of a process, and make the process a session leader.
 */
  register struct fproc *rfp;
  int slot;

  /* Make the process a session leader with no controlling tty. */
  okendpt(proc_e, &slot);
  rfp = &fproc[slot];
  rfp->fp_sesldr = TRUE;
  rfp->fp_tty = 0;
}


/*===========================================================================*
 *				do_ioctl				     *
 *===========================================================================*/
PUBLIC int do_ioctl()
{
/* Perform the ioctl(ls_fd, request, argx) system call (uses m2 fmt). */

  int suspend_reopen;
  struct filp *f;
  register struct vnode *vp;
  dev_t dev;

  if ((f = get_filp(m_in.ls_fd)) == NIL_FILP) return(err_code);
  vp = f->filp_vno;		/* get vnode pointer */
  if ((vp->v_mode & I_TYPE) != I_CHAR_SPECIAL &&
      (vp->v_mode & I_TYPE) != I_BLOCK_SPECIAL) return(ENOTTY);
  suspend_reopen= (f->filp_state != FS_NORMAL);
  dev = (dev_t) vp->v_sdev;

  return dev_io(VFS_DEV_IOCTL, dev, who_e, m_in.ADDRESS, cvu64(0),
		m_in.REQUEST, f->filp_flags, suspend_reopen);
}


/*===========================================================================*
 *				gen_io					     *
 *===========================================================================*/
PUBLIC int gen_io(task_nr, mess_ptr)
int task_nr;			/* which task to call */
message *mess_ptr;		/* pointer to message for task */
{
/* All file system I/O ultimately comes down to I/O on major/minor device
 * pairs.  These lead to calls on the following routines via the dmap table.
 */

  int r, proc_e;

  if(task_nr == SYSTEM) printf("VFS: sending %d to SYSTEM\n", mess_ptr->m_type);

  proc_e = mess_ptr->IO_ENDPT;
  r = sendrec(task_nr, mess_ptr);
  if (r != OK) {
	if (r == EDEADSRCDST || r == EDSTDIED || r == ESRCDIED) {
		printf("fs: dead driver %d\n", task_nr);
		dmap_unmap_by_endpt(task_nr);
		return(r);
	}
	if (r == ELOCKED) {
		printf("fs: ELOCKED talking to %d\n", task_nr);
		return(r);
	}
	panic(__FILE__,"call_task: can't send/receive", r);
  }

  /* Did the process we did the sendrec() for get a result? */
  if (mess_ptr->REP_ENDPT != proc_e) {
	printf("fs: strange device reply from %d, type = %d, proc = %d "
	       "(not %d) (2) ignored\n", mess_ptr->m_source, mess_ptr->m_type,
	       proc_e, mess_ptr->REP_ENDPT);
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
 * pairs.  These lead to calls on the following routines via the dmap table.
 */

  int r, proc_e;

  proc_e = mess_ptr->IO_ENDPT;

  r = asynsend(task_nr, mess_ptr);
  if (r != OK) panic(__FILE__, "asyn_io: asynsend failed", r);

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
	dp = &dmap[(fp->fp_tty >> MAJOR) & BYTE];
	mess_ptr->DEVICE = (fp->fp_tty >> MINOR) & BYTE;

	if (dp->dmap_driver == NONE) {
		printf("FS: ctty_io: no driver for dev\n");
		return(EIO);
	}

	if(isokendpt(dp->dmap_driver, &dummyproc) != OK) {
		printf("FS: ctty_io: old driver %d\n", dp->dmap_driver);
		return(EIO);
	}

	(*dp->dmap_io)(dp->dmap_driver, mess_ptr);
  }
  return(OK);
}


/*===========================================================================*
 *				no_dev					     *
 *===========================================================================*/
PUBLIC int no_dev(op, dev, proc, flags)
int op;				/* operation, DEV_OPEN or DEV_CLOSE */
dev_t dev;			/* device to open or close */
int proc;			/* process to open/close for */
int flags;			/* mode bits and flags */
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
PUBLIC int clone_opcl(op, dev, proc_e, flags)
int op;				/* operation, DEV_OPEN or DEV_CLOSE */
dev_t dev;			/* device to open or close */
int proc_e;			/* process to open/close for */
int flags;			/* mode bits and flags */
{
/* Some devices need special processing upon open.  Such a device is "cloned",
 * i.e. on a succesful open it is replaced by a new device with a new unique
 * minor device number.  This new device number identifies a new object (such
 * as a new network connection) that has been allocated within a task.
 */
  struct dmap *dp;
  int r, minor;
  message dev_mess;

  /* Determine task dmap. */
  dp = &dmap[(dev >> MAJOR) & BYTE];
  minor = (dev >> MINOR) & BYTE;

  dev_mess.m_type   = op;
  dev_mess.DEVICE   = minor;
  dev_mess.IO_ENDPT = proc_e;
  dev_mess.COUNT    = flags;


  if (dp->dmap_driver == NONE) {
	printf("VFS clone_opcl: no driver for dev %x\n", dev);
	return(ENXIO);
  }

  if(isokendpt(dp->dmap_driver, &dummyproc) != OK) {
  	printf("VFS clone_opcl: bad driver endpoint for dev %x (%d)\n", dev,
	       dp->dmap_driver);
  	return(ENXIO);
  }

  /* Call the task. */
  r = (*dp->dmap_io)(dp->dmap_driver, &dev_mess);
  if (r != OK) return(r);

  if (op == DEV_OPEN && dev_mess.REP_STATUS >= 0) {
	if (dev_mess.REP_STATUS != minor) {
                struct vnode *vp;
                struct vmnt *vmp;
                struct node_details res;

		/* A new minor device number has been returned.
                 * Request the root FS to create a temporary device file to
		 * hold it. 
                 */
		
                /* Device number of the new device. */
		dev = (dev & ~(BYTE << MINOR)) | (dev_mess.REP_STATUS << MINOR);

                /* Issue request */
                r = req_newnode(ROOT_FS_E, fp->fp_effuid, fp->fp_effgid,
			ALL_MODES | I_CHAR_SPECIAL, dev, &res);
                if (r != OK) {
                    (void) clone_opcl(DEV_CLOSE, dev, proc_e, 0);
                    return r;
                }

                /* Drop old node and use the new values */
                vp = fp->fp_filp[m_in.fd]->filp_vno;
		
                put_vnode(vp);
		if ((vp = get_free_vnode()) == NIL_VNODE) 
			vp = fp->fp_filp[m_in.fd]->filp_vno;
		
                vp->v_fs_e = res.fs_e;
                if ((vmp = find_vmnt(vp->v_fs_e)) == NIL_VMNT) 
			printf("VFS clone_opcl: no vmnt found\n");

                vp->v_vmnt = vmp;
                vp->v_dev = vmp->m_dev;
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
  int r, new_driver_e, needs_reopen, fd_nr;
  struct filp *fp;
  struct vmnt *vmp;
  struct fproc *rfp;
  struct vnode *vp;
  message m;

  /* Open a device once for every filp that's opened on it,
   * and once for every filesystem mounted from it.
   */
  new_driver_e = dmap[maj].dmap_driver;

  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp) {
	int minor;
	if (vmp->m_dev == NO_DEV) continue;
	if ( ((vmp->m_dev >> MAJOR) & BYTE) != maj) continue;
	minor = ((vmp->m_dev >> MINOR) & BYTE);

	if ((r = dev_open(vmp->m_dev, FS_PROC_NR,
		vmp->m_flags ? R_BIT : (R_BIT|W_BIT))) != OK) {
		printf("VFS: mounted dev %d/%d re-open failed: %d.\n",
			maj, minor, r);
	}

	/* Send new driver endpoint */
	if (OK != req_newdriver(vmp->m_fs_e, vmp->m_dev, new_driver_e))
		printf("VFSdev_up: error sending new driver endpoint."
		       " FS_e: %d req_nr: %d\n", vmp->m_fs_e, REQ_NEW_DRIVER);
  }

  /* Look for processes that are suspened in an OPEN call. Set SUSP_REOPEN
   * to indicate that this process was suspended before the call to dev_up.
   */
  for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	if(rfp->fp_pid == PID_FREE) continue;
	if(rfp->fp_blocked_on != FP_BLOCKED_ON_DOPEN) continue;

	printf("dev_up: found process in FP_BLOCKED_ON_DOPEN, fd %d\n",
		rfp->fp_fd >> 8);
	fd_nr = (rfp->fp_fd >> 8);
	fp = rfp->fp_filp[fd_nr];
	vp = fp->filp_vno;
	if (!vp) panic(__FILE__, "restart_reopen: no vp", NO_NUM);
	if ((vp->v_mode &  I_TYPE) != I_CHAR_SPECIAL) continue;
	if (((vp->v_sdev >> MAJOR) & BYTE) != maj) continue;

	rfp->fp_flags |= SUSP_REOPEN;
  }

  needs_reopen= FALSE;
  for (fp = filp; fp < &filp[NR_FILPS]; fp++) {
	struct vnode *vp;
	int minor;

	if(fp->filp_count < 1 || !(vp = fp->filp_vno)) continue;
	if(((vp->v_sdev >> MAJOR) & BYTE) != maj) continue;
	if(!(vp->v_mode & (I_BLOCK_SPECIAL|I_CHAR_SPECIAL))) continue;

	fp->filp_state = FS_NEEDS_REOPEN;
	needs_reopen = TRUE;
  }

  if (needs_reopen)
	restart_reopen(maj);

}


/*===========================================================================*
 *				restart_reopen				     *
 *===========================================================================*/
PRIVATE void restart_reopen(maj)
int maj;
{
  int n, r, minor, fd_nr;
  endpoint_t driver_e;
  struct vnode *vp;
  struct filp *fp;
  struct fproc *rfp;

  for (fp = filp; fp < &filp[NR_FILPS]; fp++) {
	if (fp->filp_count < 1 || !(vp = fp->filp_vno)) continue;
	if (fp->filp_state != FS_NEEDS_REOPEN) continue;
	if (((vp->v_sdev >> MAJOR) & BYTE) != maj) continue;
	if ((vp->v_mode & I_TYPE) != I_CHAR_SPECIAL) continue;
	minor = ((vp->v_sdev >> MINOR) & BYTE);
	
	if (!(fp->filp_flags & O_REOPEN)) {
		/* File descriptor is to be closed when driver restarts. */
		n = invalidate(fp);
		if (n != fp->filp_count) {
			printf("VFS: warning: invalidate/count "
				"discrepancy (%d, %d)\n", n, fp->filp_count);
		}
		fp->filp_count = 0;
		continue;
	}

	r = dev_reopen(vp->v_sdev, fp-filp, vp->v_mode & (R_BIT|W_BIT));
	if (r == OK) return;

	/* Device could not be reopened. Invalidate all filps on that device.*/
	n = invalidate(fp);
	if (n != fp->filp_count) {
		printf("VFS: warning: invalidate/count "
			"discrepancy (%d, %d)\n", n, fp->filp_count);
	}
	fp->filp_count = 0;
	printf("VFS: file on dev %d/%d re-open failed: %d; "
		"invalidated %d fd's.\n", maj, minor, r, n);
  }

  /* Nothing more to re-open. Restart suspended processes */
  driver_e= dmap[maj].dmap_driver;

  for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	if(rfp->fp_pid == PID_FREE) continue;
	if(rfp->fp_blocked_on == FP_BLOCKED_ON_OTHER &&
	   rfp->fp_task == driver_e && (rfp->fp_flags & SUSP_REOPEN)) {
		rfp->fp_flags &= ~SUSP_REOPEN;
		rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;
		reply(rfp->fp_endpoint, ERESTART);
	}
  }

  /* Look for processes that are suspened in an OPEN call */
  for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	if (rfp->fp_pid == PID_FREE) continue;
	if (rfp->fp_blocked_on == FP_BLOCKED_ON_DOPEN ||
	    !(rfp->fp_flags & SUSP_REOPEN)) continue;

	printf("restart_reopen: found process in FP_BLOCKED_ON_DOPEN, fd %d\n",
		rfp->fp_fd >> 8);
	fd_nr = (rfp->fp_fd >> 8);
	fp = rfp->fp_filp[fd_nr];

	if (!fp) {
		/* Open failed, and automatic reopen was not requested */
		rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;
		FD_CLR(fd_nr, &rfp->fp_filp_inuse);
		reply(rfp->fp_endpoint, EIO);
		continue;
	}

	vp = fp->filp_vno;
	if (!vp) panic(__FILE__, "restart_reopen: no vp", NO_NUM);
	if ((vp->v_mode &  I_TYPE) != I_CHAR_SPECIAL) continue;
	if (((vp->v_sdev >> MAJOR) & BYTE) != maj) continue;

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
  struct filp *fp;
  struct vnode *vp;
  struct dmap *dp;

  driver_e = m_in.m_source;
  filp_no = m_in.REP_ENDPT;
  status = m_in.REP_STATUS;

  if (filp_no < 0 || filp_no >= NR_FILPS) {
	printf("reopen_reply: bad filp number %d from driver %d\n", filp_no,
	       driver_e);
	return;
  }

  fp = &filp[filp_no];
  if (fp->filp_count < 1) {
	printf("reopen_reply: filp number %d not inuse (from driver %d)\n",
	       filp_no, driver_e);
	return;
  }

  vp = fp->filp_vno;
  if (!vp) {
	printf("reopen_reply: no vnode for filp number %d (from driver %d)\n",
		filp_no, driver_e);
	return;
  }

  if (fp->filp_state != FS_NEEDS_REOPEN) {
	printf("reopen_reply: bad state %d for filp number %d" 
	       " (from driver %d)\n", fp->filp_state, filp_no, driver_e);
	return;
  }

  if ((vp->v_mode & I_TYPE) != I_CHAR_SPECIAL) {
	printf("reopen_reply: bad mode 0%o for filp number %d"
	       " (from driver %d)\n", vp->v_mode, filp_no, driver_e);
	return;
  }

  maj = ((vp->v_sdev >> MAJOR) & BYTE);
  dp = &dmap[maj];
  if (dp->dmap_driver != driver_e) {
	printf("reopen_reply: bad major %d for filp number %d "
		"(from driver %d, current driver is %d)\n", maj, filp_no,
		driver_e, dp->dmap_driver);
	return;
  }

  if (status == OK) {
	fp->filp_state= FS_NORMAL;
  } else {
	printf("reopen_reply: should handle error status\n");
	return;
  }

  restart_reopen(maj);
}

#if 0
#define ASYN_NR	100
PRIVATE asynmsg_t msgtable[ASYN_NR];
PRIVATE int first_slot= 0, next_slot= 0;

PUBLIC int asynsend(dst, mp)
endpoint_t dst;
message *mp;
{
	int r, src_ind, dst_ind;
	unsigned flags;

	/* Update first_slot */
	for (; first_slot < next_slot; first_slot++)
	{
		flags= msgtable[first_slot].flags;
		if ((flags & (AMF_VALID|AMF_DONE)) == (AMF_VALID|AMF_DONE))
		{
			if (msgtable[first_slot].result != OK)
			{
				printf(
			"asynsend: found completed entry %d with error %d\n",
					first_slot,
					msgtable[first_slot].result);
			}
			continue;
		}
		if (flags != AMF_EMPTY)
			break;
	}

	if (first_slot >= next_slot)
	{
		/* Reset first_slot and next_slot */
		next_slot= first_slot= 0;
	}

	if (next_slot >= ASYN_NR)
	{
		/* Tell the kernel to stop processing */
		r= senda(NULL, 0);
		if (r != OK)
			panic(__FILE__, "asynsend: senda failed", r);

		dst_ind= 0;
		for (src_ind= first_slot; src_ind<next_slot; src_ind++)
		{
			flags= msgtable[src_ind].flags;
			if ((flags & (AMF_VALID|AMF_DONE)) ==
				(AMF_VALID|AMF_DONE))
			{
				if (msgtable[src_ind].result != OK)
				{
					printf(
			"asynsend: found completed entry %d with error %d\n",
						src_ind,
						msgtable[src_ind].result);
				}
				continue;
			}
			if (flags == AMF_EMPTY)
				continue;
#if 0
			printf("asynsend: copying entry %d to %d\n",
				src_ind, dst_ind);
#endif
			if (src_ind != dst_ind)
				msgtable[dst_ind]= msgtable[src_ind];
			dst_ind++;
		}
		first_slot= 0;
		next_slot= dst_ind;
		if (next_slot >= ASYN_NR)
			panic(__FILE__, "asynsend: msgtable full", NO_NUM);
	}

	msgtable[next_slot].dst= dst;
	msgtable[next_slot].msg= *mp;
	msgtable[next_slot].flags= AMF_VALID;	/* Has to be last. The kernel 
					 	 * scans this table while we
						 * are sleeping.
					 	 */
	next_slot++;

	/* Tell the kernel to rescan the table */
	return senda(msgtable+first_slot, next_slot-first_slot);
}
#endif

