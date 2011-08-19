/* This file deals with the suspension and revival of processes.  A process can
 * be suspended because it wants to read or write from a pipe and can't, or
 * because it wants to read or write from a special file and can't.  When a
 * process can't continue it is suspended, and revived later when it is able
 * to continue.
 *
 * The entry points into this file are
 *   do_pipe:	  perform the PIPE system call
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
#include <assert.h>
#include <minix/callnr.h>
#include <minix/endpoint.h>
#include <minix/com.h>
#include <minix/u64.h>
#include <sys/select.h>
#include <sys/time.h>
#include "file.h"
#include "fproc.h"
#include "dmap.h"
#include "param.h"
#include "select.h"
#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"


/*===========================================================================*
 *				do_pipe					     *
 *===========================================================================*/
PUBLIC int do_pipe()
{
/* Perform the pipe(fil_des) system call. */

  register struct fproc *rfp;
  int r;
  struct filp *fil_ptr0, *fil_ptr1;
  int fil_des[2];		/* reply goes here */
  struct vnode *vp;
  struct vmnt *vmp;
  struct node_details res;

  /* Get a lock on PFS */
  if ((vmp = find_vmnt(PFS_PROC_NR)) == NULL) panic("PFS gone");
  if ((r = lock_vmnt(vmp, VMNT_WRITE)) != OK) return(r);

  /* See if a free vnode is available */
  if ((vp = get_free_vnode()) == NULL) return(err_code);
  lock_vnode(vp, VNODE_OPCL);

  /* Acquire two file descriptors. */
  rfp = fp;
  if ((r = get_fd(0, R_BIT, &fil_des[0], &fil_ptr0)) != OK) {
	unlock_vnode(vp);
	unlock_vmnt(vmp);
	return(r);
  }
  rfp->fp_filp[fil_des[0]] = fil_ptr0;
  FD_SET(fil_des[0], &rfp->fp_filp_inuse);
  fil_ptr0->filp_count = 1;		/* mark filp in use */
  if ((r = get_fd(0, W_BIT, &fil_des[1], &fil_ptr1)) != OK) {
	rfp->fp_filp[fil_des[0]] = NULL;
	FD_CLR(fil_des[0], &rfp->fp_filp_inuse);
	fil_ptr0->filp_count = 0;	/* mark filp free */
	unlock_filp(fil_ptr0);
	unlock_vnode(vp);
	unlock_vmnt(vmp);
	return(r);
  }
  rfp->fp_filp[fil_des[1]] = fil_ptr1;
  FD_SET(fil_des[1], &rfp->fp_filp_inuse);
  fil_ptr1->filp_count = 1;

  /* Create a named pipe inode on PipeFS */
  r = req_newnode(PFS_PROC_NR, fp->fp_effuid, fp->fp_effgid, I_NAMED_PIPE,
		  NO_DEV, &res);

  if (r != OK) {
	rfp->fp_filp[fil_des[0]] = NULL;
	FD_CLR(fil_des[0], &rfp->fp_filp_inuse);
	fil_ptr0->filp_count = 0;
	rfp->fp_filp[fil_des[1]] = NULL;
	FD_CLR(fil_des[1], &rfp->fp_filp_inuse);
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
  vp->v_pipe = I_PIPE;
  vp->v_pipe_rd_pos= 0;
  vp->v_pipe_wr_pos= 0;
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
  fil_ptr0->filp_flags = O_RDONLY;
  fil_ptr1->filp_flags = O_WRONLY;

  m_out.reply_i1 = fil_des[0];
  m_out.reply_i2 = fil_des[1];

  unlock_filps(fil_ptr0, fil_ptr1);
  unlock_vmnt(vmp);

  return(OK);
}


/*===========================================================================*
 *				map_vnode				     *
 *===========================================================================*/
PUBLIC int map_vnode(vp, map_to_fs_e)
struct vnode *vp;
endpoint_t map_to_fs_e;
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
PUBLIC int pipe_check(vp, rw_flag, oflags, bytes, position, notouch)
register struct vnode *vp;	/* the inode of the pipe */
int rw_flag;			/* READING or WRITING */
int oflags;			/* flags set by open or fcntl */
register int bytes;		/* bytes to be read or written (all chunks) */
u64_t position;			/* current file position */
int notouch;			/* check only */
{
/* Pipes are a little different.  If a process reads from an empty pipe for
 * which a writer still exists, suspend the reader.  If the pipe is empty
 * and there is no writer, return 0 bytes.  If a process is writing to a
 * pipe and no one is reading from it, give a broken pipe error.
 */
  off_t pos;
  int r = OK;

  if (ex64hi(position) != 0)
	panic("pipe_check: position too large in pipe");
  pos = ex64lo(position);

  /* If reading, check for empty pipe. */
  if (rw_flag == READING) {
	if (pos >= vp->v_size) {
		/* Process is reading from an empty pipe. */
		if (find_filp(vp, W_BIT) != NULL) {
			/* Writer exists */
			if (oflags & O_NONBLOCK)
				r = EAGAIN;
			else
				r = SUSPEND;

			/* If need be, activate sleeping writers. */
			if (susp_count > 0)
				release(vp, WRITE, susp_count);
		}
		return(r);
	}
	return(bytes);
  }

  /* Process is writing to a pipe. */
  if (find_filp(vp, R_BIT) == NULL) {
	/* Process is writing, but there is no reader. Tell kernel to generate
	 * a SIGPIPE signal. */
	if (!notouch) sys_kill(fp->fp_endpoint, SIGPIPE);

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
				release(vp, READ, susp_count);
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
				release(vp, READ, susp_count);
			return(bytes);
		}
	}

	/* Pipe is full */
	return(SUSPEND);
  }

  /* Writing to an empty pipe.  Search for suspended reader. */
  if (pos == 0 && !notouch)
	release(vp, READ, susp_count);

  /* Requested amount fits */
  return(bytes);
}


/*===========================================================================*
 *				suspend					     *
 *===========================================================================*/
PUBLIC void suspend(int why)
{
/* Take measures to suspend the processing of the present system call.
 * Store the parameters to be used upon resuming in the process table.
 * (Actually they are not used when a process is waiting for an I/O device,
 * but they are needed for pipes, and it is not worth making the distinction.)
 * The SUSPEND pseudo error should be returned after calling suspend().
 */

#if DO_SANITYCHECKS
  if (why == FP_BLOCKED_ON_PIPE)
	panic("suspend: called for FP_BLOCKED_ON_PIPE");

  if(fp_is_blocked(fp))
	panic("suspend: called for suspended process");

  if(why == FP_BLOCKED_ON_NONE)
	panic("suspend: called for FP_BLOCKED_ON_NONE");
#endif

  if (why == FP_BLOCKED_ON_POPEN)
	  /* #procs susp'ed on pipe*/
	  susp_count++;

  fp->fp_blocked_on = why;
  assert(fp->fp_grant == GRANT_INVALID || !GRANT_VALID(fp->fp_grant));
  fp->fp_block_fd = m_in.fd;
  fp->fp_block_callnr = call_nr;
  fp->fp_flags &= ~FP_SUSP_REOPEN;		/* Clear this flag. The caller
						 * can set it when needed.
						 */
  if (why == FP_BLOCKED_ON_LOCK) {
	fp->fp_buffer = (char *) m_in.name1;	/* third arg to fcntl() */
	fp->fp_nbytes = m_in.request;		/* second arg to fcntl() */
  } else {
	fp->fp_buffer = m_in.buffer;		/* for reads and writes */
	fp->fp_nbytes = m_in.nbytes;
  }
}

/*===========================================================================*
 *				wait_for				     *
 *===========================================================================*/
PUBLIC void wait_for(endpoint_t who)
{
  if(who == NONE || who == ANY)
	panic("suspend on NONE or ANY");
  suspend(FP_BLOCKED_ON_OTHER);
  fp->fp_task = who;
}


/*===========================================================================*
 *				pipe_suspend					     *
 *===========================================================================*/
PUBLIC void pipe_suspend(rw_flag, fd_nr, buf, size)
int rw_flag;
int fd_nr;
char *buf;
size_t size;
{
/* Take measures to suspend the processing of the present system call.
 * Store the parameters to be used upon resuming in the process table.
 * (Actually they are not used when a process is waiting for an I/O device,
 * but they are needed for pipes, and it is not worth making the distinction.)
 * The SUSPEND pseudo error should be returned after calling suspend().
 */
#if DO_SANITYCHECKS
  if(fp_is_blocked(fp))
	panic("pipe_suspend: called for suspended process");
#endif

  susp_count++;					/* #procs susp'ed on pipe*/
  fp->fp_blocked_on = FP_BLOCKED_ON_PIPE;
  assert(!GRANT_VALID(fp->fp_grant));
  fp->fp_block_fd = fd_nr;
  fp->fp_block_callnr = ((rw_flag == READING) ? READ : WRITE);
  fp->fp_buffer = buf;
  fp->fp_nbytes = size;
}


/*===========================================================================*
 *				unsuspend_by_endpt			     *
 *===========================================================================*/
PUBLIC void unsuspend_by_endpt(endpoint_t proc_e)
{
/* Revive processes waiting for drivers (SUSPENDed) that have disappeared with
 * return code EAGAIN.
 */
  struct fproc *rp;

  for (rp = &fproc[0]; rp < &fproc[NR_PROCS]; rp++) {
	if (rp->fp_pid == PID_FREE) continue;
	if (rp->fp_blocked_on == FP_BLOCKED_ON_OTHER && rp->fp_task == proc_e)
		revive(rp->fp_endpoint, EAGAIN);
  }

  /* Revive processes waiting in drivers on select()s with EAGAIN too */
  select_unsuspend_by_endpt(proc_e);

  return;
}


/*===========================================================================*
 *				release					     *
 *===========================================================================*/
PUBLIC void release(vp, op, count)
register struct vnode *vp;	/* inode of pipe */
int op;				/* READ, WRITE, OPEN or CREAT */
int count;			/* max number of processes to release */
{
/* Check to see if any process is hanging on the pipe whose inode is in 'ip'.
 * If one is, and it was trying to perform the call indicated by 'call_nr',
 * release it.
 */

  register struct fproc *rp;
  struct filp *f;
  int selop;

  /* Trying to perform the call also includes SELECTing on it with that
   * operation.
   */
  if (op == READ || op == WRITE) {
	if (op == READ)
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
	if (rp->fp_pid != PID_FREE && fp_is_blocked(rp) &&
	    !(rp->fp_flags & FP_REVIVED) && rp->fp_block_callnr == op &&
	    rp->fp_filp[rp->fp_block_fd] != NULL &&
	    rp->fp_filp[rp->fp_block_fd]->filp_vno == vp) {
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
PUBLIC void revive(proc_nr_e, returned)
int proc_nr_e;			/* process to revive */
int returned;			/* if hanging on task, how many bytes read */
{
/* Revive a previously blocked process. When a process hangs on tty, this
 * is the way it is eventually released.
 */
  register struct fproc *rfp;
  int blocked_on;
  int fd_nr, slot;
  struct filp *fil_ptr;

  if (proc_nr_e == NONE || isokendpt(proc_nr_e, &slot) != OK) return;

  rfp = &fproc[slot];
  if (!fp_is_blocked(rfp) || (rfp->fp_flags & FP_REVIVED)) return;

  /* The 'reviving' flag only applies to pipes.  Processes waiting for TTY get
   * a message right away.  The revival process is different for TTY and pipes.
   * For select and TTY revival, the work is already done, for pipes it is not:
   * the proc must be restarted so it can try again.
   */
  blocked_on = rfp->fp_blocked_on;
  if (blocked_on == FP_BLOCKED_ON_PIPE || blocked_on == FP_BLOCKED_ON_LOCK) {
	/* Revive a process suspended on a pipe or lock. */
	rfp->fp_flags |= FP_REVIVED;
	reviving++;		/* process was waiting on pipe or lock */
  } else if (blocked_on == FP_BLOCKED_ON_DOPEN) {
	rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;
	fd_nr = rfp->fp_block_fd;
	if (returned < 0) {
		fil_ptr = rfp->fp_filp[fd_nr];
		lock_filp(fil_ptr, VNODE_OPCL);
		rfp->fp_filp[fd_nr] = NULL;
		FD_CLR(fd_nr, &rfp->fp_filp_inuse);
		if (fil_ptr->filp_count != 1) {
			panic("VFS: revive: bad count in filp: %d",
				fil_ptr->filp_count);
		}
		fil_ptr->filp_count = 0;
		unlock_filp(fil_ptr);
		put_vnode(fil_ptr->filp_vno);
		fil_ptr->filp_vno = NULL;
		reply(proc_nr_e, returned);
	} else {
		reply(proc_nr_e, fd_nr);
	}
  } else {
	rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;
	if (blocked_on == FP_BLOCKED_ON_POPEN) {
		/* process blocked in open or create */
		reply(proc_nr_e, rfp->fp_block_fd);
	} else if (blocked_on == FP_BLOCKED_ON_SELECT) {
		reply(proc_nr_e, returned);
	} else {
		/* Revive a process suspended on TTY or other device.
		 * Pretend it wants only what there is.
		 */
		rfp->fp_nbytes = returned;
		/* If a grant has been issued by FS for this I/O, revoke
		 * it again now that I/O is done.
		 */
		if (GRANT_VALID(rfp->fp_grant)) {
			if(cpf_revoke(rfp->fp_grant)) {
				panic("VFS: revoke failed for grant: %d",
					rfp->fp_grant);
			}
			rfp->fp_grant = GRANT_INVALID;
		}
		reply(proc_nr_e, returned);	/* unblock the process */
	}
  }
}


/*===========================================================================*
 *				unpause					     *
 *===========================================================================*/
PUBLIC void unpause(proc_nr_e)
int proc_nr_e;
{
/* A signal has been sent to a user who is paused on the file system.
 * Abort the system call with the EINTR error message.
 */

  register struct fproc *rfp, *org_fp;
  int slot, blocked_on, fild, status = EINTR, major_dev, minor_dev;
  struct filp *f;
  dev_t dev;
  message mess;
  int wasreviving = 0;

  if (isokendpt(proc_nr_e, &slot) != OK) {
	printf("VFS: ignoring unpause for bogus endpoint %d\n", proc_nr_e);
	return;
  }

  rfp = &fproc[slot];
  if (!fp_is_blocked(rfp)) return;
  blocked_on = rfp->fp_blocked_on;

  if (rfp->fp_flags & FP_REVIVED) {
	rfp->fp_flags &= ~FP_REVIVED;
	reviving--;
	wasreviving = 1;
  }

  switch (blocked_on) {
	case FP_BLOCKED_ON_PIPE:/* process trying to read or write a pipe */
		break;

	case FP_BLOCKED_ON_LOCK:/* process trying to set a lock with FCNTL */
		break;

	case FP_BLOCKED_ON_SELECT:/* process blocking on select() */
		select_forget(proc_nr_e);
		break;

	case FP_BLOCKED_ON_POPEN:	/* process trying to open a fifo */
		break;

	case FP_BLOCKED_ON_DOPEN:/* process trying to open a device */
		/* Don't cancel OPEN. Just wait until the open completes. */
		return;

	case FP_BLOCKED_ON_OTHER:/* process trying to do device I/O (e.g. tty)*/
		if (rfp->fp_flags & FP_SUSP_REOPEN) {
			/* Process is suspended while waiting for a reopen.
			 * Just reply EINTR.
			 */
			rfp->fp_flags &= ~FP_SUSP_REOPEN;
			status = EINTR;
			break;
		}

		fild = rfp->fp_block_fd;
		if (fild < 0 || fild >= OPEN_MAX)
			panic("file descriptor out-of-range");
		f = rfp->fp_filp[fild];
		dev = (dev_t) f->filp_vno->v_sdev;	/* device hung on */
		major_dev = major(dev);
		minor_dev = minor(dev);
		mess.TTY_LINE = minor_dev;
		mess.USER_ENDPT = rfp->fp_ioproc;
		mess.IO_GRANT = (char *) rfp->fp_grant;

		/* Tell kernel R or W. Mode is from current call, not open. */
		mess.COUNT = rfp->fp_block_callnr == READ ? R_BIT : W_BIT;
		mess.m_type = CANCEL;

		org_fp = fp;
		fp = rfp;	/* hack - ctty_io uses fp */
		(*dmap[major_dev].dmap_io)(rfp->fp_task, &mess);
		fp = org_fp;
		status = mess.REP_STATUS;
		if (status == SUSPEND)
			return;		/* Process will be revived at a
					 * later time.
					 */

		if (status == EAGAIN) status = EINTR;
		if (GRANT_VALID(rfp->fp_grant)) {
			(void) cpf_revoke(rfp->fp_grant);
			rfp->fp_grant = GRANT_INVALID;
		}
		break;
	default :
		panic("VFS: unknown block reason: %d", blocked_on);
  }

  rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;

  if ((blocked_on == FP_BLOCKED_ON_PIPE || blocked_on == FP_BLOCKED_ON_POPEN)&&
	!wasreviving) {
	susp_count--;
  }

  reply(proc_nr_e, status);	/* signal interrupted call */
}

#if DO_SANITYCHECKS
/*===========================================================================*
 *				check_pipe			     *
 *===========================================================================*/
PUBLIC int check_pipe(void)
{
/* Integrity check; verify that susp_count equals what the fproc table thinks
 * is suspended on a pipe */
  struct fproc *rfp;
  int count = 0;
  for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	if (rfp->fp_pid == PID_FREE) continue;
	if ( !(rfp->fp_flags & FP_REVIVED) &&
	    (rfp->fp_blocked_on == FP_BLOCKED_ON_PIPE ||
	     rfp->fp_blocked_on == FP_BLOCKED_ON_POPEN)) {
		count++;
	}
  }

  if (count != susp_count) {
	printf("check_pipe: count %d susp_count %d\n", count, susp_count);
	return(0);
  }

  return(l);
}
#endif
