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
int do_read()
{
  return(do_read_write(READING));
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
 *				do_read_write				     *
 *===========================================================================*/
int do_read_write(rw_flag)
int rw_flag;			/* READING or WRITING */
{
/* Perform read(fd, buffer, nbytes) or write(fd, buffer, nbytes) call. */
  struct filp *f;
  tll_access_t locktype;
  int r;

  scratch(fp).file.fd_nr = job_m_in.fd;
  scratch(fp).io.io_buffer = job_m_in.buffer;
  scratch(fp).io.io_nbytes = (size_t) job_m_in.nbytes;

  locktype = (rw_flag == READING) ? VNODE_READ : VNODE_WRITE;
  if ((f = get_filp(scratch(fp).file.fd_nr, locktype)) == NULL)
	return(err_code);
  if (((f->filp_mode) & (rw_flag == READING ? R_BIT : W_BIT)) == 0) {
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
  int op, oflags, r;

  position = f->filp_pos;
  oflags = f->filp_flags;
  vp = f->filp_vno;
  r = OK;
  cum_io = 0;

  if (size > SSIZE_MAX) return(EINVAL);

  if (S_ISFIFO(vp->v_mode)) {
	if (fp->fp_cum_io_partial != 0) {
		panic("VFS: read_write: fp_cum_io_partial not clear");
	}
	r = rw_pipe(rw_flag, for_e, f, buf, size);
	return(r);
  }

  op = (rw_flag == READING ? VFS_DEV_READ : VFS_DEV_WRITE);

  if (S_ISCHR(vp->v_mode)) {	/* Character special files. */
	dev_t dev;
	int suspend_reopen;

	if (vp->v_sdev == NO_DEV)
		panic("VFS: read_write tries to access char dev NO_DEV");

	suspend_reopen = (f->filp_state & FS_NEEDS_REOPEN);
	dev = (dev_t) vp->v_sdev;

	r = dev_io(op, dev, for_e, buf, position, size, oflags,
		   suspend_reopen);
	if (r >= 0) {
		cum_io = r;
		position = add64ul(position, r);
		r = OK;
	}
  } else if (S_ISBLK(vp->v_mode)) {	/* Block special files. */
	if (vp->v_sdev == NO_DEV)
		panic("VFS: read_write tries to access block dev NO_DEV");

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
		if (oflags & O_APPEND) position = cvul64(vp->v_size);
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

  if (r == OK) return(cum_io);
  return(r);
}

/*===========================================================================*
 *				do_getdents				     *
 *===========================================================================*/
int do_getdents()
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
  u64_t position, new_pos;

  /* Must make sure we're operating on locked filp and vnode */
  assert(tll_islocked(&f->filp_vno->v_lock));
  assert(mutex_trylock(&f->filp_lock) == -EDEADLK);

  oflags = f->filp_flags;
  vp = f->filp_vno;
  position = cvu64((rw_flag == READING) ? vp->v_pipe_rd_pos :
							vp->v_pipe_wr_pos);
  /* fp->fp_cum_io_partial is only nonzero when doing partial writes */
  cum_io = fp->fp_cum_io_partial;

  r = pipe_check(vp, rw_flag, oflags, req_size, position, 0);
  if (r <= 0) {
	if (r == SUSPEND) pipe_suspend(f, buf, req_size);
	return(r);
  }

  size = r;
  if (size < req_size) partial_pipe = 1;

  /* Truncate read request at size. */
  if((rw_flag == READING) &&
	cmp64ul(add64ul(position, size), vp->v_size) > 0) {
	/* Position always should fit in an off_t (LONG_MAX). */
	off_t pos32;

	assert(cmp64ul(position, LONG_MAX) <= 0);
	pos32 = cv64ul(position);
	assert(pos32 >= 0);
	assert(pos32 <= LONG_MAX);
	size = vp->v_size - pos32;
  }

  if (vp->v_mapfs_e == 0)
	panic("unmapped pipe");

  r = req_readwrite(vp->v_mapfs_e, vp->v_mapinode_nr, position, rw_flag, usr_e,
		    buf, size, &new_pos, &cum_io_incr);

  if (r >= 0) {
	if (ex64hi(new_pos))
		panic("rw_pipe: bad new pos");

	position = new_pos;
	cum_io += cum_io_incr;
	buf += cum_io_incr;
	req_size -= cum_io_incr;
  }

  /* On write, update file size and access time. */
  if (rw_flag == WRITING) {
	if (cmp64ul(position, vp->v_size) > 0) {
		if (ex64hi(position) != 0) {
			panic("read_write: file size too big for v_size");
		}
		vp->v_size = ex64lo(position);
	}
  } else {
	if (cmp64ul(position, vp->v_size) >= 0) {
		/* Reset pipe pointers */
		vp->v_size = 0;
		vp->v_pipe_rd_pos= 0;
		vp->v_pipe_wr_pos= 0;
		position = cvu64(0);
	}
  }

  if (rw_flag == READING)
	vp->v_pipe_rd_pos= cv64ul(position);
  else
	vp->v_pipe_wr_pos= cv64ul(position);

  if (r == OK) {
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

  return(r);
}
