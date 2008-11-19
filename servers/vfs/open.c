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
#include <assert.h>

#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"

#define offset_lo	m2_l1
#define offset_high	m2_l2
 
FORWARD _PROTOTYPE( int common_open, (int oflags, mode_t omode)		);
FORWARD _PROTOTYPE( int create_open, (_mnx_Mode_t omode, int excl,
				struct vnode **vpp, int *created)	);
FORWARD _PROTOTYPE( int exists_open, (struct vnode *vp, _mnx_Mode_t bits,
	int oflags));
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
  int b, m, r, created, found;
  mode_t bits;
  struct filp *fil_ptr, *filp2;
  struct vnode *vp;
  struct vmnt *vmp;
  struct dmap *dp;

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

  /* If O_CREATE, set umask */ 
  if (oflags & O_CREAT) {
        omode = I_REGULAR | (omode & ALL_MODES & fp->fp_umask);
  }

  if (oflags & O_CREAT)
	r= create_open(omode, !!(oflags & O_EXCL), &vp, &created);
  else
  {
#if 0
	printf("vfs:common_open: path '%s'\n", user_fullpath);
#endif
	created= FALSE;
	r= lookup_vp(0 /*flags*/, 0 /*!use_realuid*/, &vp);
  }
  if (r != OK)
	return r;

  if (!created)
  {
	r= exists_open(vp, bits, oflags);
	if (r != OK)
	{
		put_vnode(vp);
		return r;
	}
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
	  if (r == SUSPEND)
		suspend(XDOPEN);	/* suspend caller */
          break;

      case I_BLOCK_SPECIAL:
          /* Invoke the driver for special processing. */
          r = dev_open(vp->v_sdev, who_e, bits | (oflags & ~O_ACCMODE));
	  if (r != OK)
		break;
          
          /* Check whether the device is mounted or not */
          found = 0;
	  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp) {
	      if (vmp->m_dev == vp->v_sdev) {
		  found = 1;
		  break;
	      }
	  }
	 
	  /* Who is going to be responsible for this device? */
	  if (found) {
	      vp->v_bfs_e = vmp->m_fs_e;
	  }
	  else { /* To be handled in the root FS proc if not mounted */ 
	      vp->v_bfs_e = ROOT_FS_E;
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
	      assert(fil_ptr->filp_count == 1);
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
                  /* Nobody else found.  Claim filp. */
                  fil_ptr->filp_count = 1;
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
 *				create_open				     *
 *===========================================================================*/
PRIVATE int create_open(omode, excl, vpp, created)
mode_t omode;
int excl;
struct vnode **vpp;
int *created;
{
	int i, r, r1;
	size_t len;
	struct vnode *vp, *dir_vp, *new_vp, *start_vp;

	struct node_details res;
	char lastc[PATH_MAX+1];

	start_vp = (user_fullpath[0] == '/' ? fp->fp_rd : fp->fp_wd);
	dup_vnode(start_vp);

	for (i= 0; i<SYMLOOP_MAX; i++)
	{
#if 0
		printf("vfs:create_open: path #%d '%s'\n", i+1, user_fullpath);
#endif
		r= lookup_lastdir_rel(start_vp, 0 /*!use_realuid*/, &dir_vp);
		put_vnode(start_vp);
		if (r != OK)
			return r;

		/* Save the last component of the path */
		len= strlen(user_fullpath)+1;
		if (len > sizeof(lastc))
		{
			put_vnode(dir_vp);
			return ENAMETOOLONG;
		}
		memcpy(lastc, user_fullpath, len);

		/* Get a free vnode */
		new_vp = get_free_vnode(__FILE__, __LINE__);
		if (new_vp == NIL_VNODE) {
			put_vnode(dir_vp);
			printf("vfs:create_open: no free vnode available\n");
			return EINVAL;
		}

		r= forbidden(dir_vp, W_BIT|X_BIT, 0 /*!use_realuid*/);
		if (r == OK)
		{
			/* Try to create the file */
			r= req_create(dir_vp->v_fs_e, dir_vp->v_inode_nr,
				omode, fp->fp_effuid, fp->fp_effgid, lastc,
				&res);
		}

		if (r != EEXIST && r != EACCES)
		{
			put_vnode(dir_vp);

			if (r != OK)
				return r;

			/* Check whether vnode is already in use or not */
			vp = find_vnode(res.fs_e, res.inode_nr);
			if (vp != NIL_VNODE) {
				vp->v_ref_count++;
				vp->v_fs_count++;
			}
			else
			{
				vp= new_vp;

				/* Fill in the free vnode's fields */
				vp->v_fs_e = res.fs_e;
				vp->v_inode_nr = res.inode_nr;
				vp->v_mode = res.fmode;
				vp->v_size = res.fsize;
				vp->v_uid = res.uid;
				vp->v_gid = res.gid;
				vp->v_sdev = res.dev;

				vp->v_vmnt = dir_vp->v_vmnt; 
				vp->v_dev = vp->v_vmnt->m_dev;
				vp->v_fs_count = 1;
				vp->v_ref_count = 1;
			}

			*vpp= vp;
			*created= TRUE;

			return OK;
			
		}

		if (r == EEXIST && excl)
		{
			printf(
		"vfs:create_open: creating existing file with O_EXCL\n");
			put_vnode(dir_vp);
			return r;
		}

		/* Try a regular lookup */
		memcpy(user_fullpath, lastc, len);
		r1= lookup_rel_vp(dir_vp, 0 /*flags*/, 0 /*!use_realuid*/, &vp);
		if (r1 != ENOENT)
		{
			put_vnode(dir_vp);
			if (r1 == OK)
			{
				*vpp= vp;
				*created= FALSE;
			}

			return r1;
		}
		if (r == EACCES)
		{
			/* We cannot create a new file and the file does not
			 * already exist.
			 */
			put_vnode(dir_vp);
			return r;
		}

		/* The create failed with EEXIST and the regular lookup
		 * failed with ENOENT. We have to see whether the object
		 * we try to access actually exists, but is a symlink that 
		 * cannot be resolved. If the symlink exists, we start 
		 * with the contents of the symlink.
		 */
		memcpy(user_fullpath, lastc, len);
		r= lookup_rel_vp(dir_vp, PATH_RET_SYMLINK, 0 /*!use_realuid*/,
			&vp);
		if (r != OK)
		{
			put_vnode(dir_vp);
			return r;
		}

		if (!S_ISLNK(vp->v_mode))
		{
			/* Strange, we got an object, but it is not a symlink.
			 * Well, just return the object.
			 */
			put_vnode(dir_vp);
			*vpp= vp;
			*created= FALSE;
			return OK;
		}

		/* Get the contents of the link */
		len= sizeof(user_fullpath);
		r= req_rdlink(vp->v_fs_e, vp->v_inode_nr, FS_PROC_NR, 
			(vir_bytes)user_fullpath, len-1);
		put_vnode(vp);
		if (r < 0)
		{
			printf("vfs:create_open: req_rdlink failed with %d\n",
				r);
			put_vnode(dir_vp);
			return r;
		}
		if (r >= len)
		{
			printf(
			"vfs:create_open: got bad length %d from req_rdlink\n",
				r);
			r= len-1;
		}
		user_fullpath[r]= '\0';

		printf("got link target '%s'\n", user_fullpath);
		if (user_fullpath[0] == '/')
		{
			put_vnode(dir_vp);
			start_vp= fp->fp_rd;
			dup_vnode(start_vp);
		}
		else
			start_vp= dir_vp;
	}

	put_vnode(start_vp);
	return ELOOP;
}


/*===========================================================================*
 *				exists_open 				     *
 *===========================================================================*/
PRIVATE int exists_open(vp, bits, oflags /*, omode, lastc, vpp */)
struct vnode *vp;
mode_t bits;
int oflags;
{
  int r;

  /* Check protections. */
  if ((r = forbidden(vp, bits, 0 /*!use_realuid*/)) != OK)
	return r;

  /* Opening reg. files directories and special files differ. */
  switch (vp->v_mode & I_TYPE) {
  case I_REGULAR: 
	/* Truncate regular file if O_TRUNC. */
	if (oflags & O_TRUNC) {
		if ((r = forbidden(vp, W_BIT, 0 /*!use_realuid*/)) !=OK) break;
		truncate_vn(vp, 0);
	}
	break;

  case I_DIRECTORY: 
	/* Directories may be read but not written. */
	r = (bits & W_BIT ? EISDIR : OK);
	break;

  case I_CHAR_SPECIAL:
  case I_BLOCK_SPECIAL:
	if (vp->v_sdev == (dev_t)-1)
		panic(__FILE__, "vfs:exists_open: bad special", NO_NUM);
	break;

  case I_NAMED_PIPE:
#if 0
	printf("vfs:exists_open: fifo vp 0x%x, for %s\n",
		vp, ((bits & W_BIT) ? "writing" : "reading"));
#endif
	if (vp->v_ref_count == 1)
	{
		vp->v_pipe_rd_pos= 0;
		vp->v_pipe_wr_pos= 0;
		if (vp->v_size != 0)
			r= truncate_vn(vp, 0);
	}
	break;
  }

  return(r);
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
  int r;
  struct vnode *vp;

  /* Only the super_user may make nodes other than fifos. */
  mode_bits = (mode_t) m_in.mk_mode;		/* mode of the inode */
  if (!super_user && ((mode_bits & I_TYPE) != I_NAMED_PIPE)) return(EPERM);
  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  bits = (mode_bits & I_TYPE) | (mode_bits & ALL_MODES & fp->fp_umask);
  
  /* Request lookup */
  if ((r = lookup_lastdir(0 /*!use_realuid*/, &vp)) != OK) return r;

  /* Make sure that the object is a directory */
  if ((vp->v_mode & I_TYPE) != I_DIRECTORY)
  {
	put_vnode(vp);
	return ENOTDIR;
  }

  r= forbidden(vp, W_BIT|X_BIT, 0 /*!use_realuid*/);
  if (r != OK)
  {
	put_vnode(vp);
	return r;
  }

  /* Issue request */
  r= req_mknod(vp->v_fs_e, vp->v_inode_nr, user_fullpath,
	fp->fp_effuid, fp->fp_effgid, bits, m_in.mk_z0);
  put_vnode(vp);
  return r;
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

  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);

  bits = I_DIRECTORY | (m_in.mode & RWX_MODES & fp->fp_umask);

  /* Request lookup */
  if ((r = lookup_lastdir(0 /*!use_realuid*/, &vp)) != OK) return r;

  /* Make sure that the object is a directory */
  if ((vp->v_mode & I_TYPE) != I_DIRECTORY)
  {
	put_vnode(vp);
	return ENOTDIR;
  }

  r= forbidden(vp, W_BIT|X_BIT, 0 /*!use_realuid*/);
  if (r != OK)
  {
	put_vnode(vp);
	return r;
  }
  
  /* Issue request */
  r= req_mkdir(vp->v_fs_e, vp->v_inode_nr, user_fullpath,
	fp->fp_effuid, fp->fp_effgid, bits);
  put_vnode(vp);
  return r;
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
      r = req_inhibread(rfilp->filp_vno->v_fs_e, rfilp->filp_vno->v_inode_nr);
      if (r != OK) return r;
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
      r = req_inhibread(rfilp->filp_vno->v_fs_e, rfilp->filp_vno->v_inode_nr);
      if (r != OK) return r;
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
  if ( (rfilp = get_filp2(rfp, fd_nr)) == NIL_FILP) return(err_code);
  vp = rfilp->filp_vno;
  close_filp(rfilp);

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


/*===========================================================================*
 *				close_filp				     *
 *===========================================================================*/
PUBLIC void close_filp(fp)
struct filp *fp;
{
  int mode_word, rw;
  dev_t dev;
  struct vnode *vp;

  vp = fp->filp_vno;
  if (fp->filp_count - 1 == 0 && fp->filp_mode != FILP_CLOSED) {
	/* Check to see if the file is special. */
	mode_word = vp->v_mode & I_TYPE;
	if (mode_word == I_CHAR_SPECIAL || mode_word == I_BLOCK_SPECIAL) {
		dev = (dev_t) vp->v_sdev;
		if (mode_word == I_BLOCK_SPECIAL)  {
			if (vp->v_bfs_e == ROOT_FS_E)
			{
				/* Invalidate the cache unless the special is
				 * mounted. Assume that the root filesystem's
				 * is open only for fsck.
			 	 */
          			req_flush(vp->v_bfs_e, dev);
			}
		}
		/* Do any special processing on device close. */
		(void) dev_close(dev, fp-filp);

		/* Ignore any errors, even SUSPEND. */
	}
  }

  /* If the inode being closed is a pipe, release everyone hanging on it. */
  if (vp->v_pipe == I_PIPE) {
	rw = (fp->filp_mode & R_BIT ? WRITE : READ);
	release(vp, rw, NR_PROCS);
  }

  /* If a write has been done, the inode is already marked as DIRTY. */
  if (--fp->filp_count == 0) {
	if (vp->v_pipe == I_PIPE) {
		/* Last reader or writer is going. Tell MFS about latest
		 * pipe size.
		 */
		truncate_vn(vp, vp->v_size);
	}
		
	put_vnode(fp->filp_vno);
  }

}

/*===========================================================================*
 *				close_reply				     *
 *===========================================================================*/
PUBLIC void close_reply()
{
	/* No need to do anything */
}


/*===========================================================================*
 *				do_vm_open				     *
 *===========================================================================*/
PUBLIC int do_vm_open()
{
	int len, r, n;
	endpoint_t ep;

	len = m_in.VMVO_NAME_LENGTH;
	m_out.VMV_ENDPOINT = ep = m_in.VMVO_ENDPOINT;

	/* Do open() call on behalf of any process, performed by VM. */ 
	if(len < 2 || len > sizeof(user_fullpath)) {
		printf("do_vm_open: strange length %d\n", len);
		m_out.VMVRO_FD = EINVAL;
		return VM_VFS_REPLY_OPEN;
	}

	/* Do open on behalf of which process? */
	if(isokendpt(ep, &n) != OK) {
		printf("do_vm_open: strange endpoint %d\n", ep);
		m_out.VMVRO_FD = EINVAL;
		return VM_VFS_REPLY_OPEN;
	}

	/* XXX - do open on behalf of this process */
	fp = &fproc[n];

	/* Get path name from VM address space. */
	if((r=sys_safecopyfrom(VM_PROC_NR, m_in.VMVO_NAME_GRANT, 0,
		(vir_bytes) user_fullpath, len, D)) != OK) {
		printf("do_vm_open: sys_safecopyfrom failed: %d\n", r);
		m_out.VMVRO_FD = EPERM;
		return VM_VFS_REPLY_OPEN;
	}

	/* Check if path is null-terminated. */
	if(user_fullpath[len-1] != '\0') {
		printf("do_vm_open: name (len %d) not 0-terminated\n", len);
		m_out.VMVRO_FD = EINVAL;
		return VM_VFS_REPLY_OPEN;
	}

	/* Perform open(). */
	m_out.VMVRO_FD = common_open(m_in.VMVO_FLAGS, m_in.VMVO_MODE);
	m_out.VMV_ENDPOINT = ep;

	/* Send open() reply. */
	return VM_VFS_REPLY_OPEN;
}

/*===========================================================================*
 *				do_vm_close				     *
 *===========================================================================*/
PUBLIC int do_vm_close()
{
	int len, r, n;
	endpoint_t ep;

	len = m_in.VMVO_NAME_LENGTH;

	/* Do close() call on behalf of any process, performed by VM. */ 
	m_out.VMV_ENDPOINT = ep = m_in.VMVC_ENDPOINT;
	if(isokendpt(ep, &n) != OK) {
		printf("do_vm_close: strange endpoint %d\n", ep);
		return VM_VFS_REPLY_CLOSE;
	}

	/* Perform close(). */
	r = close_fd(&fproc[n], m_in.VMVC_FD);

	return VM_VFS_REPLY_CLOSE;
}

