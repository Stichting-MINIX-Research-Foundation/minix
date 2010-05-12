/* Touch - change file access and modification times.
 *
 * Usage: see end of file
 *
 * Conforms to P1003.2 draft 10, sec. 4.62, except that time values
 * are not checked for validity, but passed on to mktime, so that
 * 9301990000 will refer to Apr. 9th 1993. As a side effect, leap
 * seconds are not handled correctly.
 *
 * Authors: Original author unknown. Rewritten for POSIX by 
 *	Peter Holzer (hp@vmars.tuwien.ac.at).
 *
 * $Id$
 * $Log$
 * Revision 1.1  2005/04/21 14:55:35  beng
 * Initial revision
 *
 * Revision 1.1.1.1  2005/04/20 13:33:47  beng
 * Initial import of minix 2.0.4
 *
 * Revision 1.8  1994/03/17  21:39:19  hjp
 * fixed bug with 4-digit years
 *
 * Revision 1.7  1994/03/15  00:43:27  hjp
 * Changes from kjb (vmd 1.6.25.1):
 * fixed exit code
 * nonstandard flag 0 to make file very old
 *
 * Revision 1.6  1994/02/12  17:26:33  hjp
 * fixed -a and -m flags
 *
 * Revision 1.5  1994/02/12  16:04:13  hjp
 * fixed bug when -t argument was not given
 * removed debugging code
 * run through pretty to get Minix layout
 *
 * Revision 1.4  1994/02/07  21:23:11  hjp
 * POSIXified.
 *
 */

#define _POSIX_C_SOURCE 2	/* getopt */
#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>

#define val2(string) ((string)[0] * 10 + (string)[1] - '0' * 11)
#define val4(string) (val2(string) * 100 + val2(string + 2))

typedef enum {
  OLD, NEW
} formatT;

char *cmnd;
int no_creat = 0;
unsigned int to_change = 0;
#	define ATIME	1
#	define MTIME	2

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(int doit, (char *name, struct utimbuf tvp));
_PROTOTYPE(void usage, (void));
_PROTOTYPE(time_t parsetime, (const char *string, formatT format));

time_t parsetime(string, format)
const char *string;
formatT format;
{
  struct tm tm;
  time_t touchtime;
  size_t l;

  l = strspn(string, "0123456789");
  if (l % 2 == 1) return -1;
  if (string[l] != '\0' && (string[l] != '.' || format == OLD)) {
	return -1;
  }
  if (format == OLD) {
	if (l == 10) {
		/* Last two digits are year */
		tm.tm_year = val2(string + 8);
		if (tm.tm_year <= 68) tm.tm_year += 100;
	} else if (l == 8) {
		time(&touchtime);
		tm = *localtime(&touchtime);
	} else {
		return -1;
	}
  } else {
	if (l == 12) {
		/* First four digits are year */
		tm.tm_year = val4(string) - 1900;
		string += 4;
	} else if (l == 10) {
		/* First two digits are year */
		tm.tm_year = val2(string);
		if (tm.tm_year <= 68) tm.tm_year += 100;
		string += 2;
	} else if (l == 8) {
		time(&touchtime);
		tm = *localtime(&touchtime);
	} else {
		return -1;
	}
  }
  tm.tm_mon = val2(string) - 1;
  string += 2;
  tm.tm_mday = val2(string);
  string += 2;
  tm.tm_hour = val2(string);
  string += 2;
  tm.tm_min = val2(string);
  string += 2;
  if (format == NEW && string[0] == '.') {
	if (isdigit(string[1]) && isdigit(string[2]) &&
	    string[3] == '\0') {
		tm.tm_sec = val2(string + 1);
	} else {
		return -1;
	}
  } else {
	tm.tm_sec = 0;
  }
  tm.tm_isdst = -1;
  touchtime = mktime(&tm);
  return touchtime;
}


int main(argc, argv)
int argc;
char **argv;
{
  time_t auxtime;
  struct stat sb;
  int c;
  struct utimbuf touchtimes;
  int fail = 0;

  cmnd = argv[0];
  auxtime = time((time_t *) NULL);
  touchtimes.modtime = auxtime;
  touchtimes.actime = auxtime;

  while ((c = getopt(argc, argv, "r:t:acm0")) != EOF) {
	switch (c) {
	    case 'r':
		if (stat(optarg, &sb) == -1) {
			fprintf(stderr, "%s: cannot stat %s: %s\n",
				cmnd, optarg, strerror(errno));
			exit(1);
		}
		touchtimes.modtime = sb.st_mtime;
		touchtimes.actime = sb.st_atime;
		break;
	    case 't':
		auxtime = parsetime(optarg, NEW);
		if (auxtime == (time_t) - 1) usage();
		touchtimes.modtime = auxtime;
		touchtimes.actime = auxtime;
		break;
	    case 'a':	to_change |= ATIME;	break;
	    case 'm':	to_change |= MTIME;	break;
	    case 'c':	no_creat = 1;	break;
	    case '0':
		touchtimes.modtime = touchtimes.actime = 0;
		break;
	    case '?':	usage();	break;
	    default:	assert(0);
	}
  }
  if (to_change == 0) {
	to_change = ATIME | MTIME;
  }
  if (optind == argc) usage();

  /* Now check for old style time argument */
  if (strcmp(argv[optind - 1], "--") != 0 &&
      (auxtime = parsetime(argv[optind], OLD)) != (time_t) - 1) {
	touchtimes.modtime = auxtime;
	touchtimes.actime = auxtime;
	optind++;
	if (optind == argc) usage();
  }
  while (optind < argc) {
	if (doit(argv[optind], touchtimes) > 0) {
		fprintf(stderr, "%s: cannot touch %s: %s\n",
			cmnd, argv[optind], strerror(errno));
		fail = 1;
	}
	optind++;
  }
  return fail ? 1 : 0;
}


int doit(name, tvp)
char *name;
struct utimbuf tvp;
{
  int fd;
  struct stat sb;

  if (to_change != (ATIME | MTIME)) {

	if (stat(name, &sb) != -1) {
		if (!(to_change & ATIME)) {
			tvp.actime = sb.st_atime;
		} else {
			tvp.modtime = sb.st_mtime;
		}
	}
  }
  if (utime(name, &tvp) == 0) return 0;
  if (errno != ENOENT) return 1;
  if (no_creat == 1) return 0;
  if ((fd = creat(name, 0666)) >= 0) {
	if (fstat(fd, &sb) != -1) {
		if (!(to_change & ATIME)) {
			tvp.actime = sb.st_atime;
		} else {
			tvp.modtime = sb.st_mtime;
		}
	} else {
		assert(0);
	}
	close(fd);
	if (utime(name, &tvp) == 0) return 0;
  }
  return 1;
}


void usage()
{
  fprintf(stderr, "Usage: %s [-c] [-a] [-m] [-r file] [-t [CC[YY]]MMDDhhmm[.ss]] "
	"[MMDDhhmm[YY]] file...\n", cmnd);
  exit(1);
}
