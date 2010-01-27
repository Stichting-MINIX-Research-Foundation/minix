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
#include "param.h"
#include <dirent.h>
#include <assert.h>
#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"


/*===========================================================================*
 *				do_read					     *
 *===========================================================================*/
PUBLIC int do_read()
{
  return(read_write(READING));
}


/*===========================================================================*
 *				read_write				     *
 *===========================================================================*/
PUBLIC int read_write(rw_flag)
int rw_flag;			/* READING or WRITING */
{
/* Perform read(fd, buffer, nbytes) or write(fd, buffer, nbytes) call. */
  register struct filp *f;
  register struct vnode *vp;
  off_t bytes_left;
  u64_t position, res_pos, new_pos;
  unsigned int off, cum_io, cum_io_incr, res_cum_io;
  int op, oflags, r, chunk, block_spec, char_spec;
  int regular;
  mode_t mode_word;
  phys_bytes p;
  struct dmap *dp;

  /* If the file descriptor is valid, get the vnode, size and mode. */
  if (m_in.nbytes < 0) return(EINVAL);
  if ((f = get_filp(m_in.fd)) == NIL_FILP) return(err_code);
  if (((f->filp_mode) & (rw_flag == READING ? R_BIT : W_BIT)) == 0) {
	return(f->filp_mode == FILP_CLOSED ? EIO : EBADF);
  }
  if (m_in.nbytes == 0)
	return(0);	/* so char special files need not check for 0*/

  position = f->filp_pos;
  oflags = f->filp_flags;
  vp = f->filp_vno;
  r = OK;
  cum_io = 0;

  if (vp->v_pipe == I_PIPE) {
	if (fp->fp_cum_io_partial != 0) {
		panic(__FILE__, "read_write: fp_cum_io_partial not clear",
		      NO_NUM);
	}
	return rw_pipe(rw_flag, who_e, m_in.fd, f, m_in.buffer, m_in.nbytes);
  }

  op = (rw_flag == READING ? VFS_DEV_READ : VFS_DEV_WRITE);
  mode_word = vp->v_mode & I_TYPE;
  regular = mode_word == I_REGULAR;

  if ((char_spec = (mode_word == I_CHAR_SPECIAL ? 1 : 0))) {
	if (vp->v_sdev == NO_DEV)
		panic(__FILE__, "read_write tries to read from "
				"character device NO_DEV", NO_NUM);

  }

  if ((block_spec = (mode_word == I_BLOCK_SPECIAL ? 1 : 0))) {
	if (vp->v_sdev == NO_DEV)
		panic(__FILE__, "read_write tries to read from "
				" block device NO_DEV", NO_NUM);
  }

  if (char_spec) {			/* Character special files. */
	dev_t dev;
	int suspend_reopen;

	suspend_reopen = (f->filp_state != FS_NORMAL);
	dev = (dev_t) vp->v_sdev;

	r = dev_io(op, dev, who_e, m_in.buffer, position, m_in.nbytes, oflags,
		   suspend_reopen);
	if (r >= 0) {
		cum_io = r;
		position = add64ul(position, r);
		r = OK;
	}
  } else if (block_spec) {		/* Block special files. */
	r = req_breadwrite(vp->v_bfs_e, who_e, vp->v_sdev, position,
		m_in.nbytes, m_in.buffer, rw_flag, &res_pos, &res_cum_io);
	position = res_pos;
	cum_io += res_cum_io;
  } else {				/* Regular files */
	if (rw_flag == WRITING && block_spec == 0) {
		/* Check for O_APPEND flag. */
		if (oflags & O_APPEND) position = cvul64(vp->v_size);
	}

	/* Issue request */
	r = req_readwrite(vp->v_fs_e, vp->v_inode_nr, position, rw_flag, who_e,
			  m_in.buffer, m_in.nbytes, &new_pos, &cum_io_incr);

	if (r >= 0) {
		if (ex64hi(new_pos))
			panic(__FILE__, "read_write: bad new pos", NO_NUM);

		position = new_pos;
		cum_io += cum_io_incr;
	}
  }

  /* On write, update file size and access time. */
  if (rw_flag == WRITING) {
	if (regular || mode_word == I_DIRECTORY) {
		if (cmp64ul(position, vp->v_size) > 0) {
			if (ex64hi(position) != 0) {
				panic(__FILE__,
				      "read_write: file size too big ", NO_NUM);
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
PUBLIC int do_getdents()
{
/* Perform the getdents(fd, buf, size) system call. */
  int r;
  u64_t new_pos;
  cp_grant_id_t gid;
  register struct filp *rfilp;

  /* Is the file descriptor valid? */
  if ( (rfilp = get_filp(m_in.fd)) == NIL_FILP) {
	  return(err_code);
  }
  
  if (!(rfilp->filp_mode & R_BIT))
	return(EBADF);

  if ((rfilp->filp_vno->v_mode & I_TYPE) != I_DIRECTORY)
	return(EBADF);

  if (ex64hi(rfilp->filp_pos) != 0)
	panic(__FILE__, "do_getdents: should handle large offsets", NO_NUM);
	
  r = req_getdents(rfilp->filp_vno->v_fs_e, rfilp->filp_vno->v_inode_nr, 
		   rfilp->filp_pos, m_in.buffer, m_in.nbytes, &new_pos);

  if (r > 0)
	rfilp->filp_pos = new_pos;
  return(r);
}


/*===========================================================================*
 *				rw_pipe					     *
 *===========================================================================*/
PUBLIC int rw_pipe(rw_flag, usr_e, fd_nr, f, buf, req_size)
int rw_flag;			/* READING or WRITING */
endpoint_t usr_e;
int fd_nr;
struct filp *f;
char *buf;
size_t req_size;
{
  int r, oflags, op, partial_pipe = 0, r2;
  size_t size, size2, cum_io, cum_io_incr, cum_io_incr2;
  struct vnode *vp;
  u64_t position, new_pos, new_pos2;

  oflags = f->filp_flags;
  vp = f->filp_vno;
  position = cvu64((rw_flag == READING) ? vp->v_pipe_rd_pos :
	vp->v_pipe_wr_pos);
  /* fp->fp_cum_io_partial is only nonzero when doing partial writes */
  cum_io = fp->fp_cum_io_partial; 
  op = (rw_flag == READING ? VFS_DEV_READ : VFS_DEV_WRITE);

  r = pipe_check(vp, rw_flag, oflags, req_size, position, 0);
  if (r <= 0) {
	if (r == SUSPEND) pipe_suspend(rw_flag, fd_nr, buf, req_size);
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
	size2 = size;
	size = vp->v_size - pos32;
  }

  if (vp->v_mapfs_e == 0) 
	panic(__FILE__, "unmapped pipe", NO_NUM);

  r = req_readwrite(vp->v_mapfs_e, vp->v_mapinode_nr, position, rw_flag, usr_e,
		    buf, size, &new_pos, &cum_io_incr);

  if (r >= 0) {
	if (ex64hi(new_pos))
		panic(__FILE__, "rw_pipe: bad new pos", NO_NUM);

	position = new_pos;
	cum_io += cum_io_incr;
	buf += cum_io_incr;
	req_size -= cum_io_incr;
  }
  
  /* On write, update file size and access time. */
  if (rw_flag == WRITING) {
	if (cmp64ul(position, vp->v_size) > 0) {
		if (ex64hi(position) != 0) {
			panic(__FILE__,
			      "read_write: file size too big for v_size",
			      NO_NUM);
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
			pipe_suspend(rw_flag, fd_nr, buf, req_size);
			return(SUSPEND);
		}
	}
	fp->fp_cum_io_partial = 0;
	return(cum_io);
  }

  return(r);
}

