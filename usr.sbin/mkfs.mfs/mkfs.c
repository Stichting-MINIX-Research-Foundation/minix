/* mkfs  -  make the MINIX filesystem	Authors: Tanenbaum et al. */

/*	Authors: Andy Tanenbaum, Paul Ogilvie, Frans Meulenbroeks, Bruce Evans
 *
 * This program can make version 1, 2 and 3 file systems, as follows:
 *	mkfs /dev/fd0 1200	# Version 3 (default)
 *	mkfs -1 /dev/fd0 360	# Version 1
 *
 * Note that the version 1 and 2 file systems produced by this program are not
 * compatible with the original version 1 and 2 file system layout.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include "const.h"
#include "type.h"
#include "mfsdir.h"
#if defined(__minix)
#include <minix/partition.h>
#include <minix/u64.h>
#include <minix/minlib.h>
#include <sys/ioctl.h>
#endif
#include <dirent.h>

#undef EXTERN
#define EXTERN			/* get rid of EXTERN by making it null */
#include "super.h"

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#define INODE_MAP            2
#define MAX_TOKENS          10
#define LINE_LEN           200
#define BIN                  2
#define BINGRP               2
#define BIT_MAP_SHIFT       13
#define INODE_MAX       ((unsigned) 65535)
#define SECTOR_SIZE	   512


#if !defined(__minix)
#define mul64u(a,b)	((a) * (b))
#define lseek64(a,b,c,d) lseek(a,b,c)
#ifdef __linux__
#include <mntent.h>
#endif
#endif

#if !defined(__minix)
typedef uint32_t block_t;
typedef uint32_t zone_t;
#endif

extern char *optarg;
extern int optind;

int next_zone, next_inode, zoff;
block_t nrblocks;
int inode_offset, lct = 0, disk, fd, print = 0, file = 0;
unsigned int nrinodes;
int override = 0, simple = 0, dflag;
int donttest;			/* skip test if it fits on medium */
char *progname;

uint32_t current_time, bin_time;
char *zero, *lastp;
char *umap_array;	/* bit map tells if block read yet */
int umap_array_elements = 0;
block_t zone_map;		/* where is zone map? (depends on # inodes) */
int inodes_per_block;
size_t block_size;
int extra_space_percent;

FILE *proto;

#if defined(__NBSD_LIBC) || !defined(__minix)
#define getline _mkfs_getline
#endif

int main(int argc, char **argv);
block_t sizeup(char *device);
void super(zone_t zones, ino_t inodes);
void rootdir(ino_t inode);
int dir_try_enter(zone_t z, ino_t child, char *name);
void eat_dir(ino_t parent);
void eat_file(ino_t inode, int f);
void enter_dir(ino_t parent, char const *name, ino_t child);
void enter_symlink(ino_t inode, char *link);
d2_inode *get_inoblock(ino_t i, block_t *blockno, d2_inode **ino);
void incr_size(ino_t n, size_t count);
static ino_t alloc_inode(int mode, int usrid, int grpid);
static zone_t alloc_zone(void);
void add_zone(ino_t n, zone_t z, size_t bytes, uint32_t cur_time);
void add_z_1(ino_t n, zone_t z, size_t bytes, uint32_t cur_time);
void add_z_2(ino_t n, zone_t z, size_t bytes, uint32_t cur_time);
void incr_link(ino_t n);
void insert_bit(block_t block, int bit);
int mode_con(char *p);
void getline(char line[LINE_LEN], char *parse[MAX_TOKENS]);
void check_mtab(char *devname);
uint32_t file_time(int f);
__dead void pexit(char const *s);
void copy(char *from, char *to, size_t count);
void print_fs(void);
int read_and_set(block_t n);
void special(char *string);
void get_block(block_t n, char *buf);
void get_super_block(char *buf);
void put_block(block_t n, char *buf);
void mx_read(int blocknr, char *buf);
void mx_write(int blocknr, char *buf);
void dexit(char *s, int sectnum, int err);
__dead void usage(void);
void *alloc_block(void);

ino_t inocount;
zone_t zonecount;
block_t blockcount;

void detect_fs_size(void);
void sizeup_dir(void);
void detect_size(void);
void size_dir(void);
static int bitmapsize(uint32_t nr_bits, size_t block_size);

/*================================================================
 *                    mkfs  -  make filesystem
 *===============================================================*/
int main(argc, argv)
int argc;
char *argv[];
{
  int nread, mode, usrid, grpid, ch;
  block_t blocks, maxblocks;
  size_t i;
  ino_t root_inum;
  ino_t inodes;
  zone_t zones;
  char *token[MAX_TOKENS], line[LINE_LEN];
  struct stat statbuf;

  /* Get two times, the current time and the mod time of the binary of
   * mkfs itself.  When the -d flag is used, the later time is put into
   * the i_mtimes of all the files.  This feature is useful when
   * producing a set of file systems, and one wants all the times to be
   * identical. First you set the time of the mkfs binary to what you
   * want, then go.
   */
  current_time = time((time_t *) 0);	/* time mkfs is being run */
  stat(argv[0], &statbuf);
  bin_time = statbuf.st_mtime;	/* time when mkfs binary was last modified */

  /* Process switches. */
  progname = argv[0];
  blocks = 0;
  i = 0;
  inodes_per_block = 0;
  block_size = 0;
  extra_space_percent = 0;
  while ((ch = getopt(argc, argv, "b:di:lotB:x:")) != EOF)
	switch (ch) {
	    case 'b':
		blocks = strtoul(optarg, (char **) NULL, 0);
		break;
	    case 'd':
		dflag = 1;
		current_time = bin_time;
		break;
	    case 'i':
		i = strtoul(optarg, (char **) NULL, 0);
		break;
	    case 'l':	print = 1;	break;
	    case 'o':	override = 1;	break;
	    case 't':	donttest = 1;	break;
	    case 'B':	block_size = atoi(optarg);	break;
	    case 'x':	extra_space_percent = atoi(optarg); break;
	    default:	usage();
	}

  if (argc == optind) usage();

  /* Percentage of extra size must be nonnegative.
   * It can legitimately be bigger than 100 but has to make some sort of sense.
   */
  if(extra_space_percent < 0 || extra_space_percent > 2000) usage();

  {
  	if(!block_size) block_size = _MAX_BLOCK_SIZE; /* V3 default block size */
  	if(block_size%SECTOR_SIZE || block_size < _MIN_BLOCK_SIZE) {
  		fprintf(stderr, "block size must be multiple of sector (%d) "
  			"and at least %d bytes\n",
  			SECTOR_SIZE, _MIN_BLOCK_SIZE);
  		pexit("specified block size illegal");
  	}
  	if(block_size%V2_INODE_SIZE) {
  		fprintf(stderr, "block size must be a multiple of inode size (%d bytes)\n",
  			V2_INODE_SIZE);
  		pexit("specified block size illegal");
  	}
  }

  if(!inodes_per_block)
  	inodes_per_block = V2_INODES_PER_BLOCK(block_size);

  /* now that the block size is known, do buffer allocations where
   * possible.
   */
  zero = alloc_block();
  bzero(zero, block_size);

  /* Determine the size of the device if not specified as -b or proto. */
  maxblocks = sizeup(argv[optind]);
  if (argc - optind == 1 && blocks == 0) {
  	blocks = maxblocks;
  	/* blocks == 0 is checked later, but leads to a funny way of
  	 * reporting a 0-sized device (displays usage).
  	 */
  	if(blocks < 1) {
  		fprintf(stderr, "%s: zero size device.\n", progname);
  		return 1;
  	}
  }

  /* The remaining args must be 'special proto', or just 'special' if the
   * no. of blocks has already been specified.
   */
  if (argc - optind != 2 && (argc - optind != 1 || blocks == 0)) usage();

  if (blocks > maxblocks) {
 	fprintf(stderr, "%s: %s: number of blocks too large for device.\n",
  		progname, argv[optind]);
  	return 1;
  }

  /* Check special. */
  check_mtab(argv[optind]);

  /* Check and start processing proto. */
  optarg = argv[++optind];
  if (optind < argc && (proto = fopen(optarg, "r")) != NULL) {
	/* Prototype file is readable. */
	lct = 1;
	getline(line, token);	/* skip boot block info */

	/* Read the line with the block and inode counts. */
	getline(line, token);
	blocks = atol(token[0]);
	inodes = atoi(token[1]);

	/* Process mode line for root directory. */
	getline(line, token);
	mode = mode_con(token[0]);
	usrid = atoi(token[1]);
	grpid = atoi(token[2]);

	if(blocks <= 0 && inodes <= 0){
		detect_fs_size();
		blocks = blockcount;
		inodes = inocount;
		blocks += blocks*extra_space_percent/100;
		inodes += inodes*extra_space_percent/100;
		printf("dynamically sized filesystem: %d blocks, %d inodes\n", blocks, 
			(unsigned int) inodes);
	}		
  } else {
	lct = 0;
	if (optind < argc) {
		/* Maybe the prototype file is just a size.  Check. */
		blocks = strtoul(optarg, (char **) NULL, 0);
		if (blocks == 0) pexit("Can't open prototype file");
	}
	if (i == 0) {
#if defined(__minix)
		uint32_t kb = div64u(mul64u(blocks, block_size), 1024);
#else
		uint32_t kb = ((unsigned long long) blocks * block_size) / 1024;
#endif
		i = kb / 2;
		if (kb >= 100000) i = kb / 4;

		/* round up to fill inode block */
		i += inodes_per_block - 1;
		i = i / inodes_per_block * inodes_per_block;
	}
	if (blocks < 5) pexit("Block count too small");
	if (i < 1) pexit("Inode count too small");
	inodes = (ino_t) i;

	/* Make simple file system of the given size, using defaults. */
	mode = 040777;
	usrid = BIN;
	grpid = BINGRP;
	simple = 1;
  }

  nrblocks = blocks;
  nrinodes = inodes;

{
  size_t bytes;
  bytes = 1 + blocks/8;
  if(!(umap_array = malloc(bytes))) {
	fprintf(stderr, "mkfs: can't allocate block bitmap (%u bytes).\n",
		bytes);
	exit(1);
  }
  umap_array_elements = bytes;
}

  /* Open special. */
  special(argv[--optind]);

  if (!donttest) {
	short *testb;
	ssize_t w;

	testb = (short *) alloc_block();

	/* Try writing the last block of partition or diskette. */
	if(lseek64(fd, mul64u(blocks - 1, block_size), SEEK_SET, NULL) < 0) {
		pexit("couldn't seek to last block to test size (1)");
	}
	testb[0] = 0x3245;
	testb[1] = 0x11FF;
	testb[block_size/2-1] = 0x1F2F;
	if ((w=write(fd, (char *) testb, block_size)) != block_size) {
		if(w < 0) perror("write");
		printf("%d/%u\n", w, block_size);
		pexit("File system is too big for minor device (write)");
	}
	sync();			/* flush write, so if error next read fails */
	if(lseek64(fd, mul64u(blocks - 1, block_size), SEEK_SET, NULL) < 0) {
		pexit("couldn't seek to last block to test size (2)");
	}
	testb[0] = 0;
	testb[1] = 0;
	nread = read(fd, (char *) testb, block_size);
	if (nread != block_size || testb[0] != 0x3245 || testb[1] != 0x11FF ||
		testb[block_size/2-1] != 0x1F2F) {
		if(nread < 0) perror("read");
printf("nread = %d\n", nread);
printf("testb = 0x%x 0x%x 0x%x\n", testb[0], testb[1], testb[block_size-1]);
		pexit("File system is too big for minor device (read)");
	}
	lseek64(fd, mul64u(blocks - 1, block_size), SEEK_SET, NULL);
	testb[0] = 0;
	testb[1] = 0;
	if (write(fd, (char *) testb, block_size) != block_size)
		pexit("File system is too big for minor device (write2)");
	lseek(fd, 0L, SEEK_SET);
	free(testb);
  }

  /* Make the file-system */

	put_block((block_t) 0, zero);	/* Write a null boot block. */

  zones = nrblocks;

  super(zones, inodes);

  root_inum = alloc_inode(mode, usrid, grpid);
  rootdir(root_inum);
  if (simple == 0) eat_dir(root_inum);

  if (print) print_fs();
  return(0);

  /* NOTREACHED */
}				/* end main */

/*================================================================
 *        detect_fs_size  -  determine image size dynamically
 *===============================================================*/
void detect_fs_size()
{
  uint32_t point = ftell(proto);
  
  inocount = 1;	/* root directory node */
  zonecount = 0;
  blockcount = 0;
  sizeup_dir();
	
  uint32_t initb;

  initb = bitmapsize((uint32_t) (1 + inocount), block_size);
  initb += bitmapsize((uint32_t) zonecount, block_size);
  initb += START_BLOCK;
  initb += (inocount + inodes_per_block - 1) / inodes_per_block;

  blockcount = initb+zonecount;
  fseek(proto, point, SEEK_SET);
}

void sizeup_dir()
{
  char *token[MAX_TOKENS], *p;
  char line[LINE_LEN]; 
  FILE *f;
  size_t size;
  int dir_entries = 2;
  zone_t dir_zones = 0;
  zone_t nr_dzones;

  nr_dzones = V2_NR_DZONES;

  while (1) {
	getline(line, token);
	p = token[0];
	if (*p == '$') {
		dir_zones = (dir_entries / (NR_DIR_ENTRIES(block_size)));		
		if(dir_entries % (NR_DIR_ENTRIES(block_size)))
			dir_zones++;
		if(dir_zones > nr_dzones)
			dir_zones++;	/* Max single indir */
		zonecount += dir_zones;
		return;
	}

	p = token[1];
	inocount++;
	dir_entries++;

	if (*p == 'd') {
		sizeup_dir();
	} else if (*p == 'b' || *p == 'c') {

	} else if (*p == 's') {
		zonecount++; /* Symlink contents is always stored a block */
	} else {
		if ((f = fopen(token[4], "r")) == NULL) {
			fprintf(stderr, "%s: Can't open %s: %s\n",
				progname, token[4], strerror(errno));
				pexit("dynamic size detection failed");
		} else {
			if (fseek(f, 0, SEEK_END) < 0) {
			fprintf(stderr, "%s: Can't seek to end of %s\n",
				progname, token[4]);
				pexit("dynamic size detection failed");
			}
			size = ftell(f);
			fclose(f);
			zone_t fzones= (size / block_size);
			if (size % block_size)
				fzones++;
			if (fzones > nr_dzones)
				fzones++;	/* Assumes files fit within single indirect */
			zonecount += fzones;
		}
	}
  }
}

/*================================================================
 *                    sizeup  -  determine device size
 *===============================================================*/
block_t sizeup(device)
char *device;
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
	d = ULONG_MAX;
	fprintf(stderr, "mkfs: truncating FS at %u blocks\n", d);
  }
#else
  size = lseek(fd, 0, SEEK_END);
  if (size == (off_t) -1) {
	  fprintf(stderr, "Cannot get device size fd=%d\n", fd);
	  exit(-1);
  }
  d = size / block_size;
#endif

  return d;
}

/*
 * copied from fslib
 */
static int bitmapsize(uint32_t nr_bits, size_t blk_size)
{
  block_t nr_blocks;

  nr_blocks = (int) (nr_bits / FS_BITS_PER_BLOCK(blk_size));
  if (((uint32_t) nr_blocks * FS_BITS_PER_BLOCK(blk_size)) < nr_bits)
	++nr_blocks;
  return(nr_blocks);
}

/*================================================================
 *                 super  -  construct a superblock
 *===============================================================*/

void super(zones, inodes)
zone_t zones;
ino_t inodes;
{
  unsigned int i;
  int inodeblks;
  int initblks;
  uint32_t nb;
  zone_t v2sq;
  zone_t zo;
  struct super_block *sup;
  char *buf, *cp;

  buf = alloc_block();

  for (cp = buf; cp < &buf[block_size]; cp++) *cp = 0;
  sup = (struct super_block *) buf;	/* lint - might use a union */

  /* The assumption is that mkfs will create a clean FS. */
  sup->s_flags = MFSFLAG_CLEAN;

  sup->s_ninodes = inodes;
  sup->s_nzones = 0;	/* not used in V2 - 0 forces errors early */
  sup->s_zones = zones;
  
#define BIGGERBLOCKS "Please try a larger block size for an FS of this size.\n"
  sup->s_imap_blocks = nb = bitmapsize((uint32_t) (1 + inodes), block_size);
  if(sup->s_imap_blocks != nb) {
	fprintf(stderr, "mkfs: too many inode bitmap blocks.\n" BIGGERBLOCKS);
	exit(1);
  }
  sup->s_zmap_blocks = nb = bitmapsize((uint32_t) zones, block_size);
  if(nb != sup->s_zmap_blocks) {
	fprintf(stderr, "mkfs: too many block bitmap blocks.\n" BIGGERBLOCKS);
	exit(1);
  }
  inode_offset = START_BLOCK + sup->s_imap_blocks + sup->s_zmap_blocks;
  inodeblks = (inodes + inodes_per_block - 1) / inodes_per_block;
  initblks = inode_offset + inodeblks;
  sup->s_firstdatazone_old = nb = initblks;
  if(nb >= zones) pexit("bit maps too large");
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
  sup->s_log_zone_size = 0;
  {
	v2sq = (zone_t) V2_INDIRECTS(block_size) * V2_INDIRECTS(block_size);
	zo = V2_NR_DZONES + (zone_t) V2_INDIRECTS(block_size) + v2sq;
	{
		sup->s_magic = SUPER_V3;
  		sup->s_block_size = block_size;
  		sup->s_disk_version = 0;
#define MAX_MAX_SIZE 	(INT_MAX)
  		if(MAX_MAX_SIZE/block_size < zo) {
	  		sup->s_max_size = (int32_t) MAX_MAX_SIZE;
  		}
	  	else {
	  		sup->s_max_size = zo * block_size;
	  	}
	}
  }

  if (lseek(fd, (off_t) _STATIC_BLOCK_SIZE, SEEK_SET) == (off_t) -1) {
	pexit("super() couldn't seek");
  }
  if (write(fd, buf, _STATIC_BLOCK_SIZE) != _STATIC_BLOCK_SIZE) {
	pexit("super() couldn't write");
  }

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
void rootdir(inode)
ino_t inode;
{
  zone_t z;

  z = alloc_zone();
  add_zone(inode, z, 2 * sizeof(struct direct), current_time);
  enter_dir(inode, ".", inode);
  enter_dir(inode, "..", inode);
  incr_link(inode);
  incr_link(inode);
}

void enter_symlink(ino_t inode, char *lnk)
{
  zone_t z;
  char *buf;

  buf = alloc_block();
  z = alloc_zone();
  strcpy(buf, lnk);
  put_block(z, buf);

  add_zone(inode, z, (size_t) strlen(lnk), current_time);

  free(buf);
}


/*================================================================
 *	    eat_dir  -  recursively install directory
 *===============================================================*/
void eat_dir(parent)
ino_t parent;
{
  /* Read prototype lines and set up directory. Recurse if need be. */
  char *token[MAX_TOKENS], *p;
  char line[LINE_LEN];
  int mode, usrid, grpid, maj, min, f;
  ino_t n;
  zone_t z;
  size_t size;

  while (1) {
	getline(line, token);
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
void eat_file(inode, f)
ino_t inode;
int f;
{
  int ct, k;
  zone_t z = 0;
  char *buf;
  uint32_t timeval;

  buf = alloc_block();

  do {
	for (k = 0; k < block_size; k++) buf[k] = 0;
	if ((ct = read(f, buf, block_size)) > 0) {
		z = alloc_zone();
		put_block(z, buf);
	}
	timeval = (dflag ? current_time : file_time(f));
	if (ct) add_zone(inode, z, (size_t) ct, timeval);
  } while (ct == block_size);
  close(f);
  free(buf);
}

d2_inode *get_inoblock(ino_t i, block_t *blockno, d2_inode **ino)
{
	int off;
	d2_inode *inoblock = alloc_block();
	*blockno = ((i - 1) / inodes_per_block) + inode_offset;
	off = (i - 1) % inodes_per_block;
	get_block(*blockno, (char *) inoblock);
	*ino = inoblock + off;
	return inoblock;
}

int dir_try_enter(zone_t z, ino_t child, char *name)
{
	char *p1, *p2;
	struct direct *dir_entry = alloc_block();
	int r = 0;
	int i;

	get_block(z, (char *) dir_entry);

	for (i = 0; i < NR_DIR_ENTRIES(block_size); i++)
		if (!dir_entry[i].mfs_d_ino)
			break;

	if(i < NR_DIR_ENTRIES(block_size)) {
		int j;

		r = 1;
		dir_entry[i].mfs_d_ino = child;
		p1 = name;
		p2 = dir_entry[i].mfs_d_name;
		j = sizeof(dir_entry[i].mfs_d_name);
		assert(j == 60);
		while (j--) {
			*p2++ = *p1;
			if (*p1 != 0) p1++;
		}
	}

	put_block(z, (char *) dir_entry);
	free(dir_entry);

	return r;
}

/*================================================================
 *	    directory & inode management assist group
 *===============================================================*/
void enter_dir(ino_t parent, char const *name, ino_t child)
{
  /* Enter child in parent directory */
  /* Works for dir > 1 block and zone > block */
  unsigned int k;
  block_t b;
  zone_t z;
  zone_t *indirblock = alloc_block();
  d2_inode *ino;
  d2_inode *inoblock = get_inoblock(parent, &b, &ino);

  assert(!(block_size % sizeof(struct direct)));

  for (k = 0; k < V2_NR_DZONES; k++) {
	z = ino->d2_zone[k];
	if (z == 0) {
		z = alloc_zone();
		ino->d2_zone[k] = z;
	}

	if(dir_try_enter(z, child, __UNCONST(name))) {
		put_block(b, (char *) inoblock);
		free(inoblock);
		free(indirblock);
		return;
	}
  }

  /* no space in directory using just direct blocks; try indirect */
  if (ino->d2_zone[V2_NR_DZONES] == 0)
  	ino->d2_zone[V2_NR_DZONES] = alloc_zone();

  get_block(ino->d2_zone[V2_NR_DZONES], (char *) indirblock);

  for(k = 0; k < V2_INDIRECTS(block_size); k++) {
  	z = indirblock[k];
	if(!z) z = indirblock[k] = alloc_zone();

	if(dir_try_enter(z, child, __UNCONST(name))) {
		put_block(b, (char *) inoblock);
		put_block(ino->d2_zone[V2_NR_DZONES], (char *) indirblock);
		free(inoblock);
		free(indirblock);
		return;
	}
  }

  printf("Directory-inode %u beyond direct blocks.  Could not enter %s\n",
         parent, name);
  pexit("Halt");
}


void add_zone(ino_t n, zone_t z, size_t bytes, uint32_t cur_time)
{
  /* Add zone z to inode n. The file has grown by 'bytes' bytes. */

  int off, i;
  block_t b;
  zone_t indir;
  zone_t *blk;
  d2_inode *p;
  d2_inode *inode;

  if(!(blk = malloc(V2_INDIRECTS(block_size)*sizeof(*blk))))
  	pexit("Couldn't allocate indirect block");

  if(!(inode = malloc(V2_INODES_PER_BLOCK(block_size)*sizeof(*inode))))
  	pexit("Couldn't allocate block of inodes");

  b = ((n - 1) / V2_INODES_PER_BLOCK(block_size)) + inode_offset;
  off = (n - 1) % V2_INODES_PER_BLOCK(block_size);
  get_block(b, (char *) inode);
  p = &inode[off];
  p->d2_size += bytes;
  p->d2_mtime = cur_time;
  for (i = 0; i < V2_NR_DZONES; i++)
	if (p->d2_zone[i] == 0) {
		p->d2_zone[i] = z;
		put_block(b, (char *) inode);
  		free(blk);
  		free(inode);
		return;
	}
  put_block(b, (char *) inode);

  /* File has grown beyond a small file. */
  if (p->d2_zone[V2_NR_DZONES] == 0) p->d2_zone[V2_NR_DZONES] = alloc_zone();
  indir = p->d2_zone[V2_NR_DZONES];
  put_block(b, (char *) inode);
  b = indir;
  get_block(b, (char *) blk);
  for (i = 0; i < V2_INDIRECTS(block_size); i++)
	if (blk[i] == 0) {
		blk[i] = z;
		put_block(b, (char *) blk);
  		free(blk);
  		free(inode);
		return;
	}
  pexit("File has grown beyond single indirect");
}


void incr_link(n)
ino_t n;
{
  /* Increment the link count to inode n */
  int off;
  static int enter = 0;
  block_t b;

  if(enter) exit(1);

  b = ((n - 1) / inodes_per_block) + inode_offset;
  off = (n - 1) % inodes_per_block;
  {
	static d2_inode *inode2 = NULL;
	int s;

	s = sizeof(*inode2) * V2_INODES_PER_BLOCK(block_size);
	if(!inode2 && !(inode2 = malloc(s)))
		pexit("couldn't allocate a block of inodes");

	get_block(b, (char *) inode2);
	inode2[off].d2_nlinks++;
	put_block(b, (char *) inode2);
  }
  enter = 0;
}


void incr_size(n, count)
ino_t n;
size_t count;
{
  /* Increment the file-size in inode n */
  block_t b;
  int off;

  b = ((n - 1) / inodes_per_block) + inode_offset;
  off = (n - 1) % inodes_per_block;
  {
	d2_inode *inode2;
	if(!(inode2 = malloc(V2_INODES_PER_BLOCK(block_size) * sizeof(*inode2))))
		pexit("couldn't allocate a block of inodes");

	get_block(b, (char *) inode2);
	inode2[off].d2_size += count;
	put_block(b, (char *) inode2);
	free(inode2);
  }
}


/*================================================================
 * 	 	     allocation assist group
 *===============================================================*/
static ino_t alloc_inode(mode, usrid, grpid)
int mode, usrid, grpid;
{
  ino_t num;
  int off;
  block_t b;

  num = next_inode++;
  if (num > nrinodes) {
  	fprintf(stderr, "have %d inodoes\n", nrinodes);
  	pexit("File system does not have enough inodes");
  }
  b = ((num - 1) / inodes_per_block) + inode_offset;
  off = (num - 1) % inodes_per_block;
  {
	d2_inode *inode2;

	if(!(inode2 = malloc(V2_INODES_PER_BLOCK(block_size) * sizeof(*inode2))))
		pexit("couldn't allocate a block of inodes");

	get_block(b, (char *) inode2);
	inode2[off].d2_mode = mode;
	inode2[off].d2_uid = usrid;
	inode2[off].d2_gid = grpid;
	put_block(b, (char *) inode2);

	free(inode2);
  }

  /* Set the bit in the bit map. */
  /* DEBUG FIXME.  This assumes the bit is in the first inode map block. */
  insert_bit((block_t) INODE_MAP, (int) num);
  return(num);
}


static zone_t alloc_zone()
{
  /* Allocate a new zone */
  /* Works for zone > block */
  block_t b;
  zone_t z;

  z = next_zone++;
  b = z;
  if ((b + 1) > nrblocks)
	pexit("File system not big enough for all the files");
  put_block(b, zero);	/* give an empty zone */
  /* DEBUG FIXME.  This assumes the bit is in the first zone map block. */
  insert_bit(zone_map, (int) (z - zoff));	/* lint, NOT OK because
						 * z hasn't been broken
						 * up into block +
						 * offset yet. */
  return(z);
}


void insert_bit(block, bit)
block_t block;
int bit;
{
  /* Insert 'count' bits in the bitmap */
  int w, s;
#if defined(__minix)
  bitchunk_t *buf;
#else
  uint32_t *buf;
#endif

#if defined(__minix)
  buf = (bitchunk_t *) alloc_block();
#else
  buf = (uint32_t *) alloc_block();
#endif

  get_block(block, (char *) buf);
#if defined(__minix)
  w = bit / (8 * sizeof(bitchunk_t));
  s = bit % (8 * sizeof(bitchunk_t));
#else
  w = bit / (8 * sizeof(uint32_t));
  s = bit % (8 * sizeof(uint32_t));
#endif
  buf[w] |= (1 << s);
  put_block(block, (char *) buf);

  free(buf);
}


/*================================================================
 * 		proto-file processing assist group
 *===============================================================*/
int mode_con(p)
char *p;
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
  if (c1 == '-') mode |= S_IFREG;
  if (c2 == 'u') mode |= S_ISUID;
  if (c3 == 'g') mode |= S_ISGID;
  return(mode);
}

void getline(line, parse)
char *parse[MAX_TOKENS];
char line[LINE_LEN];
{
  /* Read a line and break it up in tokens */
  int k;
  char c, *p;
  int d;

  for (k = 0; k < MAX_TOKENS; k++) parse[k] = 0;
  for (k = 0; k < LINE_LEN; k++) line[k] = 0;
  k = 0;
  parse[0] = 0;
  p = line;
  while (1) {
	if (++k > LINE_LEN) pexit("Line too long");
	d = fgetc(proto);
	if (d == EOF) pexit("Unexpected end-of-file");
	*p = d;
	if (*p == '\n') lct++;
	if (*p == ' ' || *p == '\t') *p = 0;
	if (*p == '\n') {
		*p++ = 0;
		*p = '\n';
		break;
	}
	p++;
  }

  k = 0;
  p = line;
  lastp = line;
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
void check_mtab(device)
char *device;			/* /dev/hd1 or whatever */
{
/* Check to see if the special file named in s is mounted. */
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
	fprintf(stderr, "%s: stat %s failed: %s\n",
		progname, device, strerror(errno));
	exit(1);
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
		fprintf(stderr, "%s: %s is mounted on %s\n",
			progname, device, mount_point);
		exit(1);
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
#endif
}


uint32_t file_time(f)
int f;
{
  struct stat statbuf;
  fstat(f, &statbuf);
  return(statbuf.st_mtime);
}


__dead void pexit(char const * s)
{
  fprintf(stderr, "%s: %s\n", progname, s);
  if (lct != 0)
	fprintf(stderr, "Line %d being processed when error detected.\n", lct);
  exit(2);
}


void copy(from, to, count)
char *from, *to;
size_t count;
{
  while (count--) *to++ = *from++;
}

void *alloc_block()
{
	char *buf;

	if(!(buf = malloc(block_size))) {
		pexit("couldn't allocate filesystem buffer");
	}
  	bzero(buf, block_size);

	return buf;
}

void print_fs()
{
  int i, j;
  ino_t k;
  d2_inode *inode2;
  unsigned short *usbuf;
  block_t b;
  struct direct *dir;

  if(!(inode2 = malloc(V2_INODES_PER_BLOCK(block_size) * sizeof(*inode2))))
	pexit("couldn't allocate a block of inodes");

  if(!(dir = malloc(NR_DIR_ENTRIES(block_size)*sizeof(*dir))))
  	pexit("malloc of directory entry failed");

  usbuf = (unsigned short *) alloc_block();

  get_super_block((char *) usbuf);
  printf("\nSuperblock: ");
  for (i = 0; i < 8; i++) printf("%06o ", usbuf[i]);
  get_block((block_t) 2, (char *) usbuf);
  printf("...\nInode map:  ");
  for (i = 0; i < 9; i++) printf("%06o ", usbuf[i]);
  get_block((block_t) 3, (char *) usbuf);
  printf("...\nZone  map:  ");
  for (i = 0; i < 9; i++) printf("%06o ", usbuf[i]);
  printf("...\n");

  free(usbuf);
  usbuf = NULL;

  k = 0;
  for (b = inode_offset; k < nrinodes; b++) {
	get_block(b, (char *) inode2);
	for (i = 0; i < inodes_per_block; i++) {
		k = inodes_per_block * (int) (b - inode_offset) + i + 1;
		/* Lint but OK */
		if (k > nrinodes) break;
		{
			if (inode2[i].d2_mode != 0) {
				printf("Inode %2u:  mode=", k);
				printf("%06o", inode2[i].d2_mode);
				printf("  uid=%2d  gid=%2d  size=",
				inode2[i].d2_uid, inode2[i].d2_gid);
				printf("%6d", inode2[i].d2_size);
				printf("  zone[0]=%u\n", inode2[i].d2_zone[0]);
			}
			if ((inode2[i].d2_mode & S_IFMT) == S_IFDIR) {
				/* This is a directory */
				get_block(inode2[i].d2_zone[0], (char *) dir);
				for (j = 0; j < NR_DIR_ENTRIES(block_size); j++)
					if (dir[j].mfs_d_ino)
						printf("\tInode %2u: %s\n", dir[j].mfs_d_ino, dir[j].mfs_d_name);
			}
		}
	}
  }

  printf("%d inodes used.     %d zones used.\n", next_inode - 1, next_zone);
  free(dir);
  free(inode2);
}


int read_and_set(n)
block_t n;
{
/* The first time a block is read, it returns all 0s, unless there has
 * been a write.  This routine checks to see if a block has been accessed.
 */

  int w, s, mask, r;

  w = n / 8;
  if(w >= umap_array_elements) {
	pexit("umap array too small - this can't happen");
  }
  s = n % 8;
  mask = 1 << s;
  r = (umap_array[w] & mask ? 1 : 0);
  umap_array[w] |= mask;
  return(r);
}

__dead void usage()
{
  fprintf(stderr,
	  "Usage: %s [-12dlot] [-b blocks] [-i inodes]\n"
	  	"\t[-x extra] [-B blocksize] special [proto]\n",
	  progname);
  exit(1);
}

void special(string)
char *string;
{
  fd = creat(string, 0777);
  close(fd);
  fd = open(string, O_RDWR);
  if (fd < 0) pexit("Can't open special file");
}



void get_block(n, buf)
block_t n;
char *buf;
{
/* Read a block. */

  int k;

  /* First access returns a zero block */
  if (read_and_set(n) == 0) {
	copy(zero, buf, block_size);
	return;
  }
  lseek64(fd, mul64u(n, block_size), SEEK_SET, NULL);
  k = read(fd, buf, block_size);
  if (k != block_size) {
	pexit("get_block couldn't read");
  }
}

void get_super_block(buf)
char *buf;
{
/* Read a block. */

  int k;

  if(lseek(fd, (off_t) SUPER_BLOCK_BYTES, SEEK_SET) < 0) {
  	perror("lseek");
  	pexit("seek failed");
  }
  k = read(fd, buf, _STATIC_BLOCK_SIZE);
  if (k != _STATIC_BLOCK_SIZE) {
	pexit("get_super_block couldn't read");
  }
}

void put_block(n, buf)
block_t n;
char *buf;
{
/* Write a block. */

  (void) read_and_set(n);

  /* XXX - check other lseeks too. */
  if (lseek64(fd, mul64u(n, block_size), SEEK_SET, NULL) == (off_t) -1) {
	pexit("put_block couldn't seek");
  }
  if (write(fd, buf, block_size) != block_size) {
	pexit("put_block couldn't write");
  }
}

