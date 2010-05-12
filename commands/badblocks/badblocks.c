/* badblocks - collect bad blocks in a file	Author: Jacob Bunschoten */

/* Usage "badblocks block_special [Up_to_7_blocks]" */

/* This program is written to handle BADBLOCKS on a hard or floppy disk.
 * The program asks for block_numbers. These numbers can be obtained with
 * the program readall, written by A. Tanenbaum.  It then creates a
 * file on the disk containing up to 7 bad blocks.
 *
 * BUG:
 *
 *	When the zone_size > block_size it can happen that
 *	the zone is already allocated. This means some
 *	file is using this zone and may use all the blocks including
 *	the bad one. This can be cured by inspecting the zone_bitmap
 *	(is already done) and change the file if this zone is used.
 *	This means that another zone must be allocated and
 *	the inode wich claims this zone must be found and changed.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <minix/config.h>
#include <minix/type.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <dirent.h>
#include <stdlib.h>

#include "mfs/const.h"	/* must be included before stdio.h */
#undef printf			/* so its define of printf can be undone */
#include "mfs/type.h"

#include <string.h>
#include <stdio.h>

#define EXTERN extern
#include "mfs/super.h"

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void get_super, (void));
_PROTOTYPE(void rw_inode, (struct stat * stat_ptr, int rw_mode));
_PROTOTYPE(void get_inode, (struct stat * stat_ptr));
_PROTOTYPE(void put_inode, (struct stat * stat_ptr));
_PROTOTYPE(long rd_cmdline, (int argc, char *argv[]));
_PROTOTYPE(void modify, (int nr_blocks));
_PROTOTYPE(void save_blk, (block_t blk_num));
_PROTOTYPE(void reset_blks, (void));
_PROTOTYPE(void show_blks, (void));
_PROTOTYPE(int blk_is_used, (block_t blk_num));
_PROTOTYPE(int blk_ok, (block_t num));
_PROTOTYPE(void set_bit, (zone_t num));
_PROTOTYPE(long rd_num, (void));
_PROTOTYPE(int ok, (char *str));
_PROTOTYPE(void done, (int nr));

/* 		Super block table.
 *
 * 	The disk layout is:
 *
 *      Item        # block
 *    boot block      1
 *    super block     1
 *    inode map     s_imap_blocks
 *    zone map      s_zmap_blocks
 *    inodes        (s_ninodes + 1 + inodes_per_block - 1)/inodes_per_block
 *    unused
 *    data zones    (s_nzones - s_firstdatazone) << s_log_zone_size
 *
 */

#define OK	0
#define NOT_OK	1
#define QUIT	2

#define READ 	0
#define WRITE	1

#define HARMLESS	0
#define DIR_CREATED	1
#define DEV_MOUNTED	2
#define FILE_EXISTS	3
#define SUCCESS		4

#define BYTE         0377
#define BLOCK_SIZE   1024
#define SIZE_OF_INT   (sizeof (int) )

/* Define V_NR_DZONES as the larger of V1_NR_DZONES and V2_NR_DZONES. */
#if (V1_NR_DZONES > V2_NR_DZONES)
#define V_NR_DZONES V1_NR_DZONES
#define V_SMALLER   V2_NR_DZONES
#else
#define V_NR_DZONES V2_NR_DZONES
#define V_SMALLER   V1_NR_DZONES
#endif


 /* ====== globals ======= */

char *dev_name;
char f_name[] = ".Bad_XXXXXX";
char file_name[50];
char dir_name[] = "/tmpXXXXXX";

block_t block[V_NR_DZONES + 1];	/* last block contains zero */
int interactive;		/* 1 if interactive (argc == 2) */
int position = 2;		/* next block # is argv[position] */

FILE *f;
int fd;
int eofseen;			/* set if '\n' seen */
struct stat stat_buf;
struct super_block *sp, sbs;
int inodes_per_block;
size_t inode_size;
int v1fs = 0, v2fs = 0;			/* TRUE for V1 file system, FALSE for V2 */

d1_inode d1inode;		/* declare a V1 disk inode */
d1_inode *ip1;
d2_inode d2inode;		/* declare a V2 disk inode */
d2_inode *ip2;


 /* ====== super block routines ======= */

void get_super()
 /* Get super_block. global pointer sp is used */
{
  int rd;

  lseek(fd, (long) BLOCK_SIZE, SEEK_SET);	/* seek */

  rd = read(fd, (char *) sp, SUPER_SIZE);
  if (rd != SUPER_SIZE) {	/* ok ? */
	printf("Bad read in get_super() (should be %u is %d)\n",
	       (unsigned) SUPER_SIZE, rd);
	done(DIR_CREATED);
  }

  if (sp->s_magic == SUPER_MAGIC) {
	/* This is a V1 file system. */
	v1fs = 1;		/* file system is not V2 */
  } else if (sp->s_magic == SUPER_V2) {
	/* This is a V2 file system. */
	v2fs = 1;		/* this is a V2 file system */
  } else if (sp->s_magic == SUPER_V3) {
	v1fs = v2fs = 0;		/* this is a V3 file system */
  } else {
	/* Neither V1 nor V2 nor V3. */
	printf("Bad magic number in super_block (0x%x)\n",
	       (unsigned) sp->s_magic);
	done(DIR_CREATED);
  }
}


 /* ========== inode routines =========== */

void rw_inode(stat_ptr, rw_mode)
struct stat *stat_ptr;
int rw_mode;
{
  int rwd;
  ino_t i_num;
  block_t blk, offset;


  i_num = stat_ptr->st_ino;

  blk = (block_t) (START_BLOCK + sp->s_imap_blocks + sp->s_zmap_blocks);
  blk += (block_t) ((i_num - 1) / inodes_per_block);
  blk *= (block_t) (BLOCK_SIZE);/* this block */

  offset = (block_t) ((i_num - 1) % inodes_per_block);
  offset *= (block_t) (inode_size);	/* and this offset */

  lseek(fd, (off_t) (blk + offset), SEEK_SET);	/* seek */

  /* Pointer is at the inode */
  if (v1fs) {
	/* This is a V1 file system. */
	if (rw_mode == READ) {	/* read it */
		rwd = read(fd, (char *) ip1, inode_size);
	} else {		/* write it */
		rwd = write(fd, (char *) ip1, inode_size);
	}
  } else {
	/* This is a V2 file system. */
	if (rw_mode == READ) {	/* read it */
		rwd = read(fd, (char *) ip2, inode_size);
	} else {		/* write it */
		rwd = write(fd, (char *) ip2, inode_size);
	}
  }

  if (rwd != inode_size) {	/* ok ? */
	printf("Bad %s in get_inode()\n", (rw_mode == READ) ? "read" :
	       "write");
	done(DIR_CREATED);
  }
}

void get_inode(stat_ptr)
struct stat *stat_ptr;
{

  int cnt;

  rw_inode(stat_ptr, READ);

  if (v1fs) {
	for (cnt = 0; cnt < V1_NR_TZONES; cnt++)
		ip1->d1_zone[cnt] = 0;	/* Just to be safe */
  } else {
	for (cnt = 0; cnt < V2_NR_TZONES; cnt++)
		ip2->d2_zone[cnt] = 0;	/* Just to be safe */
  }
}

void put_inode(stat_ptr)
struct stat *stat_ptr;
{
  rw_inode(stat_ptr, WRITE);
}


 /* ==============  main program ================= */
int main(argc, argv)
int argc;
char *argv[];
{
  int cnt, finished;
  block_t blk_nr;
  struct stat dev_stat;
  FILE *fp;
  int block_size;

  sp = &sbs;
  ip1 = &d1inode;
  ip2 = &d2inode;

  if (argc < 2 || argc > 9) {
	fprintf(stderr, "Usage: %s block_special [up_to_7_blocks]\n", argv[0]);
	done(HARMLESS);
  }
  interactive = (argc == 2 ? 1 : 0);

  /* Do some test. */
  if (geteuid()) {
	printf("Sorry, not in superuser mode \n");
	printf("Set_uid bit must be on or you must become super_user\n");
	done(HARMLESS);
  }
  dev_name = argv[1];
  mktemp(dir_name);
  if (mkdir(dir_name, 0777) == -1) {
	fprintf(stderr, "%s is already used in system\n", dir_name);
	done(HARMLESS);
  }

  /* Mount device. This call may fail. */
  mount(dev_name, dir_name, 0, NULL, NULL);
  /* Succes. dev was mounted, try to umount */

  /* Umount device. Playing with the file system while other processes
   * have access to this device is asking for trouble */
  if (umount(dev_name) == -1) {
	printf("Could not umount device %s.\n", dev_name);
	done(HARMLESS);
  }
  mktemp(f_name);
  /* Create "/tmpXXXXpid/.BadXXpid" */
  strcat(file_name, dir_name);
  strcat(file_name, "/");
  strcat(file_name, f_name);

  if (mount(dev_name, dir_name, 0, NULL, NULL) == -1) {	/* this call should work */
	fprintf(stderr, "Could not mount device anymore\n");
	done(HARMLESS);
  }
  if (stat(file_name, &stat_buf) != -1) {
	printf("File %s already exists\n", file_name);
	done(DEV_MOUNTED);
  }
  if ((fp = fopen(file_name, "w")) == NULL) {
	printf("Cannot create file %s\n", file_name);
	done(DEV_MOUNTED);
  }
  chmod(file_name, 0);		/* "useless" file */
  if (stat(file_name, &stat_buf) == -1) {
	printf("What? Second call from stat failed\n");
	done(FILE_EXISTS);
  }

  /* Stat buf must be safed. We can now calculate the inode on disk */
  fclose(fp);

  /* ===== the badblock file is created ===== */

  if (umount(dev_name) == -1) {
	printf("Can not umount device anymore??? \n");
	done(DIR_CREATED);
  }
  if ((fd = open(dev_name, O_RDWR)) == -1) {
	printf("Can not open device %s\n", dev_name);
	done(DEV_MOUNTED);
  }
  if (fstat(fd, &dev_stat) == -1) {
	printf("fstat on device %s failed\n", dev_name);
	done(DEV_MOUNTED);
  }
  if ((dev_stat.st_mode & S_IFMT) != S_IFBLK) {
	printf("Device \"%s\" is not a block_special.\n", dev_name);
	done(DEV_MOUNTED);
  }
  get_super();
  if (sp->s_log_zone_size) {
	printf("Block_size != zone_size.");
	printf("This program can not handle it\n");
	done(DIR_CREATED);
  }
  if(v1fs || v2fs) block_size = 1024;
  else block_size = sp->s_block_size;

  /* The number of inodes in a block differs in V1 and V2. */
  if (v1fs) {
	inodes_per_block = V1_INODES_PER_BLOCK;
	inode_size = V1_INODE_SIZE;
  } else {
	inodes_per_block = V2_INODES_PER_BLOCK(block_size);
	inode_size = V2_INODE_SIZE;
  }

  /* If the s_firstdatazone_old field is zero, we have to compute the value. */
  if (sp->s_firstdatazone_old == 0)
	sp->s_firstdatazone =
		START_BLOCK + sp->s_imap_blocks + sp->s_zmap_blocks +
		(sp->s_ninodes + inodes_per_block - 1) / inodes_per_block;
  else
	sp->s_firstdatazone = sp->s_firstdatazone_old;

  get_inode(&stat_buf);

  for (finished = 0; !finished;) {
	if (interactive)
		printf("Give up to %d bad block numbers separated by spaces\n",
		       V_SMALLER);
	reset_blks();
	cnt = 0;		/* cnt keep track of the zone's */
	while (cnt < V_SMALLER) {
		int tst;

		if (interactive)
			blk_nr = rd_num();
		else
			blk_nr = rd_cmdline(argc, argv);
		if (blk_nr == -1) break;
		tst = blk_ok(blk_nr);

		/* Test if this block is free */
		if (tst == OK) {
			cnt++;
			save_blk(blk_nr);
		} else if (tst == QUIT)
			break;
	}
	if (interactive) show_blks();
	if (!cnt) done(FILE_EXISTS);
	if (interactive) {
		switch (ok("All these blocks ok <y/n/q> (y:Device will change) ")) {
		    case OK:	finished = 1;	break;
		    case NOT_OK:
			break;
		    case QUIT:	done(FILE_EXISTS);
		}
	} else {
		finished = 1;
	}
  }

  modify(cnt);
  close(fd);			/* free device */
  done(SUCCESS);
  return(0);
}

long rd_cmdline(argc, argv)
int argc;
char *argv[];
{
  if (position == argc) return(-1);
  return(atol(argv[position++]));
}


void modify(nr_blocks)
int nr_blocks;
{
  int i;

  if (nr_blocks == 0) return;
  if (v1fs) {
	/* This is a V1 file system. */
	for (i = 0; i < nr_blocks; i++) {
		set_bit(block[i]);
		ip1->d1_zone[i] = block[i];
	}
  } else {
	/* This is a V2 file system. */
	for (i = 0; i < nr_blocks; i++) {
		set_bit(block[i]);
		ip2->d2_zone[i] = block[i];
	}
  }
  if (v1fs) {
	ip1->d1_size = (long) (BLOCK_SIZE * nr_blocks);	/* give file size */
	ip1->d1_mtime = 0;	/* Who wants a file from 1970? */
  } else {
	ip2->d2_size = (long) (BLOCK_SIZE * nr_blocks);	/* give file size */
	ip2->d2_atime = ip2->d2_mtime = ip2->d2_ctime = 0;
  }

  put_inode(&stat_buf);		/* save the inode on disk */
}


static blk_cnt = 0;

void save_blk(blk_num)
block_t blk_num;
{
  block[blk_cnt++] = blk_num;
}

void reset_blks()
{
  int i;

  for (i = 0; i <= V_NR_DZONES; i++)
	block[i] = 0;		/* Note: Last block_number is set to zero */
  blk_cnt = 0;
}

void show_blks()
{
  int i;

  for (i = 0; i < blk_cnt; i++)
	printf("Block[%d] = %lu\n", i, (unsigned long) block[i]);
}

int blk_is_used(blk_num)
block_t blk_num;
{				/* return TRUE(1) if used */
  int i;

  for (i = 0; block[i] && block[i] != blk_num; i++);
  return(block[i] != 0) ? 1 : 0;
}


 /* ===== bitmap handling ======	 */

#define BIT_MAP_SHIFT	13
#define INT_BITS	(SIZE_OF_INT << 3)

int blk_ok(num)			/* is this zone free (y/n) */
block_t num;
{
  block_t blk_offset;
  int rd;
  int blk, offset, words, bit, tst_word;
  zone_t z_num;

  if (blk_is_used(num)) {
	printf("Duplicate block (%lu) given\n", (unsigned long) num);
	return NOT_OK;
  }

  /* Assumption zone_size == block_size */

  z_num = num - (sp->s_firstdatazone - 1);	/* account offset */

  /* Calculate the word in the bitmap. */
  blk = z_num >> BIT_MAP_SHIFT;	/* which block */
  offset = z_num - (blk << BIT_MAP_SHIFT);	/* offset */
  words = z_num / INT_BITS;	/* which word */

  blk_offset = (block_t) (START_BLOCK + sp->s_imap_blocks);	/* zone map */
  blk_offset *= (block_t) BLOCK_SIZE;	/* of course in block */
  blk_offset += (block_t) (words * SIZE_OF_INT);	/* offset */


  lseek(fd, (off_t) blk_offset, SEEK_SET);	/* set pointer at word */

  rd = read(fd, (char *) &tst_word, SIZE_OF_INT);
  if (rd != SIZE_OF_INT) {
	printf("Read error in bitmap\n");
	done(DIR_CREATED);
  }

  /* We have the tst_word, check if bit was off */
  bit = offset % INT_BITS;

  if (((tst_word >> bit) & 01) == 0)	/* free */
	return OK;
  else {
	printf("Bad number %lu. ", (unsigned long) num);
	printf("This zone (block) is marked in bitmap\n");
	return NOT_OK;
  }
}

void set_bit(num)		/* write in the bitmap */
zone_t num;
{
  int rwd;
  long blk_offset;
  int blk, offset, words, tst_word, bit;
  unsigned z_num;

  z_num = num - (sp->s_firstdatazone - 1);

  blk = z_num >> BIT_MAP_SHIFT;	/* which block */
  offset = z_num - (blk << BIT_MAP_SHIFT);	/* offset in block */
  words = z_num / INT_BITS;	/* which word */

  blk_offset = (long) (START_BLOCK + sp->s_imap_blocks);
  blk_offset *= (long) BLOCK_SIZE;
  blk_offset += (long) (words * SIZE_OF_INT);


  lseek(fd, (off_t) blk_offset, SEEK_SET);

  rwd = read(fd, (char *) &tst_word, SIZE_OF_INT);
  if (rwd != SIZE_OF_INT) {
	printf("Read error in bitmap\n");
	done(DEV_MOUNTED);
  }
  bit = offset % INT_BITS;
  if (((tst_word >> bit) & 01) == 0) {	/* free */
	lseek(fd, (off_t) blk_offset, SEEK_SET);
	tst_word |= (1 << bit);	/* not free anymore */
	rwd = write(fd, (char *) &tst_word, SIZE_OF_INT);
	if (rwd != SIZE_OF_INT) {
		printf("Bad write in zone map\n");
		printf("Check file system \n");
		done(DIR_CREATED);
	}
	return;
  }
  printf("Bit map indicates that block %lu is in use. Not marked.\n",
	 (unsigned long) num);
/*  done(DIR_CREATED); */
  return;
}

 /* ======= interactive interface ======= */

long rd_num()
{				/* read a number from stdin */
  long num;
  int c;

  if (eofseen) return(-1);
  do {
	c = getchar();
	if (c == EOF || c == '\n') return(-1);
  } while (c != '-' && (c < '0' || c > '9'));

  if (c == '-') {
	printf("Block numbers must be positive\n");
	exit(1);
  }
  num = 0;
  while (c >= '0' && c <= '9') {
	num *= 10;
	num += c - '0';
	c = getchar();
	if (c == '\n') eofseen = 1;
  }
  return num;
}



int ok(str)
char *str;
{
  int c;

  for (;;) {
	printf("%s", str);
	while ((c = getchar()) != EOF &&
	       c != 'y' && c != 'n' && c != 'q')
		if (c != '\n') printf(" Bad character %c\n", (char) c);
	switch (c) {
	    case EOF:
		return QUIT;
	    case 'y':
		return OK;
	    case 'n':
		return NOT_OK;
	    case 'q':	return QUIT;
	}
	printf("\n");
  }
}


void done(nr)
int nr;
{
  switch (nr) {
      case SUCCESS:
      case FILE_EXISTS:
	unlink(file_name);
      case DEV_MOUNTED:
	umount(dev_name);
      case DIR_CREATED:
	rmdir(dir_name);
      case HARMLESS:;
  }
  sync();
  exit(nr == SUCCESS ? 0 : 1);
}
