/* This package consists of 4 routines for handling the /etc/mtab file.
 * The /etc/mtab file contains information about the root and mounted file
 * systems as a series of lines, each one with exactly four fields separated 
 * by one space as follows:
 *
 *	special mounted_on version rw_flag
 *
 * where 
 *	special is the name of the block special file
 *	mounted_on is the directory on which it is mounted
 *	version is either 1 or 2 for MINIX V1 and V2 file systems
 *	rw_flag is rw or ro for read/write or read only
 *
 * An example /etc/mtab:
 *
 *	/dev/ram / 2 rw
 *	/dev/hd1 /usr 2 rw
 *	/dev/fd0 /user 1 ro
 *
 *
 * The four routines for handling /etc/mtab are as follows.  They use two
 * (hidden) internal buffers, mtab_in for input and mtab_out for output.
 *
 *	load_mtab(&prog_name)		   - read /etc/mtab into mtab_in
 *	get_mtab_entry(&s1, &s2, &s3, &s4) - arrays that are filled in
 *
 * If load_mtab works, it returns 0.  If it fails, it prints its own error
 * message on stderr and returns -1.  When get_mtab_entry
 * runs out of entries to return, it sets the first pointer to NULL and returns
 * -1 instead of 0.
 */
 
#include <sys/types.h>
#include <lib.h>
#include <minix/minlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define BUF_SIZE   512		  /* size of the /etc/mtab buffer */

char *etc_mtab = "/etc/mtab";	  /* name of the /etc/mtab file */
static char mtab_in[BUF_SIZE+1];  /* holds /etc/mtab when it is read in */
static char *iptr = mtab_in;	  /* pointer to next line to feed out. */

int load_mtab(char *prog_name);
int get_mtab_entry(char dev[PATH_MAX], char mount_point[PATH_MAX],
	char type[MNTNAMELEN], char flags[MNTFLAGLEN]);
static void err(char *prog_name, const char *str);


int load_mtab(char *prog_name)
{
/* Read in /etc/mtab and store it in /etc/mtab. */

  int fd, n;
  char *ptr;

  /* Open the file. */
  fd = open(etc_mtab, O_RDONLY);
  if (fd < 0) {
	err(prog_name, ": cannot open ");
	return(-1);
  }

  /* File opened.  Read it in. */
  n = read(fd, mtab_in, BUF_SIZE);
  if (n <= 0) {
	/* Read failed. */
	err(prog_name, ": cannot read ");
	return(-1);
  }
  if (n == BUF_SIZE) {
	/* Some nut has mounted 50 file systems or something like that. */
	std_err(prog_name);
	std_err(": file too large: ");
	std_err(etc_mtab);
	return(-1);
  }

  close(fd);

  ptr = mtab_in;

  return(0);
}

int get_mtab_entry(char dev[PATH_MAX], char mount_point[PATH_MAX],
			char type[MNTNAMELEN], char flags[MNTFLAGLEN])
{
/* Return the next entry from mtab_in. */

  int r;

  if (iptr >= &mtab_in[BUF_SIZE]) {
	dev[0] = '\0';
	return(-1);
  }

  r = sscanf(iptr, "%s on %s type %s (%[^)]s\n",
		dev, mount_point, type, flags);
  if (r != 4) {
	dev[0] = '\0';
	return(-1);
  }

  iptr = strchr(iptr, '\n');	/* Find end of line */
  if (iptr != NULL) iptr++; 	/* Move to next line */
  return(0);
  
}

static void err(char *prog_name, const char *str)
{
  std_err(prog_name); 
  std_err(str);
  std_err(etc_mtab);
  perror(" ");
}
