/* printroot - print root device on stdout	Author: Bruce Evans */

/* This program figures out what the root device is by doing a stat on it, and
 * then searching /dev until it finds an entry with the same device number.
 *
 *  9 Dec 1989	- clean up for 1.5 - full prototypes (BDE)
 * 15 Oct 1989	- avoid ACK cc bugs (BDE):
 *		- sizeof "foo" is 2 (from wrong type char *) instead of 4
 *		- char foo[10] = "bar"; allocates 4 bytes instead of 10
 *  1 Oct 1989	- Minor changes by Andy Tanenbaum
 *  5 Oct 1992	- Use readdir (kjb)
 * 26 Nov 1994	- Flag -r: print just the root device (kjb)
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <minix/minlib.h>
#include <dirent.h>

static char DEV_PATH[] = "/dev/";	/* #define would step on sizeof bug */
static char MESSAGE[] = " / ";	/* ditto */
#define UNKNOWN_DEV	"/dev/unknown"
#define ROOT		"/"
int rflag;

int main(int argc, char **argv);
void done(char *name, int status);

int main(argc, argv)
int argc;
char **argv;
{
  DIR *dp;
  struct dirent *entry;
  struct stat filestat, rootstat;
  static char namebuf[sizeof DEV_PATH + NAME_MAX];

  rflag = (argc > 1 && strcmp(argv[1], "-r") == 0);

  if (stat(ROOT, &rootstat) == 0 && (dp = opendir(DEV_PATH)) != (DIR *) NULL) {
	while ((entry = readdir(dp)) != (struct dirent *) NULL) {
		strcpy(namebuf, DEV_PATH);
		strcat(namebuf, entry->d_name);
		if (stat(namebuf, &filestat) != 0) continue;
		if ((filestat.st_mode & S_IFMT) != S_IFBLK) continue;
		if (filestat.st_rdev != rootstat.st_dev) continue;
		done(namebuf, 0);
	}
  }
  done(UNKNOWN_DEV, 1);
  return(0);			/* not reached */
}

void done(name, status)
char *name;
int status;
{
  int v;

  write(1, name, strlen(name));
  if (rflag) {
	write(1, "\n", 1);
	exit(status);
  }
  write(1, MESSAGE, sizeof MESSAGE - 1);
  v = fsversion(name, "printroot");	/* determine file system version */
  switch (v) {
	case FSVERSION_MFS1: write(1, "1 rw\n", 5);	break;
	case FSVERSION_MFS2: write(1, "2 rw\n", 5);	break;
	case FSVERSION_MFS3: write(1, "3 rw\n", 5);	break;
	case FSVERSION_EXT2: write(1, "ext2 rw\n", 8);	break;
	default: write(1, "0 rw\n", 5);			break;
  }
  exit(status);
}
