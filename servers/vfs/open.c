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
#include <assert.h>
#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"

PRIVATE char mode_map[] = {R_BIT, W_BIT, R_BIT|W_BIT, 0};

FORWARD _PROTOTYPE( int common_open, (int oflags, mode_t omode)		);
FORWARD _PROTOTYPE( struct vnode *new_node, (int oflags, mode_t bits)		);
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

  if (r != OK) return(err_code); /* name was bad */
  r = common_open(m_in.mode, create_mode);
  return(r);
}


/*===========================================================================*
 *				common_open				     *
 *===========================================================================*/
PRIVATE int common_open(register int oflags, mode_t omode)
{
/* Common code from do_creat and do_open. */
  int b, r, exist = TRUE;
  dev_t dev;
  mode_t bits;
  struct filp *fil_ptr, *filp2;
  struct vnode *vp;
  struct vmnt *vmp;
  struct dmap *dp;

  /* Remap the bottom two bits of oflags. */
  bits = (mode_t) mode_map[oflags & O_ACCMODE];
  if (!bits) return(EINVAL);

  /* See if file descriptor and filp slots are available. */
  if ((r = get_fd(0, bits, &m_in.fd, &fil_ptr)) != OK) return(r);

  /* If O_CREATE is set, try to make the file. */
  if (oflags & O_CREAT) {
        omode = I_REGULAR | (omode & ALL_MODES & fp->fp_umask);
	vp = new_node(oflags, omode);
	r = err_code;
	if (r == OK) exist = FALSE; /* We just created the file */
	else if (r != EEXIST) return(r);  /* other error */ 
	else exist = !(oflags & O_EXCL);  /* file exists, if the O_EXCL
					     flag is set this is an error */
  } else {
	/* Scan path name */
	if ((vp = eat_path(PATH_NOFLAGS, fp)) == NULL) return(err_code);
  }

  /* Claim the file descriptor and filp slot and fill them in. */
  fp->fp_filp[m_in.fd] = fil_ptr;
  FD_SET(m_in.fd, &fp->fp_filp_inuse);
  fil_ptr->filp_count = 1;
  fil_ptr->filp_vno = vp;
  fil_ptr->filp_flags = oflags;

  /* Only do the normal open code if we didn't just create the file. */
  if(exist) {
	/* Check protections. */
	if ((r = forbidden(vp, bits)) == OK) {
		/* Opening reg. files, directories, and special files differ */
		switch (vp->v_mode & I_TYPE) {
		   case I_REGULAR:
		   	/* Truncate regular file if O_TRUNC. */
		   	if (oflags & O_TRUNC) {
		   		if ((r = forbidden(vp, W_BIT)) != OK)
		   			break;
		   		truncate_vnode(vp, 0);
		   	}
		   	break;
		   case I_DIRECTORY:
		   	/* Directories may be read but not written. */
		   	r = (bits & W_BIT ? EISDIR : OK);
		   	break;
		   case I_CHAR_SPECIAL:
			/* Invoke the driver for special processing. */
			dev = (dev_t) vp->v_sdev;
			r = dev_open(dev, who_e, bits | (oflags & ~O_ACCMODE));
		   	if (r == SUSPEND) suspend(FP_BLOCKED_ON_DOPEN);
		   	break;
		   case I_BLOCK_SPECIAL:
			/* Invoke the driver for special processing. */
			dev = (dev_t) vp->v_sdev;
			r = dev_open(dev, who_e, bits | (oflags & ~O_ACCMODE));
			if (r != OK) break;

			/* Check whether the device is mounted or not. If so,
			   then that FS is responsible for this device. Else
			   we default to ROOT_FS. */
			vp->v_bfs_e = ROOT_FS_E; /* By default */
			for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp) 
				if (vmp->m_dev == vp->v_sdev) 
					vp->v_bfs_e = vmp->m_fs_e;

			/* Get the driver endpoint of the block spec device */
			dp = &dmap[(vp->v_sdev >> MAJOR) & BYTE];
			if (dp->dmap_driver == NONE) {
				printf("VFS: driver not found for device %d\n",
					vp->v_sdev);
				r = ENXIO;
				break;
			}

			/* Send the driver endpoint (even when known already)*/
			if ((r = req_newdriver(vp->v_bfs_e, vp->v_sdev,
					       dp->dmap_driver)) != OK) {
				printf("VFS: error sending driver endpoint\n");
				r = ENXIO;
			}
			break;

		   case I_NAMED_PIPE:
			/* Create a mapped inode on PFS which handles reads
			   and writes to this named pipe. */
			r = map_vnode(vp);
			if (r == OK) {
				vp->v_pipe = I_PIPE;
				if (vp->v_ref_count == 1) {
					vp->v_pipe_rd_pos = 0;
					vp->v_pipe_wr_pos = 0;
					if (vp->v_size != 0)
						r = truncate_vnode(vp, 0);
				}
				oflags |= O_APPEND;	/* force append mode */
				fil_ptr->filp_flags = oflags;
			}
			if (r == OK) {
				r = pipe_open(vp, bits, oflags);
			}
			if (r != ENXIO) {
				/* See if someone else is doing a rd or wt on
				 * the FIFO.  If so, use its filp entry so the
				 * file position will be automatically shared.
				 */
				b = (bits & R_BIT ? R_BIT : W_BIT);
				fil_ptr->filp_count = 0; /* don't find self */
				if ((filp2 = find_filp(vp, b)) != NULL) {
					/* Co-reader or writer found. Use it.*/
					fp->fp_filp[m_in.fd] = filp2;
					filp2->filp_count++;
					filp2->filp_vno = vp;
					filp2->filp_flags = oflags;

					/* v_count was incremented after the
					 * vnode has been found. i_count was
					 * incremented incorrectly in FS, not
					 * knowing that we were going to use an
					 * existing filp entry.  Correct this
					 * error.
					 */
					put_vnode(vp);
				} else {
					/* Nobody else found. Restore filp. */
					fil_ptr->filp_count = 1;
				}
			} 
			break;
		}
	}
  }

  /* If error, release inode. */
  if (r != OK) {
	if (r == SUSPEND) return(r);		/* Oops, just suspended */
	fp->fp_filp[m_in.fd] = NULL;
  	FD_CLR(m_in.fd, &fp->fp_filp_inuse);
	fil_ptr->filp_count= 0;
	put_vnode(vp);     
	fil_ptr->filp_vno = NULL;
	return(r);
  }
  
  return(m_in.fd);
}


/*===========================================================================*
 *				new_node				     *
 *===========================================================================*/
PRIVATE struct vnode *new_node(int oflags, mode_t bits)
{
/* Try to create a new inode and return a pointer to it. If the inode already
   exists, return a pointer to it as well, but set err_code accordingly.
   NULL is returned if the path cannot be resolved up to the last
   directory, or when the inode cannot be created due to permissions or
   otherwise. */ 
  struct vnode *dirp, *vp;
  int r, flags;
  struct node_details res;

  /* When O_CREAT and O_EXCL flags are set, the path may not be named by a
   * symbolic link. */
  flags = PATH_NOFLAGS;
  if (oflags & O_EXCL) flags |= PATH_RET_SYMLINK;

  /* See if the path can be opened down to the last directory. */
  if ((dirp = last_dir(fp)) == NULL) return(NULL);

  /* The final directory is accessible. Get final component of the path. */
  vp = advance(dirp, flags, fp);

  /* The combination of a symlink with absolute path followed by a danglink
   * symlink results in a new path that needs to be re-resolved entirely. */
  if (user_fullpath[0] == '/') return new_node(oflags, bits);

  if (vp == NULL && err_code == ENOENT) {
	/* Last path component does not exist. Make a new directory entry. */
	if ((vp = get_free_vnode()) == NULL) {
		/* Can't create new vnode: out of vnodes. */
		put_vnode(dirp);	
		return(NULL);
	}
	if ((r = forbidden(dirp, W_BIT|X_BIT)) != OK ||
	    (r = req_create(dirp->v_fs_e, dirp->v_inode_nr,bits, fp->fp_effuid,
			    fp->fp_effgid, user_fullpath, &res)) != OK ) {
		/* Can't create inode either due to permissions or some other
		 * problem. In case r is EEXIST, we might be dealing with a
		 * dangling symlink.*/
		if (r == EEXIST) {
			struct vnode *slp, *old_wd;

			/* Resolve path up to symlink */
			slp = advance(dirp, PATH_RET_SYMLINK, fp);
			if (slp != NULL) {
				if (S_ISLNK(slp->v_mode)) {
					/* Get contents of link */

					int max_linklen;
					max_linklen = sizeof(user_fullpath)-1;
					r = req_rdlink(slp->v_fs_e,
						       slp->v_inode_nr,
					   	       VFS_PROC_NR,
					   	       user_fullpath,
					   	       max_linklen, 0);
					if (r < 0) {
						/* Failed to read link */
						put_vnode(slp);
						put_vnode(dirp);
						err_code = r;
						return(NULL);
					}
					user_fullpath[r] = '\0';/* Term. path*/
				} 
				put_vnode(slp);
			}

			/* Try to create the inode the dangling symlink was
			 * pointing to. We have to use dirp as starting point
			 * as there might be multiple successive symlinks
			 * crossing multiple mountpoints. */
			old_wd = fp->fp_wd; /* Save orig. working dirp */
			fp->fp_wd = dirp;
			vp = new_node(oflags, bits);
			fp->fp_wd = old_wd; /* Restore */

			if (vp != NULL) {
				put_vnode(dirp);
				return(vp);
			}
			r = err_code;
		} 

		if (r == EEXIST) 
			err_code = EIO; /* Impossible, we have verified that
					 * the last component doesn't exist and
					 * is not a dangling symlink. */
		else
			err_code = r;

		put_vnode(dirp);
		return(NULL);
	}
	
	/* Store results and mark vnode in use */
	vp->v_fs_e = res.fs_e;
	vp->v_inode_nr = res.inode_nr;
	vp->v_mode = res.fmode;
	vp->v_size = res.fsize;
	vp->v_uid = res.uid;
	vp->v_gid = res.gid;
	vp->v_sdev = res.dev;
	vp->v_vmnt = dirp->v_vmnt;
	vp->v_dev = vp->v_vmnt->m_dev;
	vp->v_fs_count = 1;
	vp->v_ref_count = 1;
  } else {
  	/* Either last component exists, or there is some other problem. */
  	if (vp != NULL)
  		r = EEXIST;	/* File exists or a symlink names a file while
  				 * O_EXCL is set. */ 
  	else
		r = err_code;	/* Other problem. */
  }

  err_code = r;
  put_vnode(dirp);
  return(vp);
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

  if((bits & (R_BIT|W_BIT)) == (R_BIT|W_BIT)) return(ENXIO);

  if (find_filp(vp, bits & W_BIT ? R_BIT : W_BIT) == NULL) { 
	if (oflags & O_NONBLOCK) {
		if (bits & W_BIT) return(ENXIO);
	} else {
		suspend(FP_BLOCKED_ON_POPEN);	/* suspend caller */
		return(SUSPEND);
	}
  } else if (susp_count > 0) { /* revive blocked processes */
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
  int r;
  struct vnode *vp;
  
  /* Only the super_user may make nodes other than fifos. */
  mode_bits = (mode_t) m_in.mk_mode;		/* mode of the inode */
  if(!super_user && (((mode_bits & I_TYPE) != I_NAMED_PIPE) && ((mode_bits & I_TYPE) != I_UNIX_SOCKET))) return(EPERM);
  bits = (mode_bits & I_TYPE) | (mode_bits & ALL_MODES & fp->fp_umask);

  /* Open directory that's going to hold the new node. */
  if(fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  if((vp = last_dir(fp)) == NULL) return(err_code);

  /* Make sure that the object is a directory */
  if((vp->v_mode & I_TYPE) != I_DIRECTORY) {
	  put_vnode(vp);
	  return(ENOTDIR);
  }

  if ((r = forbidden(vp, W_BIT|X_BIT)) == OK) {
	r = req_mknod(vp->v_fs_e, vp->v_inode_nr, user_fullpath, fp->fp_effuid,
		      fp->fp_effgid, bits, m_in.mk_z0);
  }

  put_vnode(vp);
  return(r);
}


/*===========================================================================*
 *				do_mkdir				     *
 *===========================================================================*/
PUBLIC int do_mkdir()
{
/* Perform the mkdir(name, mode) system call. */
  mode_t bits;			/* mode bits for the new inode */
  int r;
  struct vnode *vp;

  if(fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);

  bits = I_DIRECTORY | (m_in.mode & RWX_MODES & fp->fp_umask);

  /* Request lookup */
  if((vp = last_dir(fp)) == NULL) return(err_code);

  /* Make sure that the object is a directory */
  if ((vp->v_mode & I_TYPE) != I_DIRECTORY) {
	  put_vnode(vp);
	  return(ENOTDIR);
  }

  if ((r = forbidden(vp, W_BIT|X_BIT)) == OK) {
	r = req_mkdir(vp->v_fs_e, vp->v_inode_nr, user_fullpath, fp->fp_effuid,
		      fp->fp_effgid, bits);
  }

  put_vnode(vp);
  return(r);
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

  /* Check to see if the file descriptor is valid. */
  if ( (rfilp = get_filp(m_in.ls_fd)) == NULL) return(err_code);

  /* No lseek on pipes. */
  if (rfilp->filp_vno->v_pipe == I_PIPE) return(ESPIPE);

  /* The value of 'whence' determines the start position to use. */
  switch(m_in.whence) {
      case SEEK_SET: pos = cvu64(0);	break;
      case SEEK_CUR: pos = rfilp->filp_pos;	break;
      case SEEK_END: pos = cvul64(rfilp->filp_vno->v_size);	break;
      default: return(EINVAL);
  }

  offset = m_in.offset_lo;
  if (offset >= 0)
	newpos= add64ul(pos, offset);
  else
	newpos= sub64ul(pos, -offset);

  /* Check for overflow. */
  if (ex64hi(newpos) != 0)
	return(EINVAL);

  if (cmp64(newpos, rfilp->filp_pos) != 0) { /* Inhibit read ahead request */
      r = req_inhibread(rfilp->filp_vno->v_fs_e, rfilp->filp_vno->v_inode_nr);
      if (r != OK) return(r);
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
  int r;

  /* Check to see if the file descriptor is valid. */
  if ( (rfilp = get_filp(m_in.ls_fd)) == NULL) return(err_code);

  /* No lseek on pipes. */
  if (rfilp->filp_vno->v_pipe == I_PIPE) return(ESPIPE);

  /* The value of 'whence' determines the start position to use. */
  switch(m_in.whence) {
      case SEEK_SET: pos = cvu64(0);	break;
      case SEEK_CUR: pos = rfilp->filp_pos;	break;
      case SEEK_END: pos = cvul64(rfilp->filp_vno->v_size);	break;
      default: return(EINVAL);
  }

  newpos = add64(pos, make64(m_in.offset_lo, m_in.offset_high));

  /* Check for overflow. */
  if (((long)m_in.offset_high > 0) && cmp64(newpos, pos) < 0) 
      return(EINVAL);
  if (((long)m_in.offset_high < 0) && cmp64(newpos, pos) > 0) 
      return(EINVAL);

  if (cmp64(newpos, rfilp->filp_pos) != 0) { /* Inhibit read ahead request */
      r = req_inhibread(rfilp->filp_vno->v_fs_e, rfilp->filp_vno->v_inode_nr);
      if (r != OK) return(r);
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
  int lock_count;

  /* First locate the vnode that belongs to the file descriptor. */
  if ( (rfilp = get_filp2(rfp, fd_nr)) == NULL) return(err_code);
  vp = rfilp->filp_vno;
  close_filp(rfilp);

  FD_CLR(fd_nr, &rfp->fp_cloexec_set);
  rfp->fp_filp[fd_nr] = NULL;
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

/*===========================================================================*
 *				close_reply				     *
 *===========================================================================*/
PUBLIC void close_reply()
{
	/* No need to do anything */
}

