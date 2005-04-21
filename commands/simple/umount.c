/* umount - unmount a file system		Author: Andy Tanenbaum */

#define _MINIX 1		/* for proto of the non-POSIX umount() */
#define _POSIX_SOURCE 1		/* for PATH_MAX from limits.h */

#include <sys/types.h>
#include <sys/svrctl.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <minix/minlib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void update_mtab, (char *devname));
_PROTOTYPE(void usage, (void));
_PROTOTYPE(void tell, (char *this));

static char mountpoint[PATH_MAX+1];

int main(argc, argv)
int argc;
char *argv[];
{
  int sflag = 0;

  while (argc > 1 && argv[1][0] == '-') {
	char *opt = argv[1]+1;
	while (*opt) if (*opt++ == 's') sflag = 1; else usage();
	argc--;
	argv++;
  }
  if (argc != 2) usage();
  if ((sflag ? svrctl(MMSWAPOFF, NULL) : umount(argv[1])) < 0) {
	if (errno == EINVAL)
		std_err("Device not mounted\n");
	else
		perror("umount");
	exit(1);
  }
  update_mtab(argv[1]);
  tell(argv[1]);
  tell(" unmounted");
  if (*mountpoint != '\0') {
	tell(" from ");
	tell(mountpoint);
  }
  tell("\n");
  return(0);
}

void update_mtab(devname)
char *devname;
{
/* Remove an entry from /etc/mtab. */
  int n;
  char special[PATH_MAX+1], mounted_on[PATH_MAX+1], version[10], rw_flag[10];

  if (load_mtab("umount") < 0) {
	std_err("/etc/mtab not updated.\n");
	exit(1);
  }
  while (1) {
	n = get_mtab_entry(special, mounted_on, version, rw_flag);
	if (n < 0) break;
	if (strcmp(devname, special) == 0) {
		strcpy(mountpoint, mounted_on);
		continue;
	}
	(void) put_mtab_entry(special, mounted_on, version, rw_flag);
  }
  n = rewrite_mtab("umount");
  if (n < 0) {
	std_err("/etc/mtab not updated.\n");
	exit(1);
  }
}

void usage()
{
  std_err("Usage: umount [-s] special\n");
  exit(1);
}

void tell(this)
char *this;
{
  write(1, this, strlen(this));
}
