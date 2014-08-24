#include "inc.h"

static struct dir_extent dir_extents[NR_DIR_EXTENT_RECORDS];

struct dir_extent* alloc_extent(void)
{
	/* Return a free extent from the pool. */
	int i;
	struct dir_extent *extent;

	for (i = 0; i < NR_DIR_EXTENT_RECORDS; i++) {
		extent = &dir_extents[i];

		if (extent->in_use == 0) {
			memset(extent, 0, sizeof(*extent));
			extent->in_use = 1;

			return extent;
		}
	}

	panic("No free extents in cache");
}

void free_extent(struct dir_extent *e)
{
	if (e == NULL)
		return;

	if (e->in_use == 0)
		panic("Trying to free unused extent");

	free_extent(e->next);
	e->in_use = 0;
}

struct buf* read_extent_block(struct dir_extent *e, size_t block)
{
	size_t block_id = get_extent_absolute_block_id(e, block);

	if (block_id == 0 || block_id >= v_pri.volume_space_size_l)
		return NULL;

	return lmfs_get_block(fs_dev, block_id, NORMAL);
}

size_t get_extent_absolute_block_id(struct dir_extent *e, size_t block)
{
	size_t extent_offset = 0;

	if (e == NULL)
		return 0;

	/* Retrieve the extent on which the block lies. */
	while(block > extent_offset + e->length) {
		if (e->next == NULL)
			return 0;

		extent_offset += e->length;
		e = e->next;
	}

	return e->location + block - extent_offset;
}

time_t date7_to_time_t(const u8_t *date)
{
	/* This function converts from the ISO 9660 7-byte time format to a
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
