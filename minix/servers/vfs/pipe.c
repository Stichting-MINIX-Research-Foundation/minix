/* This file deals with the suspension and revival of processes.  A process can
 * be suspended because it wants to read or write from a pipe and can't, or
 * because it wants to read or write from a special file and can't.  When a
 * process can't continue it is suspended, and revived later when it is able
 * to continue.
 *
 * The entry points into this file are
 *   do_pipe2:	  perform the PIPE2 system call
 *   pipe_check:  check to see that a read or write on a pipe is feasible now
 *   suspend:	  suspend a process that cannot do a requested read or write
 *   release:	  check to see if a suspended process can be released and do
 *                it
 *   revive:	  mark a suspended process as able to run again
 *   unsuspend_by_endpt: revive all processes blocking on a given process
 *   do_unpause:  a signal has been sent to a process; see if it suspended
 */

#include "fs.h"
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <assert.h>
#include <minix/callnr.h>
#include <minix/endpoint.h>
#include <minix/com.h>
#include <minix/u64.h>
#include <sys/select.h>
#include <sys/time.h>
#include "file.h"
#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"

static int create_pipe(int fil_des[2], int flags);

/*===========================================================================*
 *				do_pipe2				     *
 *===========================================================================*/
int do_pipe2(void)
{
/* Perform the pipe2(fil_des[2], flags) system call. */
  int r, flags;
  int fil_des[2];		/* reply goes here */

  flags = job_m_in.m_lc_vfs_pipe2.flags;
  flags |= job_m_in.m_lc_vfs_pipe2.oflags;	/* backward compatibility */

  r = create_pipe(fil_des, flags);
  if (r == OK) {
	job_m_out.m_vfs_lc_fdpair.fd0 = fil_des[0];
	job_m_out.m_vfs_lc_fdpair.fd1 = fil_des[1];
  }

  return r;
}

/*===========================================================================*
 *				create_pipe				     *
 *===========================================================================*/
static int create_pipe(int fil_des[2], int flags)
{
  register struct fproc *rfp;
  int r;
  struct filp *fil_ptr0, *fil_ptr1;
  struct vnode *vp;
  struct vmnt *vmp;
  struct node_details res;

  /* Get a lock on PFS */
  if ((vmp = find_vmnt(PFS_PROC_NR)) == NULL) panic("PFS gone");
  if ((r = lock_vmnt(vmp, VMNT_READ)) != OK) return(r);

  /* See if a free vnode is available */
  if ((vp = get_free_vnode()) == NULL) {
	unlock_vmnt(vmp);
	return(err_code);
  }
  lock_vnode(vp, VNODE_OPCL);

  /* Acquire two file descriptors. */
  rfp = fp;
  if ((r = get_fd(fp, 0, R_BIT, &fil_des[0], &fil_ptr0)) != OK) {
	unlock_vnode(vp);
	unlock_vmnt(vmp);
	return(r);
  }
  rfp->fp_filp[fil_des[0]] = fil_ptr0;
  fil_ptr0->filp_count = 1;		/* mark filp in use */
  if ((r = get_fd(fp, 0, W_BIT, &fil_des[1], &fil_ptr1)) != OK) {
	rfp->fp_filp[fil_des[0]] = NULL;
	fil_ptr0->filp_count = 0;	/* mark filp free */
	unlock_filp(fil_ptr0);
	unlock_vnode(vp);
	unlock_vmnt(vmp);
	return(r);
  }
  rfp->fp_filp[fil_des[1]] = fil_ptr1;
  fil_ptr1->filp_count = 1;

  /* Create a named pipe inode on PipeFS */
  r = req_newnode(PFS_PROC_NR, fp->fp_effuid, fp->fp_effgid, I_NAMED_PIPE,
		  NO_DEV, &res);

  if (r != OK) {
	rfp->fp_filp[fil_des[0]] = NULL;
	fil_ptr0->filp_count = 0;
	rfp->fp_filp[fil_des[1]] = NULL;
	fil_ptr1->filp_count = 0;
	unlock_filp(fil_ptr1);
	unlock_filp(fil_ptr0);
	unlock_vnode(vp);
	unlock_vmnt(vmp);
	return(r);
  }

  /* Fill in vnode */
  vp->v_fs_e = res.fs_e;
  vp->v_mapfs_e = res.fs_e;
  vp->v_inode_nr = res.inode_nr;
  vp->v_mapinode_nr = res.inode_nr;
  vp->v_mode = res.fmode;
  vp->v_fs_count = 1;
  vp->v_mapfs_count = 1;
  vp->v_ref_count = 1;
  vp->v_size = 0;
  vp->v_vmnt = NULL;
  vp->v_dev = NO_DEV;

  /* Fill in filp objects */
  fil_ptr0->filp_vno = vp;
  dup_vnode(vp);
  fil_ptr1->filp_vno = vp;
  fil_ptr0->filp_flags = O_RDONLY | (flags & ~O_ACCMODE);
  fil_ptr1->filp_flags = O_WRONLY | (flags & ~O_ACCMODE);
  if (flags & O_CLOEXEC) {
	FD_SET(fil_des[0], &rfp->fp_cloexec_set);
	FD_SET(fil_des[1], &rfp->fp_cloexec_set);
  }

  unlock_filps(fil_ptr0, fil_ptr1);
  unlock_vmnt(vmp);

  return(OK);
}


/*===========================================================================*
 *				map_vnode				     *
 *===========================================================================*/
int
map_vnode(struct vnode *vp, endpoint_t map_to_fs_e)
{
  int r;
  struct vmnt *vmp;
  struct node_details res;

  if(vp->v_mapfs_e != NONE) return(OK);	/* Already mapped; nothing to do. */

  if ((vmp = find_vmnt(map_to_fs_e)) == NULL)
	panic("Can't map to unknown endpoint");
  if ((r = lock_vmnt(vmp, VMNT_WRITE)) != OK) {
	if (r == EBUSY)
		vmp = NULL;	/* Already locked, do not unlock */
	else
		return(r);

  }

  /* Create a temporary mapping of this inode to another FS. Read and write
   * operations on data will be handled by that FS. The rest by the 'original'
   * FS that holds the inode. */
  if ((r = req_newnode(map_to_fs_e, fp->fp_effuid, fp->fp_effgid, I_NAMED_PIPE,
		       vp->v_dev, &res)) == OK) {
	vp->v_mapfs_e = res.fs_e;
	vp->v_mapinode_nr = res.inode_nr;
	vp->v_mapfs_count = 1;
  }

  if (vmp) unlock_vmnt(vmp);

  return(r);
}

/*===========================================================================*
 *				pipe_check				     *
 *===========================================================================*/
int pipe_check(
struct filp *filp,	/* the filp of the pipe */
int rw_flag,		/* READING or WRITING */
int oflags,		/* flags set by open or fcntl */
int bytes,		/* bytes to be read or written (all chunks) */
int notouch		/* check only */
)
{
/* Pipes are a little different.  If a process reads from an empty pipe for
 * which a writer still exists, suspend the reader.  If the pipe is empty
 * and there is no writer, return 0 bytes.  If a process is writing to a
 * pipe and no one is reading from it, give a broken pipe error.
 */
  struct vnode *vp;
  off_t pos;
  int r = OK;

  vp = filp->filp_vno;

  /* Reads start at the beginning; writes append to pipes */
  if (notouch) /* In this case we don't actually care whether data transfer
		* would succeed. See POSIX 1003.1-2008 */
	pos = 0;
  else if (rw_flag == READING)
	pos = 0;
  else {
	pos = vp->v_size;
  }

  /* If reading, check for empty pipe. */
  if (rw_flag == READING) {
	if (vp->v_size == 0) {
		/* Process is reading from an empty pipe. */
		if (find_filp(vp, W_BIT) != NULL) {
			/* Writer exists */
			if (oflags & O_NONBLOCK)
				r = EAGAIN;
			else
				r = SUSPEND;

			/* If need be, activate sleeping writers. */
			/* We ignore notouch voluntary here. */
			if (susp_count > 0)
				release(vp, VFS_WRITE, susp_count);
		}
		return(r);
	}
	return(bytes);
  }

  /* Process is writing to a pipe. */
  if (find_filp(vp, R_BIT) == NULL) {
	return(EPIPE);
  }

  /* Calculate how many bytes can be written. */
  if (pos + bytes > PIPE_BUF) {
	if (oflags & O_NONBLOCK) {
		if (bytes <= PIPE_BUF) {
			/* Write has to be atomic */
			return(EAGAIN);
		}

		/* Compute available space */
		bytes = PIPE_BUF - pos;

		if (bytes > 0)  {
			/* Do a partial write. Need to wakeup reader */
			if (!notouch)
				release(vp, VFS_READ, susp_count);
			return(bytes);
		} else {
			/* Pipe is full */
			return(EAGAIN);
		}
	}

	if (bytes > PIPE_BUF) {
		/* Compute available space */
		bytes = PIPE_BUF - pos;

		if (bytes > 0) {
			/* Do a partial write. Need to wakeup reader
			 * since we'll suspend ourself in read_write()
			 */
			if (!notouch)
				release(vp, VFS_READ, susp_count);
			return(bytes);
		}
	}

	/* Pipe is full */
	return(SUSPEND);
  }

  /* Writing to an empty pipe.  Search for suspended reader. */
  if (pos == 0 && !notouch)
	release(vp, VFS_READ, susp_count);

  /* Requested amount fits */
  return(bytes);
}


/*===========================================================================*
 *				suspend					     *
 *===========================================================================*/
void suspend(int why)
{
/* Take measures to suspend the processing of the present system call.  The
 * caller must store the parameters to be used upon resuming in the process
 * table as appropriate.  The SUSPEND pseudo error should be returned after
 * calling suspend().
 */

  assert(fp->fp_blocked_on == FP_BLOCKED_ON_NONE);

  if (why == FP_BLOCKED_ON_POPEN || why == FP_BLOCKED_ON_PIPE)
	/* #procs susp'ed on pipe*/
	susp_count++;

  fp->fp_blocked_on = why;
}


/*===========================================================================*
 *				pipe_suspend				     *
 *===========================================================================*/
void pipe_suspend(int callnr, int fd, vir_bytes buf, size_t size,
	size_t cum_io)
{
/* Take measures to suspend the processing of the present system call.
 * Store the parameters to be used upon resuming in the process table.
 */

  fp->fp_pipe.callnr = callnr;
  fp->fp_pipe.fd = fd;
  fp->fp_pipe.buf = buf;
  fp->fp_pipe.nbytes = size;
  fp->fp_pipe.cum_io = cum_io;
  suspend(FP_BLOCKED_ON_PIPE);
}


/*===========================================================================*
 *				unsuspend_by_endpt			     *
 *===========================================================================*/
void unsuspend_by_endpt(endpoint_t proc_e)
{
/* Revive processes waiting for drivers (SUSPENDed) that have disappeared, with
 * return code EIO.
 */
  struct fproc *rp;
  struct smap *sp;

  for (rp = &fproc[0]; rp < &fproc[NR_PROCS]; rp++) {
	if (rp->fp_pid == PID_FREE) continue;
	if (rp->fp_blocked_on == FP_BLOCKED_ON_CDEV &&
	    rp->fp_cdev.endpt == proc_e)
		revive(rp->fp_endpoint, EIO);
	else if (rp->fp_blocked_on == FP_BLOCKED_ON_SDEV &&
	    (sp = get_smap_by_dev(rp->fp_sdev.dev, NULL)) != NULL &&
	    sp->smap_endpt == proc_e)
		sdev_stop(rp);
  }

  /* Revive processes waiting in drivers on select()s with EAGAIN too */
  select_unsuspend_by_endpt(proc_e);

  return;
}


/*===========================================================================*
 *				release					     *
 *===========================================================================*/
void release(struct vnode * vp, int op, int count)
{
/* Check to see if any process is hanging on pipe vnode 'vp'. If one is, and it
 * was trying to perform the call indicated by 'op' - one of VFS_OPEN,
 * VFS_READ, or VFS_WRITE - release it.  The 'count' parameter indicates the
 * maximum number of processes to release, which allows us to stop searching
 * early in some cases.
 */

  register struct fproc *rp;
  struct filp *f;
  int fd, selop;

  /* Trying to perform the call also includes SELECTing on it with that
   * operation.
   */
  if (op == VFS_READ || op == VFS_WRITE) {
	if (op == VFS_READ)
		selop = SEL_RD;
	else
		selop = SEL_WR;

	for (f = &filp[0]; f < &filp[NR_FILPS]; f++) {
		if (f->filp_count < 1 || !(f->filp_pipe_select_ops & selop) ||
		    f->filp_vno != vp)
			continue;

		select_callback(f, selop);

		f->filp_pipe_select_ops &= ~selop;
	}
  }

  /* Search the proc table. */
  for (rp = &fproc[0]; rp < &fproc[NR_PROCS] && count > 0; rp++) {
	/* Just to make sure:
	 * - FP_BLOCKED_ON_POPEN implies the original request was VFS_OPEN;
	 * - FP_BLOCKED_ON_PIPE may be the result of VFS_READ and VFS_WRITE,
	 *   and one of those two numbers is stored in fp_pipe.callnr.
	 */
	if (rp->fp_pid != PID_FREE && fp_is_blocked(rp) &&
	    !(rp->fp_flags & FP_REVIVED) &&
	    ((op == VFS_OPEN && rp->fp_blocked_on == FP_BLOCKED_ON_POPEN) ||
	     (op != VFS_OPEN && rp->fp_blocked_on == FP_BLOCKED_ON_PIPE &&
	      op == rp->fp_pipe.callnr))) {
		/* Find the vnode. Depending on the reason the process was
		 * suspended, there are different ways of finding it.
		 */
		if (rp->fp_blocked_on == FP_BLOCKED_ON_POPEN)
			fd = rp->fp_popen.fd;
		else
			fd = rp->fp_pipe.fd;
		f = rp->fp_filp[fd];
		if (f == NULL || f->filp_mode == FILP_CLOSED)
			continue;
		if (f->filp_vno != vp)
			continue;

		/* We found the vnode. Revive process. */
		revive(rp->fp_endpoint, 0);
		susp_count--;	/* keep track of who is suspended */
		if(susp_count < 0)
			panic("susp_count now negative: %d", susp_count);
		if (--count == 0) return;
	}
  }
}


/*===========================================================================*
 *				revive					     *
 *===========================================================================*/
void revive(endpoint_t proc_e, int returned)
{
/* Revive a previously blocked process. When a process hangs on tty, this
 * is the way it is eventually released. For processes blocked on _SELECT,
 * _CDEV, or _SDEV, this function MUST NOT block its calling thread.
 */
  struct fproc *rfp;
  int blocked_on;
  int slot;

  if (proc_e == NONE || isokendpt(proc_e, &slot) != OK) return;

  rfp = &fproc[slot];
  if (!fp_is_blocked(rfp) || (rfp->fp_flags & FP_REVIVED)) return;

  /* The 'reviving' flag applies to pipe I/O and file locks.  Processes waiting
   * on those suspension types need more processing, and will be unblocked from
   * the main loop later.  Processes suspended for other reasons get a reply
   * right away, and as such, have their suspension cleared right here as well.
   */
  blocked_on = rfp->fp_blocked_on;
  if (blocked_on == FP_BLOCKED_ON_PIPE || blocked_on == FP_BLOCKED_ON_FLOCK) {
	/* Revive a process suspended on a pipe or lock. */
	rfp->fp_flags |= FP_REVIVED;
	reviving++;		/* process was waiting on pipe or lock */
  } else {
	rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;
	switch (blocked_on) {
	case FP_BLOCKED_ON_POPEN:
		/* process blocked in open or create */
		replycode(proc_e, rfp->fp_popen.fd);
		break;
	case FP_BLOCKED_ON_SELECT:
		replycode(proc_e, returned);
		break;
	case FP_BLOCKED_ON_CDEV:
		/* If a grant has been issued by FS for this I/O, revoke
		 * it again now that I/O is done.
		 */
		if (GRANT_VALID(rfp->fp_cdev.grant)) {
			if (cpf_revoke(rfp->fp_cdev.grant) == -1) {
				panic("VFS: revoke failed for grant: %d",
				    rfp->fp_cdev.grant);
			}
		}
		replycode(proc_e, returned);/* unblock the process */
		break;
	case FP_BLOCKED_ON_SDEV:
		/*
		 * Cleaning up socket requests is too complex to put here, and
		 * neither sdev_reply() nor sdev_stop() call revive().
		 */
		panic("revive should not be used for socket calls");
	default:
		panic("unknown block state %d", blocked_on);
	}
  }
}


/*===========================================================================*
 *				unpause					     *
 *===========================================================================*/
void unpause(void)
{
/* A signal has been sent to a user who is paused on the file system.
 * Abort the system call with the EINTR error message.
 */
  int blocked_on, status = EINTR;
  int wasreviving = 0;

  if (!fp_is_blocked(fp)) return;
  blocked_on = fp->fp_blocked_on;

  /* Clear the block status now. The procedure below might make blocking calls
   * and it is imperative that while at least cdev_cancel() or sdev_cancel()
   * are executing, other parts of VFS do not perceive this process as blocked
   * on something.
   */
  fp->fp_blocked_on = FP_BLOCKED_ON_NONE;

  if (fp->fp_flags & FP_REVIVED) {
	fp->fp_flags &= ~FP_REVIVED;
	reviving--;
	wasreviving = 1;
  }

  switch (blocked_on) {
	case FP_BLOCKED_ON_PIPE:/* process trying to read or write a pipe */
		/* If the operation succeeded partially, return the bytes
		 * processed so far.  Otherwise, return EINTR as usual.
		 */
		if (fp->fp_pipe.cum_io > 0)
			status = fp->fp_pipe.cum_io;
		break;

	case FP_BLOCKED_ON_FLOCK:/* process trying to set a lock with FCNTL */
		break;

	case FP_BLOCKED_ON_SELECT:/* process blocking on select() */
		select_forget();
		break;

	case FP_BLOCKED_ON_POPEN:	/* process trying to open a fifo */
		break;

	case FP_BLOCKED_ON_CDEV: /* process blocked on character device I/O */
		status = cdev_cancel(fp->fp_cdev.dev, fp->fp_cdev.endpt,
		    fp->fp_cdev.grant);

		break;

	case FP_BLOCKED_ON_SDEV:	/* process blocked on socket I/O */
		sdev_cancel();
		return;			/* sdev_cancel() sends its own reply */

	default :
		panic("VFS: unknown block reason: %d", blocked_on);
  }

  if ((blocked_on == FP_BLOCKED_ON_PIPE || blocked_on == FP_BLOCKED_ON_POPEN)&&
	!wasreviving) {
	susp_count--;
  }

  replycode(fp->fp_endpoint, status);	/* signal interrupted call */
}
