/* swapfs - swap a Minix file system	    Author: Niels C. Willems */


/* Swapfs, a program to convert V1 or V2 Minix file systems from big endian
   byte order to little endian and vv.

   Some examples:
   swapfs -v disk.01			! only show verbose information.
   swapfs /dev/fd0 | compress > fd0r.Z	! convert and compress filesystem.
   swapfs -v fileA fileA	! read, convert and write the same filesystem.

  This program uses one byte of heap memory for each data block (1Kbytes)
  in the file system, so with Minix-PC 16-bit you can't swap file systems
  bigger than about 32 Mbytes

  Be careful with 'swapfs fileA fileA'. If the program aborts e.g. by
  user interrupt, power failure or an inconsistent file system, you
  better have a backup of fileA

  This program only converts directories and indirect blocks of files
  that are in use. Converting indirect blocks or directories of deleted
  files is hard and not yet done.

  If you have a (1.6.xx, xx < 18) version of Minix that supports the
  mounting of reversed file systems always mount them read-only and
  avoid any attemp to modify them (mkdir, open, creat) too!
  These problems have been fixed in Minix 1.6.18.

  In this version you can get some more information about the
  file system with the -d (debug) flag.

      Please send your bug reports or ideas to ncwille@cs.vu.nl
 */


#define _POSIX_SOURCE	1

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <assert.h>

#if __STDC__ == 1
#define	_PROTOTYPE(function, params)	function params
#else
#define	_PROTOTYPE(function, params)	function()
#endif

#define BLOCK_SIZE	1024

#define BOOT_BLOCK_OFF	   (blockn_t) 0
#define SUPER_BLOCK_OFF    (blockn_t) 1

#define V1_MAGIC		0x137F
#define V2_MAGIC		0x2468
#define NINODES_OFFSET		     0
#define V1_ZONES_OFFSET		     2
#define IMAP_BLOCKS_OFFSET	     4
#define ZMAP_BLOCKS_OFFSET	     6
#define FIRSTDATAZONE_OFFSET	     8
#define LOG_ZONE_SIZE_OFFSET        10
#define MAGIC_OFFSET		    16
#define V2_ZONES_OFFSET		    20


#define NR_DIRECT_ZONES	 7
#define V1_NR_TZONES	 9
#define V2_NR_TZONES	10
#define V1_INODE_SIZE	32
#define V2_INODE_SIZE	64

#define INODE1_MODE_OFF		 0
#define INODE1_SIZE_OFF		 4
#define INODE1_DIRECT_OFF	14
#define INODE1_IND1_OFF		28
#define INODE1_IND2_OFF		30

#define INODE2_MODE_OFF		 0
#define INODE2_SIZE_OFF		 8
#define INODE2_DIRECT_OFF	24
#define INODE2_IND1_OFF		52
#define INODE2_IND2_OFF		56
#define INODE2_IND3_OFF		60

#define INODE_MODE_MASK		0xf000	/* file type mask    */
#define INODE_DIR_MODE		0x4000	/* directory         */
#define INODE_BLK_SPECIAL_MODE	0x6000	/* block special     */
#define INODE_CHR_SPECIAL_MODE  0x2000	/* character special */

#define T_MASK		0x1c
#define T_UNKNOWN	0x00
#define T_MAYBE_OLD_DIR	0x04
#define T_OLD_NON_DIR	0x08
#define T_DIR		0x0c
#define T_NON_DIR	0x10

#define INDIRECT_MASK	0x03

#define IND_PROCESSED_BIT 0x20	/* set when all blocks in ind block are
			 * marked */
#define IND_CONFLICT_BIT  0x40
#define TYPE_CONFLICT_BIT 0x80

#define DIR_ENTRY_SIZE    16

typedef enum {
  Unused_zone, Old_zone, In_use_zone
} class_t;

typedef unsigned long blockn_t;
typedef unsigned int inodesn_t;

typedef struct {
  inodesn_t ninodes;		/* # usable inodes on the minor device */
  blockn_t imap_blocks;		/* # of blocks used by inode bit map */
  blockn_t zmap_blocks;		/* # of blocks used by zone bit map */
  blockn_t firstdatazone;	/* number of first data zone */
  int log_zone_size;		/* log2 of blocks/zone */
  blockn_t zones;		/* number of zones */

  int version;			/* file system version */
  inodesn_t inodes_per_block;
  blockn_t first_imap_block;
  blockn_t first_zmap_block;
  blockn_t first_inode_block;	/* number of first block with inodes */
  size_t dzmap_size;		/* # of data zone blocks */
} super_t;


typedef struct {		/* summary of inode */
  long size;			/* current file size in bytes */
  blockn_t direct[NR_DIRECT_ZONES];	/* block numbers for direct,
					 * ind, ... */
  blockn_t ind1;		/* single indirect block number */
  blockn_t ind2;		/* double indirect block number */
  blockn_t ind3;		/* triple indirect block number */
  int ztype;			/* type of zones that belong to this inode */
} inode_t;

static int super_format[] = {2, 2, 2, 2, 2, 2, 4, 2, 2, 4, 0};
static int inode1_format[] = {2, 2, 4, 4, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0};
static int inode2_format[] = {2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
		      4, 4, 0};

static char *ind_str[4] = {"direct", "single indirect",
		   "double indirect", "triple indirect"};

static int big_endian_fs;	/* set in init_super(), 1 iff file system has
			 * big endian byte order */
static int verbose_flag;
static int debug_flag;
static int test_flag;

typedef unsigned char *dzmap_t;


int _PROTOTYPE(main, (int argc, char *argv[]));
static void _PROTOTYPE(parse_args_init_io, (int argc, char *argv[]));
static void _PROTOTYPE(rw_boot, (void));
static void _PROTOTYPE(rw_init_super, (super_t * sp));
static void _PROTOTYPE(init_dzmap, (dzmap_t * dzmap_ptr, size_t dzmap_size));
static void _PROTOTYPE(rw_ibmap, (super_t super));
static void _PROTOTYPE(rw_zbmap, (super_t super));
static void _PROTOTYPE(print_stat, (dzmap_t dzmap, super_t super));
static void _PROTOTYPE(p1_rw_inodes, (dzmap_t dzmap, super_t super));
static void _PROTOTYPE(rd_indirects, (dzmap_t dzmap, super_t super, int ind,
			       class_t required_class));
static void _PROTOTYPE(rw_data_zones, (dzmap_t dzmap, super_t super));

static int _PROTOTYPE(read_block, (char *buf, blockn_t offset));
static void _PROTOTYPE(write_block, (char *buf));
static int _PROTOTYPE(convcpy, (char *dst, char *src, int *format));
static void _PROTOTYPE(conv2_blkcpy, (char *dst, char *src));
static void _PROTOTYPE(conv4_blkcpy, (char *dst, char *src));
static void _PROTOTYPE(conv2cpy, (char *dst, char *src));
static int _PROTOTYPE(inode_size, (int version));

static void _PROTOTYPE(init_super, (super_t * sp, char *buf));
static void _PROTOTYPE(get_inode, (inode_t * ip, char *buf, int version));
static int _PROTOTYPE(check_inode, (inode_t inode, super_t super));
static int _PROTOTYPE(was_blk_special, (inode_t inode));
static int _PROTOTYPE(check_blk_number, (blockn_t num, super_t super));
static void _PROTOTYPE(cw_inode_block, (char *buf, inodesn_t ninodes,
				 int version));
static void _PROTOTYPE(proc_ind, (dzmap_t dzmap, size_t curr_ind,
			   char *buf, super_t super));
static void _PROTOTYPE(cw_dir_block, (char *buf));
static void _PROTOTYPE(dzmap_add_inode, (dzmap_t dzmap, inode_t inode,
				  super_t super));
static void _PROTOTYPE(dz_update, (dzmap_t dzmap, blockn_t blknum,
		     int new_indnum, int new_ztype, super_t super));
static class_t _PROTOTYPE(ztype_class, (int ztype));

static unsigned int _PROTOTYPE(two_bytes, (char buf[2]));
static long _PROTOTYPE(four_bytes, (char buf[4]));

static void _PROTOTYPE(fail, (char *string));
static void _PROTOTYPE(usage, (char *arg0));


int main(argc, argv)
int argc;
char *argv[];
{
  super_t super;
  dzmap_t dzmap;

  parse_args_init_io(argc, argv);
  rw_boot();
  rw_init_super(&super);
  init_dzmap(&dzmap, super.dzmap_size);
  rw_ibmap(super);
  rw_zbmap(super);
  p1_rw_inodes(dzmap, super);

  rd_indirects(dzmap, super, 3, In_use_zone);
  rd_indirects(dzmap, super, 2, In_use_zone);
  rd_indirects(dzmap, super, 1, In_use_zone);
  if (verbose_flag) putc('\n', stderr);

  print_stat(dzmap, super);
  rw_data_zones(dzmap, super);
  return 0;
}


static void parse_args_init_io(argc, argv)
int argc;
char *argv[];
{
  char *str;
  struct stat buf;
  ino_t src_ino;
  int i;

  debug_flag = 0;
  verbose_flag = 0;
  test_flag = 0;

  for (i = 1; i < argc; i++) {
	str = argv[i];
	if (*str != '-') break;
	switch (*++str) {
	    case 'v':	verbose_flag = 1;	break;
	    case 'd':
		debug_flag = 1;
		verbose_flag = 1;
		break;
	    case 't':	test_flag = 1;	break;
	    default:	usage(argv[0]);
	}
  }
  if ((argc - i == 0 && isatty(0)) || (argc - i) > 2) usage(argv[0]);

  if (argc - i > 0) {
	(void) close(0);
	if (open(argv[i], O_RDONLY) != 0) {
		fprintf(stderr, "Can't open input file %s", argv[i]);
		fail("");
	}
  }
  if (isatty(1) || argc - i == 2) {
	if (argc - i < 2)
		test_flag = 1;
	else {
		i++;
		(void) close(1);
		(void) fstat(0, &buf);
		src_ino = buf.st_ino;
		if (stat(argv[i], &buf) == 0 && src_ino == buf.st_ino) {
			/* Src and dest are the same */
			if (open(argv[i], O_WRONLY) != 1) {
				fprintf(stderr, "Can't open output file %s", argv[i]);
				fail("");
			}
		} else if (creat(argv[i], 0644) != 1) {
			fprintf(stderr, "Can't creat output file %s", argv[i]);
			fail("");
		}
	}
  }
}


static void rw_boot()
{
  char buf[BLOCK_SIZE];

  if (read_block(buf, BOOT_BLOCK_OFF) != BLOCK_SIZE)
	fail("Can't read bootblock");
  write_block(buf);
}


static void rw_init_super(sp)
super_t *sp;
{
  char ibuf[BLOCK_SIZE], obuf[BLOCK_SIZE];

  if (read_block(ibuf, SUPER_BLOCK_OFF) != BLOCK_SIZE)
	fail("Can't read superblock");

  init_super(sp, ibuf);

  memcpy(obuf, ibuf, (size_t) BLOCK_SIZE);	/* preserve 'unused' data */
  (void) convcpy(obuf, ibuf, super_format);

  write_block(obuf);
}


static void init_dzmap(dzmap_ptr, dzmap_size)
dzmap_t *dzmap_ptr;
size_t dzmap_size;
{
  if ((*dzmap_ptr = (dzmap_t) malloc(dzmap_size)) == (dzmap_t) NULL)
	fail("Not enough space for data zone map");
  memset(*dzmap_ptr, '\0', (size_t) dzmap_size);
}


static void rw_ibmap(super)
super_t super;
{
  char ibuf[BLOCK_SIZE], obuf[BLOCK_SIZE];
  blockn_t i;

  for (i = 0; i < super.imap_blocks; i++) {
	if (read_block(ibuf, super.first_imap_block + i) != BLOCK_SIZE)
		fail("Can't read inode bit map");
	conv2_blkcpy(obuf, ibuf);
	write_block(obuf);
  }
}


static void rw_zbmap(super)
super_t super;
{
  char ibuf[BLOCK_SIZE], obuf[BLOCK_SIZE];
  blockn_t i;

  for (i = 0; i < super.zmap_blocks; i++) {
	if (read_block(ibuf, super.first_zmap_block + i) != BLOCK_SIZE)
		fail("Can't read zone bit map");
	conv2_blkcpy(obuf, ibuf);
	write_block(obuf);
  }
}


static void p1_rw_inodes(dzmap, super)
dzmap_t dzmap;
super_t super;
{
  char buf[BLOCK_SIZE], *buf_ptr;
  inodesn_t i, num_inodes;
  blockn_t next_block;
  inode_t inode;


  next_block = super.first_inode_block;

  for (i = 1; i <= super.ninodes; i++) {
	if ((i - 1) % super.inodes_per_block == 0) {
		if (read_block(buf, next_block) != BLOCK_SIZE)
			fail("read failed in inode block");
		buf_ptr = buf;
		next_block++;
		num_inodes = super.ninodes + 1 - i;
		if (num_inodes > super.inodes_per_block)
			num_inodes = super.inodes_per_block;
		cw_inode_block(buf, num_inodes, super.version);
	}
	get_inode(&inode, buf_ptr, super.version);
	dzmap_add_inode(dzmap, inode, super);
	buf_ptr += inode_size(super.version);
  }
}


static void print_stat(dzmap, super)
dzmap_t dzmap;
super_t super;
{
  size_t i;
  register unsigned char dz;
  int both_conflict = 0, ind_conflict = 0, type_conflict = 0, unreferenced = 0;
  int not_in_use = 0;

  if (!verbose_flag) return;

  for (i = 0; i < super.dzmap_size; i++) {
	dz = dzmap[i];
	if (dz & IND_CONFLICT_BIT && dz & TYPE_CONFLICT_BIT)
		both_conflict++;
	else if (dz & IND_CONFLICT_BIT)
		ind_conflict++;
	else if (dz & TYPE_CONFLICT_BIT)
		type_conflict++;

	if (dz == 0) unreferenced++;
	if (ztype_class(dz & T_MASK) < In_use_zone) not_in_use++;

  }
  if (debug_flag) {
	fprintf(stderr, "%5d zone blocks with conflicting indir.\n",
		ind_conflict);
	fprintf(stderr, "%5d zone blocks with conflicting types.\n",
		type_conflict);
	fprintf(stderr, "%5d zone blocks with conflicting types and indir.\n",
		both_conflict);
	fprintf(stderr, "%5d zone blocks never referenced.\n", unreferenced);
  }
  fprintf(stderr, "%5d zone blocks not in use.\n", not_in_use);
  putc('\n', stderr);
}


static void rd_indirects(dzmap, super, ind, required_class)
dzmap_t dzmap;
super_t super;
int ind;
class_t required_class;
{
  size_t i;
  int ind_cnt;
  off_t dz_offset;
  char buf[BLOCK_SIZE];

  dz_offset = super.firstdatazone;
  ind_cnt = 0;
  for (i = 0; i < super.dzmap_size; i++) {
	if (ztype_class(dzmap[i] & T_MASK) != required_class ||
	    (dzmap[i] & INDIRECT_MASK) != ind ||
	    (dzmap[i] & IND_PROCESSED_BIT))
		continue;

	ind_cnt++;
	if (read_block(buf, dz_offset + i) != BLOCK_SIZE) {
		fprintf(stderr, "Can't read %s block", ind_str[ind]);
		fail("");
	}
	proc_ind(dzmap, i, buf, super);
  }
  if ((verbose_flag && ind_cnt > 0) || debug_flag)
	fprintf(stderr, "%5d %s zone blocks.\n", ind_cnt, ind_str[ind]);
}


static void rw_data_zones(dzmap, super)
dzmap_t dzmap;
super_t super;
{
  char ibuf[BLOCK_SIZE], obuf[BLOCK_SIZE];
  size_t i;
  int ztype, ind, last_read;
  off_t dz_offset;

  dz_offset = super.firstdatazone;
  for (i = 0; i < super.dzmap_size; i++) {
	last_read = read_block(ibuf, dz_offset + i);
	if (last_read != BLOCK_SIZE) break;

	ind = dzmap[i] & INDIRECT_MASK;
	if (ind == 0) {
		ztype = dzmap[i] & T_MASK;
		if (ztype == T_DIR)
			cw_dir_block(ibuf);
		else
			write_block(ibuf);
	} else {
		if (super.version == 1)
			conv2_blkcpy(obuf, ibuf);
		else
			conv4_blkcpy(obuf, ibuf);
		write_block(obuf);
	}
	if (verbose_flag && i && i % 1024 == 0) {
		fprintf(stderr, ".");
		fflush(stderr);
	}
  }
  if (verbose_flag && i > 1024) putc('\n', stderr);

  if (last_read != BLOCK_SIZE) for (; i < super.dzmap_size; i++)
		if (ztype_class(dzmap[i] & T_MASK) == In_use_zone)
			fail("Can't read data zone");
}


static int read_block(buf, offset)
char *buf;
blockn_t offset;
{
  static blockn_t curr_offset = 0;
  int bytes;

  if (offset != curr_offset) {
	if (lseek(0, (off_t) offset * BLOCK_SIZE, 0) == -1)
		fail("lseek failed on input file");
	curr_offset = offset;
  }
  bytes = read(0, buf, BLOCK_SIZE);
  if (bytes < 0) fail("read failed on input file");

  curr_offset += bytes;

  return bytes;
}


static void write_block(buf)
char *buf;
{
  if (test_flag) return;

  if (write(1, buf, BLOCK_SIZE) != BLOCK_SIZE)
	fail("write failed on output file");
}


static int convcpy(dst, src, format)
char *dst;
char *src;
int *format;
{
  char *old_src = src;
  register char tmp;
  int i;

  for (i = 0; format[i] > 0; i++) {
	switch (format[i]) {
	    case 1:	*dst++ = *src++;	break;
	    case 2:
		tmp = *src++;
		*dst++ = *src++;
		*dst++ = tmp;
		break;
	    case 4:
		tmp = src[0];
		dst[0] = src[3];
		dst[3] = tmp;
		tmp = src[1];
		dst[1] = src[2];
		dst[2] = tmp;
		src += 4;
		dst += 4;
		break;
	    default:
		fail("wrong format array for convcpy");
	}
  }
  return(src - old_src);
}


static void conv2_blkcpy(dst, src)
char *dst;
char *src;
{
  int i;
  register char tmp;

  for (i = 0; i < BLOCK_SIZE; i += 2) {
	tmp = *src++;
	*dst++ = *src++;
	*dst++ = tmp;
  }
}


static void conv4_blkcpy(dst, src)
char *dst;
char *src;
{
  int i;
  register char tmp;

  for (i = 0; i < BLOCK_SIZE; i += 4) {
	tmp = src[0];
	dst[0] = src[3];
	dst[3] = tmp;

	tmp = src[1];
	dst[1] = src[2];
	dst[2] = tmp;

	src += 4;
	dst += 4;
  }
}


static void conv2cpy(dst, src)
char *dst;
char *src;
{
  register char tmp;
  tmp = *src++;
  *dst++ = *src++;
  *dst++ = tmp;
}


static int inode_size(version)
int version;
{
  return(version == 1) ? V1_INODE_SIZE : V2_INODE_SIZE;
}


static void init_super(sp, buf)
super_t *sp;
char *buf;
{
  int magic;
  long imapblks, zmapblks;

  big_endian_fs = 0;		/* guess the file system is little endian */
  magic = two_bytes(buf + MAGIC_OFFSET);

  if (magic != V1_MAGIC && magic != V2_MAGIC) {
	big_endian_fs = 1;
	magic = two_bytes(buf + MAGIC_OFFSET);
  }
  switch (magic) {
      case V1_MAGIC:	sp->version = 1;	break;
      case V2_MAGIC:	sp->version = 2;	break;
      default:	fail("Not a Minix file system");
}

  if (verbose_flag) fprintf(stderr, "\nVersion = V%d, %s endian.\n",
		sp->version, big_endian_fs ? "big" : "little");

  sp->ninodes = two_bytes(buf + NINODES_OFFSET);
  imapblks = two_bytes(buf + IMAP_BLOCKS_OFFSET);
  sp->imap_blocks = imapblks;
  zmapblks = two_bytes(buf + ZMAP_BLOCKS_OFFSET);
  sp->zmap_blocks = zmapblks;
  sp->firstdatazone = two_bytes(buf + FIRSTDATAZONE_OFFSET);
  sp->log_zone_size = two_bytes(buf + LOG_ZONE_SIZE_OFFSET);

  if (sp->version == 1)
	sp->zones = two_bytes(buf + V1_ZONES_OFFSET);
  else
	sp->zones = four_bytes(buf + V2_ZONES_OFFSET);

  sp->inodes_per_block = BLOCK_SIZE / inode_size(sp->version);

  if (imapblks < 0 || zmapblks < 0 || sp->ninodes < 1 || sp->zones < 1)
	fail("Bad superblock");


  if (sp->log_zone_size != 0)
	fail("Can't swap file systems with different zone and block sizes");

  sp->first_imap_block = SUPER_BLOCK_OFF + 1;
  sp->first_zmap_block = sp->first_imap_block + sp->imap_blocks;
  sp->first_inode_block = sp->first_zmap_block + sp->zmap_blocks;

  sp->dzmap_size = sp->zones - sp->firstdatazone;
  if (verbose_flag) {
	fprintf(stderr, "nzones = %ld, ", sp->zones);
	fprintf(stderr, "ninodes = %u, ", sp->ninodes);
	fprintf(stderr, "first data zone = %ld.\n\n", sp->firstdatazone);
  }
}


static void get_inode(ip, buf, version)
inode_t *ip;
char *buf;
int version;
{
  int i;
  int mode;

  if (version == 1) {
	mode = two_bytes(buf + INODE1_MODE_OFF);
	ip->size = four_bytes(buf + INODE1_SIZE_OFF);
	ip->ind1 = two_bytes(buf + INODE1_IND1_OFF);
	ip->ind2 = two_bytes(buf + INODE1_IND2_OFF);
	ip->ind3 = 0;
	for (i = 0; i < NR_DIRECT_ZONES; i++)
		ip->direct[i] = two_bytes(buf + INODE1_DIRECT_OFF + 2 * i);
  } else {
	mode = two_bytes(buf + INODE2_MODE_OFF);
	ip->size = four_bytes(buf + INODE2_SIZE_OFF);
	ip->ind1 = four_bytes(buf + INODE2_IND1_OFF);
	ip->ind2 = four_bytes(buf + INODE2_IND2_OFF);
	ip->ind3 = four_bytes(buf + INODE2_IND3_OFF);
	for (i = 0; i < NR_DIRECT_ZONES; i++)
		ip->direct[i] = four_bytes(buf + INODE2_DIRECT_OFF + 4 * i);
  }

  if (mode == 0) {
	if (ip->size % DIR_ENTRY_SIZE == 0)
		ip->ztype = T_MAYBE_OLD_DIR;
	else
		ip->ztype = T_OLD_NON_DIR;
	if (was_blk_special(*ip)) ip->size = 0;
  } else {
	mode = mode & INODE_MODE_MASK;
	if (mode == INODE_BLK_SPECIAL_MODE || mode == INODE_CHR_SPECIAL_MODE)
		ip->size = 0;	/* prevent the use of the block numbers. */
	ip->ztype = (mode == INODE_DIR_MODE) ? T_DIR : T_NON_DIR;
  }
}


static int check_inode(inode, super)
inode_t inode;
super_t super;
{
  int i;

  for (i = 0; i < NR_DIRECT_ZONES; i++)
	if (!check_blk_number(inode.direct[i], super)) return 0;

  return(check_blk_number(inode.ind1, super) &&
	check_blk_number(inode.ind2, super) &&
	check_blk_number(inode.ind3, super));
}


static int check_blk_number(num, super)
blockn_t num;
super_t super;
{
  if (num == 0 || (num >= super.firstdatazone && num < super.zones))
	return 1;

  fprintf(stderr, "warning bad block number %ld in inode.\n", num);
  return 0;
}


static int was_blk_special(inode)
inode_t inode;
{
  int i, result;
  blockn_t block_size;

  if (inode.size % BLOCK_SIZE || inode.ind1) return 0;
  block_size = inode.size / BLOCK_SIZE;

  for (i = NR_DIRECT_ZONES - 1; i >= 0; i--)
	if (inode.direct[i] != 0) break;

  result = (i < 1 && block_size > i + 1);

  if (debug_flag && result) {
	fprintf(stderr, "old block special file detected (slot = %d).\n", i);
  }
  return result;
}


static void cw_inode_block(buf, ninodes, version)
char *buf;
inodesn_t ninodes;
int version;
{
  char output_buf[BLOCK_SIZE];
  char *src, *dst;
  inodesn_t i;
  int cnt, free_bytes;
  int *format;

  src = buf;
  dst = output_buf;

  format = (version == 1) ? inode1_format : inode2_format;
  for (i = 0; i < ninodes; i++) {
	cnt = convcpy(dst, src, format);
	src += cnt;
	dst += cnt;
  }

  assert(cnt == inode_size(version));

  free_bytes = BLOCK_SIZE - (src - buf);
  assert(free_bytes >= 0);
  if (verbose_flag && free_bytes > 0) {
	/* There is a small change that the last free inode has no
	 * matching bit in the last inode bit map block: e.g. if
	 * sp->ninodes == 8191. */
	fprintf(stderr, "%5d bytes (%d inodes) free in last inode block.\n",
		free_bytes, free_bytes / inode_size(version));
	memcpy(dst, src, (size_t) free_bytes);
  }
  write_block(output_buf);
}


static void proc_ind(dzmap, curr_ind, buf, super)
dzmap_t dzmap;
size_t curr_ind;
char *buf;
super_t super;
{
  int indnum, i, ztype;
  int word_size;		/* size of zone block number in ind. block in
			 * bytes */
  unsigned char dz, tmp_dz;
  blockn_t blk, ind_blk;
  int bad_range = 0, hidden_zero = 0, zero_flag = 0, expired = 0;
  size_t blk_index;

  dz = dzmap[curr_ind];
  indnum = dz & INDIRECT_MASK;
  ztype = dz & T_MASK;
  ind_blk = curr_ind + super.firstdatazone;

  word_size = (super.version == 1) ? 2 : 4;
  assert(indnum > 0);

  for (i = 0; i < BLOCK_SIZE; i += word_size) {
	if (word_size == 2)
		blk = two_bytes(buf + i);
	else
		blk = four_bytes(buf + i);

	if (blk == 0)
		zero_flag = 1;
	else if (blk < super.firstdatazone || blk >= super.zones)
		bad_range = 1;
	else {
		if (zero_flag) hidden_zero = 1;
		blk_index = blk - super.firstdatazone;
		tmp_dz = dzmap[blk_index];
		if (ztype_class(tmp_dz & T_MASK) == In_use_zone) expired = 1;
	}

  }

  if (ztype_class(ztype) == In_use_zone) {
	if (bad_range) {
		fprintf(stderr, "%s zone block contains ", ind_str[indnum]);
		fail("illegal value");
	}
	if ((ztype == T_DIR || indnum > 1) && hidden_zero) {
		fprintf(stderr, "WARNING: %s zone block %ld contains ",
			ind_str[indnum], ind_blk);
		fprintf(stderr, "unexpected zero block numbers\n");
	}
  } else {
	if (expired) {
		dzmap[curr_ind] &= ~(INDIRECT_MASK & IND_CONFLICT_BIT);
		return;
	}

	/* Not yet implemented. :-( if (bad_range || (indnum > 1 &&
	 * hidden_zero) || equal_values(buf, super.version ) { } */
  }

  for (i = 0; i < BLOCK_SIZE; i += word_size) {
	if (word_size == 2)
		blk = two_bytes(buf + i);
	else
		blk = four_bytes(buf + i);

	if (blk == 0) continue;
	blk_index = blk - super.firstdatazone;
	tmp_dz = dzmap[blk_index];
	if (ztype_class(tmp_dz & T_MASK) == In_use_zone) {	/* trouble */
		if ((tmp_dz & INDIRECT_MASK) == indnum - 1 &&
		    (tmp_dz & T_MASK) == ztype)
			fprintf(stderr, "WARNING: %s zone block %ld used more \
than once\n", ind_str[indnum - 1], blk);
		else {
			fprintf(stderr, "Block %ld used more than ", blk);
			fail("once with different types");
		}
	}
	dzmap[blk_index] = (dz & ~INDIRECT_MASK) | (indnum - 1);
  }
  dzmap[curr_ind] |= IND_PROCESSED_BIT;
}


static void cw_dir_block(buf)
char *buf;
{
  char output_buf[BLOCK_SIZE];
  int ino, i, old_ino_offset;

  memcpy(output_buf, buf, BLOCK_SIZE);

  for (i = 0; i < BLOCK_SIZE; i += DIR_ENTRY_SIZE) {
	ino = two_bytes(buf + i);
	if (ino == 0) {
		old_ino_offset = i + DIR_ENTRY_SIZE - 2;
		conv2cpy(output_buf + old_ino_offset, buf + old_ino_offset);
	} else
		conv2cpy(output_buf + i, buf + i);
  }
  write_block(output_buf);
}


static void dzmap_add_inode(dzmap, inode, super)
dzmap_t dzmap;
inode_t inode;
super_t super;
{
  int i;

  if (inode.size == 0 || !check_inode(inode, super)) return;

  for (i = 0; i < NR_DIRECT_ZONES; i++)
	dz_update(dzmap, inode.direct[i], 0, inode.ztype, super);

  dz_update(dzmap, inode.ind1, 1, inode.ztype, super);
  dz_update(dzmap, inode.ind2, 2, inode.ztype, super);
  dz_update(dzmap, inode.ind3, 3, inode.ztype, super);
}


static void dz_update(dzmap, blknum, new_indnum, new_ztype, super)
dzmap_t dzmap;
blockn_t blknum;
int new_indnum;
int new_ztype;
super_t super;
{
  size_t dznum;
  int old_indnum;
  int old_ztype;
  unsigned char *dz;
  char new_dz;


  if (blknum == 0) return;

  dznum = (size_t) (blknum - super.firstdatazone);

  dz = &dzmap[dznum];
  old_indnum = *dz & INDIRECT_MASK;
  old_ztype = *dz & T_MASK;

  new_dz = new_ztype | new_indnum;

  if (ztype_class(new_ztype) > ztype_class(old_ztype)) {
	*dz = new_dz;
	return;
  } else if (ztype_class(new_ztype) < ztype_class(old_ztype))
	return;

  /* Collision: old and new have the same class */

  if (ztype_class(old_ztype) == In_use_zone) {	/* trouble */
	if (new_indnum == old_indnum && new_ztype == old_ztype) {
		fprintf(stderr, "WARNING: file system corrupt, zone block %ld \
is used more than once.\n", blknum);
		return;
	}
	fprintf(stderr, "ERROR: file system corrupt, zone block %ld is used \
more than once.\n", blknum);
	fail("Can't determine its type");
  }
  assert(ztype_class(old_ztype) == Old_zone);


  if (new_indnum != old_indnum) {
	*dz |= IND_CONFLICT_BIT;
	if (new_indnum > old_indnum) {
		*dz &= ~INDIRECT_MASK;
		*dz |= new_indnum;
	}
  }
  if (new_ztype == T_MAYBE_OLD_DIR || old_ztype == T_MAYBE_OLD_DIR) {
	*dz |= TYPE_CONFLICT_BIT;
	*dz &= ~T_MASK;
	*dz |= T_MAYBE_OLD_DIR;
  }
}


static class_t ztype_class(ztype)
int ztype;
{
  class_t class;

  if (ztype == T_MAYBE_OLD_DIR || ztype == T_OLD_NON_DIR)
	class = Old_zone;
  else if (ztype == T_DIR || ztype == T_NON_DIR)
	class = In_use_zone;
  else
	class = Unused_zone;

  return class;
}


static void fail(str)
char *str;
{
  fprintf(stderr, "%s\n", str);
  exit(1);
}


static unsigned int two_bytes(buf)
char buf[2];
{
  unsigned char *ubuf = (unsigned char *) buf;

  if (big_endian_fs)
	return(ubuf[0] << 8) | ubuf[1];
  else
	return(ubuf[1] << 8) | ubuf[0];
}


static long four_bytes(buf)
char buf[4];
{
  unsigned char *ubuf = (unsigned char *) buf;
  register int r1, r2;

  if (big_endian_fs) {
	r1 = (ubuf[0] << 8) | ubuf[1];
	r2 = (ubuf[2] << 8) | ubuf[3];
  } else {
	r2 = (ubuf[1] << 8) | ubuf[0];
	r1 = (ubuf[3] << 8) | ubuf[2];
  }
  return((long) r1 << 16) | r2;
}


static void usage(arg0)
char *arg0;
{
  fprintf(stderr, "usage: %s [-v] srcfs [destfs]\n", arg0);
  exit(2);
}
