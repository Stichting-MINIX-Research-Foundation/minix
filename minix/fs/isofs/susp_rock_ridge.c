/*
 * This file contains support for Rock Ridge Interchange Protocol (RRIP)
 * extension to ISO 9660.
 */

#include "inc.h"
#include <sys/stat.h>

#ifdef ISO9660_OPTION_ROCKRIDGE

void parse_susp_rock_ridge_plcl(struct rrii_dir_record *dir, u32_t block) {
	struct inode *rep_inode;
	struct buf *bp;
	struct iso9660_dir_record *dir_rec;
	struct dir_extent extent;
	struct inode_dir_entry dummy_dir_entry;
	size_t dummy_offset = 0;

	/* Check if inode wasn't already parsed. */
	rep_inode = inode_cache_get(block);
	if (rep_inode != NULL) {
		rep_inode->i_refcount++;
		dir->reparented_inode = rep_inode;
		return;
	}

	/* Peek ahead to build extent for read_inode. */
	if (lmfs_get_block(&bp, fs_dev, block, NORMAL) != OK)
		return;

	dir_rec = (struct iso9660_dir_record*)b_data(bp);

	extent.location = block;
	extent.length = dir_rec->data_length_l / v_pri.logical_block_size_l;
	if (dir_rec->data_length_l % v_pri.logical_block_size_l)
		extent.length++;
	extent.next = NULL;
	lmfs_put_block(bp);

	memset(&dummy_dir_entry, 0, sizeof(struct inode_dir_entry));
	/* XXX what if this fails? */
	read_inode(&dummy_dir_entry, &extent, &dummy_offset);
	free(dummy_dir_entry.r_name);
	dir->reparented_inode = dummy_dir_entry.i_node;
}

void parse_susp_rock_ridge_sl(struct rrii_dir_record *dir, char *buffer, int length)
{
	/* Parse a Rock Ridge SUSP symbolic link entry (SL). */
	int offset = 0;
	int slink_size;
	u8_t flags, component_length;

	while (offset + 2 <= length) {
		flags = *((u8_t*)(buffer + offset));
		component_length = *((u8_t*)(buffer + offset + 1));

		/* Add directory separator if necessary. */
		if (strcmp(dir->slink_rrip, "") != 0 &&
		    strcmp(dir->slink_rrip, "/") != 0) {
			slink_size = strlen(dir->slink_rrip);
			if (slink_size + 2 >= ISO9660_RRIP_MAX_FILE_ID_LEN)
				return;

			dir->slink_rrip[slink_size++] = '/';
			dir->slink_rrip[slink_size] = '\0';
		}
		else
			slink_size = strlen(dir->slink_rrip);

		switch (flags & 0xF) {
			case 0:
			case 1: {
				/*
				 * Directory path component.
				 * Check if component fits within SL entry and
				 * within symbolic link field.
				 */
				if ((component_length > length - offset) ||
				    (slink_size + component_length + 1 >=
				    ISO9660_RRIP_MAX_FILE_ID_LEN)) {
					return;
				}

				strlcat(&dir->slink_rrip[slink_size],
				        buffer + offset + 2,
				        component_length + 1);

				break;
			}
			case 2: {
				/* Current directory path component. */
				if (slink_size + 2 >=
				    ISO9660_RRIP_MAX_FILE_ID_LEN) {
					return;
				}

				strcat(&dir->slink_rrip[slink_size], ".");

				break;
			}
			case 4: {
				/* Parent directory path component. */
				if (slink_size + 3 >=
				    ISO9660_RRIP_MAX_FILE_ID_LEN) {
					return;
				}

				strcat(&dir->slink_rrip[slink_size], "..");

				break;
			}
			case 8: {
				/* Root directory path component relative to
				   the current process. */
				if (slink_size + 2 >=
				    ISO9660_RRIP_MAX_FILE_ID_LEN) {
					return;
				}

				strcat(&dir->slink_rrip[slink_size], "/");

				break;
			}
			default: {
				/* Unsupported/invalid flags. */
				return;
			}
		}

		offset += component_length + 2;
	}
}

int parse_susp_rock_ridge(struct rrii_dir_record *dir, char *buffer)
{
	/* Parse Rock Ridge SUSP entries for a directory entry. */
	char susp_signature[2];
	u8_t susp_length;
	u8_t susp_version;

	int rrii_name_current_size;
	int rrii_name_append_size;
	int rrii_tf_flags;
	int rrii_tf_offset;
	u32_t rrii_pn_rdev_major;
	u32_t rrii_pn_rdev_minor;
	mode_t rrii_px_posix_mode;
	u32_t rrii_pcl_block;

	susp_signature[0] = buffer[0];
	susp_signature[1] = buffer[1];
	susp_length = *((u8_t*)buffer + 2);
	susp_version = *((u8_t*)buffer + 3);

	if ((susp_signature[0] == 'P') && (susp_signature[1] == 'X') &&
	    (susp_length >= 36) && (susp_version >= 1)) {
		/* POSIX file mode, UID and GID. */
		dir->d_mode = *((u32_t*)(buffer + 4));
		dir->uid = *((u32_t*)(buffer + 20));
		dir->gid = *((u32_t*)(buffer + 28));

		return OK;
	}
	else if ((susp_signature[0] == 'P') && (susp_signature[1] == 'N') &&
	         (susp_length >= 20) && (susp_version >= 1)) {
		/* Device ID (for character or block special inode). */

		/*
		 * XXX: Specific to how Minix ISO is generated, will have to
		 * investigate why makefs does that later.
		 */
#if 0
		rrii_pn_rdev_major = *((u32_t*)(buffer + 4));
		rrii_pn_rdev_minor = *((u32_t*)(buffer + 12));
#else
		rrii_pn_rdev_major = *((u32_t*)(buffer + 12)) >> 8;
		rrii_pn_rdev_minor = *((u32_t*)(buffer + 12)) & 0xFF;
#endif
		dir->rdev = makedev(rrii_pn_rdev_major, rrii_pn_rdev_minor);

		return OK;
	}
	else if ((susp_signature[0] == 'S') && (susp_signature[1] == 'L') &&
	         (susp_length > 5) && (susp_version >= 1)) {
		/* Symbolic link target. Multiple entries may be used to
		   concatenate the complete path target. */
		parse_susp_rock_ridge_sl(dir, buffer + 5, susp_length - 5);

		return OK;
	}
	else if ((susp_signature[0] == 'N') && (susp_signature[1] == 'M') &&
	         (susp_length > 5) && (susp_version >= 1)) {
		/* Alternate POSIX name. Multiple entries may be used to
		   concatenate the complete filename. */
		rrii_name_current_size = strlen(dir->file_id_rrip);
		rrii_name_append_size = susp_length - 5;

		/* Concatenate only if name component fits. */
		if (rrii_name_current_size + rrii_name_append_size + 1 <
		    ISO9660_RRIP_MAX_FILE_ID_LEN) {
			strlcpy(dir->file_id_rrip + rrii_name_current_size,
			    buffer + 5, rrii_name_append_size+1);
		}

		return OK;
	}
	else if ((susp_signature[0] == 'P') && (susp_signature[1] == 'L') &&
	         (susp_length >= 12) && (susp_version >= 1)) {
		/* Reparenting ".." directory entry. */
		rrii_pcl_block = *((u32_t*)(buffer + 4));
		parse_susp_rock_ridge_plcl(dir, rrii_pcl_block);

		return OK;
	}
	else if ((susp_signature[0] == 'C') && (susp_signature[1] == 'L') &&
	         (susp_length >= 12) && (susp_version >= 1)) {
		/* Reorganize deep directory entry. */
		rrii_pcl_block = *((u32_t*)(buffer + 4));
		parse_susp_rock_ridge_plcl(dir, rrii_pcl_block);

		return OK;
	}
	else if ((susp_signature[0] == 'R') && (susp_signature[1] == 'E')) {
		/* Ignored, skip. */

		return OK;
	}
	else if ((susp_signature[0] == 'T') && (susp_signature[1] == 'F') &&
	         (susp_length >= 5) && (susp_version >= 1)) {
		/* POSIX timestamp. */
		rrii_tf_flags = buffer[5];
		rrii_tf_offset = 5;

		/*
		 * ISO 9660 17-byte time format.
		 * FIXME: 17-byte time format not supported in TF entry.
		 */
		if (rrii_tf_flags & (1 << 7)) { }

		/* ISO 9660 7-byte time format. */
		else {
			/* Creation time  */
			if ((rrii_tf_flags & (1 << 0)) &&
			    (rrii_tf_offset + ISO9660_SIZE_DATE7 <= susp_length)) {
				memcpy(dir->birthtime, buffer+rrii_tf_offset,
				       ISO9660_SIZE_DATE7);
				rrii_tf_offset += ISO9660_SIZE_DATE7;
			}

			/* Modification time */
			if ((rrii_tf_flags & (1 << 1)) &&
			    (rrii_tf_offset + ISO9660_SIZE_DATE7 <= susp_length)) {
				memcpy(dir->mtime, buffer+rrii_tf_offset,
				       ISO9660_SIZE_DATE7);
				rrii_tf_offset += ISO9660_SIZE_DATE7;
			}

			/* Last access time. */
			if ((rrii_tf_flags & (1 << 2)) &&
			    (rrii_tf_offset + ISO9660_SIZE_DATE7 <= susp_length)) {
				memcpy(dir->atime, buffer+rrii_tf_offset,
				       ISO9660_SIZE_DATE7);
				rrii_tf_offset += ISO9660_SIZE_DATE7;
			}

			/* Last attribute change time. */
			if ((rrii_tf_flags & (1 << 3)) &&
			    (rrii_tf_offset + ISO9660_SIZE_DATE7 <= susp_length)) {
				memcpy(dir->ctime, buffer+rrii_tf_offset,
				       ISO9660_SIZE_DATE7);
				rrii_tf_offset += ISO9660_SIZE_DATE7;
			}

			/* The rest is ignored. */
		}

		return OK;
	}
	else if ((susp_signature[0] == 'S') && (susp_signature[1] == 'F')) {
		/* Ignored, skip. */
		return OK;
	}

	/* Not a Rock Ridge entry. */
	return EINVAL;
}

#endif
