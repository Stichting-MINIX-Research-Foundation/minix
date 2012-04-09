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
int do_readsuper()
{
/* Mount the file system.
 */
  char path[PATH_MAX];
  struct inode *ino;
  struct sffs_attr attr;
  int r;

  dprintf(("%s: readsuper (dev %x, flags %x)\n",
	sffs_name, (dev_t) m_in.REQ_DEV, m_in.REQ_FLAGS));

  if (m_in.REQ_FLAGS & REQ_ISROOT) {
	printf("%s: attempt to mount as root device\n", sffs_name);

	return EINVAL;
  }

  state.s_read_only = !!(m_in.REQ_FLAGS & REQ_RDONLY);
  state.s_dev = m_in.REQ_DEV;

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

  m_out.RES_INODE_NR = INODE_NR(ino);
  m_out.RES_MODE = get_mode(ino, attr.a_mode);
  m_out.RES_FILE_SIZE_HI = ex64hi(attr.a_size);
  m_out.RES_FILE_SIZE_LO = ex64lo(attr.a_size);
  m_out.RES_UID = sffs_params->p_uid;
  m_out.RES_GID = sffs_params->p_gid;
  m_out.RES_DEV = NO_DEV;
  m_out.RES_CONREQS = 1;	/* We can handle only 1 request at a time */

  state.s_mounted = TRUE;

  return OK;
}

/*===========================================================================*
 *				do_unmount				     *
 *===========================================================================*/
int do_unmount()
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
