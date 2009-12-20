/* lookup() is the main routine that controls the path name lookup. It  
 * handles mountpoints and symbolic links. The actual lookup requests
 * are sent through the req_lookup wrapper function.
 */

#include "fs.h"
#include <string.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/keymap.h>
#include <minix/const.h>
#include <minix/endpoint.h>
#include <unistd.h>
#include <minix/vfsif.h>
#include "fproc.h"
#include "vmnt.h"
#include "vnode.h"
#include "param.h"

/* Set to following define to 1 if you really want to use the POSIX definition
 * (IEEE Std 1003.1, 2004) of pathname resolution. POSIX requires pathnames
 * with a traling slash (and that do not entirely consist of slash characters)
 * to be treated as if a single dot is appended. This means that for example
 * mkdir("dir/", ...) and rmdir("dir/") will fail because the call tries to
 * create or remove the directory '.'. Historically, Unix systems just ignore
 * trailing slashes.
 */
#define DO_POSIX_PATHNAME_RES	0

FORWARD _PROTOTYPE( int lookup, (struct vnode *dirp, int flags,
				 node_details_t *node)			);

/*===========================================================================*
 *				advance					     *
 *===========================================================================*/
PUBLIC struct vnode *advance(dirp, flags)
struct vnode *dirp;
int flags;
{
/* Resolve a pathname (in user_fullpath) starting at dirp to a vnode. */
  int r;
  struct vnode *new_vp, *vp;
  struct vmnt *vmp;
  struct node_details res;

  /* Get a free vnode */
  if((new_vp = get_free_vnode()) == NIL_VNODE) return(NIL_VNODE);
  
  /* Lookup vnode belonging to the file. */
  if ((r = lookup(dirp, flags, &res)) != OK) {
	err_code = r;
	return(NIL_VNODE);
  }
  
  /* Check whether vnode is already in use or not */
  if ((vp = find_vnode(res.fs_e, res.inode_nr)) != NIL_VNODE) {
	  dup_vnode(vp);
	  vp->v_fs_count++;	/* We got a reference from the FS */
	  return(vp);
  }

  /* Fill in the free vnode's fields */
  new_vp->v_fs_e = res.fs_e;
  new_vp->v_inode_nr = res.inode_nr;
  new_vp->v_mode = res.fmode;
  new_vp->v_size = res.fsize;
  new_vp->v_uid = res.uid;
  new_vp->v_gid = res.gid;
  new_vp->v_sdev = res.dev;
  
  if( (vmp = find_vmnt(new_vp->v_fs_e)) == NIL_VMNT)
	  panic(__FILE__, "VFS advance: vmnt not found", NO_NUM);

  new_vp->v_vmnt = vmp; 
  new_vp->v_dev = vmp->m_dev;
  new_vp->v_fs_count = 1;
  new_vp->v_ref_count = 1;
  
  return(new_vp);
}


/*===========================================================================*
 *				eat_path				     *
 *===========================================================================*/
PUBLIC struct vnode *eat_path(flags)
int flags;
{
/* Resolve 'user_fullpath' to a vnode. advance does the actual work. */
  struct vnode *vp;

  vp = (user_fullpath[0] == '/' ? fp->fp_rd : fp->fp_wd);
  return advance(vp, flags);
}


/*===========================================================================*
 *				last_dir					     *
 *===========================================================================*/
PUBLIC struct vnode *last_dir(void)
{
/* Parse a path, 'user_fullpath', as far as the last directory, fetch the vnode
 * for the last directory into the vnode table, and return a pointer to the
 * vnode. In addition, return the final component of the path in 'string'. If
 * the last directory can't be opened, return NIL_VNODE and the reason for
 * failure in 'err_code'. We can't parse component by component as that would
 * be too expensive. Alternatively, we cut off the last component of the path,
 * and parse the path up to the penultimate component.
 */  

  int r;
  size_t len;
  char *cp;
  char dir_entry[PATH_MAX+1];
  struct vnode *vp, *res;
  
  /* Is the path absolute or relative? Initialize 'vp' accordingly. */
  vp = (user_fullpath[0] == '/' ? fp->fp_rd : fp->fp_wd);

  len = strlen(user_fullpath);

  /* If path is empty, return ENOENT. */
  if (len == 0)	{
	err_code = ENOENT;
	return(NIL_VNODE); 
  }

#if !DO_POSIX_PATHNAME_RES
  /* Remove trailing slashes */
  while (len > 1 && user_fullpath[len-1] == '/') {
	  len--;
	  user_fullpath[len]= '\0';
  }
#endif

  cp = strrchr(user_fullpath, '/');
  if (cp == NULL) {
	  /* Just one entry in the current working directory */
	  dup_vnode(vp);
	  return(vp);
  } else if (cp[1] == '\0') {
	  /* Path ends in a slash. The directory entry is '.' */
	  strcpy(dir_entry, ".");
  } else {
	  /* A path name for the directory and a directory entry */
	  strcpy(dir_entry, cp+1);
	  cp[1]= '\0';
  }

  /* Remove trailing slashes */
  while(cp > user_fullpath && cp[0] == '/') {
	  cp[0]= '\0';
	  cp--;
  }

  res = advance(vp, PATH_NOFLAGS);
  if (res == NIL_VNODE) return(NIL_VNODE);

  /* Copy the directory entry back to user_fullpath */
  strcpy(user_fullpath, dir_entry);
  
  return(res);
}


/*===========================================================================*
 *				lookup					     *
 *===========================================================================*/
PRIVATE int lookup(start_node, flags, node)
struct vnode *start_node;
int flags;
node_details_t *node;
{
/* Resolve a pathname (in user_fullpath) relative to start_node. */

  int r, symloop;
  endpoint_t fs_e;
  size_t path_off, path_left_len;
  ino_t dir_ino, root_ino;
  uid_t uid;
  gid_t gid;
  struct vnode *dir_vp;
  struct vmnt *vmp;
  struct lookup_res res;

  /* Empty (start) path? */
  if (user_fullpath[0] == '\0') {
	node->inode_nr = 0;
	return(ENOENT);
  }

  if(!fp->fp_rd || !fp->fp_wd) {
	printf("VFS: lookup_rel %d: no rd/wd\n", fp->fp_endpoint);
	return(ENOENT);
  }

  fs_e = start_node->v_fs_e;
  dir_ino = start_node->v_inode_nr;
 
  /* Is the process' root directory on the same partition?,
   * if so, set the chroot directory too. */
  if (fp->fp_rd->v_dev == fp->fp_wd->v_dev)
	root_ino = fp->fp_rd->v_inode_nr; 
  else
	root_ino = 0;

  /* Set user and group ids according to the system call */
  uid = (call_nr == ACCESS ? fp->fp_realuid : fp->fp_effuid); 
  gid = (call_nr == ACCESS ? fp->fp_realgid : fp->fp_effgid); 

  symloop = 0;	/* Number of symlinks seen so far */

  /* Issue the request */
  r = req_lookup(fs_e, dir_ino, root_ino, uid, gid, flags, &res);

  if (r != OK && r != EENTERMOUNT && r != ELEAVEMOUNT && r != ESYMLINK)
	return(r); /* i.e., an error occured */

  /* While the response is related to mount control set the 
   * new requests respectively */
  while(r == EENTERMOUNT || r == ELEAVEMOUNT || r == ESYMLINK) {
	/* Update user_fullpath to reflect what's left to be parsed. */
	path_off = res.char_processed;
	path_left_len = strlen(&user_fullpath[path_off]);
	memmove(user_fullpath, &user_fullpath[path_off], path_left_len);
	user_fullpath[path_left_len] = '\0'; /* terminate string */ 

	/* Update the current value of the symloop counter */
	symloop += res.symloop;
	if (symloop > SYMLOOP_MAX)
		return(ELOOP);

	/* Symlink encountered with absolute path */
	if (r == ESYMLINK) {
		dir_vp = fp->fp_rd;
	} else if (r == EENTERMOUNT) {
		/* Entering a new partition */
		dir_vp = 0;
		/* Start node is now the mounted partition's root node */
		for (vmp = &vmnt[0]; vmp != &vmnt[NR_MNTS]; ++vmp) {
			if (vmp->m_dev != NO_DEV) {
			   if (vmp->m_mounted_on->v_inode_nr == res.inode_nr &&
			       vmp->m_mounted_on->v_fs_e == res.fs_e) {
				dir_vp = vmp->m_root_node;
				break;
			   }
			}
		}

		if (!dir_vp) {
			panic(__FILE__,
			      "VFS lookup: can't find mounted partition",
			      NO_NUM);
		}
	} else {
		/* Climbing up mount */
		/* Find the vmnt that represents the partition on
		 * which we "climb up". */
		if ((vmp = find_vmnt(res.fs_e)) == NIL_VMNT) {
			panic(__FILE__,
			      "VFS lookup: can't find parent vmnt",NO_NUM);
		}	  

		/* Make sure that the child FS does not feed a bogus path
		 * to the parent FS. That is, when we climb up the tree, we
		 * must've encountered ".." in the path, and that is exactly
		 * what we're going to feed to the parent */
		if(strncmp(user_fullpath, "..", 2) != 0) {
			printf("VFS: bogus path: %s\n", user_fullpath);
			return(ENOENT);
		}

		/* Start node is the vnode on which the partition is
		 * mounted */
		dir_vp = vmp->m_mounted_on;
	}

	/* Set the starting directories inode number and FS endpoint */
	fs_e = dir_vp->v_fs_e;
	dir_ino = dir_vp->v_inode_nr;

	/* Is the process' root directory on the same partition?,
	 * if so, set the chroot directory too. */
	if(dir_vp->v_dev == fp->fp_rd->v_dev)
		root_ino = fp->fp_rd->v_inode_nr; 
	else
		root_ino = 0;

	r = req_lookup(fs_e, dir_ino, root_ino, uid, gid, flags, &res);

	if(r != OK && r != EENTERMOUNT && r != ELEAVEMOUNT && r != ESYMLINK)
		return(r);
  }

  /* Fill in response fields */
  node->inode_nr = res.inode_nr;
  node->fmode = res.fmode;
  node->fsize = res.fsize;
  node->dev = res.dev;
  node->fs_e = res.fs_e;
  node->uid = res.uid;
  node->gid = res.gid;
  
  return(r);
}
