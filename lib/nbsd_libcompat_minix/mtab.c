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
 *	put_mtab_entry(&s1, &s2, &s3, &s4) - append a line to mtab_out
 *	rewrite_mtab(&prog_name)	   - write mtab_out to /etc/mtab
 *
 * If load_mtab and rewrite_mtab work, they return 0.  If they fail, they
 * print their own error messages on stderr and return -1.  When get_mtab_entry
 * runs out of entries to return, it sets the first pointer to NULL and returns
 * -1 instead of 0.  Also, rewrite_mtab returns -1 if it fails.
 */
 
#include <sys/types.h>
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
static char mtab_out[BUF_SIZE+1]; /* buf to build /etc/mtab for output later */
static char *iptr = mtab_in;	  /* pointer to next line to feed out. */
static char *optr = mtab_out;	  /* pointer to place where next line goes */

_PROTOTYPE(int load_mtab, (char *prog_name ));
_PROTOTYPE(int rewrite_mtab, (char *prog_name ));
_PROTOTYPE(int get_mtab_entry, (char *special, char *mounted_on, 
					char *version, char *rw_flag));
_PROTOTYPE(int put_mtab_entry, (char *special, char *mounted_on, 
					char *version, char *rw_flag));
_PROTOTYPE(void err, (char *prog_name, char *str ));


int load_mtab(prog_name)
char *prog_name;
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

  /* Replace all the whitespace by '\0'. */
  ptr = mtab_in;
  while (*ptr != '\0') {
	if (isspace(*ptr)) *ptr = '\0';
	ptr++;
  }
  return(0);
}


int rewrite_mtab(prog_name)
char *prog_name;
{
/* Write mtab_out to /etc/mtab. */

  int fd, n;

  /* Do a creat to truncate the file. */
  fd = creat(etc_mtab, 0777);
  if (fd < 0) {
	err(prog_name, ": cannot overwrite ");
	return(-1);
  }

  /* File created.  Write it. */
  n = write(fd, mtab_out, (unsigned int)(optr - mtab_out));
  if (n <= 0) {
	/* Write failed. */
	err(prog_name, " could not write ");
	return(-1);
  }

  close(fd);
  return(0);
}


int get_mtab_entry(special, mounted_on, version, rw_flag)
char *special;
char *mounted_on;
char *version;
char *rw_flag;
{
/* Return the next entry from mtab_in. */

  if (iptr >= &mtab_in[BUF_SIZE]) {
	special[0] = '\0';
	return(-1);
  }

  strcpy(special, iptr);
  while (isprint(*iptr)) iptr++;
  while (*iptr == '\0'&& iptr < &mtab_in[BUF_SIZE]) iptr++;

  strcpy(mounted_on, iptr);
  while (isprint(*iptr)) iptr++;
  while (*iptr == '\0'&& iptr < &mtab_in[BUF_SIZE]) iptr++;

  strcpy(version, iptr);
  while (isprint(*iptr)) iptr++;
  while (*iptr == '\0'&& iptr < &mtab_in[BUF_SIZE]) iptr++;

  strcpy(rw_flag, iptr);
  while (isprint(*iptr)) iptr++;
  while (*iptr == '\0'&& iptr < &mtab_in[BUF_SIZE]) iptr++;
  return(0);
}


int put_mtab_entry(special, mounted_on, version, rw_flag)
char *special;
char *mounted_on;
char *version;
char *rw_flag;
{
/* Append an entry to the mtab_out buffer. */

  int n1, n2, n3, n4;

  n1 = strlen(special);
  n2 = strlen(mounted_on);
  n3 = strlen(version);
  n4 = strlen(rw_flag);

  if (optr + n1 + n2 + n3 + n4 + 5 >= &mtab_out[BUF_SIZE]) return(-1);
  strcpy(optr, special);
  optr += n1;
  *optr++ = ' ';

  strcpy(optr, mounted_on);
  optr += n2;
  *optr++ = ' ';

  strcpy(optr, version);
  optr += n3;
  *optr++ = ' ';

  strcpy(optr, rw_flag);
  optr += n4;
  *optr++ = '\n';
  return(0);
}


void
err(prog_name, str)
char *prog_name, *str;
{
  std_err(prog_name); 
  std_err(str);
  std_err(etc_mtab);
  perror(" ");
}
