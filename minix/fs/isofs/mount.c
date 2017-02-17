#include "inc.h"
#include <minix/vfsif.h>

int fs_mount(dev_t dev, unsigned int __unused flags,
	struct fsdriver_node *root_node, unsigned int *res_flags)
{
	int r;

	fs_dev = dev;

	/* Open the device the file system lives on in read only mode */
	if (bdev_open(fs_dev, BDEV_R_BIT) != OK)
		return EINVAL;

	/* Read the superblock */
	r = read_vds(&v_pri, fs_dev);
	if (r != OK) {
		bdev_close(fs_dev);
		return r;
	}

	/* Return some root inode properties */
	root_node->fn_ino_nr = v_pri.inode_root->i_stat.st_ino;
	root_node->fn_mode = v_pri.inode_root->i_stat.st_mode;
	root_node->fn_size = v_pri.inode_root->i_stat.st_size;
	root_node->fn_uid = SYS_UID; /* Always root */
	root_node->fn_gid = SYS_GID; /* wheel */
	root_node->fn_dev = NO_DEV;

	*res_flags = RES_NOFLAGS;

	return r;
}

int fs_mountpt(ino_t ino_nr)
{
	/*
	 * This function looks up the mount point, it checks the condition
	 * whether the partition can be mounted on the inode or not.
	 */
	struct inode *rip;

	if ((rip = get_inode(ino_nr)) == NULL)
		return EINVAL;

	if (rip->i_mountpoint)
		return EBUSY;

	/* The inode must be a directory. */
	if ((rip->i_stat.st_mode & I_TYPE) != I_DIRECTORY)
		return ENOTDIR;

	rip->i_mountpoint = TRUE;

	return OK;
}

void fs_unmount(void)
{
	release_vol_pri_desc(&v_pri);	/* Release the super block */

	bdev_close(fs_dev);

	if (check_inodes() == FALSE)
		puts("ISOFS: unmounting with in-use inodes!\n");
}
