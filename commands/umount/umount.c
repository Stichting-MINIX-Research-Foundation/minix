/* umount - unmount a file system		Author: Andy Tanenbaum */

#define _MINIX 1		/* for proto of the non-POSIX umount() */
#define _POSIX_SOURCE 1		/* for PATH_MAX from limits.h */

#include <minix/type.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <minix/minlib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <stdio.h>

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(int find_mtab_entry, (char *name));
_PROTOTYPE(void update_mtab, (void));
_PROTOTYPE(void usage, (void));

static char device[PATH_MAX+1], mountpoint[PATH_MAX+1], vs[10], rw[10];

int main(argc, argv)
int argc;
char *argv[];
{
  int found;

  if (argc != 2) usage();
  found = find_mtab_entry(argv[1]);
  if (umount(argv[1]) < 0) {
	if (errno == EINVAL)
		std_err("umount: Device not mounted\n");
	else if (errno == ENOTBLK)
		std_err("umount: Not a mountpoint\n");
	else
		perror("umount");
	exit(1);
  }
  if (found) {
	printf("%s unmounted from %s\n", device, mountpoint);
	update_mtab();
  }
  else printf("%s unmounted (mtab not updated)\n", argv[1]);
  return(0);
}

int find_mtab_entry(name)
char *name;
{
/* Find a matching mtab entry for 'name' which may be a special or a path,
 * and generate a new mtab file without this entry on the fly. Do not write
 * out the result yet. Return whether we found a matching entry.
 */
  char special[PATH_MAX+1], mounted_on[PATH_MAX+1], version[10], rw_flag[10];
  struct stat nstat, mstat;
  int n, found;

  if (load_mtab("umount") < 0) return 0;

  if (stat(name, &nstat) != 0) return 0;

  found = 0;
  while (1) {
	n = get_mtab_entry(special, mounted_on, version, rw_flag);
	if (n < 0) break;
	if (strcmp(name, special) == 0 || (stat(mounted_on, &mstat) == 0 &&
		mstat.st_dev == nstat.st_dev && mstat.st_ino == nstat.st_ino))
	{
		/* If we found an earlier match, keep that one. Mountpoints
		 * may be stacked on top of each other, and unmounting should
		 * take place in the reverse order of mounting.
		 */
		if (found) {
			(void) put_mtab_entry(device, mountpoint, vs, rw);
		}

		strcpy(device, special);
		strcpy(mountpoint, mounted_on);
		strcpy(vs, version);
		strcpy(rw, rw_flag);
		found = 1;
		continue;
	}
	(void) put_mtab_entry(special, mounted_on, version, rw_flag);
  }

  return found;
}

void update_mtab()
{
/* Write out the new mtab file. */
  int n;

  n = rewrite_mtab("umount");
  if (n < 0) {
	std_err("/etc/mtab not updated.\n");
	exit(1);
  }
}

void usage()
{
  std_err("Usage: umount name\n");
  exit(1);
}
