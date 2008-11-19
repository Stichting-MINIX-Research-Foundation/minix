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
 *
 * Changes for VFS:
 *   Jul 2006 (Balazs Gerofi)
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
  int r;
  endpoint_t linked_fs_e, link_lastdir_fs_e;
  struct vnode *vp_o, *vp_d;

  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) 
        return(err_code);
        
  /* Request lookup */
  if ((r = lookup_vp(0 /*flags*/, 0 /*!use_realuid*/, &vp_o)) != OK) return r;

  linked_fs_e = vp_o->v_fs_e;

  /* Does the final directory of 'name2' exist? */
  if (fetch_name(m_in.name2, m_in.name2_length, M1) != OK) {
	put_vnode(vp_o);
	return(err_code);
  }

  /* Request lookup */
  if ((r = lookup_lastdir(0 /*!use_realuid*/, &vp_d)) != OK)
  {
	put_vnode(vp_o);
	return r;
  }

  link_lastdir_fs_e = vp_d->v_fs_e;

  /* Check for links across devices. */
  if (linked_fs_e != link_lastdir_fs_e) 
  {
	put_vnode(vp_o);
	put_vnode(vp_d);
        return EXDEV;
  }

  /* Make sure that the object is a directory */
  if ((vp_d->v_mode & I_TYPE) != I_DIRECTORY)
  {
	put_vnode(vp_o);
	put_vnode(vp_d);
	return ENOTDIR;
  }

  r= forbidden(vp_d, W_BIT|X_BIT, 0 /*!use_realuid*/);
  if (r != OK)
  {
	put_vnode(vp_o);
	put_vnode(vp_d);
        return r;
  }
  
  /* Issue request */
  r= req_link(linked_fs_e, vp_d->v_inode_nr, user_fullpath, vp_o->v_inode_nr);
  put_vnode(vp_o);
  put_vnode(vp_d);
  return r;
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
  struct vnode *vp;
  int r;
  
  if (fetch_name(m_in.name, m_in.name_length, M3) != OK) return(err_code);

  r= lookup_lastdir(0 /*!use_realuid*/, &vp);
  if (r != OK)
	return r;

  /* Make sure that the object is a directory */
  if ((vp->v_mode & I_TYPE) != I_DIRECTORY)
  {
	put_vnode(vp);
	return ENOTDIR;
  }

  /* The caller must have both search and execute permission */
  r= forbidden(vp, X_BIT|W_BIT, 0 /*!use_realuid*/);
  if (r != OK)
  {
	put_vnode(vp);
	return r;
  }
  
  /* If a directory file has to be removed the following conditions have to met:
   *	- The directory must not be the root of a mounted file system
   *	- The directory must not be anybody's root/working directory
   */
  
  /* Issue request */
  r= ((call_nr == UNLINK) ? req_unlink : req_rmdir)(vp->v_fs_e,
	vp->v_inode_nr, user_fullpath);

  put_vnode(vp);

  return r;
}


/*===========================================================================*
 *				do_rename				     *
 *===========================================================================*/
PUBLIC int do_rename()
{
/* Perform the rename(name1, name2) system call. */
  int r;
  int old_dir_inode;
  int old_fs_e;
  int new_dir_inode;
  int new_fs_e;
  size_t len;
  struct vnode *vp_od, *vp_nd;
  char old_name[PATH_MAX+1];
  
  /* See if 'name1' (existing file) exists.  Get dir and file inodes. */
  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  
  /* Request lookup */
  if ((r = lookup_lastdir(0 /*!use_realuid*/, &vp_od)) != OK) return r;

  r= forbidden(vp_od, W_BIT|X_BIT, 0 /*!use_realuid*/);
  if (r != OK)
  {
	put_vnode(vp_od);
	return r;
  }
  
  /* Remeber FS endpoint */
  old_fs_e = vp_od->v_fs_e;

  /* Save the last component of the old name */
  len= strlen(user_fullpath);
  if (len >= sizeof(old_name))
  {
	put_vnode(vp_od);
	return ENAMETOOLONG;
  }
  memcpy(old_name, user_fullpath, len+1);

  /* See if 'name2' (new name) exists.  Get dir inode */
  if (fetch_name(m_in.name2, m_in.name2_length, M1) != OK)
  {
	put_vnode(vp_od);
	return err_code;
  }
  
  /* Request lookup */
  r = lookup_lastdir(0 /*!use_realuid*/, &vp_nd);
  if (r != OK)
  {
	put_vnode(vp_od);
	return r;
  }

  r= forbidden(vp_nd, W_BIT|X_BIT, 0 /*!use_realuid*/);
  if (r != OK)
  {
	put_vnode(vp_od);
	put_vnode(vp_nd);
	return r;
  }
  
  
  /* Remeber FS endpoint */
  new_fs_e = vp_nd->v_fs_e;
  
  /* Both parent directories must be on the same device. */
  if (old_fs_e != new_fs_e)
  {
	put_vnode(vp_od);
	put_vnode(vp_nd);
	return EXDEV;
  }

  /* Issue request */
  r= req_rename(old_fs_e, vp_od->v_inode_nr, old_name, vp_nd->v_inode_nr,
	user_fullpath);
  put_vnode(vp_od);
  put_vnode(vp_nd);
  return r;
}
  

/*===========================================================================*
 *				do_truncate				     *
 *===========================================================================*/
PUBLIC int do_truncate()
{
/* truncate_inode() does the actual work of do_truncate() and do_ftruncate().
 * do_truncate() and do_ftruncate() have to get hold of the inode, either
 * by name or fd, do checks on it, and call truncate_inode() to do the
 * work.
 */
  struct vnode *vp;
  int r;

  printf("in do_truncate\n");
  
  if (fetch_name(m_in.m2_p1, m_in.m2_i1, M1) != OK) return err_code;
  
  /* Request lookup */
  if ((r = lookup_vp(0 /*flags*/, 0 /*!use_realuid*/, &vp)) != OK) return r;
  
  r= truncate_vn(vp, m_in.m2_l1);

  put_vnode(vp);

  return r;
}



/*===========================================================================*
 *				do_ftruncate				     *
 *===========================================================================*/
PUBLIC int do_ftruncate()
{
/* As with do_truncate(), truncate_inode() does the actual work. */
  int r;
  struct filp *rfilp;
  
  if ( (rfilp = get_filp(m_in.m2_i1)) == NIL_FILP)
        return err_code;
  if ( (r = forbidden(rfilp->filp_vno, W_BIT, 0 /*!use_realuid*/)) != OK)
	return r;
  return truncate_vn(rfilp->filp_vno, m_in.m2_l1);
}


/*===========================================================================*
 *				truncate_vn				     *
 *===========================================================================*/
PUBLIC int truncate_vn(vp, newsize)
struct vnode *vp;
off_t newsize;
{
  int r;

  if ( (vp->v_mode & I_TYPE) != I_REGULAR &&
	(vp->v_mode & I_TYPE) != I_NAMED_PIPE) {
	return EINVAL;
  }
        
  /* Issue request */
  if ((r = req_ftrunc(vp->v_fs_e, vp->v_inode_nr, newsize, 0)) != OK) return r;
	  
  vp->v_size = newsize;
  return OK;
}

/*===========================================================================*
 *                             do_slink					     *
 *===========================================================================*/
PUBLIC int do_slink()
{
/* Perform the symlink(name1, name2) system call. */
  int r;
  struct vnode *vp;
  char string[NAME_MAX];       /* last component of the new dir's path name */

  if (fetch_name(m_in.name2, m_in.name2_length, M1) != OK)
       return(err_code);

  if (m_in.name1_length <= 1 || m_in.name1_length >= _MIN_BLOCK_SIZE)
       return(ENAMETOOLONG);

  /* Request lookup */
  if ((r = lookup_lastdir(0 /*!use_realuid*/, &vp)) != OK)
  {
	printf("vfs:do_slink: lookup_lastdir failed with %d\n", r);
	return r;
  }

  printf("vfs:do_slink: got dir inode %d on dev 0x%x, fs %d\n",
	vp->v_inode_nr, vp->v_dev, vp->v_fs_e);

  r= forbidden(vp, W_BIT|X_BIT, 0 /*!use_realuid*/);
  if (r != OK)
  {
	put_vnode(vp);
        return r;
  }

  /* Issue request */
  r= req_slink(vp->v_fs_e, vp->v_inode_nr, user_fullpath, who_e, m_in.name1, 
	m_in.name1_length - 1, fp->fp_effuid, fp->fp_effgid);

  put_vnode(vp);

  return r;
}

/*===========================================================================*
 *                             do_rdlink                                    *
 *===========================================================================*/
PUBLIC int do_rdlink()
{
/* Perform the readlink(name, buf) system call. */
  int r, copylen;
  struct vnode *vp;
  
  copylen = m_in.m1_i2;
  if(copylen < 0) return EINVAL;

  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  
  /* Request lookup */
  r = lookup_vp(PATH_RET_SYMLINK, 0 /*!use_realuid*/, &vp);
  if (r != OK) return r;

  /* Issue request */
  r= req_rdlink(vp->v_fs_e, vp->v_inode_nr, who_e, (vir_bytes)m_in.name2,
	copylen);

  put_vnode(vp);

  return r;
}



