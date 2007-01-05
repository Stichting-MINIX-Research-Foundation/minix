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
  int linked_fs_e;
  int linked_inode_nr;
  int link_lastdir_fs_e;
  int link_lastdir_inode_nr;
  char string[NAME_MAX];
  struct link_req req;
  struct lookup_req lookup_req;
  struct node_details res;
  int r;

  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) 
        return(err_code);
        
  /* Fill in lookup request fields */
  lookup_req.path = user_fullpath;
  lookup_req.lastc = NULL;
  lookup_req.flags = EAT_PATH;
        
  /* Request lookup */
  if ((r = lookup(&lookup_req, &res)) != OK) return r;

  linked_fs_e = res.fs_e;
  req.linked_file = res.inode_nr;

  /* Does the final directory of 'name2' exist? */
  if (fetch_name(m_in.name2, m_in.name2_length, M1) != OK) {
	return(err_code);
  }

  /* Fill in lookup request fields */
  lookup_req.path = user_fullpath;
  lookup_req.lastc = string;
  lookup_req.flags = LAST_DIR;
        
  /* Request lookup */
  if ((r = lookup(&lookup_req, &res)) != OK) return r;

  link_lastdir_fs_e = res.fs_e;
  req.link_parent = res.inode_nr;
  
  /* Check for links across devices. */
  if (linked_fs_e != link_lastdir_fs_e) 
        return EXDEV;

  /* Send link request. */
  req.fs_e = linked_fs_e;
  /* Send the last component of the link name */
  req.uid = fp->fp_effuid;
  req.gid = fp->fp_effgid;
  req.lastc = string;
  
  /* Issue request */
  return req_link(&req);
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
  char string[NAME_MAX];
  struct vnode *vp;
  struct unlink_req req;
  struct lookup_req lookup_req;
  struct node_details res;
  int r;
  string[0] = '\0';
  
  if (fetch_name(m_in.name, m_in.name_length, M3) != OK) return(err_code);
  
  /* Fill in lookup request fields */
  lookup_req.path = user_fullpath;
  lookup_req.lastc = NULL;
  lookup_req.flags = EAT_PATH_OPAQUE;
        
  /* Request lookup */
  if ((r = lookup(&lookup_req, &res)) != OK) return r;
  
  /* If a directory file has to be removed the following conditions have to met:
   *	- The directory must not be the root of a mounted file system
   *	- The directory must not be anybody's root/working directory
   */
  if ((res.fmode & I_TYPE) == I_DIRECTORY) {
      /* Only root can unlink a directory */
      if (call_nr == UNLINK && !super_user) return EPERM; 

      /* Can't remove a root directory */
      if (res.inode_nr == ROOT_INODE) return EBUSY;

      /* Can't remove anybody's working directory */
      if ((vp = find_vnode(res.fs_e, res.inode_nr)) != 
              NIL_VNODE) {
          /* Check directories */
          for (rfp = &fproc[INIT_PROC_NR + 1]; rfp < &fproc[NR_PROCS]; 
                  rfp++) {
              if (rfp->fp_pid != PID_FREE && 
                      (rfp->fp_wd == vp || rfp->fp_rd == vp))
                  return(EBUSY); 
          }
      }
  }
  
  /* Fill in lookup request fields */
  lookup_req.path = user_fullpath;
  lookup_req.lastc = string;
  lookup_req.flags = LAST_DIR;
        
  /* Request lookup */
  if ((r = lookup(&lookup_req, &res)) != OK) return r;
  
  /* Fill in request fields. */
  req.fs_e = res.fs_e;
  req.d_inode_nr = res.inode_nr;
  req.uid = fp->fp_effuid;
  req.gid = fp->fp_effgid;
  req.lastc = string;
  
  /* Issue request */
  return (call_nr == UNLINK) ? req_unlink(&req) : req_rmdir(&req);
}


/*===========================================================================*
 *				do_rename				     *
 *===========================================================================*/
PUBLIC int do_rename()
{
/* Perform the rename(name1, name2) system call. */
  int old_dir_inode;
  int old_fs_e;
  int new_dir_inode;
  int new_fs_e;
  char old_name[NAME_MAX];
  char new_name[NAME_MAX];
  struct vnode *vp;
  struct fproc *rfp;
  struct rename_req req;
  struct lookup_req lookup_req;
  struct node_details res;
  int r;
  
  /* See if 'name1' (existing file) exists.  Get dir and file inodes. */
  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  
  /* Fill in lookup request fields */
  lookup_req.path = user_fullpath;
  lookup_req.lastc = old_name;
  lookup_req.flags = LAST_DIR;
        
  /* Request lookup */
  if ((r = lookup(&lookup_req, &res)) != OK) return r;
  
  /* Remeber inode number and FS endpoint */
  old_fs_e = res.fs_e;
  req.old_dir = res.inode_nr;

  /* See if 'name2' (new name) exists.  Get dir inode */
  if (fetch_name(m_in.name2, m_in.name2_length, M1) != OK) r = err_code;
  
  /* Fill in lookup request fields */
  lookup_req.path = user_fullpath;
  lookup_req.lastc = NULL;
  lookup_req.flags = EAT_PATH_OPAQUE;
        
  /* Request lookup */
  r = lookup(&lookup_req, &res);
  
  /* If a directory file has to be removed the following conditions have to met:
   *	- The directory must not be the root of a mounted file system
   *	- The directory must not be anybody's root/working directory
   */
  if (r == OK && ((res.fmode & I_TYPE) == I_DIRECTORY)) {
      /* Can't remove a root directory */
      if (res.inode_nr == ROOT_INODE) return EBUSY;

      /* Can't remove anybody's working directory */
      if ((vp = find_vnode(res.fs_e, res.inode_nr)) != 
              NIL_VNODE) {
          /* Check directories */
          for (rfp = &fproc[INIT_PROC_NR + 1]; rfp < &fproc[NR_PROCS]; 
                  rfp++) {
              if (rfp->fp_pid != PID_FREE && 
                      (rfp->fp_wd == vp || rfp->fp_rd == vp))
                  return(EBUSY); 
          }
      }
  }
  
  /* Fill in lookup request fields */
  lookup_req.path = user_fullpath;
  lookup_req.lastc = new_name;
  lookup_req.flags = LAST_DIR;
        
  /* Request lookup */
  if ((r = lookup(&lookup_req, &res)) != OK) return r;
  
  /* Remeber inode number and FS endpoint */
  new_fs_e = res.fs_e;
  req.new_dir = res.inode_nr;
  
  /* Both parent directories must be on the same device. */
  if (old_fs_e != new_fs_e) return EXDEV;

  /* Send actual rename request */
  req.fs_e = old_fs_e;
  req.uid = fp->fp_effuid;
  req.gid = fp->fp_effgid;
  req.old_name = &old_name[0];
  req.new_name = &new_name[0];
 
  /* Issue request */
  return req_rename(&req);
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
  struct trunc_req req;
  struct lookup_req lookup_req;
  struct node_details res;
  int r;

  printf("in do_truncate\n");
  
  if (fetch_name(m_in.m2_p1, m_in.m2_i1, M1) != OK) return err_code;
  
  /* Fill in lookup request fields */
  lookup_req.path = user_fullpath;
  lookup_req.lastc = NULL;
  lookup_req.flags = EAT_PATH;
        
  /* Request lookup */
  if ((r = lookup(&lookup_req, &res)) != OK) return r;
  
  /* Check whether the file is in use or not */
  vp = find_vnode(res.fs_e, res.inode_nr);

  /* Fill in request message fields.*/
  req.fs_e = res.fs_e;
  req.length = m_in.m2_l1;
  req.inode_nr = res.inode_nr;
  req.uid = fp->fp_effuid;
  req.gid = fp->fp_effgid;
  
  /* Issue request */
  if ((r = req_trunc(&req)) != OK) return r;
	  
  /* Change vnode's size if found */
  if (vp != NIL_VNODE)
	  vp->v_size = m_in.m2_l1;
  
  return OK;
}



/*===========================================================================*
 *				do_ftruncate				     *
 *===========================================================================*/
PUBLIC int do_ftruncate()
{
/* As with do_truncate(), truncate_inode() does the actual work. */
  struct filp *rfilp;
  
  if ( (rfilp = get_filp(m_in.m2_i1)) == NIL_FILP)
        return err_code;
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
  struct ftrunc_req req;

  if ( (vp->v_mode & I_TYPE) != I_REGULAR &&
	(vp->v_mode & I_TYPE) != I_NAMED_PIPE) {
	return EINVAL;
  }
        
  /* Fill in FS request */
  req.fs_e = vp->v_fs_e; 
  req.inode_nr = vp->v_inode_nr;
  req.start = newsize;
  req.end = 0;     /* Indicate trunc in fs_freesp_trunc */

  /* Issue request */
  if ((r = req_ftrunc(&req)) != OK) return r;
	  
  vp->v_size = newsize;
  return OK;
}

/*===========================================================================*
 *                             do_slink					     *
 *===========================================================================*/
PUBLIC int do_slink()
{
/* Perform the symlink(name1, name2) system call. */
  char string[NAME_MAX];       /* last component of the new dir's path name */
  struct slink_req req;
  struct lookup_req lookup_req;
  struct node_details res;
  int r;

  if (fetch_name(m_in.name2, m_in.name2_length, M1) != OK)
       return(err_code);

  if (m_in.name1_length <= 1 || m_in.name1_length >= _MIN_BLOCK_SIZE)
       return(ENAMETOOLONG);
  
  /* Fill in lookup request fields */
  lookup_req.path = user_fullpath;
  lookup_req.lastc = string;
  lookup_req.flags = LAST_DIR;
        
  /* Request lookup */
  if ((r = lookup(&lookup_req, &res)) != OK) return r;

  /* Fill in request message */
  req.fs_e = res.fs_e;
  req.parent_dir = res.inode_nr;
  req.uid = fp->fp_effuid;
  req.gid = fp->fp_effgid;
  req.lastc = string;
  req.who_e = who_e;
  req.path_addr = m_in.name1;
  req.path_length = m_in.name1_length - 1;

  /* Issue request */
  return req_slink(&req);
}

/*===========================================================================*
 *                             do_rdlink                                    *
 *===========================================================================*/
PUBLIC int do_rdlink()
{
/* Perform the readlink(name, buf) system call. */
  int copylen;
  struct rdlink_req req;
  struct lookup_req lookup_req;
  struct node_details res;
  int r;
  
  copylen = m_in.m1_i2;
  if(copylen < 0) return EINVAL;

  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  
  /* Fill in lookup request fields */
  lookup_req.path = user_fullpath;
  lookup_req.lastc = NULL;
  lookup_req.flags = EAT_PATH_OPAQUE;
        
  /* Request lookup */
  if ((r = lookup(&lookup_req, &res)) != OK) return r;
  
  /* Fill in request message */
  req.fs_e = res.fs_e;
  req.inode_nr = res.inode_nr;
  req.uid = fp->fp_effuid;
  req.gid = fp->fp_effgid;
  req.who_e = who_e;
  req.path_buffer = m_in.name2;
  req.max_length = copylen;

  /* Issue request */
  return req_rdlink(&req);
}



