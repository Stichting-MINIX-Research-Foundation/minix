/*
 * Functions to manage the superblock of the filesystem. These functions are
 * are called at the beginning and at the end of the server.
 */

#include "inc.h"
#include <string.h>
#include <minix/com.h>
#include <minix/u64.h>
#include <minix/bdev.h>

int release_vol_pri_desc(struct iso9660_vol_pri_desc *vol_pri)
{
	/* Release the root dir root. */
	if (vol_pri->i_count > 0) {
		put_inode(vol_pri->inode_root);
		vol_pri->inode_root = NULL;
		vol_pri->i_count = 0;
	}

	return OK;
}

static int create_vol_pri_desc(struct iso9660_vol_pri_desc *vol_pri, char *buf)
{
	/*
	 * This function fullfill the super block data structure using the
	 * information contained in the buffer.
	 */
	struct iso9660_dir_record *root_record;
	struct inode_dir_entry dir_entry;
	struct dir_extent extent;
	size_t dummy_offset = 0;

	if (vol_pri->i_count > 0)
		release_vol_pri_desc(vol_pri);

	memcpy(vol_pri, buf, 2048);

	/* Check various fields for consistency. */
	if ((memcmp(vol_pri->standard_id, "CD001",
	    ISO9660_SIZE_STANDARD_ID) != 0)
	     || (vol_pri->vd_version != 1)
	     || (vol_pri->logical_block_size_l < 2048))
		return EINVAL;

	lmfs_set_blocksize(vol_pri->logical_block_size_l);
	lmfs_set_blockusage(vol_pri->volume_space_size_l,
	    vol_pri->volume_space_size_l);

	/* Read root directory record. */
	root_record = (struct iso9660_dir_record *)vol_pri->root_directory;

	extent.location =
	    root_record->loc_extent_l + root_record->ext_attr_rec_length;
	extent.length =
	    root_record->data_length_l / vol_pri->logical_block_size_l;
	extent.next = NULL;

	if (root_record->data_length_l % vol_pri->logical_block_size_l)
		extent.length++;

	if (read_inode(&dir_entry, &extent, &dummy_offset) != OK) {
		return EINVAL;
	}

	dir_entry.i_node->i_count = 1;

	vol_pri->inode_root = dir_entry.i_node;
	vol_pri->i_count = 1;

	return OK;
}

int read_vds(struct iso9660_vol_pri_desc *vol_pri, dev_t dev)
{
	/*
	 * This function reads from a ISO9660 filesystem (in the device dev)
	 * the super block and saves it in vol_pri.
	 */
	size_t offset;
	int vol_ok = FALSE, vol_pri_flag = FALSE;
	int r;
	static char sbbuf[ISO9660_MIN_BLOCK_SIZE];
	int i = 0;

	for(offset = ISO9660_SUPER_BLOCK_POSITION;
	    !vol_ok && i++ < MAX_ATTEMPTS;
	    offset += ISO9660_MIN_BLOCK_SIZE) {
		/* Read the sector of the super block. */
		r = bdev_read(dev, offset, sbbuf, ISO9660_MIN_BLOCK_SIZE,
		    BDEV_NOFLAGS);

		if (r != ISO9660_MIN_BLOCK_SIZE) {
			/* Damaged sector or what? */
			return EINVAL;
		}

		if ((sbbuf[0] & BYTE) == VD_PRIMARY) {
			/* Free already parsed descriptor, if any. */
			if (vol_pri_flag == TRUE) {
				release_vol_pri_desc(vol_pri);
				vol_pri_flag = FALSE;
			}
			/* Copy the buffer in the data structure. */
			if (create_vol_pri_desc(vol_pri, sbbuf) == OK) {
				vol_pri_flag = TRUE;
			}
		}

		if ((sbbuf[0] & BYTE) == VD_SET_TERM) {
			/* I dont need to save anything about it */
			vol_ok = TRUE;
		}
	}

	if (vol_ok == FALSE || vol_pri_flag == FALSE)
		return EINVAL;		/* If no superblock was found... */
	else
		return OK;		/* otherwise. */
}
