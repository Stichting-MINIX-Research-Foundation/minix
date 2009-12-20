/* This file handles the LINK and UNLINK system calls.  It also deals with
 * deallocating the storage used by a file when the last UNLINK is done to a
 * file and the blocks must be returned to the free block pool.
 *
 * The entry points into this file are
 *   do_link:         perform the LINK system call
 *   do_unlink:	      perform the UNLINK and RMDIR system calls
 *   do_rename:	      perform the RENAME system call
 *   do_truncate:     perform the TRUNCATE system call
 *   do_ftruncate:    perform the FTRUNCATE system call
 *   do_rdlink:       perform the RDLNK system call
 */

#include "fs.h"
#include <sys/stat.h>
#include <string.h>
#include <minix/com.h>
#include <minix/callnr.h>
#include <dirent.h>
#include "file.h"
#include "fproc.h"
#include "param.h"
#include <minix/vfsif.h>
#include "vnode.h"

/*===========================================================================*
 *				do_link					     *
 *===========================================================================*/
PUBLIC int do_link()
{
/* Perform the link(name1, name2) system call. */
  int r = OK;
  endpoint_t linked_fs_e, link_lastdir_fs_e;
  struct vnode *vp, *vp_d;

  /* See if 'name1' (file to be linked to) exists. */ 
  if(fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  if ((vp = eat_path(PATH_NOFLAGS)) == NIL_VNODE) return(err_code);

  /* Does the final directory of 'name2' exist? */
  if (fetch_name(m_in.name2, m_in.name2_length, M1) != OK) 
	r = err_code;
  else if ((vp_d = last_dir()) == NIL_VNODE)
	r = err_code; 
  if (r != OK) {
	  put_vnode(vp);
	  return(r);
  }

  /* Check for links across devices. */
  if(vp->v_fs_e != vp_d->v_fs_e)
  	r = EXDEV;
  else 
	r = forbidden(vp_d, W_BIT | X_BIT);

  if (r == OK)
	r = req_link(vp->v_fs_e, vp_d->v_inode_nr, user_fullpath,
		     vp->v_inode_nr);

  put_vnode(vp);
  put_vnode(vp_d);
  return(r);
}


/*===========================================================================*
 *				do_unlink				     *
 *===========================================================================*/
PUBLIC int do_unlink()
{
/* Perform the unlink(name) or rmdir(name) system call. The code for these two
 * is almost the same.  They differ only in some condition testing.  Unlink()
 * may be used by the superuser to do dangerous things; rmdir() may not.
 */
  register struct fproc *rfp;
  struct vnode *vldirp, *vp;
  int r;
  
  /* Get the last directory in the path. */
  if(fetch_name(m_in.name, m_in.name_length, M3) != OK) return(err_code);
  if ((vldirp = last_dir()) == NIL_VNODE) return(err_code);

  /* Make sure that the object is a directory */
  if((vldirp->v_mode & I_TYPE) != I_DIRECTORY) {
	  put_vnode(vldirp);
	  return(ENOTDIR);
  }

  /* The caller must have both search and execute permission */
  if ((r = forbidden(vldirp, X_BIT | W_BIT)) != OK) {
	put_vnode(vldirp);
	return(r);
  }
  
  /* Also, if the sticky bit is set, only the owner of the file or a privileged
     user is allowed to unlink */
  if (vldirp->v_mode & S_ISVTX == S_ISVTX) {
	/* Look up inode of file to unlink to retrieve owner */
	vp = advance(vldirp, PATH_RET_SYMLINK);
	if (vp != NIL_VNODE) {
		if(vp->v_uid != fp->fp_effuid && fp->fp_effuid != SU_UID) 
			r = EPERM;
		put_vnode(vp);
	} else
		r = err_code;
	if (r != OK) {
		put_vnode(vldirp);
		return(r);
	}
  }
  
  if(call_nr == UNLINK) 
	  r = req_unlink(vldirp->v_fs_e, vldirp->v_inode_nr, user_fullpath);
  else 
	  r = req_rmdir(vldirp->v_fs_e, vldirp->v_inode_nr, user_fullpath);
  
  put_vnode(vldirp);
  return(r);
}


/*===========================================================================*
 *				do_rename				     *
 *===========================================================================*/
PUBLIC int do_rename()
{
/* Perform the rename(name1, name2) system call. */
  int r = OK, r1;
  size_t len;
  struct vnode *old_dirp, *new_dirp, *vp;
  char old_name[PATH_MAX+1];
  
  /* See if 'name1' (existing file) exists.  Get dir and file inodes. */
  if(fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  if ((old_dirp = last_dir()) == NIL_VNODE) return(err_code);

  /* If the sticky bit is set, only the owner of the file or a privileged
     user is allowed to rename */
  if(old_dirp->v_mode & S_ISVTX == S_ISVTX) {
	/* Look up inode of file to unlink to retrieve owner */
	vp = advance(old_dirp, PATH_RET_SYMLINK);
	if (vp != NIL_VNODE) {
		if(vp->v_uid != fp->fp_effuid && fp->fp_effuid != SU_UID) 
			r = EPERM;
		put_vnode(vp);
	} else
		r = err_code;
	if (r != OK) {	
		put_vnode(old_dirp);
		return(r);
	}
  }
  
  /* Save the last component of the old name */
  if(strlen(user_fullpath) >= sizeof(old_name)) {
	put_vnode(old_dirp);
	return(ENAMETOOLONG);
  }
  strcpy(old_name, user_fullpath);
  
  /* See if 'name2' (new name) exists.  Get dir inode */
  if(fetch_name(m_in.name2, m_in.name2_length, M1) != OK) 
	r = err_code;
  else if ((new_dirp = last_dir()) == NIL_VNODE)
	r = err_code; 
  if (r != OK) {
  	put_vnode(old_dirp);
  	return r;
  }

  /* Both parent directories must be on the same device. */
  if(old_dirp->v_fs_e != new_dirp->v_fs_e) r = EXDEV; 

  /* Parent dirs must be writable, searchable and on a writable device */
  if ((r1 = forbidden(old_dirp, W_BIT|X_BIT)) != OK ||
      (r1 = forbidden(new_dirp, W_BIT|X_BIT)) != OK) r = r1;
  
  if(r == OK)
	  r = req_rename(old_dirp->v_fs_e, old_dirp->v_inode_nr, old_name,
  			 new_dirp->v_inode_nr, user_fullpath);
  put_vnode(old_dirp);
  put_vnode(new_dirp);
  return(r);
}
  

/*===========================================================================*
 *				do_truncate				     *
 *===========================================================================*/
PUBLIC int do_truncate()
{
/* truncate_vnode() does the actual work of do_truncate() and do_ftruncate().
 * do_truncate() and do_ftruncate() have to get hold of the inode, either
 * by name or fd, do checks on it, and call truncate_inode() to do the
 * work.
 */
  struct vnode *vp;
  int r;

  /* Temporarily open file */
  if (fetch_name(m_in.m2_p1, m_in.m2_i1, M1) != OK) return(err_code);
  if ((vp = eat_path(PATH_NOFLAGS)) == NIL_VNODE) return(err_code);
  
  /* Ask FS to truncate the file */
  if ((r = forbidden(vp, W_BIT)) == OK)
  	r = truncate_vnode(vp, m_in.flength);
  
  put_vnode(vp);
  return(r);
}


/*===========================================================================*
 *				do_ftruncate				     *
 *===========================================================================*/
PUBLIC int do_ftruncate()
{
/* As with do_truncate(), truncate_vnode() does the actual work. */
  int r;
  struct filp *rfilp;
  
  /* File is already opened; get a vnode pointer from filp */
  if ((rfilp = get_filp(m_in.m2_i1)) == NIL_FILP) return(err_code);
  if (!(rfilp->filp_mode & W_BIT)) return(EBADF);
  return truncate_vnode(rfilp->filp_vno, m_in.m2_l1);
}


/*===========================================================================*
 *				truncate_vnode				     *
 *===========================================================================*/
PUBLIC int truncate_vnode(vp, newsize)
struct vnode *vp;
off_t newsize;
{
  int r, file_type;

  file_type = vp->v_mode & I_TYPE;
  if (file_type != I_REGULAR && file_type != I_NAMED_PIPE) return(EINVAL);
        
  if ((r = req_ftrunc(vp->v_fs_e, vp->v_inode_nr, newsize, 0)) == OK)
	vp->v_size = newsize;

  return(r);
}


/*===========================================================================*
 *                             do_slink					     *
 *===========================================================================*/
PUBLIC int do_slink()
{
/* Perform the symlink(name1, name2) system call. */
  int r, linklen;
  struct vnode *vp;
  char string[NAME_MAX];       /* last component of the new dir's path name */

  if(m_in.name1_length <= 1) return(ENOENT);
  if(m_in.name1_length >= SYMLINK_MAX) return(ENAMETOOLONG);
  
  /* Get dir inode of 'name2' */
  if(fetch_name(m_in.name2, m_in.name2_length, M1) != OK) return(err_code);
  if ((vp = last_dir()) == NIL_VNODE) return(err_code);

  if ((r = forbidden(vp, W_BIT|X_BIT)) == OK) {
	r = req_slink(vp->v_fs_e, vp->v_inode_nr, user_fullpath, who_e,
		      m_in.name1, m_in.name1_length - 1, fp->fp_effuid,
		      fp->fp_effgid);
  }

  put_vnode(vp);
  return(r);
}


/*===========================================================================*
 *                             do_rdlink                                    *
 *===========================================================================*/
PUBLIC int do_rdlink()
{
/* Perform the readlink(name, buf, bufsize) system call. */
  int r, copylen;
  struct vnode *vp;
  
  copylen = m_in.nbytes;
  if(copylen < 0) return(EINVAL);

  /* Temporarily open the file containing the symbolic link */
  if(fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  if ((vp = eat_path(PATH_RET_SYMLINK)) == NIL_VNODE) return(err_code);

  /* Make sure this is a symbolic link */
  if((vp->v_mode & I_TYPE) != I_SYMBOLIC_LINK) 
	r = EINVAL;
  else
	r = req_rdlink(vp->v_fs_e, vp->v_inode_nr, who_e, m_in.name2, copylen);

  put_vnode(vp);
  return(r);
}


