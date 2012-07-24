/* Filter driver - middle layer - checksumming */

#include "inc.h"
#include "crc.h"
#include "md5.h"

#define GROUP_SIZE	(SECTOR_SIZE * NR_SUM_SEC)
#define SEC2SUM_NR(nr)	((nr)/NR_SUM_SEC*(NR_SUM_SEC+1) + NR_SUM_SEC)
#define LOG2PHYS(nr)	((nr)/NR_SUM_SEC*(NR_SUM_SEC+1) + (nr)%NR_SUM_SEC)

#define POS2SEC(nr)	div64u((nr), SECTOR_SIZE)
#define SEC2POS(nr)	mul64u((nr), SECTOR_SIZE)

/* Data buffers. */
static char *ext_array, *ext_buffer;	/* interspersed buffer */
static char *rb0_array;			/* write readback buffer for disk 0 */
static char *rb1_array;			/* write readback buffer for disk 1 */

/*===========================================================================*
 *				sum_init				     *
 *===========================================================================*/
void sum_init(void)
{
	/* Initialize buffers. */

	ext_array = flt_malloc(SBUF_SIZE, NULL, 0);
	rb0_array = flt_malloc(SBUF_SIZE, NULL, 0);
	rb1_array = flt_malloc(SBUF_SIZE, NULL, 0);

	if (ext_array == NULL || rb0_array == NULL || rb1_array == NULL)
		panic("no memory available");
}

/*===========================================================================*
 *				calc_sum				     *
 *===========================================================================*/
static void calc_sum(unsigned sector, char *data, char *sum)
{
	/* Compute the checksum for a sector. The sector number must be part
	 * of the checksum in some way.
	 */
	unsigned long crc, *p, *q;
	int i, j;
	struct MD5Context ctx;

	switch(SUM_TYPE) {
	case ST_NIL:
		/* No checksum at all */

		q = (unsigned long *) sum;
		*q = sector;

		break;

	case ST_XOR:
		/* Basic XOR checksum */
		p = (unsigned long *) data;

		memset(sum, 0, SUM_SIZE);
		for(i = 0; i < SECTOR_SIZE / SUM_SIZE; i++) {
			q = (unsigned long *) sum;
			for(j = 0; (size_t) j < SUM_SIZE / sizeof(*p); j++) {
				*q ^= *p;
				q++;
				p++;
			}
		}
		q = (unsigned long *) sum;
		*q ^= sector;

		break;

	case ST_CRC:
		/* CRC32 checksum */

		crc = compute_crc((unsigned char *) data, SECTOR_SIZE);

		q = (unsigned long *) sum;

		*q = crc ^ sector;

		break;

	case ST_MD5:
		/* MD5 checksum */

		MD5Init(&ctx);
		MD5Update(&ctx, (unsigned char *) data, SECTOR_SIZE);
		MD5Update(&ctx, (unsigned char *) &sector, sizeof(sector));
		MD5Final((unsigned char *) sum, &ctx);

		break;

	default:
		panic("invalid checksum type: %d", SUM_TYPE);
	}
}

/*===========================================================================*
 *				read_sectors				     *
 *===========================================================================*/
static int read_sectors(char *buf, sector_t phys_sector, int count)
{
	/* Read 'count' sectors starting at 'phys_sector' into 'buf'. If an
	 * EOF occurs, zero-fill the remaining part of the buffer.
	 */
	size_t size, wsize;
	int r;

	size = wsize = count * SECTOR_SIZE;

	r = read_write(SEC2POS(phys_sector), buf, buf, &size, FLT_READ);

	if (r != OK)
		return r;

	if (size != wsize) {
#if DEBUG
		printf("Filter: EOF reading sector %lu\n", phys_sector);
#endif

		memset(buf + size, 0, wsize - size);
	}

	return OK;
}

/*===========================================================================*
 *				make_group_sum				     *
 *===========================================================================*/
static void make_group_sum(char *bufp, char *sump, sector_t sector, int index,
 int count)
{
	/* Compute checksums for 'count' sectors within a group, starting at
	 * sector 'index' into the group, which has logical sector number
	 * 'sector'. The 'bufp' pointer points to the same first sector to
	 * start checksumming; 'sump' is a pointer to the checksum sector.
	 */

	sump += index * SUM_SIZE;

	while (count--) {
		calc_sum(sector, bufp, sump);

		bufp += SECTOR_SIZE;

		sump += SUM_SIZE;
		sector++;
	}
}

/*===========================================================================*
 *				check_group_sum				     *
 *===========================================================================*/
static int check_group_sum(char *bufp, const char *sump, sector_t sector,
  int index, int count)
{
	/* Check checksums in a group. Parameters are the same as in
	 * make_group_sum(). Return OK if all checksums check out, or RET_REDO
	 * upon failure.
	 */
	char sum_buffer[SECTOR_SIZE];

	sump += index * SUM_SIZE;

	while (count--) {
		calc_sum(sector, bufp, sum_buffer);

		if (memcmp(sum_buffer, sump, SUM_SIZE)) {
			printf("Filter: BAD CHECKSUM at sector %lu\n", sector);

			if (BAD_SUM_ERROR)
				return bad_driver(DRIVER_MAIN, BD_DATA, EIO);
		}

		bufp += SECTOR_SIZE;
		sump += SUM_SIZE;
		sector++;
	}

	return OK;
}

/*===========================================================================*
 *				make_sum				     *
 *===========================================================================*/
static int make_sum(sector_t current_sector, sector_t sectors_left)
{
	/* Compute checksums over all data in the buffer with expanded data.
	 * As side effect, possibly read in first and last checksum sectors
	 * and data to fill the gap between the last data sector and the last
	 * checksum sector.
	 */
	sector_t sector_in_group, group_left;
	size_t size, gap;
	char *extp;
	int r;

	/* See the description of the extended buffer in transfer(). A number
	 * of points are relevant for this function in particular:
	 *
	 * 1) If the "xx" head of the buffer does not cover an entire group,
	 *    we need to copy in the first checksum sector so that we can
	 *    modify it.
	 * 2) We can generate checksums for the full "yyyyy" groups without
	 *    copying in the corresponding checksum sectors first, because
	 *    those sectors will be overwritten entirely anyway.
	 * 3) We copy in not only the checksum sector for the group containing
	 *    the "zzz" tail data, but also all the data between "zzz" and the
	 *    last checksum sector. This allows us to write all the data in
	 *    the buffer in one operation. In theory, we could verify the
	 *    checksum of the data in this gap for extra early failure
	 *    detection, but we currently do not do this.
	 *
	 * If points 1 and 3 cover the same group (implying a small, unaligned
	 * write operation), the read operation is done only once. Whether
	 * point 1 or 3 is skipped depends on whether there is a gap before
	 * the checksum sector.
	 */

	sector_in_group = current_sector % NR_SUM_SEC;
	group_left = NR_SUM_SEC - sector_in_group;

	extp = ext_buffer;

	/* This loop covers points 1 and 2. */
	while (sectors_left >= group_left) {
		size = group_left * SECTOR_SIZE;

		if (sector_in_group > 0) {
			if ((r = read_sectors(extp + size,
					LOG2PHYS(current_sector) + group_left,
					1)) != OK)
				return r;
		}
		else memset(extp + size, 0, SECTOR_SIZE);

		make_group_sum(extp, extp + size, current_sector,
			sector_in_group, group_left);

		extp += size + SECTOR_SIZE;

		sectors_left -= group_left;
		current_sector += group_left;

		sector_in_group = 0;
		group_left = NR_SUM_SEC;
	}

	/* The remaining code covers point 3. */
	if (sectors_left > 0) {
		size = sectors_left * SECTOR_SIZE;

		if (group_left != NR_SUM_SEC - sector_in_group)
			panic("group_left assertion: %d", 0);

		gap = group_left - sectors_left;

		if (gap <= 0)
			panic("gap assertion: %d", 0);

		if ((r = read_sectors(extp + size,
				LOG2PHYS(current_sector) + sectors_left,
				gap + 1)) != OK)
			return r;

		make_group_sum(extp, extp + size + gap * SECTOR_SIZE,
			current_sector, sector_in_group, sectors_left);
	}

	return OK;
}

/*===========================================================================*
 *				check_sum				     *
 *===========================================================================*/
static int check_sum(sector_t current_sector, size_t bytes_left)
{
	/* Check checksums of all data in the buffer with expanded data.
	 * Return OK if all checksums are okay, or RET_REDO upon failure.
	 */
	sector_t sector_in_group;
	size_t size, groupbytes_left;
	int count;
	char *extp;

	extp = ext_buffer;

	sector_in_group = current_sector % NR_SUM_SEC;
	groupbytes_left = (NR_SUM_SEC - sector_in_group) * SECTOR_SIZE;

	while (bytes_left > 0) {
		size = MIN(bytes_left, groupbytes_left);
		count = size / SECTOR_SIZE;

		if (check_group_sum(extp, extp + groupbytes_left,
				current_sector, sector_in_group, count))
			return RET_REDO;

		extp += size + SECTOR_SIZE;

		bytes_left -= MIN(size + SECTOR_SIZE, bytes_left);
		current_sector += count;

		sector_in_group = 0;
		groupbytes_left = GROUP_SIZE;
	}

	return OK;
}

/*===========================================================================*
 *				check_write				     *
 *===========================================================================*/
static int check_write(u64_t pos, size_t size)
{
	/* Read back the data just written, from both disks if mirroring is
	 * enabled, and check the result against the original. Return OK on
	 * success; report the malfunctioning driver and return RET_REDO
	 * otherwise.
	 */
	char *rb0_buffer, *rb1_buffer;
	size_t orig_size;
	int r;

	if (size == 0)
		return OK;

	rb0_buffer = rb1_buffer =
		flt_malloc(size, rb0_array, SBUF_SIZE);
	if (USE_MIRROR)
		rb1_buffer = flt_malloc(size, rb1_array, SBUF_SIZE);

	orig_size = size;

	r = read_write(pos, rb0_buffer, rb1_buffer, &size, FLT_READ2);

	if (r != OK) {
		if (USE_MIRROR) flt_free(rb1_buffer, orig_size, rb1_array);
		flt_free(rb0_buffer, orig_size, rb0_array);

		return r;
	}

	/* If we get a size smaller than what we requested, then we somehow
	 * succeeded in writing past the disk end, and now fail to read it all
	 * back. This is not an error, and we just compare the part that we
	 * did manage to read back in.
	 */

	if (memcmp(ext_buffer, rb0_buffer, size)) {
#if DEBUG
		printf("Filter: readback from disk 0 failed (size %d)\n",
			size);
#endif

		return bad_driver(DRIVER_MAIN, BD_DATA, EFAULT);
	}

	if (USE_MIRROR && memcmp(ext_buffer, rb1_buffer, size)) {
#if DEBUG
		printf("Filter: readback from disk 1 failed (size %d)\n",
			size);
#endif

		return bad_driver(DRIVER_BACKUP, BD_DATA, EFAULT);
	}

	if (USE_MIRROR) flt_free(rb1_buffer, orig_size, rb1_array);
	flt_free(rb0_buffer, orig_size, rb0_array);

	return OK;
}

/*===========================================================================*
 *				expand					     *
 *===========================================================================*/
static void expand(sector_t first_sector, char *buffer, sector_t sectors_left)
{
	/* Expand the contiguous data in 'buffer' to interspersed format in
	 * 'ext_buffer'. The checksum areas are not touched.
	 */
	char *srcp, *dstp;
	sector_t group_left;
	size_t size;
	int count;

	srcp = buffer;
	dstp = ext_buffer;

	group_left = NR_SUM_SEC - first_sector % NR_SUM_SEC;

	while (sectors_left > 0) {
		count = MIN(sectors_left, group_left);
		size = count * SECTOR_SIZE;

		memcpy(dstp, srcp, size);

		srcp += size;
		dstp += size + SECTOR_SIZE;

		sectors_left -= count;
		group_left = NR_SUM_SEC;
	}
}

/*===========================================================================*
 *				collapse				     *
 *===========================================================================*/
static void collapse(sector_t first_sector, char *buffer, size_t *sizep)
{
	/* Collapse the interspersed data in 'ext_buffer' to contiguous format
	 * in 'buffer'. As side effect, adjust the given size to reflect the
	 * resulting contiguous data size.
	 */
	char *srcp, *dstp;
	size_t size, bytes_left, groupbytes_left;

	srcp = ext_buffer;
	dstp = buffer;

	bytes_left = *sizep;
	groupbytes_left =
		(NR_SUM_SEC - first_sector % NR_SUM_SEC) * SECTOR_SIZE;

	while (bytes_left > 0) {
		size = MIN(bytes_left, groupbytes_left);

		memcpy(dstp, srcp, size);

		srcp += size + SECTOR_SIZE;
		dstp += size;

		bytes_left -= MIN(size + SECTOR_SIZE, bytes_left);
		groupbytes_left = GROUP_SIZE;
	}

	*sizep = dstp - buffer;
}

/*===========================================================================*
 *				expand_sizes				     *
 *===========================================================================*/
static size_t expand_sizes(sector_t first_sector, sector_t nr_sectors,
	size_t *req_size)
{
	/* Compute the size of the data area including interspersed checksum
	 * sectors (req_size) and the size of the data area including
	 * interspersed and trailing checksum sectors (the return value).
	 */
	sector_t last_sector, sum_sector, phys_sector;

	last_sector = LOG2PHYS(first_sector + nr_sectors - 1);

	sum_sector = SEC2SUM_NR(first_sector + nr_sectors - 1);

	phys_sector = LOG2PHYS(first_sector);

	*req_size = (last_sector - phys_sector + 1) * SECTOR_SIZE;

	return (sum_sector - phys_sector + 1) * SECTOR_SIZE;
}

/*===========================================================================*
 *				collapse_size				     *
 *===========================================================================*/
static void collapse_size(sector_t first_sector, size_t *sizep)
{
	/* Compute the size of the contiguous user data written to disk, given
	 * the result size of the write operation with interspersed checksums.
	 */
	sector_t sector_in_group;
	size_t sectors_from_group_base, nr_sum_secs, nr_data_secs;

	sector_in_group = first_sector % NR_SUM_SEC;

	sectors_from_group_base = *sizep / SECTOR_SIZE + sector_in_group;

	nr_sum_secs = sectors_from_group_base / (NR_SUM_SEC+1);

	nr_data_secs = sectors_from_group_base - sector_in_group - nr_sum_secs;

	*sizep = nr_data_secs * SECTOR_SIZE;
}

/*===========================================================================*
 *				transfer				     *
 *===========================================================================*/
int transfer(u64_t pos, char *buffer, size_t *sizep, int flag_rw)
{
	/* Transfer data in interspersed-checksum format. When writing, first
	 * compute checksums, and read back the written data afterwards. When
	 * reading, check the stored checksums afterwards.
	 */
	sector_t first_sector, nr_sectors;
	size_t ext_size, req_size, res_size;
	u64_t phys_pos;
	int r;

	/* If we don't use checksums or even checksum layout, simply pass on
	 * the request to the drivers as is.
	 */
	if (!USE_SUM_LAYOUT)
		return read_write(pos, buffer, buffer, sizep, flag_rw);

	/* The extended buffer (for checksumming) essentially looks like this:
	 *
	 *  ------------------------------
	 *  |xx|C|yyyyy|C|yyyyy|C|zzz  |C|
	 *  ------------------------------
	 *
	 * In this example, "xxyyyyyyyyyyzzz" is our actual data. The data is
	 * split up into groups, so that each group is followed by a checksum
	 * sector C containing the checksums for all data sectors in that
	 * group. The head and tail of the actual data may cover parts of
	 * groups; the remaining data (nor their checksums) are not to be
	 * modified.
	 *
	 * The entire buffer is written or read in one operation: the
	 * read_write() call below. In order to write, we may first have to
	 * read some data; see the description in make_sum().
	 *
	 * Some points of interest here:
	 * - We need a buffer large enough to hold the all user and non-user
	 *   data, from the first "xx" to the last checksum sector. This size
	 *   is ext_size.
	 * - For writing, we need to expand the user-provided data from
	 *   contiguous layout to interspersed format. The size of the user
	 *   data after expansion is req_size.
	 * - For reading, we need to collapse the user-requested data from
	 *   interspersed to contiguous format. For writing, we still need to
	 *   compute the contiguous result size to return to the user.
	 * - In both cases, the result size may be different from the
	 *   requested write size, because an EOF (as in, disk end) may occur
	 *   and the resulting size is less than the requested size.
	 * - If we only follow the checksum layout, and do not do any
	 *   checksumming, ext_size is reduced to req_size.
	 */

	first_sector = POS2SEC(pos);
	nr_sectors = *sizep / SECTOR_SIZE;
	phys_pos = SEC2POS(LOG2PHYS(first_sector));

#if DEBUG2
	printf("Filter: transfer: pos 0x%lx:0x%lx -> phys_pos 0x%lx:0x%lx\n",
		ex64hi(pos), ex64lo(pos), ex64hi(phys_pos), ex64lo(phys_pos));
#endif

	/* Compute the size for the buffer and for the user data after
	 * expansion.
	 */
	ext_size = expand_sizes(first_sector, nr_sectors, &req_size);

	if (!USE_CHECKSUM)
		ext_size = req_size;

	ext_buffer = flt_malloc(ext_size, ext_array, SBUF_SIZE);

	if (flag_rw == FLT_WRITE) {
		expand(first_sector, buffer, nr_sectors);

		if (USE_CHECKSUM && make_sum(first_sector, nr_sectors))
			return RET_REDO;
	}

	/* Perform the actual I/O. */
	res_size = ext_size;
	r = read_write(phys_pos, ext_buffer, ext_buffer, &res_size, flag_rw);

#if DEBUG2
	printf("Filter: transfer: read_write(%"PRIx64", %u, %d) = %d, %u\n",
		phys_pos, ext_size, flag_rw, r, res_size);
#endif

	if (r != OK) {
		flt_free(ext_buffer, ext_size, ext_array);

		return r;
	}

	/* Limit the resulting size to the user data part of the buffer.
	 * The resulting size may already be less, due to an EOF.
	 */
	*sizep = MIN(req_size, res_size);

	if (flag_rw == FLT_WRITE) {
		if (USE_CHECKSUM && check_write(phys_pos, res_size))
			return RET_REDO;

		collapse_size(first_sector, sizep);
	}
	else { /* FLT_READ */
		if (USE_CHECKSUM && check_sum(first_sector, *sizep))
			return RET_REDO;

		collapse(first_sector, buffer, sizep);
	}

	flt_free(ext_buffer, ext_size, ext_array);

	return OK;
}

/*===========================================================================*
 *				convert					     *
 *===========================================================================*/
u64_t convert(u64_t size)
{
	/* Given a raw disk size, subtract the amount of disk space used for
	 * checksums, resulting in the user-visible disk size.
	 */
	sector_t sectors;

	if (!USE_SUM_LAYOUT)
		return size;

	sectors = POS2SEC(size);

	return SEC2POS(sectors / (NR_SUM_SEC + 1) * NR_SUM_SEC);
}
