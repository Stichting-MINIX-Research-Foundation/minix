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
#include <minix/vfsif.h>
#include <dirent.h>
#include <assert.h>
#include "file.h"
#include "fproc.h"
#include "path.h"
#include "vnode.h"
#include "param.h"

/*===========================================================================*
 *				do_link					     *
 *===========================================================================*/
PUBLIC int do_link()
{
/* Perform the link(name1, name2) system call. */
  int r = OK;
  struct vnode *vp = NULL, *dirp = NULL;
  struct vmnt *vmp1 = NULL, *vmp2 = NULL;
  char fullpath[PATH_MAX];
  struct lookup resolve;

  lookup_init(&resolve, fullpath, PATH_NOFLAGS, &vmp1, &vp);
  resolve.l_vmnt_lock = VMNT_WRITE;
  resolve.l_vnode_lock = VNODE_READ;

  /* See if 'name1' (file to be linked to) exists. */
  if (fetch_name(m_in.name1, m_in.name1_length, M1, fullpath) != OK)
	return(err_code);
  if ((vp = eat_path(&resolve, fp)) == NULL) return(err_code);

  /* Does the final directory of 'name2' exist? */
  lookup_init(&resolve, fullpath, PATH_NOFLAGS, &vmp2, &dirp);
  resolve.l_vmnt_lock = VMNT_READ;
  resolve.l_vnode_lock = VNODE_READ;
  if (fetch_name(m_in.name2, m_in.name2_length, M1, fullpath) != OK)
	r = err_code;
  else if ((dirp = last_dir(&resolve, fp)) == NULL)
	r = err_code;

  if (r != OK) {
	unlock_vnode(vp);
	unlock_vmnt(vmp1);
	put_vnode(vp);
	return(r);
  }

  /* Check for links across devices. */
  if (vp->v_fs_e != dirp->v_fs_e)
	r = EXDEV;
  else
	r = forbidden(dirp, W_BIT | X_BIT);

  if (r == OK)
	r = req_link(vp->v_fs_e, dirp->v_inode_nr, fullpath,
		     vp->v_inode_nr);

  unlock_vnode(vp);
  unlock_vnode(dirp);
  if (vmp2 != NULL) unlock_vmnt(vmp2);
  unlock_vmnt(vmp1);
  put_vnode(vp);
  put_vnode(dirp);
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
  struct vnode *dirp, *vp;
  struct vmnt *vmp, *vmp2;
  int r;
  char fullpath[PATH_MAX];
  struct lookup resolve;

  lookup_init(&resolve, fullpath, PATH_NOFLAGS, &vmp, &dirp);
  resolve.l_vmnt_lock = VMNT_WRITE;
  resolve.l_vnode_lock = VNODE_READ;

  /* Get the last directory in the path. */
  if (fetch_name(m_in.name, m_in.name_length, M3, fullpath) != OK)
	return(err_code);

  if ((dirp = last_dir(&resolve, fp)) == NULL) return(err_code);

  /* Make sure that the object is a directory */
  if ((dirp->v_mode & I_TYPE) != I_DIRECTORY) {
	unlock_vnode(dirp);
	unlock_vmnt(vmp);
	put_vnode(dirp);
	return(ENOTDIR);
  }

  /* The caller must have both search and execute permission */
  if ((r = forbidden(dirp, X_BIT | W_BIT)) != OK) {
	unlock_vnode(dirp);
	unlock_vmnt(vmp);
	put_vnode(dirp);
	return(r);
  }

  /* Also, if the sticky bit is set, only the owner of the file or a privileged
     user is allowed to unlink */
  if ((dirp->v_mode & S_ISVTX) == S_ISVTX) {
	/* Look up inode of file to unlink to retrieve owner */
	resolve.l_flags = PATH_RET_SYMLINK;
	resolve.l_vmp = &vmp2;	/* Shouldn't actually get locked */
	resolve.l_vmnt_lock = VMNT_READ;
	resolve.l_vnode = &vp;
	resolve.l_vnode_lock = VNODE_READ;
	vp = advance(dirp, &resolve, fp);
	assert(vmp2 == NULL);
	if (vp != NULL) {
		if (vp->v_uid != fp->fp_effuid && fp->fp_effuid != SU_UID)
			r = EPERM;
		unlock_vnode(vp);
		put_vnode(vp);
	} else
		r = err_code;
	if (r != OK) {
		unlock_vnode(dirp);
		unlock_vmnt(vmp);
		put_vnode(dirp);
		return(r);
	}
  }

  tll_upgrade(&vmp->m_lock);

  if(call_nr == UNLINK)
	  r = req_unlink(dirp->v_fs_e, dirp->v_inode_nr, fullpath);
  else
	  r = req_rmdir(dirp->v_fs_e, dirp->v_inode_nr, fullpath);
  unlock_vnode(dirp);
  unlock_vmnt(vmp);
  put_vnode(dirp);
  return(r);
}

/*===========================================================================*
 *				do_rename				     *
 *===========================================================================*/
PUBLIC int do_rename()
{
/* Perform the rename(name1, name2) system call. */
  int r = OK, r1;
  struct vnode *old_dirp, *new_dirp = NULL, *vp;
  struct vmnt *oldvmp, *newvmp, *vmp2;
  char old_name[PATH_MAX];
  char fullpath[PATH_MAX];
  struct lookup resolve;

  lookup_init(&resolve, fullpath, PATH_NOFLAGS, &oldvmp, &old_dirp);
  /* Do not yet request exclusive lock on vmnt to prevent deadlocks later on */
  resolve.l_vmnt_lock = VMNT_WRITE;
  resolve.l_vnode_lock = VNODE_READ;

  /* See if 'name1' (existing file) exists.  Get dir and file inodes. */
  if (fetch_name(m_in.name1, m_in.name1_length, M1, fullpath) != OK)
	return(err_code);
  if ((old_dirp = last_dir(&resolve, fp)) == NULL)
	return(err_code);

  /* If the sticky bit is set, only the owner of the file or a privileged
     user is allowed to rename */
  if ((old_dirp->v_mode & S_ISVTX) == S_ISVTX) {
	/* Look up inode of file to unlink to retrieve owner */
	resolve.l_flags = PATH_RET_SYMLINK;
	resolve.l_vmp = &vmp2;	/* Shouldn't actually get locked */
	resolve.l_vmnt_lock = VMNT_READ;
	resolve.l_vnode = &vp;
	resolve.l_vnode_lock = VNODE_READ;
	resolve.l_flags = PATH_RET_SYMLINK;
	vp = advance(old_dirp, &resolve, fp);
	assert(vmp2 == NULL);
	if (vp != NULL) {
		if(vp->v_uid != fp->fp_effuid && fp->fp_effuid != SU_UID)
			r = EPERM;
		unlock_vnode(vp);
		put_vnode(vp);
	} else
		r = err_code;
	if (r != OK) {
		unlock_vnode(old_dirp);
		unlock_vmnt(oldvmp);
		put_vnode(old_dirp);
		return(r);
	}
  }

  /* Save the last component of the old name */
  if(strlen(fullpath) >= sizeof(old_name)) {
	unlock_vnode(old_dirp);
	unlock_vmnt(oldvmp);
	put_vnode(old_dirp);
	return(ENAMETOOLONG);
  }
  strcpy(old_name, fullpath);

  /* See if 'name2' (new name) exists.  Get dir inode */
  lookup_init(&resolve, fullpath, PATH_NOFLAGS, &newvmp, &new_dirp);
  resolve.l_vmnt_lock = VMNT_READ;
  resolve.l_vnode_lock = VNODE_READ;
  if (fetch_name(m_in.name2, m_in.name2_length, M1, fullpath) != OK)
	r = err_code;
  else if ((new_dirp = last_dir(&resolve, fp)) == NULL)
	r = err_code;

  if (r != OK) {
	unlock_vnode(old_dirp);
	unlock_vmnt(oldvmp);
	put_vnode(old_dirp);
	return(r);
  }

  /* Both parent directories must be on the same device. */
  if (old_dirp->v_fs_e != new_dirp->v_fs_e) r = EXDEV;

  /* Parent dirs must be writable, searchable and on a writable device */
  if ((r1 = forbidden(old_dirp, W_BIT|X_BIT)) != OK ||
      (r1 = forbidden(new_dirp, W_BIT|X_BIT)) != OK) r = r1;

  if (r == OK) {
	tll_upgrade(&oldvmp->m_lock); /* Upgrade to exclusive access */
	r = req_rename(old_dirp->v_fs_e, old_dirp->v_inode_nr, old_name,
		       new_dirp->v_inode_nr, fullpath);
  }
  unlock_vnode(old_dirp);
  unlock_vnode(new_dirp);
  unlock_vmnt(oldvmp);
  if (newvmp) unlock_vmnt(newvmp);

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
  struct vmnt *vmp;
  int r;
  char fullpath[PATH_MAX];
  struct lookup resolve;

  lookup_init(&resolve, fullpath, PATH_NOFLAGS, &vmp, &vp);
  resolve.l_vmnt_lock = VMNT_EXCL;
  resolve.l_vnode_lock = VNODE_WRITE;

  if ((off_t) m_in.flength < 0) return(EINVAL);

  /* Temporarily open file */
  if (fetch_name(m_in.m2_p1, m_in.m2_i1, M1, fullpath) != OK) return(err_code);
  if ((vp = eat_path(&resolve, fp)) == NULL) return(err_code);

  /* Ask FS to truncate the file */
  if ((r = forbidden(vp, W_BIT)) == OK)
	r = truncate_vnode(vp, m_in.flength);

  unlock_vnode(vp);
  unlock_vmnt(vmp);
  put_vnode(vp);
  return(r);
}

/*===========================================================================*
 *				do_ftruncate				     *
 *===========================================================================*/
PUBLIC int do_ftruncate()
{
/* As with do_truncate(), truncate_vnode() does the actual work. */
  struct filp *rfilp;
  int r;

  if ((off_t) m_in.flength < 0) return(EINVAL);

  /* File is already opened; get a vnode pointer from filp */
  if ((rfilp = get_filp(m_in.m2_i1, VNODE_WRITE)) == NULL) return(err_code);

  if (!(rfilp->filp_mode & W_BIT))
	r = EBADF;
  else
	r = truncate_vnode(rfilp->filp_vno, m_in.flength);

  unlock_filp(rfilp);
  return(r);
}


/*===========================================================================*
 *				truncate_vnode				     *
 *===========================================================================*/
PUBLIC int truncate_vnode(vp, newsize)
struct vnode *vp;
off_t newsize;
{
/* Truncate a regular file or a pipe */
  int r, file_type;

  assert(tll_locked_by_me(&vp->v_lock));
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
  int r;
  struct vnode *vp;
  struct vmnt *vmp;
  char fullpath[PATH_MAX];
  struct lookup resolve;

  lookup_init(&resolve, fullpath, PATH_NOFLAGS, &vmp, &vp);
  resolve.l_vmnt_lock = VMNT_WRITE;
  resolve.l_vnode_lock = VNODE_READ;

  if (m_in.name1_length <= 1) return(ENOENT);
  if (m_in.name1_length >= SYMLINK_MAX) return(ENAMETOOLONG);

  /* Get dir inode of 'name2' */
  if (fetch_name(m_in.name2, m_in.name2_length, M1, fullpath) != OK)
	return(err_code);

  if ((vp = last_dir(&resolve, fp)) == NULL) return(err_code);

  if ((r = forbidden(vp, W_BIT|X_BIT)) == OK) {
	r = req_slink(vp->v_fs_e, vp->v_inode_nr, fullpath, who_e,
		      m_in.name1, m_in.name1_length - 1, fp->fp_effuid,
		      fp->fp_effgid);
  }

  unlock_vnode(vp);
  unlock_vmnt(vmp);
  put_vnode(vp);

  return(r);
}

/*===========================================================================*
 *                              rdlink_direct                                *
 *===========================================================================*/
PUBLIC int rdlink_direct(orig_path, link_path, rfp)
char *orig_path;
char link_path[PATH_MAX]; /* should have length PATH_MAX */
struct fproc *rfp;
{
/* Perform a readlink()-like call from within the VFS */
  int r;
  struct vnode *vp;
  struct vmnt *vmp;
  struct lookup resolve;

  lookup_init(&resolve, link_path, PATH_RET_SYMLINK, &vmp, &vp);
  resolve.l_vmnt_lock = VMNT_READ;
  resolve.l_vnode_lock = VNODE_READ;

  /* Temporarily open the file containing the symbolic link. Use link_path
   * for temporary storage to keep orig_path untouched. */
  strncpy(link_path, orig_path, PATH_MAX);	/* PATH_MAX includes '\0' */
  link_path[PATH_MAX - 1] = '\0';
  if ((vp = eat_path(&resolve, rfp)) == NULL) return(err_code);

  /* Make sure this is a symbolic link */
  if ((vp->v_mode & I_TYPE) != I_SYMBOLIC_LINK)
	r = EINVAL;
  else
	r = req_rdlink(vp->v_fs_e, vp->v_inode_nr, NONE, link_path,
		       PATH_MAX - 1, 1);

  if (r > 0) link_path[r] = '\0';	/* Terminate string when succesful */

  unlock_vnode(vp);
  unlock_vmnt(vmp);
  put_vnode(vp);

  return r;
}

/*===========================================================================*
 *                             do_rdlink				     *
 *===========================================================================*/
PUBLIC int do_rdlink()
{
/* Perform the readlink(name, buf, bufsize) system call. */
  int r, copylen;
  struct vnode *vp;
  struct vmnt *vmp;
  char fullpath[PATH_MAX];
  struct lookup resolve;

  lookup_init(&resolve, fullpath, PATH_RET_SYMLINK, &vmp, &vp);
  resolve.l_vmnt_lock = VMNT_READ;
  resolve.l_vnode_lock = VNODE_READ;

  copylen = m_in.nbytes;
  if (copylen < 0) return(EINVAL);

  /* Temporarily open the file containing the symbolic link */
  if (fetch_name(m_in.name1, m_in.name1_length, M1, fullpath) != OK)
	return(err_code);
  if ((vp = eat_path(&resolve, fp)) == NULL) return(err_code);

  /* Make sure this is a symbolic link */
  if ((vp->v_mode & I_TYPE) != I_SYMBOLIC_LINK)
	r = EINVAL;
  else
	r = req_rdlink(vp->v_fs_e, vp->v_inode_nr, who_e, m_in.name2,
		       copylen, 0);

  unlock_vnode(vp);
  unlock_vmnt(vmp);
  put_vnode(vp);

  return(r);
}
