/* umount - unmount a file system		Author: Andy Tanenbaum */

#define _MINIX 1		/* for proto of the non-POSIX umount() */
#define _POSIX_SOURCE 1		/* for PATH_MAX from limits.h */

#include <minix/type.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>
#include <minix/minlib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <stdio.h>

int main(int argc, char **argv);
int find_mtab_entry(char *name);
void update_mtab(void);
void usage(void);

static char device[PATH_MAX], mount_point[PATH_MAX], type[MNTNAMELEN],
		flags[MNTFLAGLEN];

int main(argc, argv)
int argc;
char *argv[];
{
  int found;
  int umount_flags = 0UL;
  int c;
  char *name;

  while ((c = getopt (argc, argv, "e")) != -1)
  {
	switch (c) {
		case 'e': umount_flags |= MS_EXISTING; break;
		default: break;
	}
  }
  
  if (argc - optind != 1) {
	   usage();
  }

  name = argv[optind];
 
  found = find_mtab_entry(name);

  if (umount2(name, umount_flags) < 0) {
	if (errno == EINVAL)
		std_err("umount: Device not mounted\n");
	else if (errno == ENOTBLK)
		std_err("umount: Not a mountpoint\n");
	else
		perror("umount");
	exit(1);
  }
  if (found) {
	printf("%s unmounted from %s\n", device, mount_point);
  }
  return(0);
}

int find_mtab_entry(name)
char *name;
{
/* Find a matching mtab entry for 'name' which may be a special or a path,
 * and generate a new mtab file without this entry on the fly. Do not write
 * out the result yet. Return whether we found a matching entry.
 */
  char e_dev[PATH_MAX], e_mount_point[PATH_MAX], e_type[MNTNAMELEN],
	e_flags[MNTFLAGLEN];
  struct stat nstat, mstat;
  int n, found;

  if (load_mtab("umount") < 0) return 0;

  if (stat(name, &nstat) != 0) return 0;

  found = 0;
  while (1) {
	n = get_mtab_entry(e_dev, e_mount_point, e_type, e_flags);
	if (n < 0) break;
	if (strcmp(name, e_dev) == 0 || (stat(e_mount_point, &mstat) == 0 &&
		mstat.st_dev == nstat.st_dev && mstat.st_ino == nstat.st_ino))
	{
		strlcpy(device, e_dev, PATH_MAX);
		strlcpy(mount_point, e_mount_point, PATH_MAX);
		strlcpy(type, e_type, MNTNAMELEN);
		strlcpy(flags, e_flags, MNTFLAGLEN);
		found = 1;
		break;
	}
  }

  return found;
}

void usage()
{
  std_err("Usage: umount [-e] name\n");
  exit(1);
}
