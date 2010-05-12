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
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#if __minix_vmd
#include <sys/mnttab.h>
#else
#include <minix/minlib.h>
#endif

#include <minix/config.h>
#include <minix/const.h>
#include <minix/type.h>
#include <servers/mfs/const.h>
#include <servers/mfs/type.h>
#include <servers/mfs/super.h>
#undef printf

#if !__minix_vmd
/* Map Minix-vmd names to Minix names. */
#define v12_super_block		super_block
#define SUPER_V1		SUPER_MAGIC

#endif

#define ISDISK(mode)	S_ISBLK(mode)	/* || S_ISCHR for raw device??? */

extern int errno;
char MTAB[] = "/etc/mtab";

struct mtab {	/* List of mounted devices from /etc/mtab. */
	struct mtab	*next;
	dev_t		device;
	char		*devname;
	char		*mountpoint;
} *mtab= NULL;

struct mtab *searchtab(char *name);
static void readmtab(const char *type);
int df(const struct mtab *mt);
bit_t bit_count(unsigned blocks, bit_t bits, int fd, int bs);

int iflag= 0;	/* Focus on inodes instead of blocks. */
int Pflag= 0;	/* Posix standard output. */
int kflag= 0;	/* Output in kilobytes instead of 512 byte units for -P. */
int istty;	/* isatty(1) */
uid_t ruid, euid;	/* To sometimes change identities. */
gid_t rgid, egid;

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
  ruid= getuid(); euid= geteuid();
  rgid= getgid(); egid= getegid();

  readmtab(type);
 
  if(!Pflag || (Pflag && kflag)) unitsize = 1024;
  else unitsize = 512;

  if (Pflag) {
	printf(!iflag ? "\
Filesystem    %4d-blocks      Used    Available  Capacity  Mounted on\n" : "\
Filesystem         Inodes       IUsed      IFree    %%IUsed    Mounted on\n",
		unitsize);
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

#if __minix_vmd
  char *devname, *mountpoint;
  FILE *mtf;
  struct mnttab mte, look;

  if ((mtf= fopen(MTAB, "r")) == NULL) {
	fprintf(stderr, "df: can't open %s\n", MTAB);
	return;
  }

  look.mnt_special= NULL;
  look.mnt_mountp= NULL;
  look.mnt_fstype= type;
  look.mnt_mntopts= NULL;

  while (getmntany(mtf, &mte, &look) >= 0) {
  	devname= mte.mnt_special;
  	mountpoint= mte.mnt_mountp;

		/* Skip bad entries, can't complain about everything. */
	if (stat(devname, &st) < 0 || !ISDISK(st.st_mode)) continue;

		/* Make new list cell. */
	if ((new= (struct mtab *) malloc(sizeof(*new))) == NULL
	  || (new->devname= (char *) malloc(strlen(devname) + 1)) == NULL
	  || (new->mountpoint= (char *) malloc(strlen(mountpoint) + 1)) == NULL
	) break;

	new->device= st.st_rdev;
	strcpy(new->devname, devname);
	strcpy(new->mountpoint, mountpoint);

	*amt= new;		/* Add the cell to the end. */
	amt= &new->next;
	*amt= NULL;
  }
  fclose(mtf);

#else /* __minix */
  char devname[128], mountpoint[128], version[10], rw_flag[10];

  if (load_mtab("df") < 0) exit(1);

  while (get_mtab_entry(devname, mountpoint, version, rw_flag),
							  devname[0] != 0) {
	if (strcmp(type, "dev") == 0) {
		if (strcmp(version, "1") != 0 && strcmp(version, "2") != 0 &&
		 	strcmp(version, "3"))
			continue;
	} else {
		if (strcmp(type, version) != 0) continue;
	}

		/* Skip bad entries, can't complain about everything. */
	if (stat(devname, &st) < 0 || !ISDISK(st.st_mode)) continue;

		/* Make new list cell. */
	if ((new= (struct mtab *) malloc(sizeof(*new))) == NULL
	  || (new->devname= (char *) malloc(strlen(devname) + 1)) == NULL
	  || (new->mountpoint= (char *) malloc(strlen(mountpoint) + 1)) == NULL
	) break;

	new->device= st.st_rdev;
	strcpy(new->devname, devname);
	strcpy(new->mountpoint, mountpoint);

	*amt= new;		/* Add the cell to the end. */
	amt= &new->next;
	*amt= NULL;
  }
#endif
}

struct mtab *searchtab(char *name)
/* See what we can do with a user supplied name, there are five possibilities:
 * 1. It's a device and it is in the mtab: Return mtab entry.
 * 2. It's a device and it is not in the mtab: Return device mounted on "".
 * 3. It's a file and lives on a device in the mtab: Return mtab entry.
 * 4. It's a file and it's not on an mtab device: Search /dev for the device
 *    and return this device as mounted on "???".
 * 5. It's junk: Return something df() will choke on.
 */
{
  static struct mtab unknown;
  static char devname[5 + NAME_MAX + 1]= "/dev/";
  struct mtab *mt;
  struct stat st;
  DIR *dp;
  struct dirent *ent;

  unknown.devname= name;
  unknown.mountpoint= "";

  if (stat(name, &st) < 0) return &unknown;	/* Case 5. */

  unknown.device= ISDISK(st.st_mode) ? st.st_rdev : st.st_dev;

  for (mt= mtab; mt != NULL; mt= mt->next) {
	if (unknown.device == mt->device)
		return mt;			/* Case 1 & 3. */
  }

  if (ISDISK(st.st_mode)) {
	return &unknown;			/* Case 2. */
  }

  if ((dp= opendir("/dev")) == NULL) return &unknown;	/* Disaster. */

  while ((ent= readdir(dp)) != NULL) {
	if (ent->d_name[0] == '.') continue;
	strcpy(devname + 5, ent->d_name);
	if (stat(devname, &st) >= 0 && ISDISK(st.st_mode)
		&& unknown.device == st.st_rdev
	) {
		unknown.devname= devname;
		unknown.mountpoint= "???";
		break;
	}
  }
  closedir(dp);
  return &unknown;				/* Case 4. */
}

/* (num / tot) in percentages rounded up. */
#define percent(num, tot)  ((int) ((100L * (num) + ((tot) - 1)) / (tot)))

/* One must be careful printing all these _t types. */
#define L(n)	((long) (n))

int df(const struct mtab *mt)
{
  int fd;
  bit_t i_count, z_count;
  block_t totblocks, busyblocks, offset;
  int n, block_size;
  struct v12_super_block super, *sp;

  /* Don't allow Joe User to df just any device. */
  seteuid(*mt->mountpoint == 0 ? ruid : euid);
  setegid(*mt->mountpoint == 0 ? rgid : egid);

  if ((fd = open(mt->devname, O_RDONLY)) < 0) {
	fprintf(stderr, "df: %s: %s\n", mt->devname, strerror(errno));
	return(1);
  }
  lseek(fd, (off_t) SUPER_BLOCK_BYTES, SEEK_SET);	/* skip boot block */

  if (read(fd, (char *) &super, sizeof(super)) != (int) sizeof(super)) {
	fprintf(stderr, "df: Can't read super block of %s\n", mt->devname);
	close(fd);
	return(1);
  }

  sp = &super;
  if (sp->s_magic != SUPER_V1 && sp->s_magic != SUPER_V2
      && sp->s_magic != SUPER_V3) {
	fprintf(stderr, "df: %s: Not a valid file system\n", mt->devname);
	close(fd);
	return(1);
  }

  if(sp->s_magic != SUPER_V3) block_size = _STATIC_BLOCK_SIZE;
  else block_size = sp->s_block_size;

  if(block_size < _MIN_BLOCK_SIZE) {
	fprintf(stderr, "df: %s: funny block size (%d)\n",
		mt->devname, block_size);
	close(fd);
	return(1);
  }

  if (sp->s_magic == SUPER_V1) {
	sp->s_zones = sp->s_nzones;
	sp->s_inodes_per_block = V1_INODES_PER_BLOCK;
  } else {
	sp->s_inodes_per_block = V2_INODES_PER_BLOCK(block_size);
  }

  /* If the s_firstdatazone_old field is zero, we have to compute the value. */
  if (sp->s_firstdatazone_old == 0) {
	offset = START_BLOCK + sp->s_imap_blocks + sp->s_zmap_blocks;
	offset += (sp->s_ninodes + sp->s_inodes_per_block - 1) /
		sp->s_inodes_per_block;

	sp->s_firstdatazone = (offset + (1 << sp->s_log_zone_size) - 1) >>
		sp->s_log_zone_size;
  } else {
	sp->s_firstdatazone = sp->s_firstdatazone_old;
  }

  lseek(fd, (off_t) block_size * 2L, SEEK_SET);	/* skip rest of super block */

  i_count = bit_count(sp->s_imap_blocks, (bit_t) (sp->s_ninodes+1),
  	fd, block_size);

  if (i_count == -1) {
	fprintf(stderr, "df: Can't find bit maps of %s\n", mt->devname);
	close(fd);
	return(1);
  }
  i_count--;	/* There is no inode 0. */

  /* The first bit in the zone map corresponds with zone s_firstdatazone - 1
   * This means that there are s_zones - (s_firstdatazone - 1) bits in the map
   */
  z_count = bit_count(sp->s_zmap_blocks,
	(bit_t) (sp->s_zones - (sp->s_firstdatazone - 1)), fd, block_size);

  if (z_count == -1) {
	fprintf(stderr, "df: Can't find bit maps of %s\n", mt->devname);
	close(fd);
	return(1);
  }
  /* Don't forget those zones before sp->s_firstdatazone - 1 */
  z_count += sp->s_firstdatazone - 1;

#ifdef __minix_vmd
  totblocks = sp->s_zones;
  busyblocks = z_count;
#else
  totblocks = (block_t) sp->s_zones << sp->s_log_zone_size;
  busyblocks = (block_t) z_count << sp->s_log_zone_size;
#endif

  busyblocks = busyblocks * (block_size/512) / (unitsize/512);
  totblocks = totblocks * (block_size/512) / (unitsize/512);

  /* Print results. */
  printf("%s", mt->devname);
  n= strlen(mt->devname);
  if (n > 15 && istty) { putchar('\n'); n= 0; }
  while (n < 15) { putchar(' '); n++; }

  if (!Pflag && !iflag) {
	printf(" %9ld  %9ld  %9ld %3d%%   %3d%%   %s\n",
		L(totblocks),				/* Blocks */
		L(totblocks - busyblocks),		/* free */
		L(busyblocks),				/* used */
		percent(busyblocks, totblocks),		/* % */
		percent(i_count, sp->s_ninodes),	/* FUsed% */
		mt->mountpoint				/* Mounted on */
	);
  }
  if (!Pflag && iflag) {
	printf(" %9ld  %9ld  %9ld %3d%%   %3d%%   %s\n",
		L(sp->s_ninodes),			/* Files */
		L(sp->s_ninodes - i_count),		/* free */
		L(i_count),				/* used */
		percent(i_count, sp->s_ninodes),	/* % */
		percent(busyblocks, totblocks),		/* BUsed% */
		mt->mountpoint				/* Mounted on */
	);
  }
  if (Pflag && !iflag) {
	printf(" %9ld   %9ld  %9ld     %4d%%    %s\n",
		L(totblocks),				/* Blocks */
		L(busyblocks),				/* Used */
		totblocks - busyblocks,			/* Available */
		percent(busyblocks, totblocks),		/* Capacity */
		mt->mountpoint				/* Mounted on */
	);
  }
  if (Pflag && iflag) {
	printf(" %9ld   %9ld  %9ld     %4d%%    %s\n",
		L(sp->s_ninodes),			/* Inodes */
		L(i_count),				/* IUsed */
		L(sp->s_ninodes - i_count),		/* IAvail */
		percent(i_count, sp->s_ninodes),	/* Capacity */
		mt->mountpoint				/* Mounted on */
	);
  }
  close(fd);
  return(0);
}

bit_t bit_count(unsigned blocks, bit_t bits, int fd, int block_size)
{
  char *wptr;
  int i, b;
  bit_t busy;
  char *wlim;
  static char *buf = NULL;
  static char bits_in_char[1 << CHAR_BIT];
  static int bufsize = 0;

  if(bufsize < block_size) {
	if(buf) free(buf);
	if(!(buf = malloc(block_size))) {
		fprintf(stderr, "df: malloc failed\n");
		exit(1);
	}
	bufsize = block_size;
  }

  /* Precalculate bitcount for each char. */
  if (bits_in_char[1] != 1) {
	for (b = (1 << 0); b < (1 << CHAR_BIT); b <<= 1)
		for (i = 0; i < (1 << CHAR_BIT); i++)
			if (i & b) bits_in_char[i]++;
  }

  /* Loop on blocks, reading one at a time and counting bits. */
  busy = 0;
  for (i = 0; i < blocks && bits != 0; i++) {
	if (read(fd, buf, block_size) != block_size) return(-1);

	wptr = &buf[0];
	if (bits >= CHAR_BIT * block_size) {
		wlim = &buf[block_size];
		bits -= CHAR_BIT * block_size;
	} else {
		b = bits / CHAR_BIT;	/* whole chars in map */
		wlim = &buf[b];
		bits -= b * CHAR_BIT;	/* bits in last char, if any */
		b = *wlim & ((1 << bits) - 1);	/* bit pattern from last ch */
		busy += bits_in_char[b];
		bits = 0;
	}

	/* Loop on the chars of a block. */
	while (wptr != wlim)
		busy += bits_in_char[*wptr++ & ((1 << CHAR_BIT) - 1)];
  }
  return(busy);
}

/*
 * $PchId: df.c,v 1.7 1998/07/27 18:42:17 philip Exp $
 */
