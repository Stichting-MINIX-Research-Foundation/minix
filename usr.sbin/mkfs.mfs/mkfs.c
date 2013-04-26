/* mkfs  -  make the MINIX filesystem	Authors: Tanenbaum et al. */

/*	Authors: Andy Tanenbaum, Paul Ogilvie, Frans Meulenbroeks, Bruce Evans */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(__minix)
#include <minix/minlib.h>
#include <minix/partition.h>
#include <minix/u64.h>
#include <sys/ioctl.h>
#elif defined(__linux__)
#include <mntent.h>
#endif

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Definition of the file system layout: */
#include "const.h"
#include "type.h"
#include "mfsdir.h"
#include "super.h"

#define INODE_MAP	START_BLOCK
/* inode zone indexes pointing to single and double indirect zones */
#define S_INDIRECT_IDX	(NR_DZONES)
#define D_INDIRECT_IDX	(NR_DZONES+1)


#define MAX_TOKENS          10
#define LINE_LEN           200
/* XXX why do we not use 0 / SU_ID ? */
#define BIN                  2
#define BINGRP               2

#if !defined(__minix)
#define mul64u(a,b)	((uint64_t)(a) * (b))
#define lseek64(a,b,c,d) lseek(a,b,c)
#endif

/* some Minix specific types that do not conflict with Posix */
#ifndef block_t
typedef uint32_t block_t;	/* block number */
#endif
#ifndef zone_t
typedef uint32_t zone_t;	/* zone number */
#endif
#ifndef bit_t
typedef uint32_t bit_t;		/* bit number in a bit map */
#endif
#ifndef bitchunk_t
typedef uint32_t bitchunk_t;	/* collection of bits in a bitmap */
#endif

struct fs_size {
  ino_t inocount; /* amount of inodes */
  zone_t zonecount; /* amount of zones */
  block_t blockcount; /* amount of bloks */
};

extern char *optarg;
extern int optind;

block_t nrblocks;
int zone_per_block, zone_shift = 0;
zone_t next_zone, zoff, nr_indirzones;
int inodes_per_block, indir_per_block, indir_per_zone;
unsigned int zone_size;
ino_t nrinodes, inode_offset, next_inode;
int lct = 0, fd, print = 0;
int simple = 0, dflag = 0, verbose = 0;
int donttest;			/* skip test if it fits on medium */
char *progname;

time_t current_time;
char *zero;
unsigned char *umap_array;	/* bit map tells if block read yet */
size_t umap_array_elements;
block_t zone_map;		/* where is zone map? (depends on # inodes) */
#ifndef MFS_STATIC_BLOCK_SIZE
size_t block_size;
#else
#define block_size	MFS_STATIC_BLOCK_SIZE
#endif

FILE *proto;

int main(int argc, char **argv);
void detect_fs_size(struct fs_size * fssize);
void sizeup_dir(struct fs_size * fssize);
block_t sizeup(char *device);
static int bitmapsize(bit_t nr_bits, size_t blk_size);
void super(zone_t zones, ino_t inodes);
void rootdir(ino_t inode);
void enter_symlink(ino_t inode, char *link);
int dir_try_enter(zone_t z, ino_t child, char const *name);
void eat_dir(ino_t parent);
void eat_file(ino_t inode, int f);
void enter_dir(ino_t parent, char const *name, ino_t child);
void add_zone(ino_t n, zone_t z, size_t bytes, time_t cur_time);
void incr_link(ino_t n);
void incr_size(ino_t n, size_t count);
static ino_t alloc_inode(int mode, int usrid, int grpid);
static zone_t alloc_zone(void);
void insert_bit(block_t block, bit_t bit);
int mode_con(char *p);
void get_line(char line[LINE_LEN], char *parse[MAX_TOKENS]);
void check_mtab(const char *devname);
time_t file_time(int f);
__dead void pexit(char const *s, ...) __printflike(1,2);
void *alloc_block(void);
void print_fs(void);
int read_and_set(block_t n);
void special(char *string);
__dead void usage(void);
void get_block(block_t n, void *buf);
void get_super_block(void *buf);
void put_block(block_t n, void *buf);

/*================================================================
 *                    mkfs  -  make filesystem
 *===============================================================*/
int
main(int argc, char *argv[])
{
  int nread, mode, usrid, grpid, ch, extra_space_percent;
  block_t blocks, maxblocks;
  ino_t inodes, root_inum;
  time_t bin_time;
  char *token[MAX_TOKENS], line[LINE_LEN], *sfx;
  struct stat statbuf;
  struct fs_size fssize;

  progname = argv[0];

  /* Get two times, the current time and the mod time of the binary of
   * mkfs itself.  When the -d flag is used, the later time is put into
   * the i_mtimes of all the files.  This feature is useful when
   * producing a set of file systems, and one wants all the times to be
   * identical. First you set the time of the mkfs binary to what you
   * want, then go.
   */
  current_time = time((time_t *) 0);	/* time mkfs is being run */
  if (stat(progname, &statbuf)) {
	perror("stat of itself");
	bin_time = current_time;	/* provide some default value */
  } else
	bin_time = statbuf.st_mtime;	/* time when mkfs binary was last modified */

  /* Process switches. */
  blocks = 0;
  inodes = 0;
#ifndef MFS_STATIC_BLOCK_SIZE
  block_size = 0;
#endif
  zone_shift = 0;
  extra_space_percent = 0;
  while ((ch = getopt(argc, argv, "B:b:di:ltvx:z:")) != EOF)
	switch (ch) {
#ifndef MFS_STATIC_BLOCK_SIZE
	    case 'B':
		block_size = strtoul(optarg, &sfx, 0);
		switch(*sfx) {
		case 'b': case 'B': /* bytes; NetBSD-compatible */
		case '\0':	break;
		case 'K':
		case 'k':	block_size*=1024; break;
		case 's': 	block_size*=SECTOR_SIZE; break;
		default:	usage();
		}
		break;
#else
	    case 'B':
		if (block_size != strtoul(optarg, (char **) NULL, 0))
			errx(4, "block size must be exactly %d bytes",
			    MFS_STATIC_BLOCK_SIZE);
		break;
		(void)sfx;	/* shut up warnings about unused variable...*/
#endif
	    case 'b':
		blocks = strtoul(optarg, (char **) NULL, 0);
		break;
	    case 'd':
		dflag = 1;
		current_time = bin_time;
		break;
	    case 'i':
		inodes = strtoul(optarg, (char **) NULL, 0);
		break;
	    case 'l':	print = 1;	break;
	    case 't':	donttest = 1;	break;
	    case 'v':	++verbose;	break;
	    case 'x':	extra_space_percent = atoi(optarg); break;
	    case 'z':	zone_shift = atoi(optarg);	break;
	    default:	usage();
	}

  if (argc == optind) usage();

  /* Percentage of extra size must be nonnegative.
   * It can legitimately be bigger than 100 but has to make some sort of sense.
   */
  if(extra_space_percent < 0 || extra_space_percent > 2000) usage();

#ifdef DEFAULT_BLOCK_SIZE
  if(!block_size) block_size = DEFAULT_BLOCK_SIZE;
#endif
  if (block_size % SECTOR_SIZE)
	errx(4, "block size must be multiple of sector (%d bytes)", SECTOR_SIZE);
#ifdef MIN_BLOCK_SIZE
  if (block_size < MIN_BLOCK_SIZE)
	errx(4, "block size must be at least %d bytes", MIN_BLOCK_SIZE);
#endif
#ifdef MAX_BLOCK_SIZE
  if (block_size > MAX_BLOCK_SIZE)
	errx(4, "block size must be at most %d bytes", MAX_BLOCK_SIZE);
#endif
  if(block_size%INODE_SIZE)
	errx(4, "block size must be a multiple of inode size (%d bytes)", INODE_SIZE);

  if(zone_shift < 0 || zone_shift > 14)
	errx(4, "zone_shift must be a small non-negative integer");
  zone_per_block = 1 << zone_shift;	/* nr of blocks per zone */

  inodes_per_block = INODES_PER_BLOCK(block_size);
  indir_per_block = INDIRECTS(block_size);
  indir_per_zone = INDIRECTS(block_size) << zone_shift;
  /* number of file zones we can address directly and with a single indirect*/
  nr_indirzones = NR_DZONES + indir_per_zone;
  zone_size = block_size << zone_shift;
  /* Checks for an overflow: only with very big block size */
  if (zone_size <= 0)
	errx(4, "Zones are too big for this program; smaller -B or -z, please!");

  /* now that the block size is known, do buffer allocations where
   * possible.
   */
  zero = alloc_block();

  /* Determine the size of the device if not specified as -b or proto. */
  maxblocks = sizeup(argv[optind]);
  if (argc - optind == 1 && blocks == 0) {
  	blocks = maxblocks;
  	/* blocks == 0 is checked later, but leads to a funny way of
  	 * reporting a 0-sized device (displays usage).
  	 */
  	if(blocks < 1) {
  		errx(1, "zero size device.");
	}
  }

  /* The remaining args must be 'special proto', or just 'special' if the
   * no. of blocks has already been specified.
   */
  if (argc - optind != 2 && (argc - optind != 1 || blocks == 0)) usage();

  if (maxblocks && blocks > maxblocks) {
 	errx(1, "%s: number of blocks too large for device.", argv[optind]);
  }

  /* Check special. */
  check_mtab(argv[optind]);

  /* Check and start processing proto. */
  optarg = argv[++optind];
  if (optind < argc && (proto = fopen(optarg, "r")) != NULL) {
	/* Prototype file is readable. */
	lct = 1;
	get_line(line, token);	/* skip boot block info */

	/* Read the line with the block and inode counts. */
	get_line(line, token);
	blocks = strtol(token[0], (char **) NULL, 10);
	inodes = strtol(token[1], (char **) NULL, 10);

	/* Process mode line for root directory. */
	get_line(line, token);
	mode = mode_con(token[0]);
	usrid = atoi(token[1]);
	grpid = atoi(token[2]);

	if(blocks <= 0 && inodes <= 0){
		detect_fs_size(&fssize);
		blocks = fssize.blockcount;
		inodes = fssize.inocount;
		blocks += blocks*extra_space_percent/100;
		inodes += inodes*extra_space_percent/100;
/* XXX is it OK to write on stdout? Use warn() instead? Also consider using verbose */
		printf("dynamically sized filesystem: %u blocks, %u inodes\n",
		    (unsigned int) blocks, (unsigned int) inodes);
	}		
  } else {
	lct = 0;
	if (optind < argc) {
		/* Maybe the prototype file is just a size.  Check. */
		blocks = strtoul(optarg, (char **) NULL, 0);
		if (blocks == 0) errx(2, "Can't open prototype file");
	}
	if (inodes == 0) {
		long long kb = ((unsigned long long)blocks*block_size) / 1024;

		inodes = kb / 2;
		if (kb >= 100000) inodes = kb / 4;
		if (kb >= 1000000) inodes = kb / 6;
		if (kb >= 10000000) inodes = kb / 8;
		if (kb >= 100000000) inodes = kb / 10;
		if (kb >= 1000000000) inodes = kb / 12;
/* XXX check overflow: with very large number of blocks, this results in insanely large number of inodes */
/* XXX check underflow (if/when ino_t is signed), else the message below will look strange */

		/* round up to fill inode block */
		inodes += inodes_per_block - 1;
		inodes = inodes / inodes_per_block * inodes_per_block;
	}
	if (blocks < 5) errx(1, "Block count too small");
	if (inodes < 1) errx(1, "Inode count too small");

	/* Make simple file system of the given size, using defaults. */
	mode = 040777;
	usrid = BIN;
	grpid = BINGRP;
	simple = 1;
  }

  nrblocks = blocks;
  nrinodes = inodes;

  umap_array_elements = 1 + blocks/8;
  if(!(umap_array = malloc(umap_array_elements)))
	err(1, "can't allocate block bitmap (%u bytes).",
		(unsigned)umap_array_elements);

  /* Open special. */
  special(argv[--optind]);

  if (!donttest) {
	uint16_t *testb;
	ssize_t w;

	testb = alloc_block();

	/* Try writing the last block of partition or diskette. */
	if(lseek64(fd, mul64u(blocks - 1, block_size), SEEK_SET, NULL) < 0) {
		err(1, "couldn't seek to last block to test size (1)");
	}
	testb[0] = 0x3245;
	testb[1] = 0x11FF;
	testb[block_size/2-1] = 0x1F2F;
	if ((w=write(fd, testb, block_size)) != block_size)
		err(1, "File system is too big for minor device (write1 %d/%u)",
		    w, block_size);
	sync();			/* flush write, so if error next read fails */
	if(lseek64(fd, mul64u(blocks - 1, block_size), SEEK_SET, NULL) < 0) {
		err(1, "couldn't seek to last block to test size (2)");
	}
	testb[0] = 0;
	testb[1] = 0;
	testb[block_size/2-1] = 0;
	nread = read(fd, testb, block_size);
	if (nread != block_size || testb[0] != 0x3245 || testb[1] != 0x11FF ||
	    testb[block_size/2-1] != 0x1F2F) {
		warn("nread = %d\n", nread);
		warnx("testb = 0x%x 0x%x 0x%x\n",
		    testb[0], testb[1], testb[block_size-1]);
		errx(1, "File system is too big for minor device (read)");
	}
	lseek64(fd, mul64u(blocks - 1, block_size), SEEK_SET, NULL);
	testb[0] = 0;
	testb[1] = 0;
	testb[block_size/2-1] = 0;
	if (write(fd, testb, block_size) != block_size)
		err(1, "File system is too big for minor device (write2)");
	lseek(fd, 0L, SEEK_SET);
	free(testb);
  }

  /* Make the file-system */

  put_block(BOOT_BLOCK, zero);		/* Write a null boot block. */
  put_block(BOOT_BLOCK+1, zero);	/* Write another null block. */

  super(nrblocks >> zone_shift, inodes);

  root_inum = alloc_inode(mode, usrid, grpid);
  rootdir(root_inum);
  if (simple == 0) eat_dir(root_inum);

  if (print) print_fs();
  else if (verbose > 1) {
	  if (zone_shift)
		fprintf(stderr, "%d inodes used.     %u zones (%u blocks) used.\n",
		    (int)next_inode-1, next_zone, next_zone*zone_per_block);
	  else
		fprintf(stderr, "%d inodes used.     %u zones used.\n",
		    (int)next_inode-1, next_zone);
  }

  return(0);

  /* NOTREACHED */
}				/* end main */

/*================================================================
 *        detect_fs_size  -  determine image size dynamically
 *===============================================================*/
void
detect_fs_size(struct fs_size * fssize)
{
  int prev_lct = lct;
  off_t point = ftell(proto);
  block_t initb;
  zone_t initz;

  fssize->inocount = 1;	/* root directory node */
  fssize->zonecount = 0;
  fssize->blockcount = 0;

  sizeup_dir(fssize);

  initb = bitmapsize(1 + fssize->inocount, block_size);
  initb += bitmapsize(fssize->zonecount, block_size);
  initb += START_BLOCK;
  initb += (fssize->inocount + inodes_per_block - 1) / inodes_per_block;
  initz = (initb + zone_per_block - 1) >> zone_shift;

  fssize->blockcount = initb+ fssize->zonecount;
  lct = prev_lct;
  fseek(proto, point, SEEK_SET);
}

void
sizeup_dir(struct fs_size * fssize)
{
  char *token[MAX_TOKENS], *p;
  char line[LINE_LEN]; 
  FILE *f;
  off_t size;
  int dir_entries = 2;
  zone_t dir_zones = 0, fzones, indirects;

  while (1) {
	get_line(line, token);
	p = token[0];
	if (*p == '$') {
		dir_zones = (dir_entries / (NR_DIR_ENTRIES(block_size) * zone_per_block));
		if(dir_entries % (NR_DIR_ENTRIES(block_size) * zone_per_block))
			dir_zones++;
		if(dir_zones > NR_DZONES)
			dir_zones++;	/* Max single indir */
		fssize->zonecount += dir_zones;
		return;
	}

	p = token[1];
	fssize->inocount++;
	dir_entries++;

	if (*p == 'd') {
		sizeup_dir(fssize);
	} else if (*p == 'b' || *p == 'c') {

	} else if (*p == 's') {
		fssize->zonecount++; /* Symlink contents is always stored a block */
	} else {
		if ((f = fopen(token[4], "rb")) == NULL) {
			pexit("dynamic size detection failed: can't open %s",
			    token[4]);
		} else if (fseek(f, 0, SEEK_END) < 0) {
			pexit("dynamic size detection failed: seek to end of %s",
			    token[4]);
		} else if ( (size = ftell(f)) == (off_t)(-1)) {
			pexit("dynamic size detection failed: can't tell size of %s",
			    token[4]);
		} else {
			fclose(f);
			fzones = roundup(size, zone_size) / zone_size;
			indirects = 0;
			/* XXX overflow? fzones is u32, size is potentially 64-bit */
			if (fzones > NR_DZONES)
				indirects++; /* single indirect needed */
			if (fzones > nr_indirzones) {
				/* Each further group of 'indir_per_zone'
				 * needs one supplementary indirect zone:
				 */
				indirects += roundup(fzones - nr_indirzones,
				    indir_per_zone) / indir_per_zone;
				indirects++;	/* + double indirect needed!*/
			}
			fssize->zonecount += fzones + indirects;
		}
	}
  }
}

/*================================================================
 *                    sizeup  -  determine device size
 *===============================================================*/
block_t
sizeup(char * device)
{
  block_t d;
#if defined(__minix)
  u64_t bytes, resize;
  u32_t rem;
#else
  off_t size;
#endif


  if ((fd = open(device, O_RDONLY)) == -1) {
	if (errno != ENOENT)
		perror("sizeup open");
	return 0;
  }

#if defined(__minix)
  if(minix_sizeup(device, &bytes) < 0) {
       perror("sizeup");
       return 0;
  }

  d = div64u(bytes, block_size);
  rem = rem64u(bytes, block_size);

  resize = add64u(mul64u(d, block_size), rem);
  if(cmp64(resize, bytes) != 0) {
	/* Assume block_t is unsigned */
	d = (block_t)(-1ul);
	fprintf(stderr, "%s: truncating FS at %lu blocks\n",
		progname, (unsigned long)d);
  }
#else
  size = lseek(fd, 0, SEEK_END);
  if (size == (off_t) -1)
	  err(1, "cannot get device size fd=%d: %s", fd, device);
  /* Assume block_t is unsigned */
  if (size / block_size > (block_t)(-1ul)) {
	d = (block_t)(-1ul);
	fprintf(stderr, "%s: truncating FS at %lu blocks\n",
		progname, (unsigned long)d);
  } else
	d = size / block_size;
#endif

  return d;
}

/*
 * copied from fslib
 */
static int
bitmapsize(bit_t nr_bits, size_t blk_size)
{
  block_t nr_blocks;

  nr_blocks = nr_bits / FS_BITS_PER_BLOCK(blk_size);
  if (nr_blocks * FS_BITS_PER_BLOCK(blk_size) < nr_bits)
	++nr_blocks;
  return(nr_blocks);
}

/*================================================================
 *                 super  -  construct a superblock
 *===============================================================*/

void
super(zone_t zones, ino_t inodes)
{
  block_t inodeblks, initblks, i;
  unsigned long nb;
  long long ind_per_zone, zo;
  void *buf;
  struct super_block *sup;

  sup = buf = alloc_block();

#ifdef MFSFLAG_CLEAN
  /* The assumption is that mkfs will create a clean FS. */
  sup->s_flags = MFSFLAG_CLEAN;
#endif

  sup->s_ninodes = inodes;
  /* Check for overflow; cannot happen on V3 file systems */
  if(inodes != sup->s_ninodes)
	errx(1, "Too much inodes for that version of Minix FS.");
  sup->s_nzones = 0;	/* not used in V2 - 0 forces errors early */
  sup->s_zones = zones;
  /* Check for overflow; can only happen on V1 file systems */
  if(zones != sup->s_zones)
	errx(1, "Too much zones (blocks) for that version of Minix FS.");
  
#ifndef MFS_STATIC_BLOCK_SIZE
#define BIGGERBLOCKS "Please try a larger block size for an FS of this size."
#else
#define BIGGERBLOCKS "Please use MinixFS V3 for an FS of this size."
#endif
  sup->s_imap_blocks = nb = bitmapsize(1 + inodes, block_size);
  /* Checks for an overflow: nb is uint32_t while s_imap_blocks is of type
   * int16_t */
  if(sup->s_imap_blocks != nb) {
	errx(1, "too many inode bitmap blocks.\n" BIGGERBLOCKS);
  }
  sup->s_zmap_blocks = nb = bitmapsize(zones, block_size);
  /* Idem here check for overflow */
  if(nb != sup->s_zmap_blocks) {
	errx(1, "too many block bitmap blocks.\n" BIGGERBLOCKS);
  }
  inode_offset = START_BLOCK + sup->s_imap_blocks + sup->s_zmap_blocks;
  inodeblks = (inodes + inodes_per_block - 1) / inodes_per_block;
  initblks = inode_offset + inodeblks;
  sup->s_firstdatazone_old = nb =
	(initblks + (1 << zone_shift) - 1) >> zone_shift;
  if(nb >= zones) errx(1, "bit maps too large");
  if(nb != sup->s_firstdatazone_old) {
	/* The field is too small to store the value. Fortunately, the value
	 * can be computed from other fields. We set the on-disk field to zero
	 * to indicate that it must not be used. Eventually, we can always set
	 * the on-disk field to zero, and stop using it.
	 */
	sup->s_firstdatazone_old = 0;
  }
  sup->s_firstdatazone = nb;
  zoff = sup->s_firstdatazone - 1;
  sup->s_log_zone_size = zone_shift;
  sup->s_magic = SUPER_MAGIC;
#ifdef MFS_SUPER_BLOCK_SIZE
  sup->s_block_size = block_size;
  /* Check for overflow */
  if(block_size != sup->MFS_SUPER_BLOCK_SIZE)
	errx(1, "block_size too large.");
  sup->s_disk_version = 0;
#endif

  ind_per_zone = (long long) indir_per_zone;
  zo = NR_DZONES + ind_per_zone + ind_per_zone*ind_per_zone;
#ifndef MAX_MAX_SIZE
#define MAX_MAX_SIZE 	(INT32_MAX)
#endif
  if(MAX_MAX_SIZE/block_size < zo) {
	sup->s_max_size = MAX_MAX_SIZE;
  }
  else {
	sup->s_max_size = zo * block_size;
  }

  if (verbose>1) {
	fprintf(stderr, "Super block values:\n"
	    "\tnumber of inodes\t%12d\n"
	    "\tnumber of zones \t%12d\n"
	    "\tinode bit map blocks\t%12d\n"
	    "\tzone bit map blocks\t%12d\n"
	    "\tfirst data zone \t%12d\n"
	    "\tblocks per zone shift\t%12d\n"
	    "\tmaximum file size\t%12d\n"
	    "\tmagic number\t\t%#12X\n",
	    sup->s_ninodes, sup->s_zones,
	    sup->s_imap_blocks, sup->s_zmap_blocks, sup->s_firstdatazone,
	    sup->s_log_zone_size, sup->s_max_size, sup->s_magic);
#ifdef MFS_SUPER_BLOCK_SIZE
	fprintf(stderr, "\tblock size\t\t%12d\n", sup->s_block_size);
#endif
  }

  if (lseek(fd, (off_t) SUPER_BLOCK_BYTES, SEEK_SET) == (off_t) -1)
	err(1, "super() couldn't seek");
  if (write(fd, buf, SUPER_BLOCK_BYTES) != SUPER_BLOCK_BYTES)
	err(1, "super() couldn't write");

  /* Clear maps and inodes. */
  for (i = START_BLOCK; i < initblks; i++) put_block((block_t) i, zero);

  next_zone = sup->s_firstdatazone;
  next_inode = 1;

  zone_map = INODE_MAP + sup->s_imap_blocks;

  insert_bit(zone_map, 0);	/* bit zero must always be allocated */
  insert_bit((block_t) INODE_MAP, 0);	/* inode zero not used but
					 * must be allocated */

  free(buf);
}


/*================================================================
 *              rootdir  -  install the root directory
 *===============================================================*/
void
rootdir(ino_t inode)
{
  zone_t z;

  z = alloc_zone();
  add_zone(inode, z, 2 * sizeof(struct direct), current_time);
  enter_dir(inode, ".", inode);
  enter_dir(inode, "..", inode);
  incr_link(inode);
  incr_link(inode);
}

void
enter_symlink(ino_t inode, char *lnk)
{
  zone_t z;
  size_t len;
  char *buf;

  buf = alloc_block();
  z = alloc_zone();
  len = strlen(lnk);
  if (len >= block_size)
	pexit("symlink too long, max length is %u", (unsigned)block_size - 1);
  strcpy(buf, lnk);
  put_block((z << zone_shift), buf);

  add_zone(inode, z, len, current_time);

  free(buf);
}


/*================================================================
 *	    eat_dir  -  recursively install directory
 *===============================================================*/
void
eat_dir(ino_t parent)
{
  /* Read prototype lines and set up directory. Recurse if need be. */
  char *token[MAX_TOKENS], *p;
  char line[LINE_LEN];
  int mode, usrid, grpid, maj, min, f;
  ino_t n;
  zone_t z;
  size_t size;

  while (1) {
	get_line(line, token);
	p = token[0];
	if (*p == '$') return;
	p = token[1];
	mode = mode_con(p);
	usrid = atoi(token[2]);
	grpid = atoi(token[3]);
	n = alloc_inode(mode, usrid, grpid);

	/* Enter name in directory and update directory's size. */
	enter_dir(parent, token[0], n);
	incr_size(parent, sizeof(struct direct));

	/* Check to see if file is directory or special. */
	incr_link(n);
	if (*p == 'd') {
		/* This is a directory. */
		z = alloc_zone();	/* zone for new directory */
		add_zone(n, z, 2 * sizeof(struct direct), current_time);
		enter_dir(n, ".", n);
		enter_dir(n, "..", parent);
		incr_link(parent);
		incr_link(n);
		eat_dir(n);
	} else if (*p == 'b' || *p == 'c') {
		/* Special file. */
		maj = atoi(token[4]);
		min = atoi(token[5]);
		size = 0;
		if (token[6]) size = atoi(token[6]);
		size = block_size * size;
		add_zone(n, (zone_t) (makedev(maj,min)), size, current_time);
	} else if (*p == 's') {
		enter_symlink(n, token[4]);
	} else {
		/* Regular file. Go read it. */
		if ((f = open(token[4], O_RDONLY)) < 0) {
			fprintf(stderr, "%s: Can't open %s: %s\n",
				progname, token[4], strerror(errno));
		} else {
			eat_file(n, f);
		}
	}
  }

}

/*================================================================
 * 		eat_file  -  copy file to MINIX
 *===============================================================*/
/* Zonesize >= blocksize */
void
eat_file(ino_t inode, int f)
{
  int ct = 0, i, j;
  zone_t z = 0;
  char *buf;
  time_t timeval;

  buf = alloc_block();

  do {
	for (i = 0, j = 0; i < zone_per_block; i++, j += ct) {
		memset(buf, 0, block_size);
		if ((ct = read(f, buf, block_size)) > 0) {
			if (i == 0) z = alloc_zone();
			put_block((z << zone_shift) + i, buf);
		}
	}
	timeval = (dflag ? current_time : file_time(f));
	if (ct) add_zone(inode, z, (size_t) j, timeval);
  } while (ct == block_size);
  close(f);
  free(buf);
}

int
dir_try_enter(zone_t z, ino_t child, char const *name)
{
  struct direct *dir_entry = alloc_block();
  int r = 0;
  block_t b;
  int i, l;

  b = z << zone_shift;
  for (l = 0; l < zone_per_block; l++, b++) {
	get_block(b, dir_entry);

	for (i = 0; i < NR_DIR_ENTRIES(block_size); i++)
		if (!dir_entry[i].d_ino)
			break;

	if(i < NR_DIR_ENTRIES(block_size)) {
		r = 1;
		dir_entry[i].d_ino = child;
		assert(sizeof(dir_entry[i].d_name) == MFS_DIRSIZ);
		if (verbose && strlen(name) > MFS_DIRSIZ)
			fprintf(stderr, "File name %s is too long, truncated\n", name);
		strncpy(dir_entry[i].d_name, name, MFS_DIRSIZ);
		put_block(b, dir_entry);
		break;
	}
  }

  free(dir_entry);

  return r;
}

/*================================================================
 *	    directory & inode management assist group
 *===============================================================*/
void
enter_dir(ino_t parent, char const *name, ino_t child)
{
  /* Enter child in parent directory */
  /* Works for dir > 1 block and zone > block */
  unsigned int k;
  block_t b, indir;
  zone_t z;
  int off;
  struct inode *ino;
  struct inode *inoblock = alloc_block();
  zone_t *indirblock = alloc_block();

  assert(!(block_size % sizeof(struct direct)));

  /* Obtain the inode structure */
  b = ((parent - 1) / inodes_per_block) + inode_offset;
  off = (parent - 1) % inodes_per_block;
  get_block(b, inoblock);
  ino = inoblock + off;

  for (k = 0; k < NR_DZONES; k++) {
	z = ino->i_zone[k];
	if (z == 0) {
		z = alloc_zone();
		ino->i_zone[k] = z;
	}

	if(dir_try_enter(z, child, __UNCONST(name))) {
		put_block(b, inoblock);
		free(inoblock);
		free(indirblock);
		return;
	}
  }

  /* no space in directory using just direct blocks; try indirect */
  if (ino->i_zone[S_INDIRECT_IDX] == 0)
  	ino->i_zone[S_INDIRECT_IDX] = alloc_zone();

  indir = ino->i_zone[S_INDIRECT_IDX] << zone_shift;
  --indir; /* Compensate for ++indir below */
  for(k = 0; k < (indir_per_zone); k++) {
	if (k % indir_per_block == 0)
		get_block(++indir, indirblock);
  	z = indirblock[k % indir_per_block];
	if(!z) {
		z = indirblock[k % indir_per_block] = alloc_zone();
		put_block(indir, indirblock);
	}
	if(dir_try_enter(z, child, __UNCONST(name))) {
		put_block(b, inoblock);
		free(inoblock);
		free(indirblock);
		return;
	}
  }

  pexit("Directory-inode %u beyond single indirect blocks.  Could not enter %s",
         (unsigned)parent, name);
}


void
add_zone(ino_t n, zone_t z, size_t bytes, time_t mtime)
{
  /* Add zone z to inode n. The file has grown by 'bytes' bytes. */

  int off, i, j;
  block_t b;
  zone_t indir, dindir;
  struct inode *p, *inode;
  zone_t *blk, *dblk;

  assert(inodes_per_block*sizeof(*inode) == block_size);
  if(!(inode = alloc_block()))
  	err(1, "Couldn't allocate block of inodes");

  b = ((n - 1) / inodes_per_block) + inode_offset;
  off = (n - 1) % inodes_per_block;
  get_block(b, inode);
  p = &inode[off];
  p->i_size += bytes;
  p->i_mtime = mtime;
#ifndef MFS_INODE_ONLY_MTIME /* V1 file systems did not have them... */
  p->i_atime = p->i_ctime = current_time;
#endif
  for (i = 0; i < NR_DZONES; i++)
	if (p->i_zone[i] == 0) {
		p->i_zone[i] = z;
		put_block(b, inode);
  		free(inode);
		return;
	}

  assert(indir_per_block*sizeof(*blk) == block_size);
  if(!(blk = alloc_block()))
  	err(1, "Couldn't allocate indirect block");

  /* File has grown beyond a small file. */
  if (p->i_zone[S_INDIRECT_IDX] == 0)
	p->i_zone[S_INDIRECT_IDX] = alloc_zone();
  indir = p->i_zone[S_INDIRECT_IDX] << zone_shift;
  put_block(b, inode);
  --indir; /* Compensate for ++indir below */
  for (i = 0; i < (indir_per_zone); i++) {
	if (i % indir_per_block == 0)
		get_block(++indir, blk);
	if (blk[i % indir_per_block] == 0) {
		blk[i] = z;
		put_block(indir, blk);
  		free(blk);
  		free(inode);
		return;
	}
  }

  /* File has grown beyond single indirect; we need a double indirect */
  assert(indir_per_block*sizeof(*dblk) == block_size);
  if(!(dblk = alloc_block()))
  	err(1, "Couldn't allocate double indirect block");

  if (p->i_zone[D_INDIRECT_IDX] == 0)
	p->i_zone[D_INDIRECT_IDX] = alloc_zone();
  dindir = p->i_zone[D_INDIRECT_IDX] << zone_shift;
  put_block(b, inode);
  --dindir; /* Compensate for ++indir below */
  for (j = 0; j < (indir_per_zone); j++) {
	if (j % indir_per_block == 0)
		get_block(++dindir, dblk);
	if (dblk[j % indir_per_block] == 0)
		dblk[j % indir_per_block] = alloc_zone();
	indir = dblk[j % indir_per_block] << zone_shift;
	--indir; /* Compensate for ++indir below */
	for (i = 0; i < (indir_per_zone); i++) {
		if (i % indir_per_block == 0)
			get_block(++indir, blk);
		if (blk[i % indir_per_block] == 0) {
			blk[i] = z;
			put_block(dindir, dblk);
			put_block(indir, blk);
	  		free(dblk);
	  		free(blk);
	  		free(inode);
			return;
		}
	}
  }

  pexit("File has grown beyond double indirect");
}


/* Increment the link count to inode n */
void
incr_link(ino_t n)
{
  int off;
  static int enter = 0;
  static struct inode *inodes = NULL;
  block_t b;

  if (enter++) pexit("internal error: recursive call to incr_link()");

  b = ((n - 1) / inodes_per_block) + inode_offset;
  off = (n - 1) % inodes_per_block;
  {
	assert(sizeof(*inodes) * inodes_per_block == block_size);
	if(!inodes && !(inodes = alloc_block()))
		err(1, "couldn't allocate a block of inodes");

	get_block(b, inodes);
	inodes[off].i_nlinks++;
	/* Check overflow (particularly on V1)... */
	if (inodes[off].i_nlinks <= 0)
		pexit("Too many links to a directory");
	put_block(b, inodes);
  }
  enter = 0;
}


/* Increment the file-size in inode n */
void
incr_size(ino_t n, size_t count)
{
  block_t b;
  int off;

  b = ((n - 1) / inodes_per_block) + inode_offset;
  off = (n - 1) % inodes_per_block;
  {
	struct inode *inodes;

	assert(inodes_per_block * sizeof(*inodes) == block_size);
	if(!(inodes = alloc_block()))
		err(1, "couldn't allocate a block of inodes");

	get_block(b, inodes);
	/* Check overflow; avoid compiler spurious warnings */
	if (inodes[off].i_size+(int)count < inodes[off].i_size ||
	    inodes[off].i_size > MAX_MAX_SIZE-(int)count)
		pexit("File has become too big to be handled by MFS");
	inodes[off].i_size += count;
	put_block(b, inodes);
	free(inodes);
  }
}


/*================================================================
 * 	 	     allocation assist group
 *===============================================================*/
static ino_t
alloc_inode(int mode, int usrid, int grpid)
{
  ino_t num;
  int off;
  block_t b;
  struct inode *inodes;

  num = next_inode++;
  if (num > nrinodes) {
  	pexit("File system does not have enough inodes (only %d)", nrinodes);
  }
  b = ((num - 1) / inodes_per_block) + inode_offset;
  off = (num - 1) % inodes_per_block;

  assert(inodes_per_block * sizeof(*inodes) == block_size);
  if(!(inodes = alloc_block()))
	err(1, "couldn't allocate a block of inodes");

  get_block(b, inodes);
  if (inodes[off].i_mode) {
	pexit("allocation new inode %d with non-zero mode - this cannot happen",
		num);
  }
  inodes[off].i_mode = mode;
  inodes[off].i_uid = usrid;
  inodes[off].i_gid = grpid;
  if (verbose && (inodes[off].i_uid != usrid || inodes[off].i_gid != grpid))
	fprintf(stderr, "Uid/gid %d.%d do not fit within inode, truncated\n", usrid, grpid);
  put_block(b, inodes);

  free(inodes);

  /* Set the bit in the bit map. */
  insert_bit((block_t) INODE_MAP, num);
  return(num);
}


/* Allocate a new zone */
static zone_t
alloc_zone(void)
{
  /* Works for zone > block */
  block_t b;
  int i;
  zone_t z;

  z = next_zone++;
  b = z << zone_shift;
  if (b > nrblocks - zone_per_block)
	pexit("File system not big enough for all the files");
  for (i = 0; i < zone_per_block; i++)
	put_block(b + i, zero);	/* give an empty zone */
  
  insert_bit(zone_map, z - zoff);
  return z;
}


/* Insert one bit into the bitmap */
void
insert_bit(block_t map, bit_t bit)
{
  int boff, w, s;
  unsigned int bits_per_block;
  block_t map_block;
  bitchunk_t *buf;

  buf = alloc_block();

  bits_per_block = FS_BITS_PER_BLOCK(block_size);
  map_block = map + bit / bits_per_block;
  if (map_block >= inode_offset)
	pexit("insertbit invades inodes area - this cannot happen");
  boff = bit % bits_per_block;

  assert(boff >=0);
  assert(boff < FS_BITS_PER_BLOCK(block_size));
  get_block(map_block, buf);
  w = boff / FS_BITCHUNK_BITS;
  s = boff % FS_BITCHUNK_BITS;
  buf[w] |= (1 << s);
  put_block(map_block, buf);

  free(buf);
}


/*================================================================
 * 		proto-file processing assist group
 *===============================================================*/
int mode_con(char *p)
{
  /* Convert string to mode */
  int o1, o2, o3, mode;
  char c1, c2, c3;

  c1 = *p++;
  c2 = *p++;
  c3 = *p++;
  o1 = *p++ - '0';
  o2 = *p++ - '0';
  o3 = *p++ - '0';
  mode = (o1 << 6) | (o2 << 3) | o3;
  if (c1 == 'd') mode |= S_IFDIR;
  if (c1 == 'b') mode |= S_IFBLK;
  if (c1 == 'c') mode |= S_IFCHR;
  if (c1 == 's') mode |= S_IFLNK;
  if (c1 == 'l') mode |= S_IFLNK;	/* just to be somewhat ls-compatible*/
/* XXX note: some other mkfs programs consider L to create hardlinks */
  if (c1 == '-') mode |= S_IFREG;
  if (c2 == 'u') mode |= S_ISUID;
  if (c3 == 'g') mode |= S_ISGID;
/* XXX There are no way to encode S_ISVTX */
  return(mode);
}

void
get_line(char line[LINE_LEN], char *parse[MAX_TOKENS])
{
  /* Read a line and break it up in tokens */
  int k;
  char c, *p;
  int d;

  for (k = 0; k < MAX_TOKENS; k++) parse[k] = 0;
  memset(line, 0, LINE_LEN);
  k = 0;
  p = line;
  while (1) {
	if (++k > LINE_LEN) pexit("Line too long");
	d = fgetc(proto);
	if (d == EOF) pexit("Unexpected end-of-file");
	*p = d;
	if (*p == ' ' || *p == '\t') *p = 0;
	if (*p == '\n') {
		lct++;
		*p++ = 0;
		*p = '\n';
		break;
	}
	p++;
  }

  k = 0;
  p = line;
  while (1) {
	c = *p++;
	if (c == '\n') return;
	if (c == 0) continue;
	parse[k++] = p - 1;
	do {
		c = *p++;
	} while (c != 0 && c != '\n');
  }
}


/*================================================================
 *			other stuff
 *===============================================================*/

/*
 * Check to see if the special file named 'device' is mounted.
 */
void
check_mtab(const char *device)		/* /dev/hd1 or whatever */
{
#if defined(__minix)
  int n, r;
  struct stat sb;
  char dev[PATH_MAX], mount_point[PATH_MAX],
	type[MNTNAMELEN], flags[MNTFLAGLEN];

  r= stat(device, &sb);
  if (r == -1)
  {
	if (errno == ENOENT)
		return;	/* Does not exist, and therefore not mounted. */
	err(1, "stat %s failed", device);
  }
  if (!S_ISBLK(sb.st_mode))
  {
	/* Not a block device and therefore not mounted. */
	return;
  }

  if (load_mtab(__UNCONST("mkfs")) < 0) return;
  while (1) {
	n = get_mtab_entry(dev, mount_point, type, flags);
	if (n < 0) return;
	if (strcmp(device, dev) == 0) {
		/* Can't mkfs on top of a mounted file system. */
		errx(1, "%s is mounted on %s", device, mount_point);
	}
  }
#elif defined(__linux__)
/* XXX: this code is copyright Theodore T'so and distributed under the GPLv2. Rewrite.
 */
	struct mntent 	*mnt;
	struct stat	st_buf;
	dev_t		file_dev=0, file_rdev=0;
	ino_t		file_ino=0;
	FILE 		*f;
	int		fd;
	char 		*mtab_file = "/proc/mounts";

	if ((f = setmntent (mtab_file, "r")) == NULL)
		goto error;

	if (stat(device, &st_buf) == 0) {
		if (S_ISBLK(st_buf.st_mode)) {
			file_rdev = st_buf.st_rdev;
		} else {
			file_dev = st_buf.st_dev;
			file_ino = st_buf.st_ino;
		}
	}
	
	while ((mnt = getmntent (f)) != NULL) {
		if (strcmp(device, mnt->mnt_fsname) == 0)
			break;
		if (stat(mnt->mnt_fsname, &st_buf) == 0) {
			if (S_ISBLK(st_buf.st_mode)) {
				if (file_rdev && (file_rdev == st_buf.st_rdev))
					break;
			} else {
				if (file_dev && ((file_dev == st_buf.st_dev) &&
						 (file_ino == st_buf.st_ino)))
					break;
			}
		}
	}

	if (mnt == NULL) {
		/*
		 * Do an extra check to see if this is the root device.  We
		 * can't trust /etc/mtab, and /proc/mounts will only list
		 * /dev/root for the root filesystem.  Argh.  Instead we
		 * check if the given device has the same major/minor number
		 * as the device that the root directory is on.
		 */
		if (file_rdev && stat("/", &st_buf) == 0) {
			if (st_buf.st_dev == file_rdev) {
				goto is_root;
			}
		}
		goto test_busy;
	}
	/* Validate the entry in case /etc/mtab is out of date */
	/* 
	 * We need to be paranoid, because some broken distributions
	 * (read: Slackware) don't initialize /etc/mtab before checking
	 * all of the non-root filesystems on the disk.
	 */
	if (stat(mnt->mnt_dir, &st_buf) < 0) {
		if (errno == ENOENT) {
			goto test_busy;
		}
		goto error;
	}
	if (file_rdev && (st_buf.st_dev != file_rdev)) {
		goto error;
	}

	fprintf(stderr, "Device %s is mounted, exiting\n", device);
	exit(-1);

	/*
	 * Check to see if we're referring to the root filesystem.
	 * If so, do a manual check to see if we can open /etc/mtab
	 * read/write, since if the root is mounted read/only, the
	 * contents of /etc/mtab may not be accurate.
	 */
	if (!strcmp(mnt->mnt_dir, "/")) {
is_root:
		fprintf(stderr, "Device %s is mounted as root file system!\n",
				device);
		exit(-1);
	}
	
test_busy:

	endmntent (f);
	if ((stat(device, &st_buf) != 0) ||
			!S_ISBLK(st_buf.st_mode))
		return;
	fd = open(device, O_RDONLY | O_EXCL);
	if (fd < 0) {
		if (errno == EBUSY) {
			fprintf(stderr, "Device %s is used by the system\n", device);
			exit(-1);
		}
	} else
		close(fd);

	return;

error:
	endmntent (f);
	fprintf(stderr, "Error while checking if device %s is mounted\n", device);
	exit(-1);
#else
	(void) device;	/* shut up warnings about unused variable... */
#endif
}


time_t
file_time(int f)
{
  struct stat statbuf;

  if (!fstat(f, &statbuf))
	return current_time;
  if (statbuf.st_mtime<0 || statbuf.st_mtime>(uint32_t)(-1))
	return current_time;
  return(statbuf.st_mtime);
}


__dead void
pexit(char const * s, ...)
{
  va_list va;

  va_start(va, s);
  vwarn(s, va);
  va_end(va);
  if (lct != 0)
	warnx("Line %d being processed when error detected.\n", lct);
  exit(2);
}


void *
alloc_block(void)
{
	void *buf;

	if(!(buf = malloc(block_size))) {
		err(1, "couldn't allocate filesystem buffer");
	}
	memset(buf, 0, block_size);

	return buf;
}

void
print_fs(void)
{
  int i, j;
  ino_t k;
  struct inode *inode2;
  unsigned short *usbuf;
  block_t b;
  struct direct *dir;

  assert(inodes_per_block * sizeof(*inode2) == block_size);
  if(!(inode2 = alloc_block()))
	err(1, "couldn't allocate a block of inodes");

  assert(NR_DIR_ENTRIES(block_size)*sizeof(*dir) == block_size);
  if(!(dir = alloc_block()))
	err(1, "couldn't allocate a block of directory entries");

  usbuf = alloc_block();
  get_super_block(usbuf);
  printf("\nSuperblock: ");
  for (i = 0; i < 8; i++) printf("%06ho ", usbuf[i]);
  printf("\n            ");
  for (i = 0; i < 8; i++) printf("%#04hX ", usbuf[i]);
  printf("\n            ");
  for (i = 8; i < 15; i++) printf("%06ho ", usbuf[i]);
  printf("\n            ");
  for (i = 8; i < 15; i++) printf("%#04hX ", usbuf[i]);
  get_block((block_t) INODE_MAP, usbuf);
  printf("...\nInode map:  ");
  for (i = 0; i < 9; i++) printf("%06ho ", usbuf[i]);
  get_block((block_t) zone_map, usbuf);
  printf("...\nZone  map:  ");
  for (i = 0; i < 9; i++) printf("%06ho ", usbuf[i]);
  printf("...\n");

  free(usbuf);
  usbuf = NULL;

  k = 0;
  for (b = inode_offset; k < nrinodes; b++) {
	get_block(b, inode2);
	for (i = 0; i < inodes_per_block; i++) {
		k = inodes_per_block * (int) (b - inode_offset) + i + 1;
		/* Lint but OK */
		if (k > nrinodes) break;
		{
			if (inode2[i].i_mode != 0) {
				printf("Inode %3u:  mode=", (unsigned)k);
				printf("%06o", (unsigned)inode2[i].i_mode);
				printf("  uid=%2d  gid=%2d  size=",
					(int)inode2[i].i_uid, (int)inode2[i].i_gid);
				printf("%6ld", (long)inode2[i].i_size);
				printf("  zone[0]=%u\n", (unsigned)inode2[i].i_zone[0]);
			}
			if ((inode2[i].i_mode & S_IFMT) == S_IFDIR) {
				/* This is a directory */
				get_block(inode2[i].i_zone[0] << zone_shift, dir);
				for (j = 0; j < NR_DIR_ENTRIES(block_size); j++)
					if (dir[j].d_ino)
						printf("\tInode %2u: %s\n",
							(unsigned)dir[j].d_ino,
							dir[j].d_name);
			}
		}
	}
  }

  if (zone_shift)
	printf("%d inodes used.     %u zones (%u blocks) used.\n",
		(int)next_inode-1, next_zone, next_zone*zone_per_block);
  else
	printf("%d inodes used.     %u zones used.\n", (int)next_inode-1, next_zone);
  free(dir);
  free(inode2);
}


/*
 * The first time a block is read, it returns all 0s, unless there has
 * been a write.  This routine checks to see if a block has been accessed.
 */
int
read_and_set(block_t n)
{
  int w, s, mask, r;

  w = n / 8;
  
  assert(n < nrblocks);
  if(w >= umap_array_elements) {
	errx(1, "umap array too small - this can't happen");
  }
  s = n % 8;
  mask = 1 << s;
  r = (umap_array[w] & mask ? 1 : 0);
  umap_array[w] |= mask;
  return(r);
}

__dead void
usage(void)
{
  fprintf(stderr, "Usage: %s [-dltv] [-b blocks] [-i inodes] [-z zone_shift]\n"
      "\t[-x extra] [-B blocksize] special [proto]\n",
      progname);
  exit(4);
}

void
special(char * string)
{
  fd = creat(string, 0777);
  close(fd);
  fd = open(string, O_RDWR);
  if (fd < 0) err(1, "Can't open special file %s", string);
}



/* Read a block. */
void
get_block(block_t n, void *buf)
{
  ssize_t k;

  /* First access returns a zero block */
  if (read_and_set(n) == 0) {
	memcpy(buf, zero, block_size);
	return;
  }
  if (lseek64(fd, mul64u(n, block_size), SEEK_SET, NULL) == (off_t)(-1))
	pexit("get_block couldn't seek");
  k = read(fd, buf, block_size);
  if (k != block_size)
	pexit("get_block couldn't read block #%u", (unsigned)n);
}

/* Read the super block. */
void
get_super_block(void *buf)
{
  ssize_t k;

  if(lseek(fd, (off_t) SUPER_BLOCK_BYTES, SEEK_SET) == (off_t) -1)
  	err(1, "seek for superblock failed");
  k = read(fd, buf, SUPER_BLOCK_BYTES);
  if (k != SUPER_BLOCK_BYTES)
	err(1, "get_super_block couldn't read super block");
}

/* Write a block. */
void
put_block(block_t n, void *buf)
{

  (void) read_and_set(n);

  if (lseek64(fd, mul64u(n, block_size), SEEK_SET, NULL) == (off_t) -1)
	pexit("put_block couldn't seek");
  if (write(fd, buf, block_size)!= block_size)
	pexit("put_block couldn't write block #%u", (unsigned)n);
}
