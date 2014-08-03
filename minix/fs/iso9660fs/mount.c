#include "inc.h"
#include <minix/vfsif.h>
#include <minix/bdev.h>
#include "const.h"
#include "glo.h"

int fs_readsuper() {
	cp_grant_id_t label_gid;
	size_t label_len;
	int r = OK;

	fs_dev    = fs_m_in.m_vfs_fs_readsuper.device;
	label_gid = fs_m_in.m_vfs_fs_readsuper.grant;
	label_len = fs_m_in.m_vfs_fs_readsuper.path_len;

	if (label_len > sizeof(fs_dev_label)) 
		return EINVAL;

	r = sys_safecopyfrom(fs_m_in.m_source, label_gid, 0, (vir_bytes)fs_dev_label,
		       label_len);
	if (r != OK) {
		printf("ISOFS %s:%d safecopyfrom failed: %d\n", __FILE__, __LINE__, r);
		return EINVAL;
	}

	/* Map the driver label for this major */
	bdev_driver(fs_dev, fs_dev_label);

	/* Open the device the file system lives on in read only mode */
	if (bdev_open(fs_dev, BDEV_R_BIT) != OK) {
		return EINVAL;
	}

	/* Read the superblock */
	r = read_vds(&v_pri, fs_dev);
	if (r != OK) {
		bdev_close(fs_dev);
		return r;
	}

	/* Return some root inode properties */
	fs_m_out.m_fs_vfs_readsuper.inode = v_pri.inode_root->i_stat.st_ino;
	fs_m_out.m_fs_vfs_readsuper.mode =  v_pri.inode_root->i_stat.st_mode;
	fs_m_out.m_fs_vfs_readsuper.file_size = v_pri.inode_root->i_stat.st_size;
	fs_m_out.m_fs_vfs_readsuper.uid = SYS_UID; /* Always root */
	fs_m_out.m_fs_vfs_readsuper.gid = SYS_GID; /* operator */
	fs_m_out.m_fs_vfs_readsuper.flags = RES_NOFLAGS;

	return r;
}

int fs_mountpoint()
{
	/*
	 * This function looks up the mount point, it checks the condition
	 * whether the partition can be mounted on the inode or not.
	 */

	struct inode *rip;
	int r = OK;

	/* Temporarily open the file. */
	if ((rip = find_inode(fs_m_in.m_vfs_fs_mountpoint.inode)) == NULL)
		return EINVAL;

	if (rip->i_mountpoint)
		r = EBUSY;

	/* If the inode is not a dir returns error */
	if ((rip->i_stat.st_mode & I_TYPE) != I_DIRECTORY)
		r = ENOTDIR;

	put_inode(rip);

	if (r == OK)
		rip->i_mountpoint = TRUE;

	return r;
}

int fs_unmount(void)
{
	release_vol_pri_desc(&v_pri);	/* Release the super block */
	bdev_close(fs_dev);
	unmountdone = TRUE;

	return OK;
}

