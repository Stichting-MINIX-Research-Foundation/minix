/* df - disk free block printout	Author: Andy Tanenbaum
 *
 * 91/04/30 Kees J. Bot (kjb@cs.vu.nl)
 *	Map filename arguments to the devices they live on.
 *	Changed output to show percentages.
 *
 * 92/12/12 Kees J. Bot
 *	Posixized.  (Almost, the normal output is in kilobytes, it should
 *	be 512-byte units.  'df -P' and 'df -kP' are as it should be.)
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <minix/minlib.h>

struct mtab {	/* List of mounted devices from /etc/mtab. */
	struct mtab	*next;
	dev_t		device;
	char		*devname;
	char		*mountpoint;
} *mtab= NULL;

struct mtab *searchtab(char *name);
static void readmtab(const char *type);
int df(const struct mtab *mt);

int iflag= 0;	/* Focus on inodes instead of blocks. */
int Pflag= 0;	/* Posix standard output. */
int kflag= 0;	/* Output in kilobytes instead of 512 byte units for -P. */
int istty;	/* isatty(1) */

void usage(void)
{
	fprintf(stderr, "Usage: df [-ikP] [-t type] [device]...\n");
	exit(1);
}

int unitsize;

int main(int argc, char *argv[])
{
  int i;
  struct mtab *mt;
  char *type= "dev";
  int ex= 0;

  while (argc > 1 && argv[1][0] == '-') {
  	char *opt= argv[1]+1;

  	while (*opt != 0) {
  		switch (*opt++) {
  		case 'i':	iflag= 1;	break;
  		case 'k':	kflag= 1;	break;
  		case 'P':	Pflag= 1;	break;
  		case 't':
			if (argc < 3) usage();
			type= argv[2];
			argv++;
			argc--;
			break;
		default:
			usage();
		}
	}
	argc--;
	argv++;
  }

  istty= isatty(1);

  readmtab(type);
 
  if(!Pflag || (Pflag && kflag)) unitsize = 1024;
  else unitsize = 512;

  if (Pflag) {
	if (!iflag)
		printf("\
Filesystem    %4d-blocks      Used    Available  Capacity  Mounted on\n",
			unitsize);
	else
		printf("\
Filesystem         Inodes       IUsed      IFree    %%IUsed    Mounted on\n"
		);
  } else {
	printf("%s\n", !iflag ? "\
Filesystem      Size (kB)       Free       Used    % Files%   Mounted on" : "\
Filesystem          Files       Free       Used    % BUsed%   Mounted on"
	);
  }

  if (argc == 1) {
	for (mt= mtab; mt != NULL; mt= mt->next) ex |= df(mt);
  } else {
	for (i = 1; i < argc; i++) ex |= df(searchtab(argv[i]));
  }
  exit(ex);
}

static void readmtab(const char *type)
/* Turn the mounted file table into a list. */
{
  struct mtab **amt= &mtab, *new;
  struct stat st;
  char devname[PATH_MAX], mountpoint[PATH_MAX], version[MNTNAMELEN],
	rw_flag[MNTFLAGLEN];

  if (load_mtab("df") < 0) exit(1);

  while (get_mtab_entry(devname, mountpoint, version, rw_flag),
							  devname[0] != 0) {
	if (strcmp(type, "dev") != 0 && strcmp(type, version) != 0) continue;

	/* Make new list cell. */
	if ((new= (struct mtab *) malloc(sizeof(*new))) == NULL
	  || (new->devname= (char *) malloc(strlen(devname) + 1)) == NULL
	  || (new->mountpoint= (char *) malloc(strlen(mountpoint) + 1)) == NULL
	) break;

	if (strcmp(devname, "none") != 0 && stat(devname, &st) == 0 &&
	  S_ISBLK(st.st_mode)) {
		new->device= st.st_rdev;
	} else if (stat(mountpoint, &st) == 0) {
		new->device= st.st_dev;
	}
	strcpy(new->devname, devname);
	strcpy(new->mountpoint, mountpoint);

	*amt= new;		/* Add the cell to the end. */
	amt= &new->next;
	*amt= NULL;
  }
}

struct mtab *searchtab(char *name)
/* See what we can do with a user supplied name, there are three possibilities:
 * 1. It's a device and it is in the mtab: Return mtab entry.
 * 2. It's a file and lives on a device in the mtab: Return mtab entry.
 * 3. It's anything else: Return something df() will choke on.
 */
{
  static struct mtab unknown;
  struct mtab *mt;
  struct stat st;

  unknown.devname= name;
  unknown.mountpoint= "";

  if (stat(name, &st) < 0) return &unknown;	/* Case 3. */

  unknown.device= S_ISBLK(st.st_mode) ? st.st_rdev : st.st_dev;

  for (mt= mtab; mt != NULL; mt= mt->next) {
	if (unknown.device == mt->device)
		return mt;			/* Case 1 & 2. */
  }

  return &unknown;				/* Case 3. */
}

/* (num / tot) in percentages rounded up. */
#define percent(num, tot) \
 ((tot > 0) ? ((int) ((100ULL * (num) + ((tot) - 1)) / (tot))) : 0)

int df(const struct mtab *mt)
{
  long totblocks, busyblocks, totinodes, busyinodes;
  struct statvfs sv;
  int n;

  if (statvfs(mt->mountpoint, &sv) < 0) {
	fprintf(stderr, "df: %s: %s\n", mt->devname, strerror(errno));
	return(1);
  }

  /* Print results. */
  printf("%s", mt->devname);
  n= strlen(mt->devname);
  if (n > 15 && istty) { putchar('\n'); n= 0; }
  while (n < 15) { putchar(' '); n++; }

  totblocks = sv.f_blocks;
  busyblocks = sv.f_blocks - sv.f_bfree;

  busyblocks = busyblocks * (sv.f_bsize/512) / (unitsize/512);
  totblocks = totblocks * (sv.f_bsize/512) / (unitsize/512);

  totinodes = sv.f_files;
  busyinodes = sv.f_files - sv.f_ffree;

  if (!Pflag && !iflag) {
	printf(" %9ld  %9ld  %9ld %3d%%   %3d%%   %s\n",
		totblocks,				/* Blocks */
		totblocks - busyblocks,			/* free */
		busyblocks,				/* used */
		percent(busyblocks, totblocks),		/* % */
		percent(busyinodes, totinodes),		/* FUsed% */
		mt->mountpoint				/* Mounted on */
	);
  }
  if (!Pflag && iflag) {
	printf(" %9ld  %9ld  %9ld %3d%%   %3d%%   %s\n",
		totinodes,				/* Files */
		totinodes - busyinodes,			/* free */
		busyinodes,				/* used */
		percent(busyinodes, totinodes),		/* % */
		percent(busyblocks, totblocks),		/* BUsed% */
		mt->mountpoint				/* Mounted on */
	);
  }
  if (Pflag && !iflag) {
	printf(" %9ld   %9ld  %9ld     %4d%%    %s\n",
		totblocks,				/* Blocks */
		busyblocks,				/* Used */
		totblocks - busyblocks,			/* Available */
		percent(busyblocks, totblocks),		/* Capacity */
		mt->mountpoint				/* Mounted on */
	);
  }
  if (Pflag && iflag) {
	printf(" %9ld   %9ld  %9ld     %4d%%    %s\n",
		totinodes,				/* Inodes */
		busyinodes,				/* IUsed */
		totinodes - busyinodes,			/* IAvail */
		percent(busyinodes, totinodes),		/* Capacity */
		mt->mountpoint				/* Mounted on */
	);
  }
  return(0);
}
