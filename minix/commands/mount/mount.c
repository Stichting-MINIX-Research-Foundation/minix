/* mount - mount a file system		Author: Andy Tanenbaum */

#include <errno.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <unistd.h>
#include <minix/minlib.h>
#include <stdio.h>
#include <fstab.h>

#define MINIX_FS_TYPE "mfs"

int main(int argc, char **argv);
void list(void);
void usage(void);
void update_mtab(char *dev, char *mountpoint, char *fstype, int mountflags);
int mount_all(void);

static int write_mtab = 1;

int main(argc, argv)
int argc;
char *argv[];
{
  int all = 0, i, v = 0, mountflags, srvflags;
  char **ap, *opt, *err, *type, *args, *device;

  if (argc == 1) list();	/* just list /etc/mtab */
  mountflags = 0;
  srvflags = 0;
  type = NULL;
  args = NULL;
  ap = argv+1;
  for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
		opt = argv[i]+1;
		while (*opt != 0) switch (*opt++) {
		case 'r':	mountflags |= MNT_RDONLY;	break;
		case 't':	if (++i == argc) usage();
				type = argv[i];
				break;
		case 'i':	srvflags |= MS_REUSE;		break;
		case 'e':	srvflags |= MS_EXISTING;		break;
		case 'n':	write_mtab = 0;			break;
		case 'o':	if (++i == argc) usage();
				args = argv[i];
				break;
		case 'a':	all = 1; break;
		default:	usage();
		}
	} else {
		*ap++ = argv[i];
	}
  }
  *ap = NULL;
  argc = (ap - argv);

  if (!all && (argc != 3 || *argv[1] == 0)) usage();
  if (all == 1) {
	return mount_all();
  }

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
  
  if (minix_mount(device, argv[2], mountflags, srvflags, type, args) < 0) {
	err = strerror(errno);
	fprintf(stderr, "mount: Can't mount %s on %s: %s\n",
		argv[1], argv[2], err);
	return(EXIT_FAILURE);
  }

  printf("%s is mounted on %s\n", argv[1], argv[2]);
  return(EXIT_SUCCESS);
}

void list()
{
  int n;
  char dev[PATH_MAX], mountpoint[PATH_MAX], type[MNTNAMELEN], flags[MNTFLAGLEN];

  /* Read and print /etc/mtab. */
  n = load_mtab("mount");
  if (n < 0) exit(1);

  while (1) {
	n = get_mtab_entry(dev, mountpoint, type, flags);
	if  (n < 0) break;
	printf("%s on %s type %s (%s)\n", dev, mountpoint, type, flags);
  }
  exit(0);
}

int
has_opt(char *mntopts, char *option)
{
	char *optbuf, *opt;
	int found = 0;

	optbuf = strdup(mntopts);
	for (opt = optbuf; (opt = strtok(opt, ",")) != NULL; opt = NULL) {
		if (!strcmp(opt, option)) found = 1;
	}
	free (optbuf);
	return(found);
}


int
mount_all()
{
	struct fstab *fs;
	int ro, mountflags;
	char mountpoint[PATH_MAX];
  	char *device, *err;

	while ((fs = getfsent()) != NULL) {
		ro = 0;
		mountflags = 0;
		device = NULL;
		if (realpath(fs->fs_file, mountpoint) == NULL) {
			fprintf(stderr, "Can't mount on %s\n", fs->fs_file);
			return(EXIT_FAILURE);
		}
		if (has_opt(fs->fs_mntops, "noauto"))
			continue;
		if (!strcmp(mountpoint, "/"))
			continue; /* Not remounting root */
		if (has_opt(fs->fs_mntops, "ro"))
			ro = 1;
		if (ro) {
			mountflags |= MNT_RDONLY;
		}

		device = fs->fs_spec;
		/* passing a null string for block special device means don't 
		 * use a device at all and this is what we need to do for 
		 * entries starting with "none"
		 */
		if (!strcmp(device, "none")) 
			device = NULL;

		if (minix_mount(device, mountpoint, mountflags, 0, fs->fs_vfstype,
		    fs->fs_mntops) != 0) {
			err = strerror(errno);
			fprintf(stderr, "mount: Can't mount %s on %s: %s\n",
				fs->fs_spec, fs->fs_file, err);
			return(EXIT_FAILURE);
		}
	}
	return(EXIT_SUCCESS);
}

void usage()
{
  std_err("Usage: mount [-a] [-r] [-e] [-t type] [-o options] special name\n");
  exit(1);
}
