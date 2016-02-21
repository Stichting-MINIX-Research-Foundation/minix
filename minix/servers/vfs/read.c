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
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/u64.h>
#include <minix/vfsif.h>
#include <assert.h>
#include <sys/dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include "file.h"
#include "vnode.h"
#include "vmnt.h"


/*===========================================================================*
 *				do_read					     *
 *===========================================================================*/
int do_read(void)
{

  /*
   * This field is currently reserved for internal usage only, and must be set
   * to zero by the caller.  We may use it for future SA_RESTART support just
   * like we are using it internally now.
   */
  if (job_m_in.m_lc_vfs_readwrite.cum_io != 0)
	return(EINVAL);

  return(do_read_write_peek(READING, job_m_in.m_lc_vfs_readwrite.fd,
	job_m_in.m_lc_vfs_readwrite.buf, job_m_in.m_lc_vfs_readwrite.len));
}


/*===========================================================================*
 *				lock_bsf				     *
 *===========================================================================*/
void lock_bsf(void)
{
  struct worker_thread *org_self;

  if (mutex_trylock(&bsf_lock) == 0)
	return;

  org_self = worker_suspend();

  if (mutex_lock(&bsf_lock) != 0)
	panic("unable to lock block special file lock");

  worker_resume(org_self);
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
 *				actual_read_write_peek			     *
 *===========================================================================*/
int actual_read_write_peek(struct fproc *rfp, int rw_flag, int fd,
	vir_bytes buf, size_t nbytes)
{
/* Perform read(fd, buffer, nbytes) or write(fd, buffer, nbytes) call. */
  struct filp *f;
  tll_access_t locktype;
  int r;
  int ro = 1;

  if(rw_flag == WRITING) ro = 0;

  locktype = rw_flag == WRITING ? VNODE_WRITE : VNODE_READ;
  if ((f = get_filp2(rfp, fd, locktype)) == NULL)
	return(err_code);

  assert(f->filp_count > 0);

  if (((f->filp_mode) & (ro ? R_BIT : W_BIT)) == 0) {
	unlock_filp(f);
	return(EBADF);
  }
  if (nbytes == 0) {
	unlock_filp(f);
	return(0);	/* so char special files need not check for 0*/
  }

  r = read_write(rfp, rw_flag, fd, f, buf, nbytes, who_e);

  unlock_filp(f);
  return(r);
}

/*===========================================================================*
 *				do_read_write_peek			     *
 *===========================================================================*/
int do_read_write_peek(int rw_flag, int fd, vir_bytes buf, size_t nbytes)
{
	return actual_read_write_peek(fp, rw_flag, fd, buf, nbytes);
}

/*===========================================================================*
 *				read_write				     *
 *===========================================================================*/
int read_write(struct fproc *rfp, int rw_flag, int fd, struct filp *f,
	vir_bytes buf, size_t size, endpoint_t for_e)
{
  register struct vnode *vp;
  off_t position, res_pos;
  size_t cum_io, res_cum_io;
  size_t cum_io_incr;
  int op, r;
  dev_t dev;

  position = f->filp_pos;
  vp = f->filp_vno;
  r = OK;
  cum_io = 0;

  assert(rw_flag == READING || rw_flag == WRITING || rw_flag == PEEKING);

  if (size > SSIZE_MAX) return(EINVAL);

  if (S_ISFIFO(vp->v_mode)) {		/* Pipes */
	if(rw_flag == PEEKING) {
	  	printf("read_write: peek on pipe makes no sense\n");
		return EINVAL;
	}
	assert(fd != -1);
	op = (rw_flag == READING ? VFS_READ : VFS_WRITE);
	r = rw_pipe(rw_flag, for_e, f, op, fd, buf, size, 0 /*cum_io*/);
  } else if (S_ISCHR(vp->v_mode)) {	/* Character special files. */
	if(rw_flag == PEEKING) {
	  	printf("read_write: peek on char device makes no sense\n");
		return EINVAL;
	}

	if (vp->v_sdev == NO_DEV)
		panic("VFS: read_write tries to access char dev NO_DEV");

	dev = vp->v_sdev;
	op = (rw_flag == READING ? CDEV_READ : CDEV_WRITE);

	r = cdev_io(op, dev, for_e, buf, position, size, f->filp_flags);
	if (r >= 0) {
		/* This should no longer happen: all calls are asynchronous. */
		printf("VFS: I/O to device %llx succeeded immediately!?\n", dev);
		cum_io = r;
		position += r;
		r = OK;
	} else if (r == SUSPEND) {
		/* FIXME: multiple read/write operations on a single filp
		 * should be serialized. They currently aren't; in order to
		 * achieve a similar effect, we optimistically advance the file
		 * position here. This works under the following assumptions:
		 * - character drivers that use the seek position at all,
		 *   expose a view of a statically-sized range of bytes, i.e.,
		 *   they are basically byte-granular block devices;
		 * - if short I/O or an error is returned, all subsequent calls
		 *   will return (respectively) EOF and an error;
		 * - the application never checks its own file seek position,
		 *   or does not care that it may end up having seeked beyond
		 *   the number of bytes it has actually read;
		 * - communication to the character driver is FIFO (this one
		 *   is actually true! whew).
		 * Many improvements are possible here, but in the end,
		 * anything short of queuing concurrent operations will be
		 * suboptimal - so we settle for this hack for now.
		 */
		position += size;
	}
  } else if (S_ISSOCK(vp->v_mode)) {
	if (rw_flag == PEEKING) {
		printf("VFS: read_write tries to peek on sock dev\n");
		return EINVAL;
	}

	if (vp->v_sdev == NO_DEV)
		panic("VFS: read_write tries to access sock dev NO_DEV");

	r = sdev_readwrite(vp->v_sdev, buf, size, 0, 0, 0, 0, 0, rw_flag,
	    f->filp_flags, 0);
  } else if (S_ISBLK(vp->v_mode)) {	/* Block special files. */
	if (vp->v_sdev == NO_DEV)
		panic("VFS: read_write tries to access block dev NO_DEV");

	lock_bsf();

	if(rw_flag == PEEKING) {
		r = req_bpeek(vp->v_bfs_e, vp->v_sdev, position, size);
	} else {
		r = req_breadwrite(vp->v_bfs_e, for_e, vp->v_sdev, position,
		       size, buf, rw_flag, &res_pos, &res_cum_io);
		if (r == OK) {
			position = res_pos;
			cum_io += res_cum_io;
		}
	}

	unlock_bsf();
  } else {				/* Regular files */
	if (rw_flag == WRITING) {
		/* Check for O_APPEND flag. */
		if (f->filp_flags & O_APPEND) position = vp->v_size;
	}

	/* Issue request */
	if(rw_flag == PEEKING) {
		r = req_peek(vp->v_fs_e, vp->v_inode_nr, position, size);
	} else {
		off_t new_pos;
		r = req_readwrite(vp->v_fs_e, vp->v_inode_nr, position,
			rw_flag, for_e, buf, size, &new_pos,
			&cum_io_incr);

		if (r >= 0) {
			position = new_pos;
			cum_io += cum_io_incr;
		}
        }
  }

  /* On write, update file size and access time. */
  if (rw_flag == WRITING) {
	if (S_ISREG(vp->v_mode) || S_ISDIR(vp->v_mode)) {
		if (position > vp->v_size) {
			vp->v_size = position;
		}
	}
  }

  f->filp_pos = position;

  if (r == EPIPE && rw_flag == WRITING) {
	/* Process is writing, but there is no reader. Tell the kernel to
	 * generate a SIGPIPE signal.
	 */
	if (!(f->filp_flags & O_NOSIGPIPE)) {
		sys_kill(rfp->fp_endpoint, SIGPIPE);
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
int do_getdents(void)
{
/* Perform the getdents(fd, buf, size) system call. */
  int fd, r = OK;
  off_t new_pos;
  vir_bytes buf;
  size_t size;
  register struct filp *rfilp;

  /* This field must always be set to zero for getdents(). */
  if (job_m_in.m_lc_vfs_readwrite.cum_io != 0)
	return(EINVAL);

  fd = job_m_in.m_lc_vfs_readwrite.fd;
  buf = job_m_in.m_lc_vfs_readwrite.buf;
  size = job_m_in.m_lc_vfs_readwrite.len;

  /* Is the file descriptor valid? */
  if ( (rfilp = get_filp(fd, VNODE_READ)) == NULL)
	return(err_code);

  if (!(rfilp->filp_mode & R_BIT))
	r = EBADF;
  else if (!S_ISDIR(rfilp->filp_vno->v_mode))
	r = EBADF;

  if (r == OK) {
	r = req_getdents(rfilp->filp_vno->v_fs_e, rfilp->filp_vno->v_inode_nr,
	    rfilp->filp_pos, buf, size, &new_pos, 0);

	if (r > 0) rfilp->filp_pos = new_pos;
  }

  unlock_filp(rfilp);
  return(r);
}


/*===========================================================================*
 *				rw_pipe					     *
 *===========================================================================*/
int rw_pipe(int rw_flag, endpoint_t usr_e, struct filp *f, int callnr, int fd,
	vir_bytes buf, size_t nbytes, size_t cum_io)
{
  int r, oflags, partial_pipe = FALSE;
  size_t size;
  size_t cum_io_incr;
  struct vnode *vp;
  off_t  position, new_pos;

  /* Must make sure we're operating on locked filp and vnode */
  assert(tll_locked_by_me(&f->filp_vno->v_lock));
  assert(mutex_trylock(&f->filp_lock) == -EDEADLK);

  oflags = f->filp_flags;
  vp = f->filp_vno;
  position = 0;	/* Not actually used */

  assert(rw_flag == READING || rw_flag == WRITING);

  r = pipe_check(f, rw_flag, oflags, nbytes, 0);
  if (r <= 0) {
	if (r == SUSPEND)
		pipe_suspend(callnr, fd, buf, nbytes, cum_io);

	/* If pipe_check returns an error instead of suspending the call, we
	 * return that error, even if we are resuming a partially completed
	 * operation (ie, a large blocking write), to match NetBSD's behavior.
	 */
	return(r);
  }

  size = r;
  if (size < nbytes) partial_pipe = TRUE;

  /* Truncate read request at size. */
  if (rw_flag == READING && size > vp->v_size) {
	size = vp->v_size;
  }

  if (vp->v_mapfs_e == 0)
	panic("unmapped pipe");

  r = req_readwrite(vp->v_mapfs_e, vp->v_mapinode_nr, position, rw_flag, usr_e,
		    buf, size, &new_pos, &cum_io_incr);

  if (r != OK) {
	assert(r != SUSPEND);
	return(r);
  }

  cum_io += cum_io_incr;
  buf += cum_io_incr;
  nbytes -= cum_io_incr;

  if (rw_flag == READING)
	vp->v_size -= cum_io_incr;
  else
	vp->v_size += cum_io_incr;

  if (partial_pipe) {
	/* partial write on pipe with */
	/* O_NONBLOCK, return write count */
	if (!(oflags & O_NONBLOCK)) {
		/* partial write on pipe with nbytes > PIPE_BUF, non-atomic */
		pipe_suspend(callnr, fd, buf, nbytes, cum_io);
		return(SUSPEND);
	}
  }

  return(cum_io);
}
