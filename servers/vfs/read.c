/* This file contains the heart of the mechanism used to read (and write)
 * files.  Read and write requests are split up into chunks that do not cross
 * block boundaries.  Each chunk is then processed in turn.  Reads on special
 * files are also detected and handled.
 *
 * The entry points into this file are
 *   do_read:	 perform the READ system call by calling read_write
 *   do_getdents: read entries from a directory (GETDENTS)
 *   read_write: actually do the work of READ and WRITE
 *
 */

#include "fs.h"
#include <fcntl.h>
#include <unistd.h>
#include <minix/com.h>
#include <minix/u64.h>
#include "file.h"
#include "fproc.h"
#include "scratchpad.h"
#include "param.h"
#include <dirent.h>
#include <assert.h>
#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"


/*===========================================================================*
 *				do_read					     *
 *===========================================================================*/
int do_read(message *UNUSED(m_out))
{
  return(do_read_write_peek(READING, job_m_in.fd,
          job_m_in.buffer, (size_t) job_m_in.nbytes));
}


/*===========================================================================*
 *				lock_bsf				     *
 *===========================================================================*/
void lock_bsf(void)
{
  struct fproc *org_fp;
  struct worker_thread *org_self;

  if (mutex_trylock(&bsf_lock) == 0)
	return;

  org_fp = fp;
  org_self = self;

  if (mutex_lock(&bsf_lock) != 0)
	panic("unable to lock block special file lock");

  fp = org_fp;
  self = org_self;
}

/*===========================================================================*
 *				unlock_bsf				     *
 *===========================================================================*/
void unlock_bsf(void)
{
  if (mutex_unlock(&bsf_lock) != 0)
	panic("failed to unlock block special file lock");
}

/*===========================================================================*
 *				check_bsf				     *
 *===========================================================================*/
void check_bsf_lock(void)
{
	int r = mutex_trylock(&bsf_lock);

	if (r == -EBUSY)
		panic("bsf_lock locked");
	else if (r != 0)
		panic("bsf_lock weird state");

	/* r == 0 */
	unlock_bsf();
}

/*===========================================================================*
 *				do_read_write_peek			     *
 *===========================================================================*/
int do_read_write_peek(int rw_flag, int io_fd, char *io_buf, size_t io_nbytes)
{
/* Perform read(fd, buffer, nbytes) or write(fd, buffer, nbytes) call. */
  struct filp *f;
  tll_access_t locktype;
  int r;
  int ro = 1;

  if(rw_flag == WRITING) ro = 0;

  scratch(fp).file.fd_nr = io_fd;
  scratch(fp).io.io_buffer = io_buf;
  scratch(fp).io.io_nbytes = io_nbytes;

  locktype = ro ? VNODE_READ : VNODE_WRITE;
  if ((f = get_filp(scratch(fp).file.fd_nr, locktype)) == NULL)
	return(err_code);
  if (((f->filp_mode) & (ro ? R_BIT : W_BIT)) == 0) {
	unlock_filp(f);
	return(f->filp_mode == FILP_CLOSED ? EIO : EBADF);
  }
  if (scratch(fp).io.io_nbytes == 0) {
	unlock_filp(f);
	return(0);	/* so char special files need not check for 0*/
  }

  r = read_write(rw_flag, f, scratch(fp).io.io_buffer, scratch(fp).io.io_nbytes,
		 who_e);

  unlock_filp(f);
  return(r);
}

/*===========================================================================*
 *				read_write				     *
 *===========================================================================*/
int read_write(int rw_flag, struct filp *f, char *buf, size_t size,
		      endpoint_t for_e)
{
  register struct vnode *vp;
  u64_t position, res_pos, new_pos;
  unsigned int cum_io, cum_io_incr, res_cum_io;
  int op, r;

  position = f->filp_pos;
  vp = f->filp_vno;
  r = OK;
  cum_io = 0;

  assert(rw_flag == READING || rw_flag == WRITING || rw_flag == PEEKING);

  if (size > SSIZE_MAX) return(EINVAL);

  op = (rw_flag == READING ? VFS_DEV_READ : VFS_DEV_WRITE);

  if (S_ISFIFO(vp->v_mode)) {		/* Pipes */
	if (fp->fp_cum_io_partial != 0) {
		panic("VFS: read_write: fp_cum_io_partial not clear");
	}
	if(rw_flag == PEEKING) return EINVAL;
	r = rw_pipe(rw_flag, for_e, f, buf, size);
  } else if (S_ISCHR(vp->v_mode)) {	/* Character special files. */
	dev_t dev;
	int suspend_reopen;
	int op = (rw_flag == READING ? VFS_DEV_READ : VFS_DEV_WRITE);

	if(rw_flag == PEEKING) return EINVAL;

	if (vp->v_sdev == NO_DEV)
		panic("VFS: read_write tries to access char dev NO_DEV");

	suspend_reopen = (f->filp_state & FS_NEEDS_REOPEN);
	dev = (dev_t) vp->v_sdev;

	r = dev_io(op, dev, for_e, buf, position, size, f->filp_flags,
		   suspend_reopen);
	if (r >= 0) {
		cum_io = r;
		position = add64ul(position, r);
		r = OK;
	}
  } else if (S_ISBLK(vp->v_mode)) {	/* Block special files. */
	if (vp->v_sdev == NO_DEV)
		panic("VFS: read_write tries to access block dev NO_DEV");

	if(rw_flag == PEEKING) return EINVAL;

	lock_bsf();

	r = req_breadwrite(vp->v_bfs_e, for_e, vp->v_sdev, position, size,
			   buf, rw_flag, &res_pos, &res_cum_io);
	if (r == OK) {
		position = res_pos;
		cum_io += res_cum_io;
	}

	unlock_bsf();
  } else {				/* Regular files */
	if (rw_flag == WRITING) {
		/* Check for O_APPEND flag. */
		if (f->filp_flags & O_APPEND) position = cvul64(vp->v_size);
	}

	/* Issue request */
	r = req_readwrite(vp->v_fs_e, vp->v_inode_nr, position, rw_flag, for_e,
			  buf, size, &new_pos, &cum_io_incr);

	if (r >= 0) {
		if (ex64hi(new_pos))
			panic("read_write: bad new pos");

		position = new_pos;
		cum_io += cum_io_incr;
	}
  }

  /* On write, update file size and access time. */
  if (rw_flag == WRITING) {
	if (S_ISREG(vp->v_mode) || S_ISDIR(vp->v_mode)) {
		if (cmp64ul(position, vp->v_size) > 0) {
			if (ex64hi(position) != 0) {
				panic("read_write: file size too big ");
			}
			vp->v_size = ex64lo(position);
		}
	}
  }

  f->filp_pos = position;

  if (r == EPIPE && rw_flag == WRITING) {
	/* Process is writing, but there is no reader. Tell the kernel to
	 * generate s SIGPIPE signal.
	 */
	if (!(f->filp_flags & O_NOSIGPIPE)) {
		sys_kill(fp->fp_endpoint, SIGPIPE);
	}
  }

  if (r == OK) {
	return(cum_io);
  }
  return(r);
}

/*===========================================================================*
 *				do_getdents				     *
 *===========================================================================*/
int do_getdents(message *UNUSED(m_out))
{
/* Perform the getdents(fd, buf, size) system call. */
  int r = OK;
  u64_t new_pos;
  register struct filp *rfilp;

  scratch(fp).file.fd_nr = job_m_in.fd;
  scratch(fp).io.io_buffer = job_m_in.buffer;
  scratch(fp).io.io_nbytes = (size_t) job_m_in.nbytes;

  /* Is the file descriptor valid? */
  if ( (rfilp = get_filp(scratch(fp).file.fd_nr, VNODE_READ)) == NULL)
	return(err_code);

  if (!(rfilp->filp_mode & R_BIT))
	r = EBADF;
  else if (!S_ISDIR(rfilp->filp_vno->v_mode))
	r = EBADF;

  if (r == OK) {
	if (ex64hi(rfilp->filp_pos) != 0)
		panic("do_getdents: can't handle large offsets");

	r = req_getdents(rfilp->filp_vno->v_fs_e, rfilp->filp_vno->v_inode_nr,
			 rfilp->filp_pos, scratch(fp).io.io_buffer,
			 scratch(fp).io.io_nbytes, &new_pos,0);

	if (r > 0) rfilp->filp_pos = new_pos;
  }

  unlock_filp(rfilp);
  return(r);
}


/*===========================================================================*
 *				rw_pipe					     *
 *===========================================================================*/
int rw_pipe(rw_flag, usr_e, f, buf, req_size)
int rw_flag;			/* READING or WRITING */
endpoint_t usr_e;
struct filp *f;
char *buf;
size_t req_size;
{
  int r, oflags, partial_pipe = 0;
  size_t size, cum_io, cum_io_incr;
  struct vnode *vp;
  u64_t  position, new_pos;

  /* Must make sure we're operating on locked filp and vnode */
  assert(tll_locked_by_me(&f->filp_vno->v_lock));
  assert(mutex_trylock(&f->filp_lock) == -EDEADLK);

  oflags = f->filp_flags;
  vp = f->filp_vno;
  position = cvu64(0);	/* Not actually used */

  /* fp->fp_cum_io_partial is only nonzero when doing partial writes */
  cum_io = fp->fp_cum_io_partial;

  r = pipe_check(f, rw_flag, oflags, req_size, 0);
  if (r <= 0) {
	if (r == SUSPEND) pipe_suspend(f, buf, req_size);
	return(r);
  }

  size = r;
  if (size < req_size) partial_pipe = 1;

  /* Truncate read request at size. */
  if (rw_flag == READING && size > vp->v_size) {
	size = vp->v_size;
  }

  if (vp->v_mapfs_e == 0)
	panic("unmapped pipe");

  r = req_readwrite(vp->v_mapfs_e, vp->v_mapinode_nr, position, rw_flag, usr_e,
		    buf, size, &new_pos, &cum_io_incr);

  if (r != OK) {
	return(r);
  }

  if (ex64hi(new_pos))
	panic("rw_pipe: bad new pos");

  cum_io += cum_io_incr;
  buf += cum_io_incr;
  req_size -= cum_io_incr;

  vp->v_size = ex64lo(new_pos);

  if (partial_pipe) {
	/* partial write on pipe with */
	/* O_NONBLOCK, return write count */
	if (!(oflags & O_NONBLOCK)) {
		/* partial write on pipe with req_size > PIPE_SIZE,
		 * non-atomic
		 */
		fp->fp_cum_io_partial = cum_io;
		pipe_suspend(f, buf, req_size);
		return(SUSPEND);
	}
  }

  fp->fp_cum_io_partial = 0;

  return(cum_io);
}
