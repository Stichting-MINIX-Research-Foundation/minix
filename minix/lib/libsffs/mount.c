/* This file contains mount and unmount functionality.
 *
 * The entry points into this file are:
 *   do_mount		perform the READSUPER file system call
 *   do_unmount		perform the UNMOUNT file system call
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

/*===========================================================================*
 *				do_mount				     *
 *===========================================================================*/
int do_mount(dev_t __unused dev, unsigned int flags,
	struct fsdriver_node *root_node, unsigned int *res_flags)
{
/* Mount the file system.
 */
  char path[PATH_MAX];
  struct inode *ino;
  struct sffs_attr attr;
  int r;

  dprintf(("%s: mount (dev %"PRIx64", flags %x)\n", sffs_name, dev, flags));

  if (flags & REQ_ISROOT) {
	printf("%s: attempt to mount as root device\n", sffs_name);

	return EINVAL;
  }

  read_only = !!(flags & REQ_RDONLY);

  init_dentry();
  ino = init_inode();

  attr.a_mask = SFFS_ATTR_MODE | SFFS_ATTR_SIZE;

  /* We cannot continue if we fail to get the properties of the root inode at
   * all, because we cannot guess the details of the root node to return to
   * VFS. Print a (hopefully) helpful error message, and abort the mount.
   */
  if ((r = verify_inode(ino, path, &attr)) != OK) {
	if (r == EAGAIN)
		printf("%s: shared folders disabled\n", sffs_name);
	else if (sffs_params->p_prefix[0] && (r == ENOENT || r == EACCES))
		printf("%s: unable to access the given prefix directory\n",
			sffs_name);
	else
		printf("%s: unable to access shared folders\n", sffs_name);

	return r;
  }

  root_node->fn_ino_nr = INODE_NR(ino);
  root_node->fn_mode = get_mode(ino, attr.a_mode);
  root_node->fn_size = attr.a_size;
  root_node->fn_uid = sffs_params->p_uid;
  root_node->fn_gid = sffs_params->p_gid;
  root_node->fn_dev = NO_DEV;

  *res_flags = RES_64BIT;

  return OK;
}

/*===========================================================================*
 *				do_unmount				     *
 *===========================================================================*/
void do_unmount(void)
{
/* Unmount the file system.
 */
  struct inode *ino;

  dprintf(("%s: unmount\n", sffs_name));

  /* Decrease the reference count of the root inode. */
  if ((ino = find_inode(ROOT_INODE_NR)) == NULL)
	return;

  put_inode(ino);

  /* There should not be any referenced inodes anymore now. */
  if (have_used_inode())
	printf("%s: in-use inodes left at unmount time!\n", sffs_name);
}
