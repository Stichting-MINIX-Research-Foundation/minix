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
#include <minix/swap.h>
#include <sys/svrctl.h>
#include <stdio.h>
#include "../../servers/mfs/const.h"

#define MINIX_FS_TYPE "mfs"

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void list, (void));
_PROTOTYPE(void usage, (void));
_PROTOTYPE(void tell, (char *this));
_PROTOTYPE(void swapon, (char *file));

static u8_t MAGIC[] = { SWAP_MAGIC0, SWAP_MAGIC1, SWAP_MAGIC2, SWAP_MAGIC3 };

int main(argc, argv)
int argc;
char *argv[];
{
  int i, swap, n, v, mountflags;
  char **ap, *vs, *opt, *err, *type, *args;
  char special[PATH_MAX+1], mounted_on[PATH_MAX+1], version[10], rw_flag[10];

  if (argc == 1) list();	/* just list /etc/mtab */
  mountflags = 0;
  swap = 0;
  type = NULL;
  args = NULL;
  ap = argv+1;
  for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
		opt = argv[i]+1;
		while (*opt != 0) switch (*opt++) {
		case 'r':	mountflags |= MS_RDONLY;	break;
		case 's':	swap = 1;			break;
		case 't':	if (++i == argc) usage();
				type = argv[i];
				break;
		case 'i':	mountflags |= MS_REUSE;		break;                
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

  if (mountflags & MS_RDONLY && swap) usage();

  if (swap) {
	if (argc != 2) usage();
	swapon(argv[1]);
	tell(argv[1]);
	tell(" is swapspace\n");
  } else {
	if (argc != 3) usage();
	if (mount(argv[1], argv[2], mountflags, type, args) < 0) {
		err = strerror(errno);
		std_err("mount: Can't mount ");
		std_err(argv[1]);
		std_err(" on ");
		std_err(argv[2]);
		std_err(": ");
		std_err(err);
		std_err("\n");
		exit(1);
	}
	/* The mount has completed successfully. Tell the user. */
	tell(argv[1]);
	tell(" is read-");
	tell(mountflags & MS_RDONLY ? "only" : "write");
	tell(" mounted on ");
	tell(argv[2]);
	tell("\n");
  }

  /* Update /etc/mtab. */
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
  if (swap) {
	vs = "swap";
  } else {
  	/* For MFS, use a version number. Otherwise, use the FS type name. */
	if (type == NULL || !strcmp(type, MINIX_FS_TYPE)) {
		v = fsversion(argv[1], "mount");
		if (v == 1)
			vs = "1";
		else if (v == 2)
			vs = "2";
		else if (v == 3)
			vs = "3";
		else
			vs = "0";
	} else {
		/* Keep the version field sufficiently short. */
		if (strlen(type) < sizeof(version))
			vs = type;
		else
			vs = "-";
	}
  }
  n = put_mtab_entry(argv[1], swap ? "swap" : argv[2], vs,
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
	write(1, special, strlen(special));
	if (strcmp(version, "swap") == 0) {
		tell(" is swapspace\n");
	} else {
		tell(" is read-");
		tell(strcmp(rw_flag, "rw") == 0 ? "write" : "only");
		tell(" mounted on ");
		tell(mounted_on);
		tell("\n");
	}
  }
  exit(0);
}


void usage()
{
  std_err("Usage: mount [-r] [-t type] [-o options] special name\n"
  	  "       mount -s special\n");
  exit(1);
}


void tell(this)
char *this;
{
  write(1, this, strlen(this));
}

void swapon(file)
char *file;
{
  u32_t super[2][_MAX_BLOCK_SIZE / 2 / sizeof(u32_t)];
  swap_hdr_t *sp;
  struct mmswapon mmswapon;
  int fd, r;
  char *err;
  
  if ((fd = open(file, O_RDWR)) < 0
	|| lseek(fd, SUPER_BLOCK_BYTES, SEEK_SET) == -1
	|| (r = read(fd, super, _STATIC_BLOCK_SIZE)) < 0
  ) {
	err = strerror(errno);
	std_err("mount: ");
	std_err(file);
	std_err(": ");
	std_err(err);
	std_err("\n");
	exit(1);
  }
  sp = (swap_hdr_t *) &super[0];
  if (memcmp(sp->sh_magic, MAGIC, sizeof(MAGIC)) != 0)
	sp = (swap_hdr_t *) &super[1];
  if (r == _STATIC_BLOCK_SIZE && memcmp(sp->sh_magic, MAGIC, sizeof(MAGIC)) != 0
			|| sp->sh_version > SH_VERSION) {
	std_err("mount: ");
	std_err(file);
	std_err(" is not swapspace\n");
	exit(1);
  }
  close(fd);
  mmswapon.offset = sp->sh_offset;
  mmswapon.size = sp->sh_swapsize;
  strncpy(mmswapon.file, file, sizeof(mmswapon.file));
  mmswapon.file[sizeof(mmswapon.file)-1] = 0;
  if (svrctl(MMSWAPON, &mmswapon) < 0) {
	err = strerror(errno);
	std_err("mount: ");
	std_err(file);
	std_err(": ");
	std_err(err);
	std_err("\n");
	exit(1);
  }
}
