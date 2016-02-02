#include "inc.h"

void free_extent(struct dir_extent *e) {
	if (e == NULL)
		return;

	free_extent(e->next);
	free(e);
}

/* Free the contents of an inode dir entry, but not the pointer itself. */
void free_inode_dir_entry(struct inode_dir_entry *e) {
	if (e == NULL)
		return;

	free(e->r_name);
	e->r_name = NULL;
}

struct buf* read_extent_block(struct dir_extent *e, size_t block) {
	size_t block_id = get_extent_absolute_block_id(e, block);
	struct buf *bp;

	if (block_id == 0 || block_id >= v_pri.volume_space_size_l)
		return NULL;

	if(lmfs_get_block(&bp, fs_dev, block_id, NORMAL) != OK)
		return NULL;

	return bp;
}

size_t get_extent_absolute_block_id(struct dir_extent *e, size_t block) {
	size_t extent_offset = 0;
	block /= v_pri.logical_block_size_l;

	if (e == NULL)
		return 0;

	/* Retrieve the extent on which the block lies. */
	while(block >= extent_offset + e->length) {
		if (e->next == NULL)
			return 0;

		extent_offset += e->length;
		e = e->next;
	}

	return e->location + block - extent_offset;
}

time_t date7_to_time_t(const u8_t *date) {
	/*
	 * This function converts from the ISO 9660 7-byte time format to a
	 * time_t.
	 */
	struct tm ltime;
	signed char time_zone = (signed char)date[6];

	ltime.tm_year = date[0];
	ltime.tm_mon = date[1] - 1;
	ltime.tm_mday = date[2];
	ltime.tm_hour = date[3];
	ltime.tm_min = date[4];
	ltime.tm_sec = date[5];
	ltime.tm_isdst = 0;

	/* Offset from Greenwich Mean Time */
	if (time_zone >= -52 && time_zone <= 52)
		ltime.tm_hour += time_zone;

	return mktime(&ltime);
}

void* alloc_mem(size_t s) {
	void *ptr = calloc(1, s);
	assert(ptr != NULL);

	return ptr;
}
