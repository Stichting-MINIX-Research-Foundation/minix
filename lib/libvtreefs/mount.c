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
	fs_dev = fs_m_in.REQ_DEV;

	/* The VTreeFS must not be mounted as a root file system. */
	if (fs_m_in.REQ_FLAGS & REQ_ISROOT)
		return EINVAL;

	/* Get the root inode and increase its reference count. */
	root = get_root_inode();
	ref_inode(root);

	/* The system is now mounted. Call the initialization hook. */
	if (vtreefs_hooks->init_hook != NULL)
		vtreefs_hooks->init_hook();

	/* Return the root inode's properties. */
	fs_m_out.RES_INODE_NR = get_inode_number(root);
	fs_m_out.RES_MODE = root->i_stat.mode;
	fs_m_out.RES_FILE_SIZE_HI = 0;
	fs_m_out.RES_FILE_SIZE_LO = root->i_stat.size;
	fs_m_out.RES_UID = root->i_stat.uid;
	fs_m_out.RES_GID = root->i_stat.gid;
	fs_m_out.RES_DEV = NO_DEV;

	fs_m_out.RES_CONREQS = 1;/* We can handle only 1 request at a time */

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
