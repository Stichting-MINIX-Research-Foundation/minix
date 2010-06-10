/* du - report on disk usage		Author: Alistair G. Crooks */

/*
 *	du.c		1.1	27/5/87		agc	Joypace Ltd.
 *			1.2	24 Mar 89	nick@nswitgould.oz
 *			1.3	31 Mar 89	nick@nswitgould.oz
 *			1.4	22 Feb 90	meulenbr@cst.prl.philips.nl
 *			1.5	09 Jul 91	hp@vmars.tuwien.ac.at
 *			1.6	01 Oct 92	kjb@cs.vu.nl
 *			1.7	04 Jan 93	bde
 *			1.8	19 Sep 94	kjb
 *			1.9	28 Oct 99	kjb
 *
 *	Copyright 1987, Joypace Ltd., London UK. All rights reserved.
 *	This code may be freely distributed, provided that this notice
 *	remains attached.
 *
 *	du - a public domain interpretation of du(1).
 *
 *  1.2: 	Fixed bug involving 14 character long filenames
 *  1.3:	Add [-l levels] option to restrict printing.
 *  1.4:	Added processing of multiple arguments
 *  1.5:	Fixed processing of multiple arguments. General cleanup.
 *  1.6:	Use readdir
 *  1.7:	Merged 1.5 and 1.6.
 *		Print totals even for non-dirs at top level.
 *		Count blocks for each dir before printing total for the dir.
 *		Count blocks for all non-special files.
 *		Don't clutter link buffer with directories.
 *  1.8:	Remember all links.
 *  1.9:	Added -x flag to not cross device boundaries.  Type fixes.
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <minix/config.h>
#include <minix/const.h>

#include "mfs/const.h"

extern char *optarg;
extern int optind;

#define	LINELEN		256
#define	NR_ALREADY	512

#ifdef S_IFLNK
#define	LSTAT lstat
#else
#define	LSTAT stat
#endif

typedef struct already {
  struct already *al_next;
  int al_dev;
  ino_t al_inum;
  nlink_t al_nlink;
} ALREADY;

_PROTOTYPE(int main, (int argc, char **argv));
PRIVATE _PROTOTYPE(int makedname, (char *d, char *f, char *out, int outlen));
_PROTOTYPE(int done, (dev_t dev, ino_t inum, nlink_t nlink));
_PROTOTYPE(long dodir, (char *d, int thislev, dev_t dev));

char *prog;			/* program name */
char *optstr = "aFsxdl:";	/* options */
int silent = 0;			/* silent mode */
int all = 0;			/* all directory entries mode */
int crosschk = 0;		/* do not cross device boundaries mode */
int fsoverhead = 0;		/* include FS overhead */
char *startdir = ".";		/* starting from here */
int levels = 20000;		/* # of directory levels to print */
ALREADY *already[NR_ALREADY];
int alc;


/*
 *	makedname - make the pathname from the directory name, and the
 *	directory entry, placing it in out. If this would overflow,
 *	return 0, otherwise 1.
 */
PRIVATE int makedname(char *d, char *f, char *out, int outlen)
{
  char *cp;
  int length = strlen(f);

  if (strlen(d) + length + 2 > outlen) return(0);
  for (cp = out; *d; *cp++ = *d++);
  if (*(cp - 1) != '/') *cp++ = '/';
  while (length--) *cp++ = *f++;
  *cp = '\0';
  return(1);
}

/*
 *	done - have we encountered (dev, inum) before? Returns 1 for yes,
 *	0 for no, and remembers (dev, inum, nlink).
 */
int done(dev_t dev, ino_t inum, nlink_t nlink)
{
  register ALREADY **pap, *ap;

  if (fsoverhead) return 0;

  pap = &already[(unsigned) inum % NR_ALREADY];
  while ((ap = *pap) != NULL) {
	if (ap->al_inum == inum && ap->al_dev == dev) {
		if (--ap->al_nlink == 0) {
			*pap = ap->al_next;
			free(ap);
		}
		return(1);
	}
	pap = &ap->al_next;
  }
  if ((ap = malloc(sizeof(*ap))) == NULL) {
	fprintf(stderr, "du: Out of memory\n");
	exit(1);
  }
  ap->al_next = NULL;
  ap->al_inum = inum;
  ap->al_dev = dev;
  ap->al_nlink = nlink - 1;
  *pap = ap;
  return(0);
}

int get_block_size(const char *dir, const struct stat *st)
{
  struct statfs stfs;
  static int fs_block_size = -1, fs_dev = -1;
  int d;

  if(st->st_dev == fs_dev)
  	return fs_block_size;

  if((d = open(dir, O_RDONLY)) < 0) {
  	perror(dir);
  	return 0;
  }

  if(fstatfs(d, &stfs) < 0) {
  	perror(dir);
  	return 0;
  }

  fs_block_size = stfs.f_bsize;
  fs_dev = st->st_dev;

  return fs_block_size;
}


/*
 *	dodir - process the directory d. Return the long size (in blocks)
 *	of d and its descendants.
 */
long dodir(char *d, int thislev, dev_t dev)
{
  int maybe_print;
  struct stat s;
  long indir_blocks, indir2_blocks, indir_per_block;
  long total_blocks, total_kb;
  char dent[LINELEN];
  DIR *dp;
  struct dirent *entry;
  int block_size;

  if (LSTAT(d, &s) < 0) {
	fprintf(stderr,
		"%s: %s: %s\n", prog, d, strerror(errno));
    	return 0L;
  }
  if (s.st_dev != dev && dev != 0 && crosschk) return 0;
  block_size = get_block_size(d, &s);
  if(block_size < 1) {
	fprintf(stderr,
		"%s: %s: funny block size found (%d)\n", 
			prog, d, block_size);
    	return 0L;
  }
  total_blocks = (s.st_size + (block_size - 1)) / block_size;
  if (fsoverhead) {
	/* file system overhead: indirect blocks */
	indir_per_block = block_size / sizeof(zone_t);
	indir_blocks = (total_blocks - V2_NR_DZONES) / indir_per_block;
	total_blocks += indir_blocks;
	indir2_blocks = (indir_blocks - 1) / (indir_per_block * indir_per_block);
	total_blocks += indir2_blocks;
  }
  total_kb = total_blocks * block_size / 1024;
  switch (s.st_mode & S_IFMT) {
    case S_IFDIR:
	/* Directories should not be linked except to "." and "..", so this
	 * directory should not already have been done.
	 */
	maybe_print = !silent;
	if ((dp = opendir(d)) == NULL) break;
	while ((entry = readdir(dp)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 ||
		    strcmp(entry->d_name, "..") == 0)
			continue;
		if (!makedname(d, entry->d_name, dent, sizeof(dent))) continue;
		total_kb += dodir(dent, thislev - 1, s.st_dev);
	}
	closedir(dp);
	break;
    case S_IFBLK:
    case S_IFCHR:
	/* st_size for special files is not related to blocks used. */
	total_kb = 0;
	/* Fall through. */
    default:
	if (s.st_nlink > 1 && done(s.st_dev, s.st_ino, s.st_nlink)) return 0L;
	maybe_print = all;
	break;
  }
  if (thislev >= levels || (maybe_print && thislev >= 0)) {
	printf("%ld\t%s\n", total_kb, d);
  }
  return(total_kb);
}

int main(argc, argv)
int argc;
char **argv;
{
  int c;

  prog = argv[0];
  while ((c = getopt(argc, argv, optstr)) != EOF) switch (c) {
	    case 'a':	all = 1;	break;
	    case 's':	silent = 1;	break;
	    case 'x':
	    case 'd':	crosschk = 1;	break;
	    case 'F':	fsoverhead = 1;	break;
	    case 'l':	levels = atoi(optarg);	break;
	    default:
		fprintf(stderr,
			"Usage: %s [-asxF] [-l levels] [startdir]\n", prog);
		exit(1);
	}
  do {
	if (optind < argc) startdir = argv[optind++];
	alc = 0;
	(void) dodir(startdir, levels, 0);
  } while (optind < argc);
  return(0);
}
