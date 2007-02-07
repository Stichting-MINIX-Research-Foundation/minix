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
  u64_t position;
  unsigned int off, cum_io;
  int op, oflags, r, chunk, usr, seg, block_spec, char_spec;
  int regular, partial_pipe = 0, partial_cnt = 0;
  mode_t mode_word;
  struct filp *wf;
  phys_bytes p;
  struct dmap *dp;

  /* Request and response structures */
  struct readwrite_req req;
  struct readwrite_res res;

  /* For block spec files */
  struct breadwrite_req breq;

  /* PM loads segments by putting funny things in other bits of the
   * message, indicated by a high bit in fd. */
  if (who_e == PM_PROC_NR && (m_in.fd & _PM_SEG_FLAG)) {
      seg = (int) m_in.m1_p2;
      usr = (int) m_in.m1_p3;
      m_in.fd &= ~(_PM_SEG_FLAG);	/* get rid of flag bit */
  } 
  else {
      usr = who_e;		/* normal case */
      seg = D;
  }

  /* If the file descriptor is valid, get the vnode, size and mode. */
  if (m_in.nbytes < 0) return(EINVAL);
  if ((f = get_filp(m_in.fd)) == NIL_FILP) return(err_code);
  if (((f->filp_mode) & (rw_flag == READING ? R_BIT : W_BIT)) == 0) {
      return(f->filp_mode == FILP_CLOSED ? EIO : EBADF);
  }

  if (m_in.nbytes == 0)
      return(0);	/* so char special files need not check for 0*/

  /* check if user process has the memory it needs.
   * if not, copying will fail later.
   * do this after 0-check above because umap doesn't want to map 0 bytes. */
  if ((r = sys_umap(usr, seg, (vir_bytes) m_in.buffer, m_in.nbytes, &p)) != OK)
  {
      printf("VFS: read_write: umap failed for process %d\n", usr);
      return r;
  }

  position = f->filp_pos;
  oflags = f->filp_flags;

  vp = f->filp_vno;

  r = OK;
  if (vp->v_pipe == I_PIPE) {
      /* fp->fp_cum_io_partial is only nonzero when doing partial writes */
      cum_io = fp->fp_cum_io_partial; 
  } 
  else {
      cum_io = 0;
  }

  op = (rw_flag == READING ? VFS_DEV_READ : VFS_DEV_WRITE);
  mode_word = vp->v_mode & I_TYPE;
  regular = mode_word == I_REGULAR || mode_word == I_NAMED_PIPE;

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
      /*dev = (dev_t) f->filp_ino->i_zone[0];*/
      dev = (dev_t) vp->v_sdev;
      r = dev_io(op, dev, usr, m_in.buffer, position, m_in.nbytes, oflags);
      if (r >= 0) {
          cum_io = r;
          position = add64ul(position, r);
          r = OK;
      }
  } 
  /* Block special files. */
  else if (block_spec) {
      /* Fill in the fields of the request */
      breq.rw_flag = rw_flag;
      breq.fs_e = vp->v_bfs_e;
      breq.blocksize = vp->v_blocksize;
      breq.dev = vp->v_sdev;
      breq.user_e = usr;
      breq.pos = position;
      breq.num_of_bytes = m_in.nbytes;
      breq.user_addr = m_in.buffer;

      /* Issue request */
      r = req_breadwrite(&breq, &res);

      position = res.new_pos;
      cum_io += res.cum_io;
  }
  /* Regular files (and pipes) */
  else {
      if (rw_flag == WRITING && block_spec == 0) {
          /* Check for O_APPEND flag. */
          if (oflags & O_APPEND) position = cvul64(vp->v_size);

          /* Check in advance to see if file will grow too big. */
          if (cmp64ul(position, vp->v_vmnt->m_max_file_size - m_in.nbytes) > 0)
              return(EFBIG);

      }

      /* Pipes are a little different. Check. */
      if (vp->v_pipe == I_PIPE) {
          r = pipe_check(vp, rw_flag, oflags,
                  m_in.nbytes, position, &partial_cnt, 0);
          if (r <= 0) return(r);
      }

      if (partial_cnt > 0) {
          /* So that we don't need to deal with partial count 
           * in the FS process.
	   */
          m_in.nbytes = MIN(m_in.nbytes, partial_cnt);
          partial_pipe = 1;
      }

      /* Fill in request structure */
      req.fs_e = vp->v_fs_e;
      req.rw_flag = rw_flag;
      req.inode_nr = vp->v_inode_nr;
      req.user_e = usr;
      req.seg = seg;
      req.pos = position;
      req.num_of_bytes = m_in.nbytes;
      req.user_addr = m_in.buffer;
      req.inode_index = vp->v_index;

      /* Truncate read request at size (mustn't do this for special files). */
      if((rw_flag == READING) &&
	cmp64ul(add64ul(position, req.num_of_bytes), vp->v_size) > 0) {
	/* Position always should fit in an off_t (LONG_MAX). */
	off_t pos32;
	assert(cmp64ul(position, LONG_MAX) <= 0);
	pos32 = cv64ul(position);
	assert(pos32 >= 0);
	assert(pos32 <= LONG_MAX);
	req.num_of_bytes = vp->v_size - pos32;
	assert(req.num_of_bytes >= 0);
      }

      /* Issue request */
      r = req_readwrite(&req, &res);

      if (r >= 0)
      {
	if (ex64hi(res.new_pos))
		panic(__FILE__, "read_write: bad new pos", NO_NUM);

	position = res.new_pos;
	cum_io += res.cum_io;
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
  else {
      if (vp->v_pipe == I_PIPE) {
          if (cmp64ul(position, vp->v_size) >= 0) {
              /* Reset pipe pointers */
              vp->v_size = 0;
              position = cvu64(0);
              wf = find_filp(vp, W_BIT);
              if (wf != NIL_FILP) wf->filp_pos = cvu64(0);
          }
      }
  }

  f->filp_pos = position;

  if (r == OK) {
      if (partial_pipe) {
          partial_pipe = 0;
          /* partial write on pipe with */
          /* O_NONBLOCK, return write count */
          if (!(oflags & O_NONBLOCK)) {
              fp->fp_cum_io_partial = cum_io;
              suspend(XPIPE);   /* partial write on pipe with */
              return(SUSPEND);  /* nbyte > PIPE_SIZE - non-atomic */
          }
      }
      fp->fp_cum_io_partial = 0;
      return cum_io;
  }

  return r;
}


/* Original "uncached" code for block spec files */
#if 0      
  else if (block_spec) {
        char buf[_MIN_BLOCK_SIZE];
        block_t b;
        int bleft = m_in.nbytes;
	dev_t dev = vp->v_sdev;
        
        b = position / _MIN_BLOCK_SIZE;
        off = position % _MIN_BLOCK_SIZE;
       
        while (bleft) {
            /* First read the whole block */
            r = dev_bio(VFS_DEV_READ, dev, FS_PROC_NR, buf,
		b * _MIN_BLOCK_SIZE, _MIN_BLOCK_SIZE);

            if (r != _MIN_BLOCK_SIZE)
                break;

            /* How many bytes to copy? */
            chunk = MIN(bleft, _MIN_BLOCK_SIZE - off);

            if (rw_flag == READING) {
                /* Copy a chunk from the buffer to user space. */
                r = sys_vircopy(FS_PROC_NR, D, (phys_bytes) (&buf[off]),
                        usr, seg, (phys_bytes) m_in.buffer,
                        (phys_bytes) chunk);
            } 
            else {
                /* Copy a chunk from user space to the buffer. */
                r = sys_vircopy(usr, seg, (phys_bytes) m_in.buffer,
                        FS_PROC_NR, D, (phys_bytes) (&buf[off]),
                        (phys_bytes) chunk);
            }

            /* Write back if WRITE */
            if (rw_flag == WRITING) {
                r = dev_bio(DEV_WRITE, dev, FS_PROC_NR, buf, 
			b * _MIN_BLOCK_SIZE, _MIN_BLOCK_SIZE);
                
                if (r != _MIN_BLOCK_SIZE)
                    break;
            }
           
            bleft -= chunk;
            m_in.buffer += chunk;
           
            /* 0 offset in the next block */
            b++;
            off = 0;
        } 
        
        cum_io = m_in.nbytes - bleft;
        position += cum_io;
	r = OK;
  }        
#endif        


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



