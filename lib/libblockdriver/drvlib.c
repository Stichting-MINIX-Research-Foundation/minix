/* IBM device driver utility functions.			Author: Kees J. Bot
 *								7 Dec 1995
 * Entry point:
 *   partition:	partition a disk to the partition table(s) on it.
 */

#include <minix/blockdriver.h>
#include <minix/drvlib.h>
#include <unistd.h>

/* Extended partition? */
#define ext_part(s)	((s) == 0x05 || (s) == 0x0F)

static void parse_part_table(struct blockdriver *bdp, int device, int
	style, int atapi, u8_t *tmp_buf);
static void extpartition(struct blockdriver *bdp, int extdev, unsigned
	long extbase, u8_t *tmp_buf);
static int get_part_table(struct blockdriver *bdp, int device, unsigned
	long offset, struct part_entry *table, u8_t *tmp_buf);
static void sort(struct part_entry *table);

/*============================================================================*
 *				partition				      *
 *============================================================================*/
void partition(bdp, device, style, atapi)
struct blockdriver *bdp;	/* device dependent entry points */
int device;			/* device to partition */
int style;			/* partitioning style: floppy, primary, sub. */
int atapi;			/* atapi device */
{
/* This routine is called on first open to initialize the partition tables
 * of a device.
 */
  u8_t *tmp_buf;

  if ((*bdp->bdr_part)(device) == NULL)
	return;

  /* For multithreaded drivers, multiple partition() calls may be made on
   * different devices in parallel. Hence we need a separate temporary buffer
   * for each request.
   */
  if (!(tmp_buf = alloc_contig(CD_SECTOR_SIZE, AC_ALIGN4K, NULL)))
	panic("partition: unable to allocate temporary buffer");

  parse_part_table(bdp, device, style, atapi, tmp_buf);

  free_contig(tmp_buf, CD_SECTOR_SIZE);
}

/*============================================================================*
 *				parse_part_table			      *
 *============================================================================*/
static void parse_part_table(bdp, device, style, atapi, tmp_buf)
struct blockdriver *bdp;	/* device dependent entry points */
int device;			/* device to partition */
int style;			/* partitioning style: floppy, primary, sub. */
int atapi;			/* atapi device */
u8_t *tmp_buf;			/* temporary buffer */
{
/* This routine reads and parses a partition table.  It may be called
 * recursively.  It makes sure that each partition falls safely within the
 * device's limits.  Depending on the partition style we are either making
 * floppy partitions, primary partitions or subpartitions.  Only primary
 * partitions are sorted, because they are shared with other operating
 * systems that expect this.
 */
  struct part_entry table[NR_PARTITIONS], *pe;
  int disk, par;
  struct device *dv;
  unsigned long base, limit, part_limit;

  /* Get the geometry of the device to partition */
  if ((dv = (*bdp->bdr_part)(device)) == NULL
				|| cmp64u(dv->dv_size, 0) == 0) return;
  base = div64u(dv->dv_base, SECTOR_SIZE);
  limit = base + div64u(dv->dv_size, SECTOR_SIZE);

  /* Read the partition table for the device. */
  if(!get_part_table(bdp, device, 0L, table, tmp_buf)) {
	  return;
  }

  /* Compute the device number of the first partition. */
  switch (style) {
  case P_FLOPPY:
	device += MINOR_fd0p0;
	break;
  case P_PRIMARY:
	sort(table);		/* sort a primary partition table */
	device += 1;
	break;
  case P_SUB:
	disk = device / DEV_PER_DRIVE;
	par = device % DEV_PER_DRIVE - 1;
	device = MINOR_d0p0s0 + (disk * NR_PARTITIONS + par) * NR_PARTITIONS;
  }

  /* Find an array of devices. */
  if ((dv = (*bdp->bdr_part)(device)) == NULL) return;

  /* Set the geometry of the partitions from the partition table. */
  for (par = 0; par < NR_PARTITIONS; par++, dv++) {
	/* Shrink the partition to fit within the device. */
	pe = &table[par];
	part_limit = pe->lowsec + pe->size;
	if (part_limit < pe->lowsec) part_limit = limit;
	if (part_limit > limit) part_limit = limit;
	if (pe->lowsec < base) pe->lowsec = base;
	if (part_limit < pe->lowsec) part_limit = pe->lowsec;

	dv->dv_base = mul64u(pe->lowsec, SECTOR_SIZE);
	dv->dv_size = mul64u(part_limit - pe->lowsec, SECTOR_SIZE);

	if (style == P_PRIMARY) {
		/* Each Minix primary partition can be subpartitioned. */
		if (pe->sysind == MINIX_PART)
			parse_part_table(bdp, device + par, P_SUB, atapi,
				tmp_buf);

		/* An extended partition has logical partitions. */
		if (ext_part(pe->sysind))
			extpartition(bdp, device + par, pe->lowsec, tmp_buf);
	}
  }
}

/*============================================================================*
 *				extpartition				      *
 *============================================================================*/
static void extpartition(bdp, extdev, extbase, tmp_buf)
struct blockdriver *bdp;	/* device dependent entry points */
int extdev;			/* extended partition to scan */
unsigned long extbase;		/* sector offset of the base ext. partition */
u8_t *tmp_buf;			/* temporary buffer */
{
/* Extended partitions cannot be ignored alas, because people like to move
 * files to and from DOS partitions.  Avoid reading this code, it's no fun.
 */
  struct part_entry table[NR_PARTITIONS], *pe;
  int subdev, disk, par;
  struct device *dv;
  unsigned long offset, nextoffset;

  disk = extdev / DEV_PER_DRIVE;
  par = extdev % DEV_PER_DRIVE - 1;
  subdev = MINOR_d0p0s0 + (disk * NR_PARTITIONS + par) * NR_PARTITIONS;

  offset = 0;
  do {
	if (!get_part_table(bdp, extdev, offset, table, tmp_buf)) return;
	sort(table);

	/* The table should contain one logical partition and optionally
	 * another extended partition.  (It's a linked list.)
	 */
	nextoffset = 0;
	for (par = 0; par < NR_PARTITIONS; par++) {
		pe = &table[par];
		if (ext_part(pe->sysind)) {
			nextoffset = pe->lowsec;
		} else
		if (pe->sysind != NO_PART) {
			if ((dv = (*bdp->bdr_part)(subdev)) == NULL) return;

			dv->dv_base = mul64u(extbase + offset + pe->lowsec,
								SECTOR_SIZE);
			dv->dv_size = mul64u(pe->size, SECTOR_SIZE);

			/* Out of devices? */
			if (++subdev % NR_PARTITIONS == 0) return;
		}
	}
  } while ((offset = nextoffset) != 0);
}

/*============================================================================*
 *				get_part_table				      *
 *============================================================================*/
static int get_part_table(bdp, device, offset, table, tmp_buf)
struct blockdriver *bdp;
int device;
unsigned long offset;		/* sector offset to the table */
struct part_entry *table;	/* four entries */
u8_t *tmp_buf;			/* temporary buffer */
{
/* Read the partition table for the device, return true iff there were no
 * errors.
 */
  iovec_t iovec1;
  u64_t position;
  int r;

  position = mul64u(offset, SECTOR_SIZE);
  iovec1.iov_addr = (vir_bytes) tmp_buf;
  iovec1.iov_size = CD_SECTOR_SIZE;
  r = (*bdp->bdr_transfer)(device, FALSE /*do_write*/, position, SELF,
	&iovec1, 1, BDEV_NOFLAGS);
  if (r != CD_SECTOR_SIZE) {
	return 0;
  }
  if (tmp_buf[510] != 0x55 || tmp_buf[511] != 0xAA) {
	/* Invalid partition table. */
	return 0;
  }
  memcpy(table, (tmp_buf + PART_TABLE_OFF), NR_PARTITIONS * sizeof(table[0]));
  return 1;
}

/*===========================================================================*
 *				sort					     *
 *===========================================================================*/
static void sort(table)
struct part_entry *table;
{
/* Sort a partition table. */
  struct part_entry *pe, tmp;
  int n = NR_PARTITIONS;

  do {
	for (pe = table; pe < table + NR_PARTITIONS-1; pe++) {
		if (pe[0].sysind == NO_PART
			|| (pe[0].lowsec > pe[1].lowsec
					&& pe[1].sysind != NO_PART)) {
			tmp = pe[0]; pe[0] = pe[1]; pe[1] = tmp;
		}
	}
  } while (--n > 0);
}
