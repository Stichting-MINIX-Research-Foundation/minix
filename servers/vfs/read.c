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
 * Changes for VFS:
 *   Jul 2006 (Balazs Gerofi)
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
  unsigned int off, cum_io, cum_io_incr, res_cum_io, num_of_bytes;
  int op, oflags, r, chunk, usr, block_spec, char_spec;
  int regular;
  mode_t mode_word;
  phys_bytes p;
  struct dmap *dp;

  /* PM loads segments by putting funny things in other bits of the
   * message, indicated by a high bit in fd. */
  if (who_e == PM_PROC_NR && (m_in.fd & _PM_SEG_FLAG)) {
	panic(__FILE__,
	"read_write: special read/write calls by PM no longer supported",
		NO_NUM);
  } 
  else {
      usr = who_e;		/* normal case */
  }

  /* If the file descriptor is valid, get the vnode, size and mode. */
  if (m_in.nbytes < 0)
  	return(EINVAL);

  if ((f = get_filp(m_in.fd)) == NIL_FILP)
  {
  	printf("vfs:read_write: returning %d to endpoint %d\n",
		err_code, who_e);
  	return(err_code);
  }
  if (((f->filp_mode) & (rw_flag == READING ? R_BIT : W_BIT)) == 0) {
	printf("vfs:read_write: returning error\n");
      return(f->filp_mode == FILP_CLOSED ? EIO : EBADF);
  }

  if (m_in.nbytes == 0)
      return(0);	/* so char special files need not check for 0*/

  position = f->filp_pos;
  oflags = f->filp_flags;

  vp = f->filp_vno;

  if (vp->v_pipe)
  {
	if (fp->fp_cum_io_partial != 0)
	{
		panic(__FILE__, "read_write: fp_cum_io_partial not clear",
			NO_NUM);
	}
	return rw_pipe(rw_flag, usr, m_in.fd, f, m_in.buffer, m_in.nbytes);
  }

  r = OK;
  cum_io = 0;

  op = (rw_flag == READING ? VFS_DEV_READ : VFS_DEV_WRITE);
  mode_word = vp->v_mode & I_TYPE;
  regular = mode_word == I_REGULAR;

  if ((char_spec = (mode_word == I_CHAR_SPECIAL ? 1 : 0))) {
      if (vp->v_sdev == NO_DEV)
          panic(__FILE__,"read_write tries to read from "
                  "character device NO_DEV", NO_NUM);
  }

  if ((block_spec = (mode_word == I_BLOCK_SPECIAL ? 1 : 0))) {
      if (vp->v_sdev == NO_DEV)
          panic(__FILE__,"read_write tries to read from "
                  " block device NO_DEV", NO_NUM);
  }

  /* Character special files. */
  if (char_spec) {
	dev_t dev;
	int suspend_reopen;

	suspend_reopen= (f->filp_state != FS_NORMAL);

	dev = (dev_t) vp->v_sdev;
	r = dev_io(op, dev, usr, m_in.buffer, position, m_in.nbytes, oflags,
		suspend_reopen);
	if (r >= 0) {
		cum_io = r;
		position = add64ul(position, r);
		r = OK;
	}
  } 
  /* Block special files. */
  else if (block_spec) {

      /* Issue request */
      r = req_breadwrite(vp->v_bfs_e, usr, vp->v_sdev, position,
	m_in.nbytes, m_in.buffer, rw_flag, &res_pos, &res_cum_io);

      position = res_pos;
      cum_io += res_cum_io;
  }
  /* Regular files */
  else {
      if (rw_flag == WRITING && block_spec == 0) {
          /* Check for O_APPEND flag. */
          if (oflags & O_APPEND) position = cvul64(vp->v_size);
      }


      /* Fill in request structure */
      num_of_bytes = m_in.nbytes;

#if 0  /* Don't truncate read request at size. The filesystem process will
	* do this itself.
	*/
      if((rw_flag == READING) &&
	cmp64ul(add64ul(position, num_of_bytes), vp->v_size) > 0) {
		/* Position always should fit in an off_t (LONG_MAX). */
		off_t pos32;
		assert(cmp64ul(position, LONG_MAX) <= 0);
		pos32 = cv64ul(position);
		assert(pos32 >= 0);
		assert(pos32 <= LONG_MAX);
		num_of_bytes = vp->v_size - pos32;
		assert(num_of_bytes >= 0);
      }
#endif

      /* Issue request */
      r = req_readwrite(vp->v_fs_e, vp->v_inode_nr, vp->v_index, position,
	rw_flag, usr, m_in.buffer, num_of_bytes, &new_pos, &cum_io_incr);

      if (r >= 0)
      {
	if (ex64hi(new_pos))
		panic(__FILE__, "read_write: bad new pos", NO_NUM);

	position = new_pos;
	cum_io += cum_io_incr;
      }
  }

  /* On write, update file size and access time. */
  if (rw_flag == WRITING) {
      if (regular || mode_word == I_DIRECTORY) {
          if (cmp64ul(position, vp->v_size) > 0)
	  {
		if (ex64hi(position) != 0)
		{
			panic(__FILE__,
				"read_write: file size too big for v_size",
				NO_NUM);
		}
		vp->v_size = ex64lo(position);
	  }
      }
  }

  f->filp_pos = position;

  if (r == OK) {
      return cum_io;
  }

  return r;
}


/*===========================================================================*
 *				do_getdents				     *
 *===========================================================================*/
PUBLIC int do_getdents()
{
/* Perform the getdents(fd, buf, size) system call. */
  int r;
  off_t pos_change;
  cp_grant_id_t gid;
  register struct filp *rfilp;

  /* Is the file descriptor valid? */
  if ( (rfilp = get_filp(m_in.fd)) == NIL_FILP) {
	  return(err_code);
  }
  
  if (!(rfilp->filp_mode & R_BIT))
	return EBADF;

  if ((rfilp->filp_vno->v_mode & I_TYPE) != I_DIRECTORY)
	return EBADF;

  gid=cpf_grant_magic(rfilp->filp_vno->v_fs_e, who_e, (vir_bytes) m_in.buffer,
	m_in.nbytes, CPF_WRITE);
  if (gid < 0) panic(__FILE__, "cpf_grant_magic failed", gid);

  /* Issue request */
  if (ex64hi(rfilp->filp_pos) != 0)
	panic(__FILE__, "do_getdents: should handle large offsets", NO_NUM);
	
  r= req_getdents(rfilp->filp_vno->v_fs_e, rfilp->filp_vno->v_inode_nr, 
	ex64lo(rfilp->filp_pos), gid, m_in.nbytes, &pos_change);

  cpf_revoke(gid);

  if (r > 0)
	rfilp->filp_pos= add64ul(rfilp->filp_pos, pos_change);
  return r;
}


/*===========================================================================*
 *				rw_pipe					     *
 *===========================================================================*/
PUBLIC int rw_pipe(rw_flag, usr, fd_nr, f, buf, req_size)
int rw_flag;			/* READING or WRITING */
endpoint_t usr;
int fd_nr;
struct filp *f;
char *buf;
size_t req_size;
{
  int r, oflags, op, partial_pipe;
  size_t size, cum_io, cum_io_incr;
  struct vnode *vp;
  u64_t position, new_pos;

  oflags = f->filp_flags;

  vp = f->filp_vno;
  position = cvu64((rw_flag == READING) ? vp->v_pipe_rd_pos :
	vp->v_pipe_wr_pos);

#if 0
  printf("vfs:rw_pipe: filp 0x%x pipe %s, buf 0x%x, size %d\n",
	f, rw_flag == READING ? "read" : "write", buf, req_size);
  printf("vfs:rw_pipe: pipe vp 0x%x, dev/num 0x%x/%d size %d, pos 0x%x:%08x\n",
  	vp, vp->v_dev, vp->v_inode_nr, 
  	vp->v_size, ex64hi(position), ex64lo(position));
#endif

  /* fp->fp_cum_io_partial is only nonzero when doing partial writes */
  cum_io = fp->fp_cum_io_partial; 

  op = (rw_flag == READING ? VFS_DEV_READ : VFS_DEV_WRITE);

  r = Xpipe_check(vp, rw_flag, oflags, req_size, position, 0);
  if (r <= 0)
  {
  	if (r == SUSPEND)
		pipe_suspend(rw_flag, fd_nr, buf, req_size);
  	return(r);
  }

  size = r;
  if (r < req_size)
	partial_pipe = 1;
  else 
  	partial_pipe = 0;

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
  /* Issue request */
  r = req_readwrite(vp->v_fs_e, vp->v_inode_nr, vp->v_index, position,
	rw_flag, usr, buf, size, &new_pos, &cum_io_incr);

  if (r >= 0)
  {
	if (ex64hi(new_pos))
		panic(__FILE__, "read_write: bad new pos", NO_NUM);

	position = new_pos;
	cum_io += cum_io_incr;
	buf += cum_io_incr;
	req_size -= cum_io_incr;
  }

  /* On write, update file size and access time. */
  if (rw_flag == WRITING) {
	if (cmp64ul(position, vp->v_size) > 0)
	{
		if (ex64hi(position) != 0)
		{
			panic(__FILE__,
				"read_write: file size too big for v_size",
				NO_NUM);
		}
		vp->v_size = ex64lo(position);
	}
  }
  else {
	if (cmp64ul(position, vp->v_size) >= 0) {
		/* Reset pipe pointers */
#if 0
		printf("vfs:rw_pipe: resetting pipe size/positions\n");
#endif
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
	return cum_io;
  }

  return r;
}



