/* This file contains the heart of the mechanism used to read (and write)
 * files.  Read and write requests are split up into chunks that do not cross
 * block boundaries.  Each chunk is then processed in turn.  Reads on special
 * files are also detected and handled.
 *
 * The entry points into this file are
 *   do_read:	 perform the READ system call by calling read_write
 *   read_write: actually do the work of READ and WRITE
 *
 * Changes for VFS:
 *   Jul 2006 (Balazs Gerofi)
 */

#include "fs.h"
#include <fcntl.h>
#include <unistd.h>
#include <minix/com.h>
#include "file.h"
#include "fproc.h"
#include "param.h"
#include <dirent.h>

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
  off_t bytes_left, f_size, position;
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
  if ((r = sys_umap(usr, seg, (vir_bytes) m_in.buffer, m_in.nbytes, &p)) != OK) {
      printf("VFS: read_write: umap failed for process %d\n", usr);
      return r;
  }
  position = f->filp_pos;
  oflags = f->filp_flags;

  vp = f->filp_vno;
  f_size = vp->v_size;

  r = OK;
  if (vp->v_pipe == I_PIPE) {
      /* fp->fp_cum_io_partial is only nonzero when doing partial writes */
      cum_io = fp->fp_cum_io_partial; 
  } 
  else {
      cum_io = 0;
  }

  op = (rw_flag == READING ? DEV_READ : DEV_WRITE);
  mode_word = vp->v_mode & I_TYPE;
  regular = mode_word == I_REGULAR || mode_word == I_NAMED_PIPE;

  if ((char_spec = (mode_word == I_CHAR_SPECIAL ? 1 : 0))) {
      if (vp->v_sdev == NO_DEV)
          panic(__FILE__,"read_write tries to read from "
                  "character device NO_DEV", NO_NUM);
  }

  if ((block_spec = (mode_word == I_BLOCK_SPECIAL ? 1 : 0))) {
      f_size = ULONG_MAX;
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
          position += r;
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
  /* Regular files */
  else {
      if (rw_flag == WRITING && block_spec == 0) {
          /* Check in advance to see if file will grow too big. */
          if (position > vp->v_vmnt->m_max_file_size - m_in.nbytes) 
              return(EFBIG);

          /* Check for O_APPEND flag. */
          if (oflags & O_APPEND) position = f_size;
      }

      /* Pipes are a little different. Check. */
      if (vp->v_pipe == I_PIPE) {
          r = pipe_check(vp, rw_flag, oflags,
                  m_in.nbytes, position, &partial_cnt, 0);
          if (r <= 0) return(r);
      }

      if (partial_cnt > 0) {
          /* So taht we don't need to deal with partial count 
           * in the FS process */
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

      /* Issue request */
      r = req_readwrite(&req, &res);

      position = res.new_pos;
      cum_io += res.cum_io;
  }

  /* On write, update file size and access time. */
  if (rw_flag == WRITING) {
      if (regular || mode_word == I_DIRECTORY) {
          if (position > f_size) vp->v_size = position;
      }
  }
  else {
      if (vp->v_pipe == I_PIPE) {
          if (position >= vp->v_size) {
              /* Reset pipe pointers */
              vp->v_size = 0;
              position = 0;
              wf = find_filp(vp, W_BIT);
              if (wf != NIL_FILP) wf->filp_pos = 0;
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
            r = dev_bio(DEV_READ, dev, FS_PROC_NR, buf, b * _MIN_BLOCK_SIZE, 
                    _MIN_BLOCK_SIZE);

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

