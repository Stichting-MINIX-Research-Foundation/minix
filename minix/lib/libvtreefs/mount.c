/* VTreeFS - mount.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"

/*===========================================================================*
 *				fs_readsuper				     *
 *===========================================================================*/
int fs_readsuper(void)
{
	/* This function gets the root inode and sends back its details.
	 */
	struct inode *root;

	/* Get the device number, for stat requests. */
	fs_dev = fs_m_in.m_vfs_fs_readsuper.device;

	/* The VTreeFS must not be mounted as a root file system. */
	if (fs_m_in.m_vfs_fs_readsuper.flags & REQ_ISROOT)
		return EINVAL;

	/* Get the root inode and increase its reference count. */
	root = get_root_inode();
	ref_inode(root);

	/* The system is now mounted. Call the initialization hook. */
	if (vtreefs_hooks->init_hook != NULL)
		vtreefs_hooks->init_hook();

	/* Return the root inode's properties. */
	fs_m_out.m_fs_vfs_readsuper.inode = get_inode_number(root);
	fs_m_out.m_fs_vfs_readsuper.mode = root->i_stat.mode;
	fs_m_out.m_fs_vfs_readsuper.file_size = root->i_stat.size;
	fs_m_out.m_fs_vfs_readsuper.uid = root->i_stat.uid;
	fs_m_out.m_fs_vfs_readsuper.gid = root->i_stat.gid;
	fs_m_out.m_fs_vfs_readsuper.device = NO_DEV;
	fs_m_out.m_fs_vfs_readsuper.flags = RES_NOFLAGS;

	fs_mounted = TRUE;

	return OK;
}

/*===========================================================================*
 *				fs_unmount				     *
 *===========================================================================*/
int fs_unmount(void)
{
	/* Unmount the file system.
	 */
	struct inode *root;

	/* Decrease the count of the root inode. */
	root = get_root_inode();

	put_inode(root);

	/* The system is unmounted. Call the cleanup hook. */
	if (vtreefs_hooks->cleanup_hook != NULL)
		vtreefs_hooks->cleanup_hook();

	/* We can now be shut down safely. */
	fs_mounted = FALSE;

	return OK;
}
