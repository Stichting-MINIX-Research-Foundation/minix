/* This file contains routines that verify inodes and paths against the host.
 *
 * The entry points into this file are:
 *   verify_path	check whether a path,inode pair is still valid
 *   verify_inode	construct a path for an inode and verify the inode
 *   verify_dentry	check a directory inode and look for a directory entry
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

/*===========================================================================*
 *				verify_path				     *
 *===========================================================================*/
int verify_path(char path[PATH_MAX], struct inode *ino,
	struct sffs_attr *attr, int *stale)
{
/* Given a path, and the inode associated with that path, verify if the inode
 * still matches the real world. Obtain the attributes of the file identified
 * by the given path, and see if they match. If not, possibly mark the inode
 * as deleted and return an error. Only upon success is the inode guaranteed
 * to be usable.
 *
 * The caller must set the a_mask field of the passed attr struct.
 * If 'stale' is not NULL, the value it points to must be initialized to 0,
 * and will be set to 1 if the path was valid but the inode wasn't.
 */
  int r;

  attr->a_mask |= SFFS_ATTR_MODE;

  r = sffs_table->t_getattr(path, attr);

  dprintf(("%s: verify_path: getattr('%s') returned %d\n",
	sffs_name, path, r));

  if (r != OK) {
	/* If we are told that the path does not exist, delete the inode */
	if (r == ENOENT || r == ENOTDIR)
		del_dentry(ino);

	return r; /* path isn't valid */
  }

  /* If the file type (reg, dir) isn't what we thought, delete the inode */
  if ((ino->i_flags & I_DIR) != MODE_TO_DIRFLAG(attr->a_mode)) {
	del_dentry(ino);

	if (stale != NULL) *stale = 1;
	return ENOENT; /* path is valid, inode wasn't */
  }

  return OK; /* path and inode are valid */
}

/*===========================================================================*
 *				verify_inode				     *
 *===========================================================================*/
int verify_inode(
	struct inode *ino,    	/* inode to verify */
	char path[PATH_MAX],  	/* buffer in which to store the path */
	struct sffs_attr *attr	/* buffer for attributes, or NULL */
)
{
/* Given an inode, construct a path identifying the inode, and check whether
 * that path is still valid for that inode (as far as we can tell). As a side
 * effect, store attributes in the given attribute structure if not NULL (its
 * a_mask member must then be set).
 */
  struct sffs_attr attr2;
  int r;

  if ((r = make_path(path, ino)) != OK) return r;

  if (attr == NULL) {
	attr2.a_mask = 0;

	attr = &attr2;
  }

  return verify_path(path, ino, attr, NULL);
}

/*===========================================================================*
 *				verify_dentry				     *
 *===========================================================================*/
int verify_dentry(
	struct inode *parent, 	/* parent inode: the inode to verify */
	char name[NAME_MAX+1],	/* the given directory entry path component */
	char path[PATH_MAX],  	/* buffer to store the resulting path in */
	struct inode **res_ino	/* pointer for addressed inode (or NULL) */
)
{
/* Given a directory inode and a name, construct a path identifying that
 * directory entry, check whether the path to the parent is still valid, and
 * check whether there is an inode pointed to by the full path. Upon success,
 * res_ino will contain either the inode for the full path, with increased
 * refcount, or NULL if no such inode exists.
 */
  int r;

  if ((r = verify_inode(parent, path, NULL)) != OK)
	return r;

  dprintf(("%s: verify_dentry: given path is '%s', name '%s'\n",
	sffs_name, path, name));

  if ((r = push_path(path, name)) != OK)
	return r;

  dprintf(("%s: verify_dentry: path now '%s'\n", sffs_name, path));

  *res_ino = lookup_dentry(parent, name);

  return OK;
}
