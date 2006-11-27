/* This file contains the procedures for creating, opening, closing, and
 * seeking on files.
 *
 * The entry points into this file are
 *   do_creat:	perform the CREAT system call
 *   do_open:	perform the OPEN system call
 *   do_mknod:	perform the MKNOD system call
 *   do_mkdir:	perform the MKDIR system call
 *   do_close:	perform the CLOSE system call
 *   do_lseek:  perform the LSEEK system call
 *
 * Changes for VFS:
 *   Jul 2006 (Balazs Gerofi)
 */

#include "fs.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/u64.h>
#include "file.h"
#include "fproc.h"
#include "lock.h"
#include "param.h"
#include <dirent.h>

#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"

#define offset_lo	m2_l1
#define offset_high	m2_l2
 

FORWARD _PROTOTYPE( int common_open, (int oflags, mode_t omode)		);
FORWARD _PROTOTYPE( int pipe_open, (struct vnode *vp,mode_t bits,int oflags));

/*===========================================================================*
 *				do_creat				     *
 *===========================================================================*/
PUBLIC int do_creat()
{
/* Perform the creat(name, mode) system call. */
  int r;

  if (fetch_name(m_in.name, m_in.name_length, M3) != OK) return(err_code);
  r = common_open(O_WRONLY | O_CREAT | O_TRUNC, (mode_t) m_in.mode);
  return(r);
}

/*===========================================================================*
 *				do_open					     *
 *===========================================================================*/
PUBLIC int do_open()
{
/* Perform the open(name, flags,...) system call. */
  int create_mode = 0;		/* is really mode_t but this gives problems */
  int r;

  /* If O_CREAT is set, open has three parameters, otherwise two. */
  if (m_in.mode & O_CREAT) {
	create_mode = m_in.c_mode;	
	r = fetch_name(m_in.c_name, m_in.name1_length, M1);
  } else {
	r = fetch_name(m_in.name, m_in.name_length, M3);
  }

  if (r != OK) {
	  return(err_code); /* name was bad */
  }
  r = common_open(m_in.mode, create_mode);
  return(r);
}

/*===========================================================================*
 *				common_open				     *
 *===========================================================================*/
PRIVATE int common_open(register int oflags, mode_t omode)
{
/* Common code from do_creat and do_open. */
  int r, b, found;
  dev_t dev;
  mode_t bits;
  off_t pos;
  struct dmap *dp;
  struct filp *fil_ptr, *filp2;
  struct vnode *vp, *vp2;
  struct vmnt *vmp;
  char lastc[NAME_MAX];
  int m;

  /* Request and response structures */
  struct lookup_req lookup_req;
  struct open_req req;
  struct node_details res;

  /* Remap the bottom two bits of oflags. */
  m = oflags & O_ACCMODE;
  switch(m) {
	case O_RDONLY:	bits = R_BIT; break;
	case O_WRONLY:	bits = W_BIT; break;
	case O_RDWR:	bits = R_BIT | W_BIT; break;
	default:	return EINVAL;
  }

  /* See if file descriptor and filp slots are available. */
  if ((r = get_fd(0, bits, &m_in.fd, &fil_ptr)) != OK) return(r);

  /* See if a free vnode is available */
  if ((vp = get_free_vnode()) == NIL_VNODE) {
      printf("VFS: no vnode available!\n");
      return err_code;
  }

  /* If O_CREATE, set umask */ 
  if (oflags & O_CREAT) {
        omode = I_REGULAR | (omode & ALL_MODES & fp->fp_umask);
  }

  /* Fill in lookup request fields */
  lookup_req.path = user_fullpath;
  lookup_req.lastc = lastc;
  lookup_req.flags = oflags&O_CREAT ? (oflags&O_EXCL ? LAST_DIR : 
              LAST_DIR_EATSYM) : EAT_PATH;
  
  /* Request lookup */
  if ((r = lookup(&lookup_req, &res)) != OK) return r;

  /* Lookup was okay, fill in request fields for 
   * the actual open request. */
  req.inode_nr = res.inode_nr;
  req.fs_e = res.fs_e;
  req.oflags = oflags;
  req.omode = omode;
  req.lastc = lastc;
  req.uid = fp->fp_effuid;
  req.gid = fp->fp_effgid;

  /* Issue request */
  if ((r = req_open(&req, &res)) != OK) return r;
  
  /* Check whether the vnode is already in use */
  if ((vp2 = find_vnode(res.fs_e, res.inode_nr)) != NIL_VNODE) {
      vp = vp2;
      vp->v_size = res.fsize;; /* In case of trunc... */
      vp->v_count++;
  }
  /* Otherwise use the free one */
  else {
      vp->v_fs_e = res.fs_e;
      if ( (vmp = find_vmnt(vp->v_fs_e)) == NIL_VMNT)
          printf("VFS: vmnt not found by open()");
      
      vp->v_dev = vmp->m_dev;
      vp->v_inode_nr = res.inode_nr;
      vp->v_mode = res.fmode;
      vp->v_size = res.fsize;
      vp->v_sdev = res.dev; 
      vp->v_count = 1;
      vp->v_vmnt = vmp; 
      vp->v_index = res.inode_index;
  }

  /* Claim the file descriptor and filp slot and fill them in. */
  fp->fp_filp[m_in.fd] = fil_ptr;
  FD_SET(m_in.fd, &fp->fp_filp_inuse);
  fil_ptr->filp_count = 1;
  fil_ptr->filp_flags = oflags;
  fil_ptr->filp_vno = vp;
  
  switch (vp->v_mode & I_TYPE) {
      case I_CHAR_SPECIAL:
          /* Invoke the driver for special processing. */
          r = dev_open(vp->v_sdev, who_e, bits | (oflags & ~O_ACCMODE));
          break;

      case I_BLOCK_SPECIAL:
          /* Invoke the driver for special processing. */
          r = dev_open(vp->v_sdev, who_e, bits | (oflags & ~O_ACCMODE));
          
          /* Check whether the device is mounted or not */
          found = 0;
          if (r == OK) {
printf("VFS: opening block spec %d, handled by ", vp->v_sdev); 
              for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp) {
                  if (vmp->m_dev == vp->v_sdev) {
                      found = 1;
                      break;
                  }
              }
             
              /* Who is going to be responsible for this device? */
              if (found) {
printf("the FS of the mounted partition...\n"); 
                  vp->v_bfs_e = vmp->m_fs_e;
                  vp->v_blocksize - vmp->m_block_size;
              }
              else { /* To be handled in the root FS proc if not mounted */ 
printf("the root FS...\n");
                  vp->v_bfs_e = ROOT_FS_E;
                  vp->v_blocksize = _MIN_BLOCK_SIZE;
              }
              
              /* Get the driver endpoint of the block spec device */
              dp = &dmap[(vp->v_sdev >> MAJOR) & BYTE];
              if (dp->dmap_driver == NONE) {
                  printf("VFSblock_spec_open: driver not found for device %d\n", 
                          vp->v_sdev);
                  r = EINVAL;
                  break;
              }

              /* Send the driver endpoint (even if it is known already...) */
              if ((r = req_newdriver(vp->v_bfs_e, vp->v_sdev, dp->dmap_driver))
                      != OK) {
                  printf("VFSblock_spec_open: error sending driver endpoint\n");
              }
          }
          break;

      case I_NAMED_PIPE:
	  vp->v_pipe = I_PIPE;
          oflags |= O_APPEND;	/* force append mode */
          fil_ptr->filp_flags = oflags;
          r = pipe_open(vp, bits, oflags);
          if (r != ENXIO) {
              /* See if someone else is doing a rd or wt on
               * the FIFO.  If so, use its filp entry so the
               * file position will be automatically shared.
               */
              b = (bits & R_BIT ? R_BIT : W_BIT);
              fil_ptr->filp_count = 0; /* don't find self */
              if ((filp2 = find_filp(vp, b)) != NIL_FILP) {
                  /* Co-reader or writer found. Use it.*/
                  fp->fp_filp[m_in.fd] = filp2;
                  filp2->filp_count++;
		  filp2->filp_vno = vp;
                  filp2->filp_flags = oflags;

                  /* v_count was incremented after the vnode has 
                   * been found, i_count was incremented incorrectly
                   * by eatpath in FS, not knowing that we were going to 
                   * use an existing filp entry.  Correct this error.
                   */
                  put_vnode(vp);
	      } else {
                  /* Nobody else found.  Restore filp. */
                  fil_ptr->filp_count = 1;
		  if (fil_ptr->filp_mode == R_BIT)
			fil_ptr->filp_pos = cvul64(vp->v_pipe_rd_pos);
		  else
			fil_ptr->filp_pos = cvul64(vp->v_pipe_wr_pos);
              }
          }
          break;
  }

  /* If error, release inode. */
  if (r != OK) {
	if (r == SUSPEND) return(r);		/* Oops, just suspended */
	fp->fp_filp[m_in.fd] = NIL_FILP;
  	FD_CLR(m_in.fd, &fp->fp_filp_inuse);
	fil_ptr->filp_count= 0;
	put_vnode(vp);     
	fil_ptr->filp_vno = NIL_VNODE;
	return(r);
  }
  
  return(m_in.fd);
}



/*===========================================================================*
 *				pipe_open				     *
 *===========================================================================*/
PRIVATE int pipe_open(register struct vnode *vp, register mode_t bits,
	register int oflags)
{
/*  This function is called from common_open. It checks if
 *  there is at least one reader/writer pair for the pipe, if not
 *  it suspends the caller, otherwise it revives all other blocked
 *  processes hanging on the pipe.
 */

  vp->v_pipe = I_PIPE; 

  if((bits & (R_BIT|W_BIT)) == (R_BIT|W_BIT)) {
	printf("pipe opened RW.\n");
	return ENXIO;
  }

  if (find_filp(vp, bits & W_BIT ? R_BIT : W_BIT) == NIL_FILP) { 
	if (oflags & O_NONBLOCK) {
		if (bits & W_BIT) return(ENXIO);
	} else {
		suspend(XPOPEN);	/* suspend caller */
		return(SUSPEND);
	}
  } else if (susp_count > 0) {/* revive blocked processes */
	release(vp, OPEN, susp_count);
	release(vp, CREAT, susp_count);
  }
  return(OK);
}




/*===========================================================================*
 *				do_mknod				     *
 *===========================================================================*/
PUBLIC int do_mknod()
{
/* Perform the mknod(name, mode, addr) system call. */
  register mode_t bits, mode_bits;
  char lastc[NAME_MAX];         /* last component of the path */
  struct mknod_req req;
  struct lookup_req lookup_req;
  struct node_details res;
  int r;

  /* Only the super_user may make nodes other than fifos. */
  mode_bits = (mode_t) m_in.mk_mode;		/* mode of the inode */
  if (!super_user && ((mode_bits & I_TYPE) != I_NAMED_PIPE)) return(EPERM);
  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  bits = (mode_bits & I_TYPE) | (mode_bits & ALL_MODES & fp->fp_umask);
  
  /* Fill in lookup request fields */
  lookup_req.path = user_fullpath;
  lookup_req.lastc = lastc;
  lookup_req.flags = LAST_DIR;
        
  /* Request lookup */
  if ((r = lookup(&lookup_req, &res)) != OK) return r;

  /* Lookup was okay, fill in request fields for the actual
   * mknod request. */
  req.fs_e = res.fs_e;
  req.inode_nr = res.inode_nr;
  req.rmode = bits;
  req.dev = m_in.mk_z0;
  req.lastc = lastc;
  req.uid = fp->fp_effuid;
  req.gid = fp->fp_effgid;
                                          
  /* Issue request */
  return req_mknod(&req);
}


/*===========================================================================*
 *				do_mkdir				     *
 *===========================================================================*/
PUBLIC int do_mkdir()
{
/* Perform the mkdir(name, mode) system call. */
  mode_t bits;			/* mode bits for the new inode */
  char lastc[NAME_MAX];
  struct mkdir_req req;
  struct lookup_req lookup_req;
  struct node_details res;
  int r;

/*printf("VFS: mkdir() START:");*/
  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);

  bits = I_DIRECTORY | (m_in.mode & RWX_MODES & fp->fp_umask);

  /* Fill in lookup request fields */
  lookup_req.path = user_fullpath;
  lookup_req.lastc = lastc;
  lookup_req.flags = LAST_DIR;
        
  /* Request lookup */
  if ((r = lookup(&lookup_req, &res)) != OK) return r;
  
  /* Lookup was okay, fill in request message fields 
   * for the actual mknod request. */
  req.fs_e = res.fs_e;
  req.d_inode_nr = res.inode_nr;
  req.rmode = bits;
  req.lastc = lastc;
  req.uid = fp->fp_effuid;
  req.gid = fp->fp_effgid;
                                          
  /* Issue request */
  return req_mkdir(&req);
}




/*===========================================================================*
 *				do_lseek				     *
 *===========================================================================*/
PUBLIC int do_lseek()
{
/* Perform the lseek(ls_fd, offset, whence) system call. */
  register struct filp *rfilp;
  int r;
  long offset;
  u64_t pos, newpos;
  struct node_req req;

  /* Check to see if the file descriptor is valid. */
  if ( (rfilp = get_filp(m_in.ls_fd)) == NIL_FILP) return(err_code);

  /* No lseek on pipes. */
  if (rfilp->filp_vno->v_pipe == I_PIPE) return(ESPIPE);

  /* The value of 'whence' determines the start position to use. */
  switch(m_in.whence) {
      case SEEK_SET: pos = cvu64(0);	break;
      case SEEK_CUR: pos = rfilp->filp_pos;	break;
      case SEEK_END: pos = cvul64(rfilp->filp_vno->v_size);	break;
      default: return(EINVAL);
  }

  offset= m_in.offset_lo;
  if (offset >= 0)
	newpos= add64ul(pos, offset);
  else
	newpos= sub64ul(pos, -offset);

  /* Check for overflow. */
  if (ex64hi(newpos) != 0)
	return EINVAL;

  if (cmp64(newpos, rfilp->filp_pos) != 0) { /* Inhibit read ahead request */
      /* Fill in request message */
      req.fs_e = rfilp->filp_vno->v_fs_e;
      req.inode_nr = rfilp->filp_vno->v_inode_nr;

      /* Issue request */
      if ((r = req_inhibread(&req)) != OK) return r;
  }

  rfilp->filp_pos = newpos;

  /* insert the new position into the output message */
  m_out.reply_l1 = ex64lo(newpos);

  return(OK);
}


/*===========================================================================*
 *				do_llseek				     *
 *===========================================================================*/
PUBLIC int do_llseek()
{
/* Perform the llseek(ls_fd, offset, whence) system call. */
  register struct filp *rfilp;
  u64_t pos, newpos;
  struct node_req req;
  int r;

  /* Check to see if the file descriptor is valid. */
  if ( (rfilp = get_filp(m_in.ls_fd)) == NIL_FILP) return(err_code);

  /* No lseek on pipes. */
  if (rfilp->filp_vno->v_pipe == I_PIPE) return(ESPIPE);

  /* The value of 'whence' determines the start position to use. */
  switch(m_in.whence) {
      case SEEK_SET: pos = cvu64(0);	break;
      case SEEK_CUR: pos = rfilp->filp_pos;	break;
      case SEEK_END: pos = cvul64(rfilp->filp_vno->v_size);	break;
      default: return(EINVAL);
  }

  newpos= add64(pos, make64(m_in.offset_lo, m_in.offset_high));

  /* Check for overflow. */
  if (((long)m_in.offset_high > 0) && cmp64(newpos, pos) < 0) 
      return(EINVAL);
  if (((long)m_in.offset_high < 0) && cmp64(newpos, pos) > 0) 
      return(EINVAL);

  if (cmp64(newpos, rfilp->filp_pos) != 0) { /* Inhibit read ahead request */
      /* Fill in request message */
      req.fs_e = rfilp->filp_vno->v_fs_e;
      req.inode_nr = rfilp->filp_vno->v_inode_nr;

      /* Issue request */
      if ((r = req_inhibread(&req)) != OK) return r;
  }

  rfilp->filp_pos = newpos;
  m_out.reply_l1 = ex64lo(newpos);
  m_out.reply_l2 = ex64hi(newpos);
  return(OK);
}


/*===========================================================================*
 *				do_close				     *
 *===========================================================================*/
PUBLIC int do_close()
{
/* Perform the close(fd) system call. */
  return close_fd(fp, m_in.fd);
}


/*===========================================================================*
 *				close_fd				     *
 *===========================================================================*/
PUBLIC int close_fd(rfp, fd_nr)
struct fproc *rfp;
int fd_nr;
{
/* Perform the close(fd) system call. */
  register struct filp *rfilp;
  register struct vnode *vp;
  struct file_lock *flp;
  int rw, mode_word, lock_count;
  dev_t dev;

  /* First locate the vnode that belongs to the file descriptor. */
  if ( (rfilp = get_filp2(rfp, fd_nr)) == NIL_FILP) return(err_code);

  vp = rfilp->filp_vno;

  if (rfilp->filp_count - 1 == 0 && rfilp->filp_mode != FILP_CLOSED) {
	/* Check to see if the file is special. */
	mode_word = vp->v_mode & I_TYPE;
	if (mode_word == I_CHAR_SPECIAL || mode_word == I_BLOCK_SPECIAL) {
		dev = (dev_t) vp->v_sdev;
		if (mode_word == I_BLOCK_SPECIAL)  {
printf("VFSclose: closing block spec 0x%x\n", dev);		
			if (vp->v_bfs_e == ROOT_FS_E)
			{
				/* Invalidate the cache unless the special is
				 * mounted. Assume that the root filesystem's
				 * is open only for fsck.
			 	 */
printf("VFSclose: flushing block spec 0x%x\n", dev);		
          			req_flush(vp->v_bfs_e, dev);
			}
		}
		/* Do any special processing on device close. */
		dev_close(dev);
	}
  }

  /* If the inode being closed is a pipe, release everyone hanging on it. */
  if (vp->v_pipe == I_PIPE) {
	rw = (rfilp->filp_mode & R_BIT ? WRITE : READ);
	release(vp, rw, NR_PROCS);
  }

  /* If a write has been done, the inode is already marked as DIRTY. */
  if (--rfilp->filp_count == 0) {
	if (vp->v_pipe == I_PIPE && vp->v_count > 1) {
		/* Save the file position in the v-node in case needed later.
		 * The read and write positions are saved separately.  
		 */
		if (rfilp->filp_mode == R_BIT)
			vp->v_pipe_rd_pos = ex64lo(rfilp->filp_pos);
		else
			vp->v_pipe_wr_pos = ex64lo(rfilp->filp_pos);
		
	}
	else {
		/* Otherwise zero the pipe position fields */
		vp->v_pipe_rd_pos = 0;
		vp->v_pipe_wr_pos = 0;
	}
		
	put_vnode(rfilp->filp_vno);
  }

  FD_CLR(fd_nr, &rfp->fp_cloexec_set);
  rfp->fp_filp[fd_nr] = NIL_FILP;
  FD_CLR(fd_nr, &rfp->fp_filp_inuse);

  /* Check to see if the file is locked.  If so, release all locks. */
  if (nr_locks == 0) return(OK);
  lock_count = nr_locks;	/* save count of locks */
  for (flp = &file_lock[0]; flp < &file_lock[NR_LOCKS]; flp++) {
	if (flp->lock_type == 0) continue;	/* slot not in use */
	if (flp->lock_vnode == vp && flp->lock_pid == rfp->fp_pid) {
		flp->lock_type = 0;
		nr_locks--;
	}
  }
  if (nr_locks < lock_count) lock_revive();	/* lock released */
  return(OK);
}


