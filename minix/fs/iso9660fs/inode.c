
/*
 * This file contains all the function that handle the dir records
 * (inodes) for the ISO9660 filesystem.
 */

#include "inc.h"

static struct inode inodes[NR_INODE_RECORDS];
static struct buf* fetch_inode(struct dir_extent *extent, size_t *offset);

int fs_putnode(ino_t ino_nr, unsigned int count)
{
	/*
	 * Find the inode specified by the request message and decrease its
	 * counter.
	 */
	struct inode *i_node;

	if ((i_node = find_inode(ino_nr)) == NULL) {
		printf("ISOFS: trying to free unused inode\n");
		return EINVAL;
	}
	if (count > i_node->i_count) {
		printf("ISOFS: put_node count too high\n");
		return EINVAL;
	}

	i_node->i_count -= count - 1;
	put_inode(i_node);
	return OK;
}

struct inode* alloc_inode(void)
{
	/*
	 * Return a free inode from the pool.
	 */
	static int i;
	int end = i;
	struct inode *i_node;

	i = (i + 1) % NR_INODE_RECORDS;
	do {
		i_node = &inodes[i];

		if (i_node->i_count == 0) {
			free_extent(i_node->extent);

			memset(i_node, 0, sizeof(*i_node));
			i_node->i_count = 1;

			return i_node;
		}

		i = (i + 1) % NR_INODE_RECORDS;
	}
	while(i != end);

	panic("No free inodes in cache");
}

struct inode* find_inode(ino_t i)
{
	/* Get inode from cache. */
	int cpt;
	struct inode *i_node;

	if (i == 0)
		return NULL;

	for (cpt = 0; cpt < NR_INODE_RECORDS; cpt++) {
		i_node = &inodes[cpt];

		if ((i_node->i_stat.st_ino == i) && (i_node->i_count > 0))
			return i_node;
	}

	return NULL;
}

struct inode* get_inode(ino_t i)
{
	struct inode *i_node;
	struct dir_extent *extent;

	if (i == 0)
		return NULL;

	/* Try to get inode from cache. */
	i_node = find_inode(i);
	if (i_node != NULL) {
		dup_inode(i_node);
		return i_node;
	}

	/*
	 * Inode wasn't in cache, try to load it.
	 * FIXME: a fake extent of one logical block is created for
	 * read_inode(). Reading a inode this way could be problematic if
	 * additional extents are stored behind the block boundary.
	 */
	i_node = alloc_inode();
	extent = alloc_extent();
	extent->location = i / v_pri.logical_block_size_l;
	extent->length = 1;

	if (read_inode(i_node, extent, i % v_pri.logical_block_size_l,
	    NULL) != OK) {
		free_extent(extent);
		put_inode(i_node);
		return NULL;
	}

	free_extent(extent);
	return i_node;
}

void put_inode(struct inode *i_node)
{
	if (i_node == NULL)
		return;

	assert(i_node->i_count > 0);

	i_node->i_count--;
}

void dup_inode(struct inode *i_node)
{
	assert(i_node != NULL);

	i_node->i_count++;
}

static struct buf* fetch_inode(struct dir_extent *extent, size_t *offset)
{
	struct iso9660_dir_record *dir_rec;
	struct buf *bp;

	/*
	 * Directory entries aren't allowed to cross a logical block boundary in
	 * ISO 9660, so we keep searching until we find something or reach the
	 * end of the extent.
	 */
	bp = read_extent_block(extent, *offset / v_pri.logical_block_size_l);
	while (bp != NULL) {
		dir_rec = (struct iso9660_dir_record*)(b_data(bp) + *offset %
		          v_pri.logical_block_size_l);
		if (dir_rec->length == 0) {
			*offset -= *offset % v_pri.logical_block_size_l;
			*offset += v_pri.logical_block_size_l;
		}
		else {
			break;
		}

		lmfs_put_block(bp, FULL_DATA_BLOCK);
		bp = read_extent_block(extent, *offset /
		    v_pri.logical_block_size_l);
	}

	return bp;
}

int read_inode(struct inode *i_node, struct dir_extent *extent, size_t offset,
	size_t *new_offset)
{
	struct iso9660_dir_record *dir_rec;
	struct buf *bp;

	/* Find inode. */
	bp = fetch_inode(extent, &offset);
	if (bp == NULL)
		return EOF;

	dir_rec = (struct iso9660_dir_record*)(b_data(bp) + offset %
	          v_pri.logical_block_size_l);

	/* Parse basic ISO 9660 specs. */
	if (check_dir_record(dir_rec,
	    offset % v_pri.logical_block_size_l) != OK) {
		lmfs_put_block(bp, FULL_DATA_BLOCK);
		return EINVAL;
	}

	memset(&i_node->i_stat, 0, sizeof(struct stat));

	i_node->i_stat.st_ino = get_extent_absolute_block_id(extent,
	    offset / v_pri.logical_block_size_l) * v_pri.logical_block_size_l +
	    offset % v_pri.logical_block_size_l;

	read_inode_iso9660(i_node, dir_rec);

	/* Parse extensions. */
	read_inode_susp(i_node, dir_rec, bp,
	    offset % v_pri.logical_block_size_l);

	offset += dir_rec->length;
	read_inode_extents(i_node, dir_rec, extent, &offset);

	lmfs_put_block(bp, FULL_DATA_BLOCK);
	if (new_offset != NULL)
		*new_offset = offset;
	return OK;
}

void read_inode_iso9660(struct inode *i,
	const struct iso9660_dir_record *dir_rec)
{
	char *cp;

	/* Parse first extent. */
	if (dir_rec->data_length_l > 0) {
		assert(i->extent == NULL);
		i->extent = alloc_extent();
		i->extent->location = dir_rec->loc_extent_l +
		                      dir_rec->ext_attr_rec_length;
		i->extent->length = dir_rec->data_length_l /
		                    v_pri.logical_block_size_l;
		if (dir_rec->data_length_l % v_pri.logical_block_size_l)
			i->extent->length++;

		i->i_stat.st_size = dir_rec->data_length_l;
	}

	/* Parse timestamps (record date). */
	i->i_stat.st_atime = i->i_stat.st_mtime = i->i_stat.st_ctime =
	    i->i_stat.st_birthtime = date7_to_time_t(dir_rec->rec_date);

	if ((dir_rec->file_flags & D_TYPE) == D_DIRECTORY) {
		i->i_stat.st_mode = S_IFDIR;
		i->i_stat.st_ino =
		    i->extent->location * v_pri.logical_block_size_l;
	}
	else
		i->i_stat.st_mode = S_IFREG;
	i->i_stat.st_mode |= 0555;

	/* Parse file name. */
	if (dir_rec->file_id[0] == 0)
		strcpy(i->i_name, ".");
	else if (dir_rec->file_id[0] == 1)
		strcpy(i->i_name, "..");
	else {
		memcpy(i->i_name, dir_rec->file_id, dir_rec->length_file_id);

		/* Truncate/ignore file version suffix. */
		cp = strchr(i->i_name, ';');
		if (cp != NULL)
			*cp = '\0';
		/* Truncate dot if file has no extension. */
		if (strchr(i->i_name, '.') + 1 == cp)
			*(cp-1) = '\0';
	}

	/* Initialize stat. */
	i->i_stat.st_dev = fs_dev;
	i->i_stat.st_blksize = v_pri.logical_block_size_l;
	i->i_stat.st_blocks =
	    dir_rec->data_length_l / v_pri.logical_block_size_l;
	i->i_stat.st_nlink = 1;
}

void read_inode_extents(struct inode *i,
	const struct iso9660_dir_record *dir_rec,
	struct dir_extent *extent, size_t *offset)
{
	struct buf *bp;
	struct iso9660_dir_record *extent_rec;
	struct dir_extent *cur_extent = i->extent;
	int done = FALSE;

	/*
	 * No need to search extents if file is empty or has final directory
	 * record flag set.
	 */
	if (cur_extent == NULL ||
	    ((dir_rec->file_flags & D_NOT_LAST_EXTENT) == 0))
		return;

	while (!done) {
		bp = fetch_inode(extent, offset);
		if (bp == NULL)
			return;

		bp = read_extent_block(extent,
		    *offset / v_pri.logical_block_size_l);
		extent_rec = (struct iso9660_dir_record*)(b_data(bp) +
		    *offset % v_pri.logical_block_size_l);

		if (check_dir_record(dir_rec,
		    *offset % v_pri.logical_block_size_l) != OK) {
			lmfs_put_block(bp, FULL_DATA_BLOCK);
			return;
		}

		/* Extent entries should share the same name. */
		if ((dir_rec->length_file_id == extent_rec->length_file_id) &&
		    (memcmp(dir_rec->file_id, extent_rec->file_id,
		    dir_rec->length_file_id) == 0)) {
			/* Add the extent at the end of the linked list. */
			assert(cur_extent->next == NULL);
			cur_extent->next = alloc_extent();
			cur_extent->next->location = dir_rec->loc_extent_l +
			    dir_rec->ext_attr_rec_length;
			cur_extent->next->length = dir_rec->data_length_l /
			    v_pri.logical_block_size_l;
			if (dir_rec->data_length_l % v_pri.logical_block_size_l)
				cur_extent->next->length++;

			i->i_stat.st_size += dir_rec->data_length_l;
			i->i_stat.st_blocks += cur_extent->next->length;

			cur_extent = cur_extent->next;
			*offset += extent_rec->length;
		}
		else
			done = TRUE;

		/* Check if not last extent bit is not set. */
		if ((dir_rec->file_flags & D_NOT_LAST_EXTENT) == 0)
			done = TRUE;

		lmfs_put_block(bp, FULL_DATA_BLOCK);
	}
}

void read_inode_susp(struct inode *i, const struct iso9660_dir_record *dir_rec,
	struct buf *bp, size_t offset)
{
	int susp_offset, susp_size;
	struct rrii_dir_record rrii_data;

	susp_offset = 33 + dir_rec->length_file_id;
	/* Get rid of padding byte. */
	if(dir_rec->length_file_id % 2 == 0) {
		susp_offset++;
	}

	if(dir_rec->length - susp_offset >= 4) {
		susp_size = dir_rec->length - susp_offset;

		/* Initialize record with known, sane data. */
		memcpy(rrii_data.mtime, dir_rec->rec_date, ISO9660_SIZE_DATE7);
		memcpy(rrii_data.atime, dir_rec->rec_date, ISO9660_SIZE_DATE7);
		memcpy(rrii_data.ctime, dir_rec->rec_date, ISO9660_SIZE_DATE7);
		memcpy(rrii_data.birthtime, dir_rec->rec_date,
		    ISO9660_SIZE_DATE7);

		rrii_data.d_mode = i->i_stat.st_mode;
		rrii_data.uid    = 0;
		rrii_data.gid    = 0;
		rrii_data.rdev   = NO_DEV;
		rrii_data.file_id_rrip[0] = '\0';
		rrii_data.slink_rrip[0]   = '\0';

		parse_susp_buffer(&rrii_data, b_data(bp)+offset+susp_offset,
		    susp_size);

		/* Copy back data from rrii_dir_record structure. */
		i->i_stat.st_atime = date7_to_time_t(rrii_data.atime);
		i->i_stat.st_ctime = date7_to_time_t(rrii_data.ctime);
		i->i_stat.st_mtime = date7_to_time_t(rrii_data.mtime);
		i->i_stat.st_birthtime = date7_to_time_t(rrii_data.birthtime);

		i->i_stat.st_mode = rrii_data.d_mode;
		i->i_stat.st_uid  = rrii_data.uid;
		i->i_stat.st_gid  = rrii_data.gid;
		i->i_stat.st_rdev = rrii_data.rdev;

		if (rrii_data.file_id_rrip[0] != '\0')
			strlcpy(i->i_name, rrii_data.file_id_rrip,
			   sizeof(i->i_name));
		if (rrii_data.slink_rrip[0] != '\0')
			strlcpy(i->s_link, rrii_data.slink_rrip,
			   sizeof(i->s_link));
	}
}

int check_dir_record(const struct iso9660_dir_record *d, size_t offset)
{
	/* Run some consistency check on a directory entry. */
	if ((d->length < 33) || (d->length_file_id < 1))
		return EINVAL;
	if (d->length_file_id + 32 > d->length)
		return EINVAL;
	if (offset + d->length > v_pri.logical_block_size_l)
		return EINVAL;

	return OK;
}

int check_inodes(void)
{
	/* Check whether there are no more inodes in use. Called on unmount. */
	int i;

	for (i = 0; i < NR_INODE_RECORDS; i++)
		if (inodes[i].i_count > 0)
			return FALSE;

	return TRUE;
}
