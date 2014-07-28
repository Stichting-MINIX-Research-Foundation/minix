/* This file contains mount and unmount functionality.
 *
 * The entry points into this file are:
 *   do_readsuper	perform the READSUPER file system call
 *   do_unmount		perform the UNMOUNT file system call
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

/*===========================================================================*
 *				do_readsuper				     *
 *===========================================================================*/
int do_readsuper(void)
{
/* Mount the file system.
 */
  char path[PATH_MAX];
  struct inode *ino;
  struct sffs_attr attr;
  int r;

  dprintf(("%s: readsuper (dev %x, flags %x)\n",
	sffs_name, m_in.m_vfs_fs_readsuper.device, m_in.vfs_fs_readsuper.flags));

  if (m_in.m_vfs_fs_readsuper.flags & REQ_ISROOT) {
	printf("%s: attempt to mount as root device\n", sffs_name);

	return EINVAL;
  }

  state.s_read_only = !!(m_in.m_vfs_fs_readsuper.flags & REQ_RDONLY);
  state.s_dev = m_in.m_vfs_fs_readsuper.device;

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

  m_out.m_fs_vfs_readsuper.inode = INODE_NR(ino);
  m_out.m_fs_vfs_readsuper.mode = get_mode(ino, attr.a_mode);
  m_out.m_fs_vfs_readsuper.file_size = attr.a_size;
  m_out.m_fs_vfs_readsuper.uid = sffs_params->p_uid;
  m_out.m_fs_vfs_readsuper.gid = sffs_params->p_gid;
  m_out.m_fs_vfs_readsuper.device = NO_DEV;
  m_out.m_fs_vfs_readsuper.flags = RES_64BIT;

  state.s_mounted = TRUE;

  return OK;
}

/*===========================================================================*
 *				do_unmount				     *
 *===========================================================================*/
int do_unmount(void)
{
/* Unmount the file system.
 */
  struct inode *ino;

  dprintf(("%s: do_unmount\n", sffs_name));

  /* Decrease the reference count of the root inode. */
  if ((ino = find_inode(ROOT_INODE_NR)) == NULL)
	return EINVAL;

  put_inode(ino);

  /* There should not be any referenced inodes anymore now. */
  if (have_used_inode())
	printf("%s: in-use inodes left at unmount time!\n", sffs_name);

  state.s_mounted = FALSE;

  return OK;
}
