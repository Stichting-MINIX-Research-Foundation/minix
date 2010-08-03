/* mount - mount a file system		Author: Andy Tanenbaum */

#include <errno.h>
#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/mount.h>
#include <unistd.h>
#include <fcntl.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/minlib.h>
#include <sys/svrctl.h>
#include <stdio.h>
#include "mfs/const.h"

#define MINIX_FS_TYPE "mfs"

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void list, (void));
_PROTOTYPE(void usage, (void));

int main(argc, argv)
int argc;
char *argv[];
{
  int i, n, v = 0, mountflags, write_mtab;
  char **ap, *vs, *opt, *err, *type, *args, *device;
  char special[PATH_MAX+1], mounted_on[PATH_MAX+1], version[10], rw_flag[10];

  if (argc == 1) list();	/* just list /etc/mtab */
  mountflags = 0;
  write_mtab = 1;
  type = NULL;
  args = NULL;
  ap = argv+1;
  for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
		opt = argv[i]+1;
		while (*opt != 0) switch (*opt++) {
		case 'r':	mountflags |= MS_RDONLY;	break;
		case 't':	if (++i == argc) usage();
				type = argv[i];
				break;
		case 'i':	mountflags |= MS_REUSE;		break;
		case 'n':	write_mtab = 0;			break;
		case 'o':	if (++i == argc) usage();
				args = argv[i];
				break;
		default:	usage();
		}
	} else {
		*ap++ = argv[i];
	}
  }
  *ap = NULL;
  argc = (ap - argv);

  if (argc != 3 || *argv[1] == 0) usage();

  device = argv[1];
  if (!strcmp(device, "none")) device = NULL;

  if ((type == NULL || !strcmp(type, MINIX_FS_TYPE)) && device != NULL) {
	/* auto-detect type and/or version */
	v = fsversion(device, "mount");
	switch (v) {
		case FSVERSION_MFS1:
		case FSVERSION_MFS2: 
		case FSVERSION_MFS3: type = MINIX_FS_TYPE; break;		
		case FSVERSION_EXT2: type = "ext2"; break;
	}
  }
  
  if (mount(device, argv[2], mountflags, type, args) < 0) {
	err = strerror(errno);
	fprintf(stderr, "mount: Can't mount %s on %s: %s\n",
		argv[1], argv[2], err);
	exit(1);
  }

  /* The mount has completed successfully. Tell the user. */
  printf("%s is read-%s mounted on %s\n",
	argv[1], mountflags & MS_RDONLY ? "only" : "write", argv[2]);
  
  /* Update /etc/mtab. */
  if (!write_mtab) return 0;
  n = load_mtab("mount");
  if (n < 0) exit(1);		/* something is wrong. */

  /* Loop on all the /etc/mtab entries, copying each one to the output buf. */
  while (1) {
	n = get_mtab_entry(special, mounted_on, version, rw_flag);
	if (n < 0) break;
	n = put_mtab_entry(special, mounted_on, version, rw_flag);
	if (n < 0) {
		std_err("mount: /etc/mtab has grown too large\n");
		exit(1);
	}
  }
  /* For MFS, use a version number. Otherwise, use the FS type name. */
  if (!strcmp(type, MINIX_FS_TYPE)) {
	switch (v) {
		case FSVERSION_MFS1: vs = "1"; break;
		case FSVERSION_MFS2: vs = "2"; break;
		case FSVERSION_MFS3: vs = "3"; break;		
		default: vs = "0"; break;
	}
  } else {
	/* Keep the version field sufficiently short. */
	if (strlen(type) < sizeof(version))
		vs = type;
	else
		vs = "-";
  }
  n = put_mtab_entry(argv[1], argv[2], vs,
		     (mountflags & MS_RDONLY ? "ro" : "rw") );
  if (n < 0) {
	std_err("mount: /etc/mtab has grown too large\n");
	exit(1);
  }

  n = rewrite_mtab("mount");
  return(0);
}


void list()
{
  int n;
  char special[PATH_MAX+1], mounted_on[PATH_MAX+1], version[10], rw_flag[10];

  /* Read and print /etc/mtab. */
  n = load_mtab("mount");
  if (n < 0) exit(1);

  while (1) {
	n = get_mtab_entry(special, mounted_on, version, rw_flag);
	if  (n < 0) break;
	printf("%s is read-%s mounted on %s (type %s)\n",
		special, strcmp(rw_flag, "rw") == 0 ? "write" : "only",
		mounted_on, version);
  }
  exit(0);
}


void usage()
{
  std_err("Usage: mount [-r] [-t type] [-o options] special name\n");
  exit(1);
}
