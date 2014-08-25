/* VTreeFS - mount.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"
#include <minix/vfsif.h>

/*===========================================================================*
 *				fs_mount				     *
 *===========================================================================*/
int fs_mount(dev_t dev, unsigned int flags, struct fsdriver_node *root_node,
	unsigned int *res_flags)
{
	/* This function gets the root inode and sends back its details.
	 */
	struct inode *root;

	/* Get the device number, for stat requests. */
	fs_dev = dev;

	/* The VTreeFS must not be mounted as a root file system. */
	if (flags & REQ_ISROOT)
		return EINVAL;

	/* Get the root inode and increase its reference count. */
	root = get_root_inode();
	ref_inode(root);

	/* The system is now mounted. Call the initialization hook. */
	if (vtreefs_hooks->init_hook != NULL)
		vtreefs_hooks->init_hook();

	/* Return the root inode's properties. */
	root_node->fn_ino_nr = get_inode_number(root);
	root_node->fn_mode = root->i_stat.mode;
	root_node->fn_size = root->i_stat.size;
	root_node->fn_uid = root->i_stat.uid;
	root_node->fn_gid = root->i_stat.gid;
	root_node->fn_dev = NO_DEV;

	*res_flags = RES_NOFLAGS;

	return OK;
}

/*===========================================================================*
 *				fs_unmount				     *
 *===========================================================================*/
void fs_unmount(void)
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
}
