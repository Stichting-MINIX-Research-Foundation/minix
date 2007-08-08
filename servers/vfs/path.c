
/* lookup() is the main routine that controls the path name lookup. It  
 * handles mountpoints and symbolic links. The actual lookup requests
 * are sent through the req_lookup wrapper function.
 *
 *  Jul 2006 (Balazs Gerofi)
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

FORWARD _PROTOTYPE( int lookup_rel, (struct vnode *start_node,
		int flags, int use_realuid, node_details_t *node)	);

/*===========================================================================*
 *				lookup_rel_vp				     *
 *===========================================================================*/
PUBLIC int lookup_rel_vp(start_node, flags, use_realuid, vpp)
struct vnode *start_node;
int flags;
int use_realuid;
struct vnode **vpp;
{
  /* Resolve a pathname (in user_fullpath) starting at start_node to a vnode. */
  int r, lookup_res;
  struct vnode *new_vp, *vp;
  struct vmnt *vmp;
  struct node_details res;

  /* See if free vnode is available */
  if ((new_vp = get_free_vnode(__FILE__, __LINE__)) == NIL_VNODE) {
      printf("vfs:lookup_rel_vp: no free vnode available\n");
      *vpp= NULL;
      return EINVAL;
  }

  lookup_res = lookup_rel(start_node, flags, use_realuid, &res);

  if (lookup_res != OK)
  {
#if 0
	printf("vfs:lookup_rel_vp: lookup_rel failed with %d\n", lookup_res);
#endif
	return lookup_res;
  }

  /* Check whether vnode is already in use or not */
  if ((vp = find_vnode(res.fs_e, res.inode_nr)) != NIL_VNODE) {
        vp->v_ref_count++;
	vp->v_fs_count++;	/* We got a reference from the FS */
	*vpp= vp;
	return OK;
  }

  /* Fill in the free vnode's fields */
  new_vp->v_fs_e = res.fs_e;
  new_vp->v_inode_nr = res.inode_nr;
  new_vp->v_mode = res.fmode;
  new_vp->v_size = res.fsize;
  new_vp->v_uid = res.uid;
  new_vp->v_gid = res.gid;
  new_vp->v_sdev = res.dev;

  if ( (vmp = find_vmnt(new_vp->v_fs_e)) == NIL_VMNT)
	panic(__FILE__, "vfs:lookup_rel_vp: vmnt not found", NO_NUM);

  new_vp->v_vmnt = vmp; 
  new_vp->v_dev = vmp->m_dev;
  new_vp->v_fs_count = 1;
  new_vp->v_ref_count = 1;

  *vpp= new_vp;
  return OK;
}


/*===========================================================================*
 *				lookup_vp				     *
 *===========================================================================*/
PUBLIC int lookup_vp(flags, use_realuid, vpp)
int flags;
int use_realuid;
struct vnode **vpp;
{
  /* Resolve a pathname (in user_fullpath) starting to a vnode. Call
   * lookup_rel_vp to do the actual work.
   */
  struct vnode *vp;

  vp= (user_fullpath[0] == '/' ? fp->fp_rd : fp->fp_wd);

  return lookup_rel_vp(vp, flags, use_realuid, vpp);
}


/*===========================================================================*
 *				lookup_lastdir_rel			     *
 *===========================================================================*/
PUBLIC int lookup_lastdir_rel(start_node, use_realuid, vpp)
struct vnode *start_node;
int use_realuid;
struct vnode **vpp;
{
	/* This function is for calls that insert or delete entries from a
	 * directory. The path name (implicitly taken from user_fullpath)
	 * is split into to parts: the name of the directory and the
	 * directory entry. The name of the directory is resolved to a 
	 * vnode. The directory entry is copied back to user_fullpath.
	 * The lookup starts at start_node.
	 */
	int r;
	size_t len;
	char *cp;
	char dir_entry[PATH_MAX+1];

	len= strlen(user_fullpath);
	if (len == 0)
	{
		/* Empty path, always fail */
		return ENOENT;
	}

#if !DO_POSIX_PATHNAME_RES
	/* Remove trailing slashes */
	while (len > 1 && user_fullpath[len-1] == '/')
	{
		len--;
		user_fullpath[len]= '\0';
	}
#endif

	cp= strrchr(user_fullpath, '/');
	if (cp == NULL)
	{
		/* Just one entry in the current working directory */
		dup_vnode(start_node);
		*vpp= start_node;

		return OK;
	}
	else if (cp[1] == '\0')
	{
		/* Path ends in a slash. The directory entry is '.' */
		strcpy(dir_entry, ".");
	}
	else
	{
		/* A path name for the directory and a directory entry */
		strcpy(dir_entry, cp+1);
		cp[1]= '\0';
	}

	/* Remove trailing slashes */
	while(cp > user_fullpath && cp[0] == '/')
	{
		cp[0]= '\0';
		cp--;
	}

	/* Request lookup */
	r = lookup_rel_vp(start_node, 0 /*no flags*/, use_realuid, vpp);
	if (r != OK)
		return r;

	/* Copy the directory entry back to user_fullpath */
	strcpy(user_fullpath, dir_entry);

	return OK;
}


/*===========================================================================*
 *				lookup_lastdir				     *
 *===========================================================================*/
PUBLIC int lookup_lastdir(use_realuid, vpp)
int use_realuid;
struct vnode **vpp;
{
	/* This function is for calls that insert or delete entries from a
	 * directory. The path name (implicitly taken from user_fullpath)
	 * is split into to parts: the name of the directory and the
	 * directory entry. The name of the directory is resolved to a 
	 * vnode. The directory entry is copied back to user_fullpath.
	 * Just call lookup_lastdir_rel with the appropriate starting vnode.
	 */
	struct vnode *vp;

	vp= (user_fullpath[0] == '/' ? fp->fp_rd : fp->fp_wd);
	return lookup_lastdir_rel(vp, use_realuid, vpp);
}


/*===========================================================================*
 *				lookup_rel				     *
 *===========================================================================*/
PRIVATE int lookup_rel(start_node, flags, use_realuid, node)
struct vnode *start_node;
int flags;
int use_realuid;
node_details_t *node;
{
  /* Resolve a pathname (in user_fullpath) relative to start_node. */
  int r, symloop;
  endpoint_t fs_e;
  size_t path_off;
  ino_t dir_ino, root_ino;
  uid_t uid;
  gid_t gid;
  struct vnode *dir_vp;
  struct vmnt *vmp;
  struct lookup_res res;
  
  /* Empty (start) path? */
  if (user_fullpath[0] == '\0') {
	node->inode_nr = 0;
	printf("vfs:lookup_rel: returning ENOENT\n");
	return ENOENT;
  }

  fs_e = start_node->v_fs_e;
  path_off = 0;
  dir_ino = start_node->v_inode_nr;
  /* Is the process' root directory on the same partition?,
   * if so, set the chroot directory too. */
  if (fp->fp_rd->v_dev == fp->fp_wd->v_dev)
      root_ino = fp->fp_rd->v_inode_nr; 
  else
      root_ino = 0;

  /* Set user and group ids according to the system call */
  uid = (use_realuid ? fp->fp_realuid : fp->fp_effuid); 
  gid = (use_realuid ? fp->fp_realgid : fp->fp_effgid); 

  symloop= 0;	/* Number of symlinks seen so far */

  /* Issue the request */
  r = req_lookup(fs_e, path_off, dir_ino, root_ino, uid, gid, flags, &res);

  if (r != OK && r != EENTERMOUNT && r != ELEAVEMOUNT && r != ESYMLINK)
  {
#if 0
	printf("vfs:lookup_rel: req_lookup_s failed with %d\n", r);
#endif
	return r;
  }

  /* While the response is related to mount control set the 
   * new requests respectively */
  while (r == EENTERMOUNT || r == ELEAVEMOUNT || r == ESYMLINK) {
	/* Save the place in the (possibly updated) path where we have to
	 * continue witht henext lookup request.
	 */
	path_off= res.char_processed;

	/* Update the current value of the symloop counter */
	symloop += res.symloop;
	if (symloop > SYMLOOP_MAX)
	{
		printf("vfs:lookup_rel: returning ELOOP\n");
		return ELOOP;
	}

	/* Symlink encountered with absolute path */
	if (r == ESYMLINK) {
		dir_vp = fp->fp_rd;
	}
	else if (r == EENTERMOUNT) {
		/* Entering a new partition */
		dir_vp = 0;
		/* Start node is now the mounted partition's root node */
		for (vmp = &vmnt[0]; vmp != &vmnt[NR_MNTS]; ++vmp) {
			if (vmp->m_mounted_on->v_inode_nr == res.inode_nr &&
				vmp->m_mounted_on->v_fs_e == res.fs_e) {
				dir_vp = vmp->m_root_node;
				break;
			}
		}
		if (!dir_vp) {
			printf(
			"vfs:lookup_rel: res.inode_nr = %d, res.fs_e = %d\n",
				res.inode_nr, res.fs_e);
			panic(__FILE__,
			"vfs:lookup_s: mounted partition couldn't be found",
				NO_NUM);
		}

	}
	else {
		/* Climbing up mount */
		/* Find the vmnt that represents the partition on
		 * which we "climb up". */
		if ((vmp = find_vmnt(res.fs_e)) == NIL_VMNT) {
			panic(__FILE__,
			"vfs:lookup_s: couldn't find vmnt during the climbup",
				NO_NUM);
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
	if (fp->fp_rd->v_dev == fp->fp_wd->v_dev)
		root_ino = fp->fp_rd->v_inode_nr; 
	else
		root_ino = 0;

	/* Issue the request */
	r = req_lookup(fs_e, path_off, dir_ino, root_ino, uid, gid, flags,
		&res);

	if (r != OK && r != EENTERMOUNT && r != ELEAVEMOUNT && r != ESYMLINK)
	{
#if 0
		printf("vfs:lookup_rel: req_lookup_s failed with %d\n", r);
#endif
		return r;
	}
  }

  /* Fill in response fields */
  node->inode_nr = res.inode_nr;
  node->fmode = res.fmode;
  node->fsize = res.fsize;
  node->dev = res.dev;
  node->fs_e = res.fs_e;
  node->uid = res.uid;
  node->gid = res.gid;
  
  return r;
}
