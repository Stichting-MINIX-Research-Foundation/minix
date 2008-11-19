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
 *
 * Changes for VFS:
 *   Jul 2006 (Balazs Gerofi)
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

  /* See if a free vnode is available */
  if ( (vp = get_free_vnode(__FILE__, __LINE__)) == NIL_VNODE) {
      printf("VFS: no vnode available!\n");
      return err_code;
  }

  /* Acquire two file descriptors. */
  rfp = fp;
  if ( (r = get_fd(0, R_BIT, &fil_des[0], &fil_ptr0)) != OK) return(r);
  rfp->fp_filp[fil_des[0]] = fil_ptr0;
  FD_SET(fil_des[0], &rfp->fp_filp_inuse);
  fil_ptr0->filp_count = 1;
  if ( (r = get_fd(0, W_BIT, &fil_des[1], &fil_ptr1)) != OK) {
      rfp->fp_filp[fil_des[0]] = NIL_FILP;
      FD_CLR(fil_des[0], &rfp->fp_filp_inuse);
      fil_ptr0->filp_count = 0;
      return(r);
  }
  rfp->fp_filp[fil_des[1]] = fil_ptr1;
  FD_SET(fil_des[1], &rfp->fp_filp_inuse);
  fil_ptr1->filp_count = 1;

  /* Send request */
  r = req_newnode(ROOT_FS_E, fp->fp_effuid, fp->fp_effgid, I_NAMED_PIPE,
	(dev_t)0, &res);

  /* Handle error */
  if (r != OK) {
      rfp->fp_filp[fil_des[0]] = NIL_FILP;
      FD_CLR(fil_des[0], &rfp->fp_filp_inuse);
      fil_ptr0->filp_count = 0;
      rfp->fp_filp[fil_des[1]] = NIL_FILP;
      FD_CLR(fil_des[1], &rfp->fp_filp_inuse);
      fil_ptr1->filp_count = 0;
      return r;
  }

  /* Fill in vnode */
  vp->v_fs_e = res.fs_e;
  vp->v_inode_nr = res.inode_nr; 
  vp->v_mode = res.fmode;
  vp->v_index = res.inode_index;
  vp->v_pipe = I_PIPE;
  vp->v_pipe_rd_pos= 0;
  vp->v_pipe_wr_pos= 0;
  vp->v_fs_count = 1;
  vp->v_ref_count = 1;
  vp->v_size = 0;

  if ( (vmp = find_vmnt(vp->v_fs_e)) == NIL_VMNT) {
      printf("VFS: vmnt not found by pipe() ==>> USING ROOT VMNT\n");
      vp->v_vmnt = &vmnt[0];
  }
  else {
      vp->v_vmnt = vmp; 
      vp->v_dev = vmp->m_dev;
  }

  /* Fill in filp objects */
  fil_ptr0->filp_vno = vp;
  dup_vnode(vp);
  fil_ptr1->filp_vno = vp;
  fil_ptr0->filp_flags = O_RDONLY;
  fil_ptr1->filp_flags = O_WRONLY;

  m_out.reply_i1 = fil_des[0];
  m_out.reply_i2 = fil_des[1];

  return(OK);
}

/*===========================================================================*
 *				pipe_check				     *
 *===========================================================================*/
PUBLIC int Xpipe_check(vp, rw_flag, oflags, bytes, position, notouch)
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

  if (ex64hi(position) != 0)
	panic(__FILE__, "pipe_check: position too large in pipe", NO_NUM);
  pos= ex64lo(position);

  /* If reading, check for empty pipe. */
  if (rw_flag == READING) {
	if (pos >= vp->v_size) {
		/* Process is reading from an empty pipe. */
		int r = 0;
		if (find_filp(vp, W_BIT) != NIL_FILP) {
			/* Writer exists */
			if (oflags & O_NONBLOCK) {
				r = EAGAIN;
			} else {
				r = SUSPEND;
			}
			/* If need be, activate sleeping writers. */
			if (susp_count > 0)
				release(vp, WRITE, susp_count);
		}
		return(r);
	}

	return bytes;
  }

  /* Process is writing to a pipe. */
  if (find_filp(vp, R_BIT) == NIL_FILP) {
	/* Tell kernel to generate a SIGPIPE signal. */
	if (!notouch) {
		sys_kill(fp->fp_endpoint, SIGPIPE);
	}
	return(EPIPE);
  }

  if (pos + bytes > PIPE_BUF) {
	if (oflags & O_NONBLOCK)
	{
		if (bytes <= PIPE_BUF) {
			/* Write has to be atomic */
			return(EAGAIN);
		}

		/* Compute available space */
		bytes= PIPE_BUF-pos;

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
		bytes= PIPE_BUF-pos;

		if (bytes > 0) {
			/* Do a partial write. Need to wakeup reader
			 * since we'll suspend ourself in read_write()
			 */
			if (!notouch)
				release(vp, READ, susp_count);
			return(bytes);
		}
	}

	/* Pipe is full, or we need an atomic write */
	return(SUSPEND);
  }

  /* Writing to an empty pipe.  Search for suspended reader. */
  if (pos == 0 && !notouch)
	release(vp, READ, susp_count);

  /* Requested amount fits */
  return bytes;
}

/*===========================================================================*
 *				suspend					     *
 *===========================================================================*/
PUBLIC void suspend(task)
int task;			/* who is proc waiting for? (PIPE = pipe) */
{
/* Take measures to suspend the processing of the present system call.
 * Store the parameters to be used upon resuming in the process table.
 * (Actually they are not used when a process is waiting for an I/O device,
 * but they are needed for pipes, and it is not worth making the distinction.)
 * The SUSPEND pseudo error should be returned after calling suspend().
 */

  if (task == XPIPE)
	panic(__FILE__, "suspend: called for XPIPE", NO_NUM);

  if (task == XPOPEN) susp_count++;/* #procs susp'ed on pipe*/
  fp->fp_suspended = SUSPENDED;
  assert(fp->fp_grant == GRANT_INVALID || !GRANT_VALID(fp->fp_grant));
  fp->fp_fd = m_in.fd << 8 | call_nr;
  if(task == NONE)
	panic(__FILE__,"suspend on NONE",NO_NUM);
  fp->fp_task = -task;
  fp->fp_flags &= ~SUSP_REOPEN;			/* Clear this flag. The caller
						 * can set it when needed.
						 */
  if (task == XLOCK) {
	fp->fp_buffer = (char *) m_in.name1;	/* third arg to fcntl() */
	fp->fp_nbytes = m_in.request;		/* second arg to fcntl() */
  } else {
	fp->fp_buffer = m_in.buffer;		/* for reads and writes */
	fp->fp_nbytes = m_in.nbytes;
  }
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

  susp_count++;					/* #procs susp'ed on pipe*/
  fp->fp_suspended = SUSPENDED;
  assert(!GRANT_VALID(fp->fp_grant));
  fp->fp_fd = (fd_nr << 8) | ((rw_flag == READING) ? READ : WRITE);
  fp->fp_task = -XPIPE;
  fp->fp_buffer = buf;		
  fp->fp_nbytes = size;
}

/*===========================================================================*
 *				unsuspend_by_endpt			     *
 *===========================================================================*/
PUBLIC void unsuspend_by_endpt(int proc_e)
{
  struct fproc *rp;
  int client = 0;

  /* Revive processes waiting for drivers (SUSPENDed) that have
   * disappeared with return code EAGAIN.
   */
  for (rp = &fproc[0]; rp < &fproc[NR_PROCS]; rp++, client++)
	if(rp->fp_pid != PID_FREE &&
	   rp->fp_suspended == SUSPENDED && rp->fp_task == -proc_e) {
		revive(rp->fp_endpoint, EAGAIN);
	}

  /* Revive processes waiting in drivers on select()s
   * with EAGAIN too.
   */
  select_unsuspend_by_endpt(proc_e);

  return;
}


/*===========================================================================*
 *				release					     *
 *===========================================================================*/
PUBLIC void release(vp, call_nr, count)
register struct vnode *vp;	/* inode of pipe */
int call_nr;			/* READ, WRITE, OPEN or CREAT */
int count;			/* max number of processes to release */
{
/* Check to see if any process is hanging on the pipe whose inode is in 'ip'.
 * If one is, and it was trying to perform the call indicated by 'call_nr',
 * release it.
 */

  register struct fproc *rp;
  struct filp *f;

#if 0
  printf("vfs:release: vp 0x%x, call %d, count %d\n", vp, call_nr, count);
#endif

  /* Trying to perform the call also includes SELECTing on it with that
   * operation.
   */
  if (call_nr == READ || call_nr == WRITE) {
  	  int op;
  	  if (call_nr == READ)
  	  	op = SEL_RD;
  	  else
  	  	op = SEL_WR;
	  for(f = &filp[0]; f < &filp[NR_FILPS]; f++) {
  		if (f->filp_count < 1 || !(f->filp_pipe_select_ops & op) ||
  		   f->filp_vno != vp)
  			continue;
  		 select_callback(f, op);
		f->filp_pipe_select_ops &= ~op;
  	}
  }

  /* Search the proc table. */
  for (rp = &fproc[0]; rp < &fproc[NR_PROCS]; rp++) {
	if (rp->fp_pid != PID_FREE && rp->fp_suspended == SUSPENDED &&
			rp->fp_revived == NOT_REVIVING &&
			(rp->fp_fd & BYTE) == call_nr &&
			rp->fp_filp[rp->fp_fd>>8]->filp_vno == vp) {
		revive(rp->fp_endpoint, 0);
		susp_count--;	/* keep track of who is suspended */
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
  register int task;
  int fd_nr, proc_nr;
  struct filp *fil_ptr;

  if(isokendpt(proc_nr_e, &proc_nr) != OK)
	return;

  rfp = &fproc[proc_nr];
  if (rfp->fp_suspended == NOT_SUSPENDED || rfp->fp_revived == REVIVING)return;

  /* The 'reviving' flag only applies to pipes.  Processes waiting for TTY get
   * a message right away.  The revival process is different for TTY and pipes.
   * For select and TTY revival, the work is already done, for pipes it is not:
   *  the proc must be restarted so it can try again.
   */
  task = -rfp->fp_task;
  if (task == XPIPE || task == XLOCK) {
	/* Revive a process suspended on a pipe or lock. */
	rfp->fp_revived = REVIVING;
	reviving++;		/* process was waiting on pipe or lock */
  }
  else if (task == XDOPEN)
  {
	rfp->fp_suspended = NOT_SUSPENDED;
	fd_nr= rfp->fp_fd>>8;
	if (returned < 0)
	{
		fil_ptr= rfp->fp_filp[fd_nr];
		rfp->fp_filp[fd_nr] = NIL_FILP;
		FD_CLR(fd_nr, &rfp->fp_filp_inuse);
		if (fil_ptr->filp_count != 1)
		{
			panic(__FILE__, "revive: bad count in filp",
				fil_ptr->filp_count);
		}
		fil_ptr->filp_count= 0;
		put_vnode(fil_ptr->filp_vno);     
		fil_ptr->filp_vno = NIL_VNODE;
		reply(proc_nr_e, returned);
	}
	else
		reply(proc_nr_e, fd_nr);
  }
  else {
	rfp->fp_suspended = NOT_SUSPENDED;
	if (task == XPOPEN) /* process blocked in open or create */
		reply(proc_nr_e, rfp->fp_fd>>8);
	else if (task == XSELECT) {
		reply(proc_nr_e, returned);
	}
	else {
		/* Revive a process suspended on TTY or other device. 
		 * Pretend it wants only what there is.
		 */
		rfp->fp_nbytes = returned; 
		/* If a grant has been issued by FS for this I/O, revoke
		 * it again now that I/O is done.
		 */
		if(GRANT_VALID(rfp->fp_grant)) {
			if(cpf_revoke(rfp->fp_grant)) {
				panic(__FILE__,"FS: revoke failed for grant",
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

  register struct fproc *rfp;
  int proc_nr_p, task, fild, status = EINTR;
  struct filp *f;
  dev_t dev;
  message mess;

  if(isokendpt(proc_nr_e, &proc_nr_p) != OK) {
	printf("VFS: ignoring unpause for bogus endpoint %d\n", proc_nr_e);
	return;
  }

  rfp = &fproc[proc_nr_p];
  if (rfp->fp_suspended == NOT_SUSPENDED)
	return;
  task = -rfp->fp_task;

  if (rfp->fp_revived == REVIVING)
  {
	rfp->fp_revived = NOT_REVIVING;
	reviving--;
  }

  switch (task) {
	case XPIPE:		/* process trying to read or write a pipe */
		break;

	case XLOCK:		/* process trying to set a lock with FCNTL */
		break;

	case XSELECT:		/* process blocking on select() */
		select_forget(proc_nr_e);
		break;

	case XPOPEN:		/* process trying to open a fifo */
		break;

	case XDOPEN:		/* process trying to open a device */
		/* Don't cancel OPEN. Just wait until the open completes. */
		return;	

	default:		/* process trying to do device I/O (e.g. tty)*/
		if (rfp->fp_flags & SUSP_REOPEN)
		{
			/* Process is suspended while waiting for a reopen.
			 * Just reply EINTR.
			 */
			rfp->fp_flags &= ~SUSP_REOPEN;
			status= EINTR;
			break;
		}
		
		fild = (rfp->fp_fd >> 8) & BYTE;/* extract file descriptor */
		if (fild < 0 || fild >= OPEN_MAX)
			panic(__FILE__,"unpause err 2",NO_NUM);
		f = rfp->fp_filp[fild];
		dev = (dev_t) f->filp_vno->v_sdev;	/* device hung on */
		mess.TTY_LINE = (dev >> MINOR) & BYTE;
		mess.IO_ENDPT = rfp->fp_ioproc;
		mess.IO_GRANT = (char *) rfp->fp_grant;

		/* Tell kernel R or W. Mode is from current call, not open. */
		mess.COUNT = (rfp->fp_fd & BYTE) == READ ? R_BIT : W_BIT;
		mess.m_type = CANCEL;
		fp = rfp;	/* hack - ctty_io uses fp */
		(*dmap[(dev >> MAJOR) & BYTE].dmap_io)(task, &mess);
		status = mess.REP_STATUS;
		if (status == SUSPEND)
			return;		/* Process will be revived at a
					 * later time.
					 */

		if(status == EAGAIN) status = EINTR;
		if(GRANT_VALID(rfp->fp_grant)) {
			if(cpf_revoke(rfp->fp_grant)) {
				panic(__FILE__,"FS: revoke failed for grant (cancel)",
					rfp->fp_grant);
			} 
			rfp->fp_grant = GRANT_INVALID;
		}
  }

  rfp->fp_suspended = NOT_SUSPENDED;
  reply(proc_nr_e, status);	/* signal interrupted call */
}


/*===========================================================================*
 *				select_request_pipe			     *
 *===========================================================================*/
PUBLIC int select_request_pipe(struct filp *f, int *ops, int block)
{
	int orig_ops, r = 0, err, canwrite;
	orig_ops = *ops;
	if ((*ops & (SEL_RD|SEL_ERR))) {
		if ((err = Xpipe_check(f->filp_vno, READING, 0,
			1, f->filp_pos, 1)) != SUSPEND)
			r |= SEL_RD;
		if (err < 0 && err != SUSPEND)
			r |= SEL_ERR;
	}
	if ((*ops & (SEL_WR|SEL_ERR))) {
		if ((err = Xpipe_check(f->filp_vno, WRITING, 0,
			1, f->filp_pos, 1)) != SUSPEND)
			r |= SEL_WR;
		if (err < 0 && err != SUSPEND)
			r |= SEL_ERR;
	}

	/* Some options we collected might not be requested. */
	*ops = r & orig_ops;

	if (!*ops && block) {
		f->filp_pipe_select_ops |= orig_ops;
	}

	return SEL_OK;
}

/*===========================================================================*
 *				select_match_pipe			     *
 *===========================================================================*/
PUBLIC int select_match_pipe(struct filp *f)
{
	/* recognize either pipe or named pipe (FIFO) */
	if (f && f->filp_vno && (f->filp_vno->v_mode & I_NAMED_PIPE))
		return 1;
	return 0;
}




