/*	$NetBSD: fdisk.c,v 1.156 2018/11/14 12:05:29 mlelstv Exp $ */

/*
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
 * 14-Dec-89  Robert Baron (rvb) at Carnegie-Mellon University
 *	Copyright (c) 1989	Robert. V. Baron
 *	Created.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>

#ifndef lint
__RCSID("$NetBSD: fdisk.c,v 1.156 2018/11/14 12:05:29 mlelstv Exp $");
#endif /* not lint */

#define MBRPTYPENAMES
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>
#include <zlib.h>

#if !HAVE_NBTOOL_CONFIG_H
#include <sys/disklabel.h>
#include <sys/disklabel_gpt.h>
#include <sys/bootblock.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <disktab.h>
#include <util.h>
#else
#include <nbinclude/sys/disklabel.h>
#include <nbinclude/sys/disklabel_gpt.h>
#include <nbinclude/sys/bootblock.h>
#include "../../include/disktab.h"
/* We enforce -F, so none of these possibly undefined items can be needed */
#define opendisk(path, fl, buf, buflen, cooked) (-1)
#endif /* HAVE_NBTOOL_CONFIG_H */

#ifndef	DEFAULT_BOOTDIR
#define	DEFAULT_BOOTDIR		"/usr/mdec"
#endif

#define	LE_MBR_MAGIC		htole16(MBR_MAGIC)
#define	LE_MBR_BS_MAGIC		htole16(MBR_BS_MAGIC)

#ifdef BOOTSEL

#define	DEFAULT_BOOTCODE	"mbr"
#define	DEFAULT_BOOTSELCODE	"mbr_bootsel"
#define	DEFAULT_BOOTEXTCODE	"mbr_ext"

/* Scan values for the various keys we use, as returned by the BIOS */
#define	SCAN_ENTER	0x1c
#define	SCAN_F1		0x3b
#define	SCAN_1		0x2


#define	MAX_BIOS_DISKS	16	/* Going beyond F12 is hard though! */

/* We same the dflt 'boot partition' as a disk block, with some magic values. */
#define DEFAULT_ACTIVE	(~(daddr_t)0)
#define	DEFAULT_DISK(n)	(DEFAULT_ACTIVE - MAX_BIOS_DISKS + (n))

#endif

#define GPT_TYPE(offs) ((offs) == GPT_HDR_BLKNO ?  "primary" : "secondary")

#ifndef PRIdaddr
#define PRIdaddr PRId64
#endif

#ifndef _PATH_DEFDISK
#define _PATH_DEFDISK	"/dev/rwd0d"
#endif

struct {
	struct mbr_sector *ptn;		/* array of pbrs */
	daddr_t		base;		/* first sector of ext. ptn */
	daddr_t		limit;		/* last sector of ext. ptn */
	int		num_ptn;	/* number of contained partitions */
	int		ptn_id;		/* entry in mbr */
	int		is_corrupt;	/* 1 if extended chain illegal */
} ext;

#define LBUF 100
static char lbuf[LBUF];

static const char *disk = _PATH_DEFDISK;

static struct disklabel disklabel;		/* disk parameters */

static struct mbr_sector mboot;

static const char *boot_dir = DEFAULT_BOOTDIR;
static char *boot_path = NULL;			/* name of file we actually opened */

#ifdef BOOTSEL
#define BOOTSEL_OPTIONS	"B"
#else
#define BOOTSEL_OPTIONS	
#define change_part(e, p, id, st, sz, bm) change__part(e, p, id, st, sz)
#endif
#define OPTIONS	BOOTSEL_OPTIONS "0123FSafiIluvA:b:c:E:r:s:w:z:"

/*
 * Disk geometry and partition alignment.
 *
 * Modern disks do not have a fixed geomery and will always give a 'faked'
 * geometry that matches the ATA standard - max 16 heads and 256 sec/track.
 * The ATA geometry allows access to 2^28 sectors (as does LBA mode).
 *
 * The BIOS calls originally used an 8bit register for cylinder, head and
 * sector. Later 2 bits were stolen from the sector number and added to
 * cylinder number. The BIOS will translate this faked geometry either to
 * the geometry reported by the disk, or do LBA reads (possibly LBA48).
 * BIOS CHS reads have all sorts of limits, but 2^24 is absolute.
 * For historic reasons the BIOS geometry is the called the dos geometry!
 *
 * If you know the disks real geometry it is usually worth aligning
 * disk partitions to cylinder boundaries (certainly traditional!).
 * For 'mbr' disks this has always been done with the BIOS geometry.
 * The first track (typically 63 sectors) is reserved because the first
 * sector is used for boot code. Similarly the data partition in an
 * extended partition will start one track in. If an extended partition
 * starts at the beginning of the disk you lose 2 tracks.
 *
 * However non-magnetic media in particular has physical sectors that are
 * not the same size as those reported, so has to do read modify write
 * sequences for misaligned transfers. The alignment of partitions to
 * cylinder boundaries makes this happen all the time.
 *
 * It is thus sensible to align partitions on a sensible sector boundary.
 * For instance 1MB (2048 sectors).
 * Common code can do this by using a geometry with 1 head and 2048
 * sectors per track.
 */

/* Disks reported geometry and overall size from device driver */
static unsigned int cylinders, sectors, heads;
static daddr_t disksectors;
#define cylindersectors (heads * sectors)

/* Geometry from the BIOS */
static unsigned int dos_cylinders;
static unsigned int dos_heads;
static unsigned int dos_sectors;
static daddr_t dos_disksectors;
#define dos_cylindersectors (dos_heads * dos_sectors)
#define dos_totalsectors (dos_heads * dos_sectors * dos_cylinders)

#define DOSSECT(s,c)	(((s) & 0x3f) | (((c) >> 2) & 0xc0))
#define DOSCYL(c)	((c) & 0xff)
#define SEC_IN_1M (1024 * 1024 / secsize)
#define SEC_TO_MB(sec) ((unsigned int)(((sec) + SEC_IN_1M / 2) / SEC_IN_1M))
#define SEC_TO_CYL(sec) (((sec) + dos_cylindersectors/2) / dos_cylindersectors)

#define MAXCYL		1024	/* Usual limit is 1023 */
#define	MAXHEAD		256	/* Usual limit is 255 */
#define	MAXSECTOR	63
static int partition = -1;

/* Alignment of partition, and offset if first sector unusable */
static unsigned int ptn_alignment;	/* default dos_cylindersectors */
static unsigned int ptn_0_offset;	/* default dos_sectors */

static int fd = -1, wfd = -1, *rfd = &fd;
static char *disk_file = NULL;
static char *disk_type = NULL;

static int a_flag;		/* set active partition */
static int i_flag;		/* init bootcode */
static int I_flag;		/* ignore errors */
static int u_flag;		/* update partition data */
static int v_flag;		/* more verbose */
static int sh_flag;		/* Output data as shell defines */
static int f_flag;		/* force --not interactive */
static int s_flag;		/* set id,offset,size */
static int b_flag;		/* Set cyl, heads, secs (as c/h/s) */
static int B_flag;		/* Edit/install bootselect code */
static int E_flag;		/* extended partition number */
static int b_cyl, b_head, b_sec;  /* b_flag values. */

#if !HAVE_NBTOOL_CONFIG_H
static int F_flag = 0;
#else
/* Tool - force 'file' mode to avoid unsupported functions and ioctls */
static int F_flag = 1;
#endif

static struct gpt_hdr gpt1, gpt2;	/* GUID partition tables */

static struct mbr_sector bootcode[8192 / sizeof (struct mbr_sector)];
static ssize_t secsize = 512;	/* sector size */
static char *iobuf;		/* buffer for non 512 sector I/O */
static int bootsize;		/* actual size of bootcode */
static int boot_installed;	/* 1 if we've copied code into the mbr */

#if defined(USE_DISKLIST)
#include <machine/cpu.h>
static struct disklist *dl;
#endif


#define KNOWN_SYSIDS	(sizeof(mbr_ptypes)/sizeof(mbr_ptypes[0]))

__dead static void	usage(void);
static void	print_s0(int);
static void	print_part(struct mbr_sector *, int, daddr_t);
static void	print_mbr_partition(struct mbr_sector *, int, daddr_t, daddr_t, int);
static void	print_pbr(daddr_t, int, uint8_t);
static int	is_all_zero(const unsigned char *, size_t);
static void	printvis(int, const char *, const char *, size_t);
static int	read_boot(const char *, void *, size_t, int);
static void	init_sector0(int);
static void	intuit_translated_geometry(void);
static void	get_bios_geometry(void);
static void	get_extended_ptn(void);
static void	get_ptn_alignmemt(void);
#if defined(USE_DISKLIST)
static void	get_diskname(const char *, char *, size_t);
#endif
static int	change_part(int, int, int, daddr_t, daddr_t, char *);
static void	print_geometry(void);
static int	first_active(void);
static void	change_active(int);
static void	change_bios_geometry(void);
static void	dos(int, unsigned char *, unsigned char *, unsigned char *);
static int	open_disk(int);
static ssize_t	read_disk(daddr_t, void *);
static ssize_t	write_disk(daddr_t, void *);
static int	get_params(void);
static int	read_s0(daddr_t, struct mbr_sector *);
static int	write_mbr(void);
static int	read_gpt(daddr_t, struct gpt_hdr *);
static int	delete_gpt(struct gpt_hdr *);
static int	yesno(const char *, ...) __printflike(1, 2);
static int64_t	decimal(const char *, int64_t, int, int64_t, int64_t);
#define DEC_SEC		1		/* asking for a sector number */
#define	DEC_RND		2		/* round to end of first track */
#define	DEC_RND_0	4		/* convert 0 to size of a track */
#define DEC_RND_DOWN	8		/* subtract 1 track */
#define DEC_RND_DOWN_2	16		/* subtract 2 tracks */
static int	ptn_id(const char *, int *);
static int	type_match(const void *, const void *);
static const char *get_type(int);
static int	get_mapping(int, unsigned int *, unsigned int *, unsigned int *, unsigned long *);
#ifdef BOOTSEL
static daddr_t	configure_bootsel(daddr_t);
static void	install_bootsel(int);
static daddr_t	get_default_boot(void);
static void	set_default_boot(daddr_t);
static void	string(const char *, int, char *);
#endif

static void
initvar_disk(const char **diskp)
{
#if !HAVE_NBTOOL_CONFIG_H
	int mib[2];
	size_t len;
	char *root_device;

	mib[0] = CTL_KERN;
	mib[1] = KERN_ROOT_DEVICE;
	if (sysctl(mib, 2, NULL, &len, NULL, 0) == -1 ||
	    (root_device = malloc(len)) == NULL ||
	    sysctl(mib, 2, root_device, &len, NULL, 0) == -1)
		return;

	*diskp = root_device;
#endif /* HAVE_NBTOOL_CONFIG_H */
}

static int
getnum(const char *str, int *num)
{
	char *e;
	long l;

	errno = 0;
	l = strtol(str, &e, 0);
	if (str[0] == '\0' || *e != '\0')
		return -1;
	if (errno == ERANGE && (l == LONG_MAX || l == LONG_MIN))
		return -1;
	/* XXX: truncation */
	*num = (int)l;
	return 0;
}

/* [<sysid>][/[<start>][/[<size>][/[<bootmenu>]]]] */
static int
parse_s(char *arg, int *csysid, unsigned *cstart, unsigned *csize,
    char **cbootmenu)
{
	char *ptr;
	int num;

	if ((ptr = strchr(arg, '/')) != NULL)
		*ptr++ = '\0';

	if (*arg) {
		if (getnum(arg, &num) == -1)
			return -1;
		*csysid = num;
	}
	if (ptr == NULL)
		return 0;

	arg = ptr;
	if ((ptr = strchr(arg, '/')) != NULL)
		*ptr++ = '\0';
	if (*arg) {
		if (getnum(arg, &num) == -1)
			return -1;
		*cstart = num;
	}
	if (ptr == NULL)
		return 0;

	arg = ptr;
	if ((ptr = strchr(arg, '/')) != NULL)
		*ptr++ = '\0';
	if (*arg) {
		if (getnum(arg, &num) == -1)
			return -1;
		*csize = num;
	}
	if (ptr != NULL && *ptr)
		*cbootmenu = ptr;
	return 0;
}

int
main(int argc, char *argv[])
{
	struct stat sb;
	int ch;
	size_t len;
	char *cp;
	int n;
#ifdef BOOTSEL
	daddr_t default_ptn;		/* start sector of default ptn */
#endif
	char *cbootmenu = 0;

	int csysid;	/* For the s_flag. */
	unsigned int cstart, csize;
	a_flag = u_flag = sh_flag = f_flag = s_flag = b_flag = 0;
	i_flag = B_flag = 0;
	v_flag = 0;
	E_flag = 0;
	csysid = -1;
	cstart = csize = ~0;
	while ((ch = getopt(argc, argv, OPTIONS)) != -1) {
		switch (ch) {
		case '0':
			partition = 0;
			break;
		case '1':
			partition = 1;
			break;
		case '2':
			partition = 2;
			break;
		case '3':
			partition = 3;
			break;
		case 'E':	/* Extended partition number */
			E_flag = 1;
			partition = strtoul(optarg, &cp, 0);
			if (*cp || partition < 0)
				errx(1, "Bad partition number -E %s.", optarg);
			break;
#ifdef BOOTSEL
		case 'B':	/* Bootselect parameters */
			B_flag = 1;
			break;
#endif
		case 'F':	/* device argument is really a file */
			F_flag = 1;
			break;
		case 'S':	/* Output as shell variables */
			sh_flag = 1;
			break;
		case 'a':	/* Set active partition */
			a_flag = 1;
			break;
		case 'f':	/* Non interactive */
			f_flag = 1;
			break;
		case 'i':	/* Always update bootcode */
			i_flag = 1;
			break;
		case 'I':	/* Ignore errors */
			I_flag = 1;
			break;
		case 'l':	/* List known partition types */
			for (len = 0; len < KNOWN_SYSIDS; len++)
				printf("%03d %s\n", mbr_ptypes[len].id,
				    mbr_ptypes[len].name);
			return 0;
		case 'u':	/* Update partition details */
			u_flag = 1;
			break;
		case 'v':	/* Be verbose */
			v_flag++;
			break;
		case 's':	/* Partition details */
			s_flag = 1;

			if (parse_s(optarg, &csysid, &cstart, &csize,
			    &cbootmenu) == -1)
				errx(1, "Bad argument to the -s flag.");
			break;
		case 'b':	/* BIOS geometry */
			b_flag = 1;
			if (sscanf(optarg, "%d/%d/%d%n", &b_cyl, &b_head,
			    &b_sec, &n) != 3 || optarg[n] != 0)
				errx(1, "Bad argument to the -b flag.");
			if (b_cyl > MAXCYL)
				b_cyl = MAXCYL;
			break;
		case 'A':	/* Partition alignment[/offset] */
			if (sscanf(optarg, "%u%n/%u%n", &ptn_alignment,
				    &n, &ptn_0_offset, &n) < 1
			    || optarg[n] != 0
			    || ptn_0_offset > ptn_alignment)
				errx(1, "Bad argument to the -A flag.");
			if (ptn_0_offset == 0)
				ptn_0_offset = ptn_alignment;
			break;
		case 'c':	/* file/directory containing boot code */
			if (strchr(optarg, '/') != NULL &&
			    stat(optarg, &sb) == 0 &&
			    (sb.st_mode & S_IFMT) == S_IFDIR) {
				boot_dir = optarg;
				break;
			}
			bootsize = read_boot(optarg, bootcode,
						sizeof bootcode, 1);
			i_flag = 1;
			break;
		case 'r':	/* read data from disk_file (not raw disk) */
			rfd = &wfd;
			/* FALLTHROUGH */
		case 'w':	/* write data to disk_file */
			disk_file = optarg;
			break;
		case 't':
			if (setdisktab(optarg) == -1)
				errx(EXIT_FAILURE, "bad disktab");
			break;
		case 'T':
			disk_type = optarg;
			break;
		case 'z':
			secsize = atoi(optarg);
			if (secsize <= 512)
out:				 errx(EXIT_FAILURE, "Invalid sector size %zd",
				    secsize);
			for (ch = secsize; (ch & 1) == 0; ch >>= 1)
				continue;
			if (ch != 1)
				goto out;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (disk_type != NULL && getdiskbyname(disk_type) == NULL)
		errx(EXIT_FAILURE, "bad disktype");

	if (sh_flag && (a_flag || i_flag || u_flag || f_flag || s_flag))
		usage();

	if (B_flag && f_flag) {
		warnx("Bootselector may only be configured interactively");
		usage();
	}

	if (f_flag && u_flag && !s_flag) {
		warnx("Partition data not specified");
		usage();
	}

	if (s_flag && partition == -1) {
		warnx("-s flag requires a partition selected.");
		usage();
	}

	if (argc > 1)
		usage();

	if (argc > 0)
		disk = argv[0];
	else if (!F_flag) {
		/* Default to boot device */
		initvar_disk(&disk);
	}

	if (!F_flag && stat(disk, &sb) == 0 && S_ISREG(sb.st_mode))
		F_flag = 1;

	if (open_disk(B_flag || a_flag || i_flag || u_flag) < 0)
		exit(1);

	if (secsize > 512) {
		if ((iobuf = malloc(secsize)) == NULL)
			err(EXIT_FAILURE, "Cannot allocate %zd buffer",
			    secsize);
	}

	if (read_s0(0, &mboot))
		/* must have been a blank disk */
		init_sector0(1);

	read_gpt(GPT_HDR_BLKNO, &gpt1);
	read_gpt(disksectors - 1, &gpt2);

	if (b_flag) {
		dos_cylinders = b_cyl;
		dos_heads = b_head;
		dos_sectors = b_sec;
	} else {
		get_bios_geometry();
	}

	if (ptn_alignment == 0)
		get_ptn_alignmemt();

	get_extended_ptn();

#ifdef BOOTSEL
	default_ptn = get_default_boot();
#endif

	if (E_flag && !u_flag && partition >= ext.num_ptn)
		errx(1, "Extended partition %d is not defined.", partition);

	/* Do the update stuff! */
	if (u_flag) {
		if (!f_flag && !b_flag)
			change_bios_geometry();

		if (s_flag)
			change_part(E_flag, partition, csysid, cstart, csize,
				cbootmenu);
		else {
			int part = partition, chg_ext = E_flag, prompt = 1;
			do {
				if (prompt) {
					printf("\n");
					print_s0(partition);
				}
				if (partition == -1)
					part = ptn_id(
				    "Which partition do you want to change?",
							&chg_ext);
				if (part < 0)
					break;
				prompt = change_part(chg_ext, part, 0, 0, 0, 0);
			} while (partition == -1);
		}
	} else {
		if (!i_flag && !B_flag) {
			print_geometry();
			print_s0(partition);
		}
	}

	if (a_flag && !E_flag)
		change_active(partition);

#ifdef BOOTSEL
	if (B_flag || u_flag || i_flag)
		/* Ensure the mbr code supports this configuration */
		install_bootsel(0);
	if (B_flag)
		default_ptn = configure_bootsel(default_ptn);
	set_default_boot(default_ptn);
#else
	if (i_flag)
		init_sector0(0);
#endif

	if (u_flag || a_flag || i_flag || B_flag) {
		if (!f_flag) {
			printf("\nWe haven't written the MBR back to disk "
			       "yet.  This is your last chance.\n");
			if (u_flag)
				print_s0(-1);
			if (gpt1.hdr_size != 0 || gpt2.hdr_size != 0)
				printf("\nWARNING: The disk is carrying "
				       "GUID Partition Tables.\n"
				       "         If you continue, "
				       "GPT headers will be deleted.\n\n");
			if (yesno("Should we write new partition table?")) {
				delete_gpt(&gpt1);
				delete_gpt(&gpt2);
				write_mbr();
			}
		} else {
			if (delete_gpt(&gpt1) > 0)
				warnx("Primary GPT header was deleted");
			if (delete_gpt(&gpt2) > 0)
				warnx("Secondary GPT header was deleted");
			write_mbr();
		}
	}

	exit(0);
}

static void
usage(void)
{
	int indent = 7 + (int)strlen(getprogname()) + 1;

	(void)fprintf(stderr, "usage: %s [-aBFfIilSuv] "
		"[-A ptn_alignment[/ptn_0_offset]] \\\n"
		"%*s[-b cylinders/heads/sectors] \\\n"
		"%*s[-0123 | -E num "
		"[-s [id][/[start][/[size][/bootmenu]]]] \\\n"
		"%*s[-t disktab] [-T disktype] \\\n"
		"%*s[-c bootcode] "
		"[-r|-w file] [-z sectorsize] [device]\n"
		"\t-a change active partition\n"
		"\t-f force - not interactive\n"
		"\t-i initialise MBR code\n"
		"\t-I ignore errors about no space or overlapping partitions\n"
		"\t-l list partition types\n"
		"\t-u update partition data\n"
		"\t-v verbose output, -v -v more verbose still\n"
		"\t-B update bootselect options\n"
		"\t-F treat device as a regular file\n"
		"\t-S output as shell defines\n"
		"\t-r and -w access 'file' for non-destructive testing\n",
		getprogname(), indent, "", indent, "", indent, "", indent, "");
	exit(1);
}

static daddr_t
ext_offset(int part)
{
	daddr_t offset = ext.base;

	if (part != 0)
		offset += le32toh(ext.ptn[part - 1].mbr_parts[1].mbrp_start);
	return offset;
}

static void
print_s0(int which)
{
	int part;

	if (which == -1) {
		if (!sh_flag)
			printf("Partition table:\n");
		for (part = 0; part < MBR_PART_COUNT; part++) {
			if (!sh_flag)
				printf("%d: ", part);
			print_part(&mboot, part, 0);
		}
		if (!sh_flag) {
			if (ext.is_corrupt)
				printf("Extended partition table is corrupt\n");
			else
				if (ext.num_ptn != 0)
					printf("Extended partition table:\n");
		}
		for (part = 0; part < ext.num_ptn; part++) {
			if (!sh_flag)
				printf("E%d: ", part);
			print_part(&ext.ptn[part], 0, ext_offset(part));
			if (!sh_flag && v_flag >= 2) {
				printf("link: ");
				print_mbr_partition(&ext.ptn[part], 1,
						ext_offset(part), ext.base, 0);
			}
		}
#ifdef BOOTSEL
		if (!sh_flag && mboot.mbr_bootsel_magic == LE_MBR_BS_MAGIC) {
			int tmo;

			printf("Bootselector ");
			if (mboot.mbr_bootsel.mbrbs_flags & MBR_BS_ACTIVE) {
				printf("enabled");
				tmo = le16toh(mboot.mbr_bootsel.mbrbs_timeo);
				if (tmo == 0xffff)
					printf(", infinite timeout");
				else
					printf(", timeout %d seconds",
						    (10 * tmo + 9) / 182);
			} else
				printf("disabled");
			printf(".\n");
		}
#endif
		if (!sh_flag) {
			int active = first_active();
			if (active == MBR_PART_COUNT)
				printf("No active partition.\n");
			else
				printf("First active partition: %d\n", active);
		}
		if (!sh_flag)
			printf("Drive serial number: %"PRIu32" (0x%08x)\n",
			    le32toh(mboot.mbr_dsn),
			    le32toh(mboot.mbr_dsn));
		return;
	}

	if (E_flag) {
		if (!sh_flag)
			printf("Extended partition E%d:\n", which);
		if (which > ext.num_ptn)
			printf("Undefined\n");
		else
			print_part(&ext.ptn[which], 0, ext_offset(which));
	} else {
		if (!sh_flag)
			printf("Partition %d:\n", which);
		print_part(&mboot, which, 0);
	}
}

static void
print_part(struct mbr_sector *boot, int part, daddr_t offset)
{
	struct mbr_partition *partp;
	const char *e;

	if (!sh_flag) {
		print_mbr_partition(boot, part, offset, 0, 0);
		return;
	}

	partp = &boot->mbr_parts[part];
	if (boot != &mboot) {
		part = boot - ext.ptn;
		e = "E";
	} else
		e = "";

	if (partp->mbrp_type == 0) {
		printf("PART%s%dSIZE=0\n", e, part);
		return;
	}

	printf("PART%s%dID=%d\n", e, part, partp->mbrp_type);
	printf("PART%s%dSIZE=%u\n", e, part, le32toh(partp->mbrp_size));
	printf("PART%s%dSTART=%"PRIdaddr"\n", e, part,
	    offset + le32toh(partp->mbrp_start));
	printf("PART%s%dFLAG=0x%x\n", e, part, partp->mbrp_flag);
	printf("PART%s%dBCYL=%d\n", e, part,
	    MBR_PCYL(partp->mbrp_scyl, partp->mbrp_ssect));
	printf("PART%s%dBHEAD=%d\n", e, part, partp->mbrp_shd);
	printf("PART%s%dBSEC=%d\n", e, part, MBR_PSECT(partp->mbrp_ssect));
	printf("PART%s%dECYL=%d\n", e, part,
	    MBR_PCYL(partp->mbrp_ecyl, partp->mbrp_esect));
	printf("PART%s%dEHEAD=%d\n", e, part, partp->mbrp_ehd);
	printf("PART%s%dESEC=%d\n", e, part, MBR_PSECT(partp->mbrp_esect));
}

static void
pr_cyls(daddr_t sector, int is_end)
{
	unsigned long cyl, head, sect;
	cyl = sector / dos_cylindersectors;
	sect = sector - cyl * dos_cylindersectors;
	head = sect / dos_sectors;
	sect -= head * dos_sectors;

	printf("%lu", cyl);

	if (is_end) {
		if (head == dos_heads - 1 && sect == dos_sectors - 1)
			return;
	} else {
		if (head == 0 && sect == 0)
			return;
	}

	printf("/%lu/%lu", head, sect + 1);
}

static void
print_mbr_partition(struct mbr_sector *boot, int part,
    daddr_t offset, daddr_t exoffset, int indent)
{
	daddr_t	start;
	daddr_t	size;
	struct mbr_partition *partp = &boot->mbr_parts[part];
	struct mbr_sector eboot;
	int p;
	static int dumped = 0;

	if (partp->mbrp_type == 0 && v_flag < 2) {
		printf("<UNUSED>\n");
		return;
	}

	start = le32toh(partp->mbrp_start);
	size = le32toh(partp->mbrp_size);
	if (MBR_IS_EXTENDED(partp->mbrp_type))
		start += exoffset;
	else
		start += offset;

	printf("%s (sysid %d)\n", get_type(partp->mbrp_type), partp->mbrp_type);
#ifdef BOOTSEL
	if (boot->mbr_bootsel_magic == LE_MBR_BS_MAGIC &&
	    boot->mbr_bootsel.mbrbs_nametab[part][0])
		printf("%*s    bootmenu: %s\n", indent, "",
		    boot->mbr_bootsel.mbrbs_nametab[part]);
#endif

	printf("%*s    start %"PRIdaddr", size %"PRIdaddr,
	    indent, "", start, size);
	if (size != 0) {
		printf(" (%u MB, Cyls ", SEC_TO_MB(size));
		if (v_flag == 0 && le32toh(partp->mbrp_start) == ptn_0_offset)
			pr_cyls(start - ptn_0_offset, 0);
		else
			pr_cyls(start, 0);
		printf("-");
		pr_cyls(start + size - 1, 1);
		printf(")");
	}

	switch (partp->mbrp_flag) {
	case 0:
		break;
	case MBR_PFLAG_ACTIVE:
		printf(", Active");
		break;
	default:
		printf(", flag 0x%x", partp->mbrp_flag);
		break;
	}
	printf("\n");

	if (v_flag) {
		printf("%*s        beg: cylinder %4d, head %3d, sector %2d\n",
		    indent, "",
		    MBR_PCYL(partp->mbrp_scyl, partp->mbrp_ssect),
		    partp->mbrp_shd, MBR_PSECT(partp->mbrp_ssect));
		printf("%*s        end: cylinder %4d, head %3d, sector %2d\n",
		    indent, "",
		    MBR_PCYL(partp->mbrp_ecyl, partp->mbrp_esect),
		    partp->mbrp_ehd, MBR_PSECT(partp->mbrp_esect));
	}

	if (partp->mbrp_type == 0 && start == 0 && v_flag < 3)
		return;

	if (! MBR_IS_EXTENDED(partp->mbrp_type))
		print_pbr(start, indent + 8, partp->mbrp_type);

	if (!MBR_IS_EXTENDED(partp->mbrp_type) ||
	    (v_flag <= 2 && !ext.is_corrupt))
		return;

	/*
	 * Recursive dump extended table,
	 * This is read from the disk - so is wrong during editing.
	 * Just ensure we only show it once.
	 */
	if (dumped)
		return;

	printf("%*s    Extended partition table:\n", indent, "");
	indent += 4;
	if (read_s0(start, &eboot) == -1)
		return;
	for (p = 0; p < MBR_PART_COUNT; p++) {
		printf("%*s%d: ", indent, "", p);
		print_mbr_partition(&eboot, p, start,
				    exoffset ? exoffset : start, indent);
	}

	if (exoffset == 0)
		dumped = 1;
}

/* Print a line with a label and a vis-encoded string */
static void
printvis(int indent, const char *label, const char *buf, size_t size)
{
	char *visbuf;

	if ((visbuf = malloc(size * 4 + 1)) == NULL)
		err(1, "Malloc failed");
	strsvisx(visbuf, buf, size, VIS_TAB|VIS_NL|VIS_OCTAL, "\"");
	printf("%*s%s: \"%s\"\n",
	    indent, "",
	    label, visbuf);
	free(visbuf);
}

/* Check whether a buffer contains all bytes zero */
static int
is_all_zero(const unsigned char *p, size_t size)
{

	while (size-- > 0) {
		if (*p++ != 0)
			return 0;
	}
	return 1;
}

/*
 * Report on the contents of a PBR sector.
 *
 * We first perform several sanity checks.  If vflag >= 2, we report all
 * failing tests, but for smaller values of v_flag we stop after the
 * first failing test.  Tests are ordered in an attempt to get the most
 * useful error message from the first failing test.
 *
 * If v_flag >= 2, we also report some decoded values from the PBR.
 * These results may be meaningless, if the PBR doesn't follow common
 * conventions.
 *
 * Trying to decode anything more than the magic number in the last
 * two bytes is a layering violation, but it can be very useful in
 * diagnosing boot failures.
 */
static void
print_pbr(daddr_t sector, int indent, uint8_t part_type)
{
	struct mbr_sector pboot;
	unsigned char *p, *endp;
	unsigned char val;
	int ok;
	int errcount = 0;

#define PBR_ERROR(...)							\
	do {								\
		++errcount;						\
		printf("%*s%s: ", indent, "",				\
		    (v_flag < 2 ? "PBR is not bootable" : "Not bootable")); \
		printf(__VA_ARGS__);					\
		if (v_flag < 2)						\
			return;						\
	} while (/*CONSTCOND*/ 0)

	if (v_flag >= 2) {
		printf("%*sInformation from PBR:\n",
		    indent, "");
		indent += 4;
	}

	if (read_disk(sector, &pboot) == -1) {
		PBR_ERROR("Sector %"PRIdaddr" is unreadable (%s)\n",
		    sector, strerror(errno));
		return;
	}

	/* all bytes identical? */
	p = (unsigned char *)&pboot;
	endp = p + sizeof(pboot);
	val = *p;
	ok = 0;
	for (; p < endp; p++) {
		if (*p != val) {
			ok = 1;
			break;
		}
	}
	if (! ok)
		PBR_ERROR("All bytes are identical (0x%02x)\n", val);

	if (pboot.mbr_magic != LE_MBR_MAGIC)
		PBR_ERROR("Bad magic number (0x%04x)\n",
			le16toh(pboot.mbr_magic));

#if 0
	/* Some i386 OS might fail this test.  All non-i386 will fail. */
	if (pboot.mbr_jmpboot[0] != 0xE9
	    && pboot.mbr_jmpboot[0] != 0xEB) {
		PBR_ERROR("Does not begin with i386 JMP instruction"
			" (0x%02x 0x%02x0 0x%02x)\n",
		    pboot.mbr_jmpboot[0], pboot.mbr_jmpboot[1],
		    pboot.mbr_jmpboot[2]);
	}
#endif

	if (v_flag > 0 && errcount == 0)
		printf("%*sPBR appears to be bootable\n",
		    indent, "");
	if (v_flag < 2)
		return;

	if (! is_all_zero(pboot.mbr_oemname, sizeof(pboot.mbr_oemname))) {
		printvis(indent, "OEM name", (char *)pboot.mbr_oemname,
			sizeof(pboot.mbr_oemname));
	}

	if (pboot.mbr_bpb.bpb16.bsBootSig == 0x29)
		printf("%*sBPB FAT16 boot signature found\n",
		    indent, "");
	if (pboot.mbr_bpb.bpb32.bsBootSig == 0x29)
		printf("%*sBPB FAT32 boot signature found\n",
		    indent, "");

#undef PBR_ERROR
}

static int
read_boot(const char *name, void *buf, size_t len, int err_exit)
{
	int bfd, ret;
	struct stat st;

	if (boot_path != NULL)
		free(boot_path);
	if (strchr(name, '/') == 0)
		asprintf(&boot_path, "%s/%s", boot_dir, name);
	else
		boot_path = strdup(name);
	if (boot_path == NULL)
		err(1, "Malloc failed");

	if ((bfd = open(boot_path, O_RDONLY)) < 0 || fstat(bfd, &st) == -1) {
		warn("%s", boot_path);
		goto fail;
	}

	if (st.st_size > (off_t)len) {
		warnx("%s: bootcode too large", boot_path);
		goto fail;
	}
	ret = st.st_size;
	if (ret < 0x200) {
		warnx("%s: bootcode too small", boot_path);
		goto fail;
	}
	if (read(bfd, buf, len) != ret) {
		warn("%s", boot_path);
		goto fail;
	}

	/*
	 * Do some sanity checking here
	 */
	if (((struct mbr_sector *)buf)->mbr_magic != LE_MBR_MAGIC) {
		warnx("%s: invalid magic", boot_path);
		goto fail;
	}

	close(bfd);
	ret = (ret + 0x1ff) & ~0x1ff;
	return ret;

    fail:
	if (bfd >= 0)
		close(bfd);
	if (err_exit)
		exit(1);
	return 0;
}

static void
init_sector0(int zappart)
{
	int i;
	int copy_size = offsetof(struct mbr_sector, mbr_dsn);

#ifdef DEFAULT_BOOTCODE
	if (bootsize == 0)
		bootsize = read_boot(DEFAULT_BOOTCODE, bootcode,
			sizeof bootcode, 0);
#endif
#ifdef BOOTSEL
	if (mboot.mbr_bootsel_magic == LE_MBR_BS_MAGIC
	    && bootcode[0].mbr_bootsel_magic == LE_MBR_BS_MAGIC)
		copy_size = MBR_BS_OFFSET;
#endif

	if (bootsize != 0) {
		boot_installed = 1;
		memcpy(&mboot, bootcode, copy_size);
		mboot.mbr_bootsel_magic = bootcode[0].mbr_bootsel_magic;
	}
	mboot.mbr_magic = LE_MBR_MAGIC;
	
	if (!zappart)
		return;
	for (i = 0; i < MBR_PART_COUNT; i++)
		memset(&mboot.mbr_parts[i], 0, sizeof(mboot.mbr_parts[i]));
}

static void
get_extended_ptn(void)
{
	struct mbr_partition *mp;
	struct mbr_sector *boot;
	daddr_t offset;
	struct mbr_sector *nptn;

	/* find first (there should only be one) extended partition */
	for (mp = mboot.mbr_parts; !MBR_IS_EXTENDED(mp->mbrp_type); mp++)
		if (mp >= &mboot.mbr_parts[MBR_PART_COUNT])
			return;

	/*
	 * The extended partition should be structured as a linked list
	 * (even though it appears, at first glance, to be a tree).
	 */
	ext.base = le32toh(mp->mbrp_start);
	ext.limit = ext.base + le32toh(mp->mbrp_size);
	ext.ptn_id = mp - mboot.mbr_parts;
	for (offset = 0;; offset = le32toh(boot->mbr_parts[1].mbrp_start)) {
		nptn = realloc(ext.ptn, (ext.num_ptn + 1) * sizeof *ext.ptn);
		if (nptn == NULL)
			err(1, "Malloc failed");
		ext.ptn = nptn;
		boot = ext.ptn + ext.num_ptn;
		if (read_s0(offset + ext.base, boot) == -1)
			break;
		/* expect p0 to be valid and p1 to be another extended ptn */
		if (MBR_IS_EXTENDED(boot->mbr_parts[0].mbrp_type))
			break;
		if (boot->mbr_parts[1].mbrp_type != 0 &&
		    !MBR_IS_EXTENDED(boot->mbr_parts[1].mbrp_type))
			break;
		/* p2 and p3 should be unallocated */
		if (boot->mbr_parts[2].mbrp_type != 0 ||
		    boot->mbr_parts[3].mbrp_type != 0)
			break;
		/* data ptn inside extended one */
		if (boot->mbr_parts[0].mbrp_type != 0 &&
		    offset + le32toh(boot->mbr_parts[0].mbrp_start)
		    + le32toh(boot->mbr_parts[0].mbrp_size) > ext.limit)
			break;

		ext.num_ptn++;

		if (boot->mbr_parts[1].mbrp_type == 0)
			/* end of extended partition chain */
			return;
		/* must be in sector order */
		if (offset >= le32toh(boot->mbr_parts[1].mbrp_start))
			break;
	}

	warnx("Extended partition table is corrupt");
	ext.is_corrupt = 1;
	ext.num_ptn = 0;
}

#if defined(USE_DISKLIST)
static void
get_diskname(const char *fullname, char *diskname, size_t size)
{
	const char *p, *p2;
	size_t len;

	p = strrchr(fullname, '/');
	if (p == NULL)
		p = fullname;
	else
		p++;

	if (*p == 0) {
		strlcpy(diskname, fullname, size);
		return;
	}

	if (*p == 'r')
		p++;

	for (p2 = p; *p2 != 0; p2++)
		if (isdigit((unsigned char)*p2))
			break;
	if (*p2 == 0) {
		/* XXX invalid diskname? */
		strlcpy(diskname, fullname, size);
		return;
	}
	while (isdigit((unsigned char)*p2))
		p2++; 

	len = p2 - p;
	if (len > size) {
		/* XXX */
		strlcpy(diskname, fullname, size);
		return;
	}
 
	memcpy(diskname, p, len);
	diskname[len] = 0;
}
#endif

static void
get_ptn_alignmemt(void)
{
	struct mbr_partition *partp = &mboot.mbr_parts[0];
	uint32_t ptn_0_base, ptn_0_limit;

	/* Default to using 'traditional' cylinder alignment */
	ptn_alignment = dos_cylindersectors;
	ptn_0_offset = dos_sectors;

	if (partp->mbrp_type != 0) {
		/* Try to copy alignment of first partition */
		ptn_0_base = le32toh(partp->mbrp_start);
		ptn_0_limit = ptn_0_base + le32toh(partp->mbrp_size);
		if (!(ptn_0_limit & 2047)) {
			/* Partition ends on a 1MB boundary, align to 1MB */
			ptn_alignment = 2048;
			if (ptn_0_base <= 2048
			    && !(ptn_0_base & (ptn_0_base - 1))) {
				/* ptn_base is a power of 2, use it */
				ptn_0_offset = ptn_0_base;
			}
		}
	} else {
		/* Use 1MB alignment for large disks */
		if (disksectors > 2048 * 1024 * 128) {
			ptn_alignment = 2048;
			ptn_0_offset = 2048;
		}
	}
}

static void
get_bios_geometry(void)
{
#if defined(USE_DISKLIST)
	int mib[2], i;
	size_t len;
	struct biosdisk_info *bip;
	struct nativedisk_info *nip;
	char diskname[8];

	mib[0] = CTL_MACHDEP;
	mib[1] = CPU_DISKINFO;
	if (sysctl(mib, 2, NULL, &len, NULL, 0) < 0) {
		goto out;
	}
	dl = (struct disklist *) malloc(len);
	if (dl == NULL)
		err(1, "Malloc failed");
	if (sysctl(mib, 2, dl, &len, NULL, 0) < 0) {
		free(dl);
		dl = 0;
		goto out;
	}

	get_diskname(disk, diskname, sizeof diskname);

	for (i = 0; i < dl->dl_nnativedisks; i++) {
		nip = &dl->dl_nativedisks[i];
		if (strcmp(diskname, nip->ni_devname))
			continue;
		/*
		 * XXX listing possible matches is better. This is ok for
		 * now because the user has a chance to change it later.
		 * Also, if all the disks have the same parameters then we can
		 * just use them, we don't need to know which disk is which.
		 */
		if (nip->ni_nmatches != 0) {
			bip = &dl->dl_biosdisks[nip->ni_biosmatches[0]];
			dos_cylinders = bip->bi_cyl;
			dos_heads = bip->bi_head;
			dos_sectors = bip->bi_sec;
			if (bip->bi_lbasecs)
				dos_disksectors = bip->bi_lbasecs;
			return;
		}
	}
 out:
#endif
	/* Allright, allright, make a stupid guess.. */
	intuit_translated_geometry();
}

#ifdef BOOTSEL
static daddr_t
get_default_boot(void)
{
	unsigned int id;
	int p;

	if (mboot.mbr_bootsel_magic != LE_MBR_BS_MAGIC)
		/* default to first active partition */
		return DEFAULT_ACTIVE;

	id = mboot.mbr_bootsel.mbrbs_defkey;

	if (mboot.mbr_bootsel.mbrbs_flags & MBR_BS_ASCII) {
		/* Keycode is ascii */
		if (id == '\r')
		    return DEFAULT_ACTIVE;
		/* '1'+ => allocated partition id, 'a'+ => disk 0+ */
		if (id >= 'a' && id < 'a' + MAX_BIOS_DISKS)
			return DEFAULT_DISK(id - 'a');
		id -= '1';
	} else {
		/* keycode is PS/2 keycode */
		if (id == SCAN_ENTER)
			return DEFAULT_ACTIVE;
		/* 1+ => allocated partition id, F1+ => disk 0+ */
		if (id >= SCAN_F1 && id < SCAN_F1 + MAX_BIOS_DISKS)
			return DEFAULT_DISK(id - SCAN_F1);
		id -= SCAN_1;
	}

	/* Convert partition index to the invariant start sector number */

	for (p = 0; p < MBR_PART_COUNT; p++) {
		if (mboot.mbr_parts[p].mbrp_type == 0)
			continue;
		if (mboot.mbr_bootsel.mbrbs_nametab[p][0] == 0)
			continue;
		if (id-- == 0)
			return le32toh(mboot.mbr_parts[p].mbrp_start);
	}

	for (p = 0; p < ext.num_ptn; p++) {
		if (ext.ptn[p].mbr_parts[0].mbrp_type == 0)
			continue;
		if (ext.ptn[p].mbr_bootsel.mbrbs_nametab[0][0] == 0)
			continue;
		if (id-- == 0)
			return ext_offset(p)
				+ le32toh(ext.ptn[p].mbr_parts[0].mbrp_start);
	}

	return DEFAULT_ACTIVE;
}

static void
set_default_boot(daddr_t default_ptn)
{
	int p;
	static const unsigned char key_list[] = { SCAN_ENTER, SCAN_F1, SCAN_1,
						'\r', 'a', '1' };
	const unsigned char *key = key_list;

	if (mboot.mbr_bootsel_magic != LE_MBR_BS_MAGIC)
		/* sanity */
		return;

	if (mboot.mbr_bootsel.mbrbs_flags & MBR_BS_ASCII)
		/* Use ascii values */
		key += 3;

	if (default_ptn == DEFAULT_ACTIVE) {
		mboot.mbr_bootsel.mbrbs_defkey = key[0];
		return;
	}

	if (default_ptn >= DEFAULT_DISK(0)
	    && default_ptn < DEFAULT_DISK(MAX_BIOS_DISKS)) {
		mboot.mbr_bootsel.mbrbs_defkey = key[1]
		    + default_ptn - DEFAULT_DISK(0);
		return;
	}

	mboot.mbr_bootsel.mbrbs_defkey = key[2];
	for (p = 0; p < MBR_PART_COUNT; p++) {
		if (mboot.mbr_parts[p].mbrp_type == 0)
			continue;
		if (mboot.mbr_bootsel.mbrbs_nametab[p][0] == 0)
			continue;
		if (le32toh(mboot.mbr_parts[p].mbrp_start) == default_ptn)
			return;
		mboot.mbr_bootsel.mbrbs_defkey++;
	}

	if (mboot.mbr_bootsel.mbrbs_flags & MBR_BS_EXTLBA) {
		for (p = 0; p < ext.num_ptn; p++) {
			if (ext.ptn[p].mbr_parts[0].mbrp_type == 0)
				continue;
			if (ext.ptn[p].mbr_bootsel.mbrbs_nametab[0][0] == 0)
				continue;
			if (le32toh(ext.ptn[p].mbr_parts[0].mbrp_start) +
			    ext_offset(p) == default_ptn)
				return;
			mboot.mbr_bootsel.mbrbs_defkey++;
		}
	}

	/* Default to first active partition */
	mboot.mbr_bootsel.mbrbs_defkey = key[0];
}

static void
install_bootsel(int needed)
{
	struct mbr_bootsel *mbs = &mboot.mbr_bootsel;
	int p;
	int ext13 = 0;
	const char *code;

	needed |= MBR_BS_NEWMBR;	/* need new bootsel code */

	/* Work out which boot code we need for this configuration */
	for (p = 0; p < MBR_PART_COUNT; p++) {
		if (mboot.mbr_parts[p].mbrp_type == 0)
			continue;
		if (mboot.mbr_bootsel_magic != LE_MBR_BS_MAGIC)
			break;
		if (mbs->mbrbs_nametab[p][0] == 0)
			continue;
		needed |= MBR_BS_ACTIVE;
		if (le32toh(mboot.mbr_parts[p].mbrp_start) >= dos_totalsectors)
			ext13 = MBR_BS_EXTINT13;
	}

	for (p = 0; p < ext.num_ptn; p++) {
		if (ext.ptn[p].mbr_bootsel_magic != LE_MBR_BS_MAGIC)
			continue;
		if (ext.ptn[p].mbr_parts[0].mbrp_type == 0)
			continue;
		if (ext.ptn[p].mbr_bootsel.mbrbs_nametab[p][0] == 0)
			continue;
		needed |= MBR_BS_EXTLBA | MBR_BS_ACTIVE;
	}

	if (B_flag)
		needed |= MBR_BS_ACTIVE;

	/* Is the installed code good enough ? */
	if (!i_flag && (needed == 0 ||
	    (mboot.mbr_bootsel_magic == LE_MBR_BS_MAGIC
	    && (mbs->mbrbs_flags & needed) == needed))) {
		/* yes - just set flags */
		mbs->mbrbs_flags |= ext13;
		return;
	}

	/* ok - we need to replace the bootcode */

	if (f_flag && !(i_flag || B_flag)) {
		warnx("Installed bootfile doesn't support required options.");
		return;
	}

	if (!f_flag && bootsize == 0 && !i_flag)
		/* Output an explanation for the 'update bootcode' prompt. */
		printf("\n%s\n",
		    "Installed bootfile doesn't support required options.");

	/* Were we told a specific file ? (which we have already read) */
	/* If so check that it supports what we need. */
	if (bootsize != 0 && needed != 0
	    && (bootcode[0].mbr_bootsel_magic != LE_MBR_BS_MAGIC
	    || ((bootcode[0].mbr_bootsel.mbrbs_flags & needed) != needed))) {
		/* No it doesn't... */
		if (f_flag)
			warnx("Bootfile %s doesn't support "
				    "required bootsel options", boot_path );
			/* But install it anyway */
		else
			if (yesno("Bootfile %s doesn't support the required "
			    "options,\ninstall default bootfile instead?",
			    boot_path))
				bootsize = 0;
	}

	if (bootsize == 0) {
		/* Get name of bootfile that supports the required facilities */
		code = DEFAULT_BOOTCODE;
		if (needed & MBR_BS_ACTIVE)
			code = DEFAULT_BOOTSELCODE;
#ifdef DEFAULT_BOOTEXTCODE
		if (needed & MBR_BS_EXTLBA)
			code = DEFAULT_BOOTEXTCODE;
#endif

		bootsize = read_boot(code, bootcode, sizeof bootcode, 0);
		if (bootsize == 0)
			/* The old bootcode is better than no bootcode at all */
			return;
		if ((bootcode[0].mbr_bootsel.mbrbs_flags & needed) != needed)
			warnx("Default bootfile %s doesn't support required "
				"options.  Got flags 0x%x, wanted 0x%x\n",
				boot_path, bootcode[0].mbr_bootsel.mbrbs_flags,
				needed);
	}

	if (!f_flag && !yesno("Update the bootcode from %s?", boot_path))
		return;

	init_sector0(0);

	if (mboot.mbr_bootsel_magic == LE_MBR_BS_MAGIC)
		mbs->mbrbs_flags = bootcode[0].mbr_bootsel.mbrbs_flags | ext13;
}

static daddr_t
configure_bootsel(daddr_t default_ptn)
{
	struct mbr_bootsel *mbs = &mboot.mbr_bootsel;
	int i, item, opt;
	int tmo;
	daddr_t *off;
	int num_bios_disks;

#if defined(USE_DISKLIST)
	if (dl != NULL) {
		num_bios_disks = dl->dl_nbiosdisks;
		if (num_bios_disks > MAX_BIOS_DISKS)
			num_bios_disks = MAX_BIOS_DISKS;
	} else
#endif
		num_bios_disks = MAX_BIOS_DISKS;

	printf("\nBoot selector configuration:\n");

	/* The timeout value is in ticks, ~18.2 Hz. Avoid using floats.
	 * Ticks are nearly 64k/3600 - so our long timers are sligtly out!
	 * Newer bootcode always waits for 1 tick, so treats 0xffff
	 * as wait forever.
	 */
	tmo = le16toh(mbs->mbrbs_timeo);
	tmo = tmo == 0xffff ? -1 : (10 * tmo + 9) / 182;
	tmo = decimal("Timeout value (0 to 3600 seconds, -1 => never)",
			tmo, 0, -1, 3600);
	mbs->mbrbs_timeo = htole16(tmo == -1 ? 0xffff : (tmo * 182) / 10);

	off = calloc(1 + MBR_PART_COUNT + ext.num_ptn + num_bios_disks, sizeof *off);
	if (off == NULL)
		err(1, "Malloc failed");

	printf("Select the default boot option. Options are:\n\n");
	item = 0;
	opt = 0;
	off[opt] = DEFAULT_ACTIVE;
	printf("%d: The first active partition\n", opt);
	for (i = 0; i < MBR_PART_COUNT; i++) {
		if (mboot.mbr_parts[i].mbrp_type == 0)
			continue;
		if (mbs->mbrbs_nametab[i][0] == 0)
			continue;
		printf("%d: %s\n", ++opt, &mbs->mbrbs_nametab[i][0]);
		off[opt] = le32toh(mboot.mbr_parts[i].mbrp_start);
		if (off[opt] == default_ptn)
			item = opt;
	}
	if (mbs->mbrbs_flags & MBR_BS_EXTLBA) {
		for (i = 0; i < ext.num_ptn; i++) {
			if (ext.ptn[i].mbr_parts[0].mbrp_type == 0)
				continue;
			if (ext.ptn[i].mbr_bootsel.mbrbs_nametab[0][0] == 0)
				continue;
			printf("%d: %s\n",
			    ++opt, ext.ptn[i].mbr_bootsel.mbrbs_nametab[0]);
			off[opt] = ext_offset(i) +
			    le32toh(ext.ptn[i].mbr_parts[0].mbrp_start);
			if (off[opt] == default_ptn)
				item = opt;
		}
	}
	for (i = 0; i < num_bios_disks; i++) {
		printf("%d: Harddisk %d\n", ++opt, i);
		off[opt] = DEFAULT_DISK(i);
		if (DEFAULT_DISK(i) == default_ptn)
			item = opt;
	}

	item = decimal("Default boot option", item, 0, 0, opt);

	default_ptn = off[item];
	free(off);
	return default_ptn;
}
#endif /* BOOTSEL */


/* Prerequisite: the disklabel parameters and master boot record must
 *		 have been read (i.e. dos_* and mboot are meaningful).
 * Specification: modifies dos_cylinders, dos_heads, dos_sectors, and
 *		  dos_cylindersectors to be consistent with what the
 *		  partition table is using, if we can find a geometry
 *		  which is consistent with all partition table entries.
 *		  We may get the number of cylinders slightly wrong (in
 *		  the conservative direction).  The idea is to be able
 *		  to create a NetBSD partition on a disk we don't know
 *		  the translated geometry of.
 * This routine is only used for non-x86 systems or when we fail to
 * get the BIOS geometry from the kernel.
 */
static void
intuit_translated_geometry(void)
{
	uint32_t xcylinders;
	int xheads = -1, xsectors = -1, i, j;
	unsigned int c1, h1, s1, c2, h2, s2;
	unsigned long a1, a2;
	uint64_t num, denom;

	/*
	 * The physical parameters may be invalid as bios geometry.
	 * If we cannot determine the actual bios geometry, we are
	 * better off picking a likely 'faked' geometry than leaving
	 * the invalid physical one.
	 */

	if (dos_cylinders > MAXCYL || dos_heads > MAXHEAD ||
	    dos_sectors > MAXSECTOR) {
		h1 = MAXHEAD - 1;
		c1 = MAXCYL - 1;
#if defined(USE_DISKLIST)
		if (dl != NULL) {
			/* BIOS may use 256 heads or 1024 cylinders */
			for (i = 0; i < dl->dl_nbiosdisks; i++) {
				if (h1 < (unsigned int)dl->dl_biosdisks[i].bi_head)
					h1 = dl->dl_biosdisks[i].bi_head;
				if (c1 < (unsigned int)dl->dl_biosdisks[i].bi_cyl)
					c1 = dl->dl_biosdisks[i].bi_cyl;
			}
		}
#endif
		dos_sectors = MAXSECTOR;
		dos_heads = h1;
		dos_cylinders = disklabel.d_secperunit / (MAXSECTOR * h1);
		if (dos_cylinders > c1)
			dos_cylinders = c1;
	}

	/* Try to deduce the number of heads from two different mappings. */
	for (i = 0; i < MBR_PART_COUNT * 2 - 1; i++) {
		if (get_mapping(i, &c1, &h1, &s1, &a1) < 0)
			continue;
		a1 -= s1;
		for (j = i + 1; j < MBR_PART_COUNT * 2; j++) {
			if (get_mapping(j, &c2, &h2, &s2, &a2) < 0)
				continue;
			a2 -= s2;
			num = (uint64_t)h1 * a2 - (uint64_t)h2 * a1;
			denom = (uint64_t)c2 * a1 - (uint64_t)c1 * a2;
			if (denom != 0 && num != 0 && num % denom == 0) {
				xheads = num / denom;
				xsectors = a1 / (c1 * xheads + h1);
				break;
			}
		}
		if (xheads != -1)	
			break;
	}

	if (xheads == -1) {
		if (F_flag)
			return;
		warnx("Cannot determine the number of heads");
		return;
	}

	if (xsectors == -1) {
		warnx("Cannot determine the number of sectors");
		return;
	}

	/* Estimate the number of cylinders. */
	xcylinders = disklabel.d_secperunit / xheads / xsectors;
	if (disklabel.d_secperunit > xcylinders * xheads * xsectors)
		xcylinders++;

	/*
	 * Now verify consistency with each of the partition table entries.
	 * Be willing to shove cylinders up a little bit to make things work,
	 * but translation mismatches are fatal.
	 */
	for (i = 0; i < MBR_PART_COUNT * 2; i++) {
		if (get_mapping(i, &c1, &h1, &s1, &a1) < 0)
			continue;
		if (c1 >= MAXCYL - 2)
			continue;
		if (xsectors * (c1 * xheads + h1) + s1 != a1)
			return;
	}


	/* Everything checks out.
	 * Reset the geometry to use for further calculations.
	 * But cylinders cannot be > 1024.
	 */
	if (xcylinders > MAXCYL)
		dos_cylinders = MAXCYL;
	else
		dos_cylinders = xcylinders;
	dos_heads = xheads;
	dos_sectors = xsectors;
}

/*
 * For the purposes of intuit_translated_geometry(), treat the partition
 * table as a list of eight mapping between (cylinder, head, sector)
 * triplets and absolute sectors.  Get the relevant geometry triplet and
 * absolute sectors for a given entry, or return -1 if it isn't present.
 * Note: for simplicity, the returned sector is 0-based.
 */
static int
get_mapping(int i, unsigned int *cylinder, unsigned int *head, unsigned int *sector,
    unsigned long *absolute)
{
	struct mbr_partition *part = &mboot.mbr_parts[i / 2];

	if (part->mbrp_type == 0)
		return -1;
	if (i % 2 == 0) {
		*cylinder = MBR_PCYL(part->mbrp_scyl, part->mbrp_ssect);
		*head = part->mbrp_shd;
		*sector = MBR_PSECT(part->mbrp_ssect);
		*absolute = le32toh(part->mbrp_start);
	} else {
		*cylinder = MBR_PCYL(part->mbrp_ecyl, part->mbrp_esect);
		*head = part->mbrp_ehd;
		*sector = MBR_PSECT(part->mbrp_esect);
		*absolute = le32toh(part->mbrp_start)
		    + le32toh(part->mbrp_size) - 1;
	}
	/* Sanity check the data against all zeroes */
	if ((*cylinder == 0) && (*sector == 0) && (*head == 0))
		return -1;
	/* sector numbers in the MBR partition table start at 1 */
	*sector = *sector - 1;
	/* Sanity check the data against max values */
	if ((((*cylinder * MAXHEAD) + *head) * MAXSECTOR + *sector) < *absolute)
		/* cannot be a CHS mapping */
		return -1;
	return 0;
}

static void
delete_ptn(int part)
{
	if (part == ext.ptn_id) {
		/* forget all about the extended partition */
		free(ext.ptn);
		memset(&ext, 0, sizeof ext);
	}

	mboot.mbr_parts[part].mbrp_type = 0;
}

static void
delete_ext_ptn(int part)
{

	if (part == 0) {
		ext.ptn[0].mbr_parts[0].mbrp_type = 0;
		return;
	}
	ext.ptn[part - 1].mbr_parts[1] = ext.ptn[part].mbr_parts[1];
	memmove(&ext.ptn[part], &ext.ptn[part + 1],
		(ext.num_ptn - part - 1) * sizeof ext.ptn[0]);
	ext.num_ptn--;
}

static int
add_ext_ptn(daddr_t start, daddr_t size)
{
	int part;
	struct mbr_partition *partp;
	struct mbr_sector *nptn;

	nptn = realloc(ext.ptn, (ext.num_ptn + 1) * sizeof *ext.ptn);
	if (!nptn)
		err(1, "realloc");
	ext.ptn = nptn;
	for (part = 0; part < ext.num_ptn; part++)
		if (ext_offset(part) > start)
			break;
	/* insert before 'part' - make space... */
	memmove(&ext.ptn[part + 1], &ext.ptn[part],
		(ext.num_ptn - part) * sizeof ext.ptn[0]);
	memset(&ext.ptn[part], 0, sizeof ext.ptn[0]);
	ext.ptn[part].mbr_magic = LE_MBR_MAGIC;
	/* we will be 'part' */
	if (part == 0) {
		/* link us to 'next' */
		partp = &ext.ptn[0].mbr_parts[1];
		/* offset will be fixed by caller */
		partp->mbrp_size = htole32(
		    le32toh(ext.ptn[1].mbr_parts[0].mbrp_start) +
		    le32toh(ext.ptn[1].mbr_parts[0].mbrp_size));
	} else {
		/* link us to prev's next */
		partp = &ext.ptn[part - 1].mbr_parts[1];
		ext.ptn[part].mbr_parts[1] = *partp;
		/* and prev onto us */
		partp->mbrp_start = htole32(start - ptn_0_offset - ext.base);
		partp->mbrp_size = htole32(size + ptn_0_offset);
	}
	partp->mbrp_type = 5;	/* as used by win98 */
	partp->mbrp_flag = 0;
	/* wallop in some CHS values - win98 doesn't saturate them */
	dos(le32toh(partp->mbrp_start),
	    &partp->mbrp_scyl, &partp->mbrp_shd, &partp->mbrp_ssect);
	dos(le32toh(partp->mbrp_start) + le32toh(partp->mbrp_size) - 1,
	    &partp->mbrp_ecyl, &partp->mbrp_ehd, &partp->mbrp_esect);
	ext.num_ptn++;

	return part;
}

static const char *
check_overlap(int part, int sysid, daddr_t start, daddr_t size, int fix)
{
	int p;
	unsigned int p_s, p_e;

	if (sysid != 0) {
		if (start == 0)
			return "Sector zero is reserved for the MBR";
#if 0
		if (start < ptn_0_offset)
			/* This is just a convention, not a requirement */
			return "Track zero is reserved for the BIOS";
#endif
		if (start + size > disksectors)
			return "Partition exceeds size of disk";
		for (p = 0; p < MBR_PART_COUNT; p++) {
			if (p == part || mboot.mbr_parts[p].mbrp_type == 0)
				continue;
			p_s = le32toh(mboot.mbr_parts[p].mbrp_start);
			p_e = p_s + le32toh(mboot.mbr_parts[p].mbrp_size);
			if (start + size <= p_s || start >= p_e)
				continue;
			if (f_flag) {
				if (fix)
					delete_ptn(p);
				return 0;
			}
			return "Overlaps another partition";
		}
	}

	/* Are we trying to create an extended partition */
	if (!MBR_IS_EXTENDED(mboot.mbr_parts[part].mbrp_type)) {
		/* this wasn't the extended partition */
		if (!MBR_IS_EXTENDED(sysid))
			return 0;
		/* making an extended partition */
		if (ext.base != 0) {
			if (!f_flag)
				return "There cannot be 2 extended partitions";
			if (fix)
				delete_ptn(ext.ptn_id);
		}
		if (fix) {
			/* allocate a new extended partition */
			ext.ptn = calloc(1, sizeof ext.ptn[0]);
			if (ext.ptn == NULL)
				err(1, "Malloc failed");
			ext.ptn[0].mbr_magic = LE_MBR_MAGIC;
			ext.ptn_id = part;
			ext.base = start;
			ext.limit = start + size;
			ext.num_ptn = 1;
		}
		return 0;
	}

	/* Check we haven't cut space allocated to an extended ptn */
	
	if (!MBR_IS_EXTENDED(sysid)) {
		/* no longer an extended partition */
		if (fix) {
			/* Kill all memory of the extended partitions */
			delete_ptn(part);
			return 0;
		}
		if (ext.num_ptn == 0 ||
		    (ext.num_ptn == 1 && ext.ptn[0].mbr_parts[0].mbrp_type == 0))
			/* nothing in extended partition */
			return 0;
		if (f_flag)
			return 0;
		if (yesno("Do you really want to delete all the extended partitions?"))
			return 0;
		return "Extended partition busy";
	}

	if (le32toh(mboot.mbr_parts[part].mbrp_start) != ext.base)
		/* maybe impossible, but an extra sanity check */
		return 0;

	for (p = ext.num_ptn; --p >= 0;) {
		if (ext.ptn[p].mbr_parts[0].mbrp_type == 0)
			continue;
		p_s = ext_offset(p);
		p_e = p_s + le32toh(ext.ptn[p].mbr_parts[0].mbrp_start)
			  + le32toh(ext.ptn[p].mbr_parts[0].mbrp_size);
		if (p_s >= start && p_e <= start + size)
			continue;
		if (!f_flag)
			return "Extended partition outside main partition";
		if (fix)
			delete_ext_ptn(p);
	}

	if (fix && start != ext.base) {
		/* The internal offsets need to be fixed up */
		for (p = 0; p < ext.num_ptn - 1; p++)
			ext.ptn[p].mbr_parts[1].mbrp_start = htole32(
			    le32toh(ext.ptn[p].mbr_parts[1].mbrp_start)
				    + ext.base - start);
		/* and maybe an empty partition at the start */
		if (ext.ptn[0].mbr_parts[0].mbrp_type == 0) {
			if (le32toh(ext.ptn[0].mbr_parts[1].mbrp_start) == 0) {
				/* don't need the empty slot */
				memmove(&ext.ptn[0], &ext.ptn[1],
					(ext.num_ptn - 1) * sizeof ext.ptn[0]);
				ext.num_ptn--;
			}
		} else {
			/* must create an empty slot */
			add_ext_ptn(start, ptn_0_offset);
			ext.ptn[0].mbr_parts[1].mbrp_start = htole32(ext.base
								- start);
		}
	}
	if (fix) {
		ext.base = start;
		ext.limit = start + size;
	}
	return 0;
}

static const char *
check_ext_overlap(int part, int sysid, daddr_t start, daddr_t size, int fix)
{
	int p;
	unsigned int p_s, p_e;

	if (sysid == 0)
		return 0;

	if (MBR_IS_EXTENDED(sysid))
		return "Nested extended partitions are not allowed";

	/* allow one track at start for extended partition header */
	start -= ptn_0_offset;
	size += ptn_0_offset;
	if (start < ext.base || start + size > ext.limit)
		return "Outside bounds of extended partition";

	if (f_flag && !fix)
		return 0;

	for (p = ext.num_ptn; --p >= 0;) {
		if (p == part || ext.ptn[p].mbr_parts[0].mbrp_type == 0)
			continue;
		p_s = ext_offset(p);
		p_e = p_s + le32toh(ext.ptn[p].mbr_parts[0].mbrp_start)
			+ le32toh(ext.ptn[p].mbr_parts[0].mbrp_size);
		if (p == 0)
			p_s += le32toh(ext.ptn[p].mbr_parts[0].mbrp_start)
							- ptn_0_offset;
		if (start < p_e && start + size > p_s) {
			if (!f_flag)
				return "Overlaps another extended partition";
			if (fix) {
				if (part == -1)
					delete_ext_ptn(p);
				else
					/* must not change numbering yet */
					ext.ptn[p].mbr_parts[0].mbrp_type = 0;
			}
		}
	}
	return 0;
}

static int
change_part(int extended, int part, int sysid, daddr_t start, daddr_t size,
	char *bootmenu)
{
	struct mbr_partition *partp;
	struct mbr_sector *boot;
	daddr_t offset;
	const char *e;
	int upart = part;
	int p;
	int fl;
	daddr_t n_s, n_e;
	const char *errtext;
#ifdef BOOTSEL
	char tmp_bootmenu[MBR_PART_COUNT * (MBR_BS_PARTNAMESIZE + 1)];
	int bootmenu_len = (extended ? MBR_PART_COUNT : 1) * (MBR_BS_PARTNAMESIZE + 1);
#endif

	if (extended) {
		if (part != -1 && part < ext.num_ptn) {
			boot = &ext.ptn[part];
			partp = &boot->mbr_parts[0];
			offset = ext_offset(part);
		} else {
			part = -1;
			boot = 0;
			partp = 0;
			offset = 0;
		}
		upart = 0;
		e = "E";
	} else {
		boot = &mboot;
		partp = &boot->mbr_parts[part];
		offset = 0;
		e = "";
	}

	if (!f_flag && part != -1) {
		printf("The data for partition %s%d is:\n", e, part);
		print_part(boot, upart, offset);
	}

#ifdef BOOTSEL
	if (bootmenu != NULL)
		strlcpy(tmp_bootmenu, bootmenu, bootmenu_len);
	else
		if (boot != NULL && boot->mbr_bootsel_magic == LE_MBR_BS_MAGIC)
			strlcpy(tmp_bootmenu,
				boot->mbr_bootsel.mbrbs_nametab[upart],
				bootmenu_len);
		else
			tmp_bootmenu[0] = 0;
#endif

	if (partp != NULL) {
		if (!s_flag) {
			/* values not specified, default to current ones */
			sysid = partp->mbrp_type;
			start = offset + le32toh(partp->mbrp_start);
			size = le32toh(partp->mbrp_size);
		} else {
			if (sysid == -1)
				sysid = partp->mbrp_type;
			if (start == (daddr_t)0xffffffff) {
				start = offset + le32toh(partp->mbrp_start);
				if (start == 0)
					start = offset = ptn_0_offset;
			}
			if (size == (daddr_t)0xffffffff) {
				size = le32toh(partp->mbrp_size);
				if (size == 0) 
					size = disksectors - start;
			}
		}
	}

	/* creating a new partition, default to free space */
	if (!s_flag && sysid == 0 && extended) {
		/* non-extended partition */
		start = ext.base;
		for (p = 0; p < ext.num_ptn; p++) {
			if (ext.ptn[p].mbr_parts[0].mbrp_type == 0)
				continue;
			n_s = ext_offset(p);
			if (n_s > start + ptn_0_offset)
				break;
			start = ext_offset(p)
				+ le32toh(ext.ptn[p].mbr_parts[0].mbrp_start)
				+ le32toh(ext.ptn[p].mbr_parts[0].mbrp_size);
		}
		if (ext.limit - start <= ptn_0_offset) {
			printf("No space in extended partition\n");
			return 0;
		}
		start += ptn_0_offset;
	}

	if (!s_flag && sysid == 0 && !extended) {
		/* same for non-extended partition */
		/* first see if old start is free */
		if (start < ptn_0_offset)
			start = 0;
		for (p = 0; start != 0 && p < MBR_PART_COUNT; p++) {
			if (mboot.mbr_parts[p].mbrp_type == 0)
				continue;
			n_s = le32toh(mboot.mbr_parts[p].mbrp_start);
			if (start >= n_s &&
			    start < n_s + le32toh(mboot.mbr_parts[p].mbrp_size))
				start = 0;
		}
		if (start == 0) {
			/* Look for first gap */
			start = ptn_0_offset;
			for (p = 0; p < MBR_PART_COUNT; p++) {
				if (mboot.mbr_parts[p].mbrp_type == 0)
					continue;
				n_s = le32toh(mboot.mbr_parts[p].mbrp_start);
				n_e = n_s + le32toh(mboot.mbr_parts[p].mbrp_size);
				if (start >= n_s && start < n_e) {
					start = n_e;
					p = -1;
				}
			}
			if (start >= disksectors && !I_flag) {
				printf("No free space\n");
				return 0;
			}
		}
	}

	if (!f_flag) {
		/* request new values from user */
		if (sysid == 0)
			sysid = 169;
		sysid = decimal("sysid", sysid, 0, 0, 255);
		if (sysid == 0 && !v_flag) {
			start = 0;
			size = 0;
#ifdef BOOTSEL
			tmp_bootmenu[0] = 0;
#endif
		} else {
			daddr_t old = start;
			daddr_t lim = extended ? ext.limit : disksectors;
			start = decimal("start", start,
				DEC_SEC | DEC_RND_0 | (extended ? DEC_RND : 0),
				extended ? ext.base : 0, lim);
			/* Adjust 'size' so that end doesn't move when 'start'
			 * is only changed slightly.
			 */
			if (size > start - old)
				size -= start - old;
			else
				size = 0;
			/* Find end of available space from this start point */
			if (extended) {
				for (p = 0; p < ext.num_ptn; p++) {
					if (p == part)
						continue;
					if (ext.ptn[p].mbr_parts[0].mbrp_type == 0)
						continue;
					n_s = ext_offset(p);
					if (n_s > start && n_s < lim)
						lim = n_s;
					if (start >= n_s && start < n_s
				+ le32toh(ext.ptn[p].mbr_parts[0].mbrp_start)
				+ le32toh(ext.ptn[p].mbr_parts[0].mbrp_size)) {
						lim = start;
						break;
					}
				}
			} else {
				for (p = 0; p < MBR_PART_COUNT; p++) {
					if (p == part)
						continue;
					if (mboot.mbr_parts[p].mbrp_type == 0)
						continue;
					n_s = le32toh(mboot.mbr_parts[p].mbrp_start);
					if (n_s > start && n_s < lim)
						lim = n_s;
					if (start >= n_s && start < n_s
				    + le32toh(mboot.mbr_parts[p].mbrp_size)) {
						lim = start;
						break;
					}
				}
			}
			lim -= start;
			if (lim == 0) {
				printf("Start sector already allocated\n");
				return 0;
			}
			if (size == 0 || size > lim)
				size = lim;
			fl = DEC_SEC;
			if (start % ptn_alignment == ptn_0_offset)
				fl |= DEC_RND_DOWN;
			if (start == 2 * ptn_0_offset)
				fl |= DEC_RND_DOWN | DEC_RND_DOWN_2;
			size = decimal("size", size, fl, 0, lim);
#ifdef BOOTSEL
#ifndef DEFAULT_BOOTEXTCODE
			if (!extended)
#endif
				string("bootmenu", bootmenu_len, tmp_bootmenu);
#endif
		}
	}

	/*
	 * Before we write these away, we must verify that nothing
	 * untoward has been requested.
	 */

	if (extended)
		errtext = check_ext_overlap(part, sysid, start, size, 0);
	else
		errtext = check_overlap(part, sysid, start, size, 0);
	if (errtext != NULL && !I_flag) {
		if (f_flag)
			errx(2, "%s", errtext);
		printf("%s\n", errtext);
		return 0;
	}

	/*
	 * Before proceeding, delete any overlapped partitions.
	 * This can only happen if '-f' was supplied on the command line.
	 * Just hope the caller knows what they are doing.
	 * This also fixes the base of each extended partition if the
	 * partition itself has moved.
	 */
	if (!I_flag) {
		if (extended)
			errtext = check_ext_overlap(part, sysid, start, size, 1);
		else
			errtext = check_overlap(part, sysid, start, size, 1);
		if (errtext)
			errx(1, "%s", errtext);
	}


	if (sysid == 0) {
		/* delete this partition - save info though */
		if (partp == NULL)
			/* must have been trying to create an extended ptn */
			return 0;
		if (start == 0 && size == 0)
			memset(partp, 0, sizeof *partp);
#ifdef BOOTSEL
		if (boot->mbr_bootsel_magic == LE_MBR_BS_MAGIC)
			memset(boot->mbr_bootsel.mbrbs_nametab[upart], 0,
				sizeof boot->mbr_bootsel.mbrbs_nametab[0]);
#endif
		if (extended)
			delete_ext_ptn(part);
		else
			delete_ptn(part);
		return 1;
	}


	if (extended) {
		if (part != -1)
			delete_ext_ptn(part);
		if (start == ext.base + ptn_0_offset)
			/* First one must have been free */
			part = 0;
		else
			part = add_ext_ptn(start, size);

		/* These must be re-calculated because of the realloc */
		boot = &ext.ptn[part];
		partp = &boot->mbr_parts[0];
		offset = ext_offset(part);
	}

	partp->mbrp_type = sysid;
	partp->mbrp_start = htole32( start - offset);
	partp->mbrp_size = htole32( size);
	dos(start, &partp->mbrp_scyl, &partp->mbrp_shd, &partp->mbrp_ssect);
	dos(start + size - 1,
		    &partp->mbrp_ecyl, &partp->mbrp_ehd, &partp->mbrp_esect);
#ifdef BOOTSEL
	if (extended) {
		boot->mbr_bootsel_magic = LE_MBR_BS_MAGIC;
		strncpy(boot->mbr_bootsel.mbrbs_nametab[upart], tmp_bootmenu,
			bootmenu_len);
	} else {
		/* We need to bootselect code installed in order to have
		 * somewhere to safely write the menu tag.
		 */
		if (boot->mbr_bootsel_magic != LE_MBR_BS_MAGIC) {
			if (f_flag ||
			    yesno("The bootselect code is not installed, "
				"do you want to install it now?"))
				install_bootsel(MBR_BS_ACTIVE);
		}
		if (boot->mbr_bootsel_magic == LE_MBR_BS_MAGIC) {
			strncpy(boot->mbr_bootsel.mbrbs_nametab[upart],
				tmp_bootmenu, bootmenu_len);
		}
	}
#endif

	if (v_flag && !f_flag && yesno("Explicitly specify beg/end address?")) {
		/* this really isn't a good idea.... */
		int tsector, tcylinder, thead;

		tcylinder = MBR_PCYL(partp->mbrp_scyl, partp->mbrp_ssect);
		thead = partp->mbrp_shd;
		tsector = MBR_PSECT(partp->mbrp_ssect);
		tcylinder = decimal("beginning cylinder",
				tcylinder, 0, 0, dos_cylinders - 1);
		thead = decimal("beginning head",
				thead, 0, 0, dos_heads - 1);
		tsector = decimal("beginning sector",
				tsector, 0, 1, dos_sectors);
		partp->mbrp_scyl = DOSCYL(tcylinder);
		partp->mbrp_shd = thead;
		partp->mbrp_ssect = DOSSECT(tsector, tcylinder);

		tcylinder = MBR_PCYL(partp->mbrp_ecyl, partp->mbrp_esect);
		thead = partp->mbrp_ehd;
		tsector = MBR_PSECT(partp->mbrp_esect);
		tcylinder = decimal("ending cylinder",
				tcylinder, 0, 0, dos_cylinders - 1);
		thead = decimal("ending head",
				thead, 0, 0, dos_heads - 1);
		tsector = decimal("ending sector",
				tsector, 0, 1, dos_sectors);
		partp->mbrp_ecyl = DOSCYL(tcylinder);
		partp->mbrp_ehd = thead;
		partp->mbrp_esect = DOSSECT(tsector, tcylinder);
	}

	/* If we had to mark an extended partition as deleted because
	 * another request would have overlapped it, now is the time
	 * to do the actual delete.
	 */
	if (extended && f_flag) {
		for (p = ext.num_ptn; --p >= 0;)
			if (ext.ptn[p].mbr_parts[0].mbrp_type == 0)
				delete_ext_ptn(p);
	}
	return 1;
}

static void
print_geometry(void)
{

	if (sh_flag) {
		printf("DISK=%s\n", disk);
		printf("DLCYL=%d\nDLHEAD=%d\nDLSEC=%d\nDLSIZE=%"PRIdaddr"\n",
			cylinders, heads, sectors, disksectors);
		printf("BCYL=%d\nBHEAD=%d\nBSEC=%d\nBDLSIZE=%"PRIdaddr"\n",
			dos_cylinders, dos_heads, dos_sectors, dos_disksectors);
		printf("NUMEXTPTN=%d\n", ext.num_ptn);
		return;
	}

	/* Not sh_flag */
	printf("Disk: %s\n", disk);
	printf("NetBSD disklabel disk geometry:\n");
	printf("cylinders: %d, heads: %d, sectors/track: %d "
	    "(%d sectors/cylinder)\ntotal sectors: %"PRIdaddr", "
	    "bytes/sector: %zd\n\n", cylinders, heads, sectors,
	    cylindersectors, disksectors, secsize);
	printf("BIOS disk geometry:\n");
	printf("cylinders: %d, heads: %d, sectors/track: %d "
	    "(%d sectors/cylinder)\ntotal sectors: %"PRIdaddr"\n\n",
	    dos_cylinders, dos_heads, dos_sectors, dos_cylindersectors,
	    dos_disksectors);
	printf("Partitions aligned to %d sector boundaries, offset %d\n\n",
	    ptn_alignment, ptn_0_offset);
}

/* Find the first active partition, else return MBR_PART_COUNT */
static int
first_active(void)
{
	struct mbr_partition *partp = &mboot.mbr_parts[0];
	int part;

	for (part = 0; part < MBR_PART_COUNT; part++)
		if (partp[part].mbrp_flag & MBR_PFLAG_ACTIVE)
			return part;
	return MBR_PART_COUNT;
}

static void
change_active(int which)
{
	struct mbr_partition *partp;
	int part;
	int active = MBR_PART_COUNT;

	partp = &mboot.mbr_parts[0];

	if (a_flag && which != -1)
		active = which;
	else
		active = first_active();
	if (!f_flag) {
		if (yesno("Do you want to change the active partition?")) {
			printf ("Choosing %d will make no partition active.\n",
			    MBR_PART_COUNT);
			do {
				active = decimal("active partition",
						active, 0, 0, MBR_PART_COUNT);
			} while (!yesno("Are you happy with this choice?"));
		} else
			return;
	} else
		if (active != MBR_PART_COUNT)
			printf ("Making partition %d active.\n", active);

	for (part = 0; part < MBR_PART_COUNT; part++)
		partp[part].mbrp_flag &= ~MBR_PFLAG_ACTIVE;
	if (active < MBR_PART_COUNT)
		partp[active].mbrp_flag |= MBR_PFLAG_ACTIVE;
}

static void
change_bios_geometry(void)
{
	print_geometry();
	if (!yesno("Do you want to change our idea of what BIOS thinks?"))
		return;

#if defined(USE_DISKLIST)
	if (dl != NULL) {
		struct biosdisk_info *bip;
		int i;

		for (i = 0; i < dl->dl_nbiosdisks; i++) {
			if (i == 0)
				printf("\nGeometries of known disks:\n");
			bip = &dl->dl_biosdisks[i];
			printf("Disk %d: cylinders %u, heads %u, sectors %u"
				" (%"PRIdaddr" sectors, %dMB)\n",
			    i, bip->bi_cyl, bip->bi_head, bip->bi_sec,
			    bip->bi_lbasecs, SEC_TO_MB(bip->bi_lbasecs));
				
		}
		printf("\n");
	}
#endif
	do {
		dos_cylinders = decimal("BIOS's idea of #cylinders",
					dos_cylinders, 0, 0, MAXCYL);
		dos_heads = decimal("BIOS's idea of #heads",
					dos_heads, 0, 0, MAXHEAD);
		dos_sectors = decimal("BIOS's idea of #sectors",
					dos_sectors, 0, 1, MAXSECTOR);
		print_geometry();
	} while (!yesno("Are you happy with this choice?"));
}


/***********************************************\
* Change real numbers into strange dos numbers	*
\***********************************************/
static void
dos(int sector, unsigned char *cylinderp, unsigned char *headp,
    unsigned char *sectorp)
{
	int cylinder, head;

	cylinder = sector / dos_cylindersectors;
	sector -= cylinder * dos_cylindersectors;

	head = sector / dos_sectors;
	sector -= head * dos_sectors;
	if (cylinder > 1023)
		cylinder = 1023;

	*cylinderp = DOSCYL(cylinder);
	*headp = head;
	*sectorp = DOSSECT(sector + 1, cylinder);
}

static int
open_disk(int update)
{
	static char namebuf[MAXPATHLEN + 1];
	int flags = update && disk_file == NULL ? O_RDWR : O_RDONLY;

	if (!F_flag) {
		fd = opendisk(disk, flags, namebuf, sizeof(namebuf), 0);
		if (fd < 0) {
			if (errno == ENODEV)
				warnx("%s is not a character device", namebuf);
			else
				warn("cannot opendisk %s", namebuf);
			return (-1);
		}
		disk = namebuf;
	} else {
		fd = open(disk, flags, 0);
		if (fd == -1) {
			warn("cannot open %s", disk);
			return -1;
		}
	}

	if (get_params() == -1) {
		close(fd);
		fd = -1;
		return (-1);
	}
	if (disk_file != NULL) {
		/* for testing: read/write data from a disk file */
		wfd = open(disk_file, update ? O_RDWR|O_CREAT : O_RDONLY, 0777);
		if (wfd == -1) {
			warn("%s", disk_file);
			close(fd);
			fd = -1;
			return -1;
		}
	} else
		wfd = fd;
	return (0);
}

static ssize_t
read_disk(daddr_t sector, void *buf)
{
	ssize_t nr;

	if (*rfd == -1)
		errx(1, "read_disk(); fd == -1");

	off_t offs = sector * (off_t)secsize;
	off_t mod = offs & (secsize - 1);
	off_t rnd = offs & ~(secsize - 1);

	if (lseek(*rfd, rnd, SEEK_SET) == (off_t)-1)
		return -1;

	if (secsize == 512)
		return read(*rfd, buf, 512);

	if ((nr = read(*rfd, iobuf, secsize)) != secsize)
		return nr;

	memcpy(buf, &iobuf[mod], 512);

	return 512;
}	

static ssize_t
write_disk(daddr_t sector, void *buf)
{
	ssize_t nr;

	if (wfd == -1)
		errx(1, "write_disk(); wfd == -1");

	off_t offs = sector * (off_t)secsize;
	off_t mod = offs & (secsize - 1);
	off_t rnd = offs & ~(secsize - 1);

	if (lseek(wfd, rnd, SEEK_SET) == (off_t)-1)
		return -1;

	if (secsize == 512)
		return write(wfd, buf, 512);

	if ((nr = read(wfd, iobuf, secsize)) != secsize)
		return nr;

	if (lseek(wfd, rnd, SEEK_SET) == (off_t)-1)
		return -1;

	memcpy(&iobuf[mod], buf, 512);

	if ((nr = write(wfd, iobuf, secsize)) != secsize)
		return nr;

	return 512;
}

static void
guess_geometry(daddr_t _sectors)
{
	dos_sectors = MAXSECTOR;
	dos_heads = MAXHEAD - 1;	/* some BIOS might use 256 */
	dos_cylinders = _sectors / (MAXSECTOR * (MAXHEAD - 1));
	if (dos_cylinders < 1)
		dos_cylinders = 1;
	else if (dos_cylinders > MAXCYL - 1)
		dos_cylinders = MAXCYL - 1;
}

static int
get_params(void)
{
	if (disk_type != NULL) {
		struct disklabel *tmplabel;

		if ((tmplabel = getdiskbyname(disk_type)) == NULL) {
			warn("bad disktype");
			return (-1);
		}
		disklabel = *tmplabel;
	} else if (F_flag) {
		struct stat st;
		if (fstat(fd, &st) == -1) {
			warn("fstat");
			return (-1);
		}
		if (st.st_size % 512 != 0) {
			warnx("%s size (%lld) is not divisible "
			    "by sector size (%d)", disk, (long long)st.st_size,
			    512);
		}
		disklabel.d_secperunit = st.st_size / 512;
		guess_geometry(disklabel.d_secperunit);
		disklabel.d_ncylinders = dos_cylinders;
		disklabel.d_ntracks = dos_heads;
		disklabel.d_secsize = 512;
		disklabel.d_nsectors = dos_sectors;
	}
#if !HAVE_NBTOOL_CONFIG_H
	else if (ioctl(fd, DIOCGDEFLABEL, &disklabel) == -1) {
		warn("DIOCGDEFLABEL");
		if (ioctl(fd, DIOCGDINFO, &disklabel) == -1) {
			warn("DIOCGDINFO");
			return (-1);
		}
	}
#endif

	disksectors = disklabel.d_secperunit;
	cylinders = disklabel.d_ncylinders;
	heads = disklabel.d_ntracks;
	secsize = disklabel.d_secsize;
	sectors = disklabel.d_nsectors;

	/* pick up some defaults for the BIOS sizes */
	if (sectors <= MAXSECTOR) {
		dos_cylinders = cylinders;
		dos_heads = heads;
		dos_sectors = sectors;
	} else {
		/* guess - has to better than the above */
		guess_geometry(disksectors);
	}
	dos_disksectors = disksectors;

	return (0);
}

#ifdef BOOTSEL
/*
 * Rather unfortunately the bootsel 'magic' number is at the end of the
 * the structure, and there is no checksum.  So when other operating
 * systems install mbr code by only writing the length of their code they
 * can overwrite part of the structure but keeping the magic number intact.
 * This code attempts to empirically detect this problem.
 */
static int
validate_bootsel(struct mbr_bootsel *mbs)
{
	unsigned int key = mbs->mbrbs_defkey;
	unsigned int tmo;
	size_t i, j;

	if (v_flag)
		return 0;

	/*
	 * Check default key is sane
	 * - this is the most likely field to be stuffed
	 * 16 disks and 16 bootable partitions seems enough!
	 * (the keymap decode starts falling apart at that point)
	 */
	if (mbs->mbrbs_flags & MBR_BS_ASCII) {
		if (key != 0 && !(key == '\r'
		    || (key >= '1' && key < '1' + MAX_BIOS_DISKS)
		    || (key >= 'a' && key < 'a' + MAX_BIOS_DISKS)))
			return 1;
	} else {
		if (key != 0 && !(key == SCAN_ENTER
		    || (key >= SCAN_1 && key < SCAN_1 + MAX_BIOS_DISKS)
		    || (key >= SCAN_F1 && key < SCAN_F1 + MAX_BIOS_DISKS)))
			return 1;
	}

	/* Checking the flags will lead to breakage... */

	/* Timeout value is expected to be a multiple of a second */
	tmo = htole16(mbs->mbrbs_timeo);
	if (tmo != 0 && tmo != 0xffff && tmo != (10 * tmo + 9) / 182 * 182 / 10)
		return 2;

	/* Check the menu strings are printable */
	/* Unfortunately they aren't zero filled... */
	for (j = 0; j < __arraycount(mbs->mbrbs_nametab); ++j)
	for (i = 0; i < sizeof(mbs->mbrbs_nametab[j]); i++) {
		int c = (uint8_t)mbs->mbrbs_nametab[j][i];
		if (c == 0 || isprint(c))
			continue;
		return 3;
	}

	return 0;
}
#endif

static int
read_s0(daddr_t offset, struct mbr_sector *boot)
{
	const char *tabletype = offset ? "extended" : "primary";
#ifdef BOOTSEL
	static int reported;
#endif

	if (read_disk(offset, boot) == -1) {
		warn("Can't read %s partition table", tabletype);
		return -1;
	}
	if (boot->mbr_magic != LE_MBR_MAGIC) {
		if (F_flag && boot->mbr_magic == 0)
			return -1;
		warnx("%s partition table invalid, "
		    "no magic in sector %"PRIdaddr, tabletype, offset);
		return -1;

	}
#ifdef BOOTSEL
	if (boot->mbr_bootsel_magic == LE_MBR_BS_MAGIC) {
		/* mbr_bootsel in new location */
		if (validate_bootsel(&boot->mbr_bootsel)) {
			warnx("removing corrupt bootsel information");
			boot->mbr_bootsel_magic = 0;
		}
		return 0;
	}
	if (boot->mbr_bootsel_magic != LE_MBR_MAGIC)
		return 0;

	/* mbr_bootsel in old location */
	if (!reported)
		warnx("%s partition table: using old-style bootsel information",
		    tabletype);
	reported = 1;
	if (validate_bootsel((void *)((uint8_t *)boot + MBR_BS_OFFSET + 4))) {
		warnx("%s bootsel information corrupt - ignoring", tabletype);
		return 0;
	}
	memmove((uint8_t *)boot + MBR_BS_OFFSET,
		(uint8_t *)boot + MBR_BS_OFFSET + 4,
		sizeof(struct mbr_bootsel));
	if ( ! (boot->mbr_bootsel.mbrbs_flags & MBR_BS_NEWMBR)) {
			/* old style default key */
		int id;
			/* F1..F4 => ptn 0..3, F5+ => disk 0+ */
		id = boot->mbr_bootsel.mbrbs_defkey;
		id -= SCAN_F1;
		if (id >= MBR_PART_COUNT)
			id -= MBR_PART_COUNT; /* Use number of disk */
		else if (mboot.mbr_parts[id].mbrp_type != 0)
			id = le32toh(boot->mbr_parts[id].mbrp_start);
		else
			id = DEFAULT_ACTIVE;
		boot->mbr_bootsel.mbrbs_defkey = id;
	}
	boot->mbr_bootsel_magic = LE_MBR_BS_MAGIC;
		/* highlight that new bootsel code is necessary */
	boot->mbr_bootsel.mbrbs_flags &= ~MBR_BS_NEWMBR;
#endif /* BOOTSEL */
	return 0;
}

static int
write_mbr(void)
{
	int flag, i;
	daddr_t offset;
	int rval = -1;

	/*
	 * write enable label sector before write (if necessary),
	 * disable after writing.
	 * needed if the disklabel protected area also protects
	 * sector 0. (e.g. empty disk)
	 */
	flag = 1;
#if !HAVE_NBTOOL_CONFIG_H
	if (wfd == fd && F_flag == 0 && ioctl(wfd, DIOCWLABEL, &flag) < 0)
		warn("DIOCWLABEL");
#endif
	if (write_disk(0, &mboot) == -1) {
		warn("Can't write fdisk partition table");
		goto protect_label;
	}
	if (boot_installed)
		for (i = bootsize; (i -= 0x200) > 0;)
			if (write_disk(i / 0x200, &bootcode[i / 0x200]) == -1) {
				warn("Can't write bootcode");
				goto protect_label;
			}
	for (offset = 0, i = 0; i < ext.num_ptn; i++) {
		if (write_disk(ext.base + offset, ext.ptn + i) == -1) {
			warn("Can't write %dth extended partition", i);
			goto protect_label;
		}
		offset = le32toh(ext.ptn[i].mbr_parts[1].mbrp_start);
	}
	rval = 0;
    protect_label:
	flag = 0;
#if !HAVE_NBTOOL_CONFIG_H
	if (wfd == fd && F_flag == 0 && ioctl(wfd, DIOCWLABEL, &flag) < 0)
		warn("DIOCWLABEL");
#endif
	return rval;
}

static int
yesno(const char *str, ...)
{
	int ch, first;
	va_list ap;

	va_start(ap, str);
	vprintf(str, ap);
	va_end(ap);
	printf(" [n] ");

	first = ch = getchar();
	while (ch != '\n' && ch != EOF)
		ch = getchar();
	if (ch == EOF)
		errx(1, "EOF");
	return (first == 'y' || first == 'Y');
}

static int64_t
decimal(const char *prompt, int64_t dflt, int flags, int64_t minval, int64_t maxval)
{
	int64_t acc = 0;
	int valid;
	int len;
	char *cp;

	for (;;) {
		if (flags & DEC_SEC) {
			printf("%s: [%" PRId64 "..%" PRId64 "cyl default: %" PRId64 ", %" PRId64 "cyl, %uMB] ",
			    prompt, SEC_TO_CYL(minval), SEC_TO_CYL(maxval),
			    dflt, SEC_TO_CYL(dflt), SEC_TO_MB(dflt));
		} else
			printf("%s: [%" PRId64 "..%" PRId64 " default: %" PRId64 "] ",
			    prompt, minval, maxval, dflt);

		if (!fgets(lbuf, LBUF, stdin))
			errx(1, "EOF");
		cp = lbuf;

		cp += strspn(cp, " \t");
		if (*cp == '\n')
			return dflt;

		if (cp[0] == '$' && cp[1] == '\n')
			return maxval;

		if (isdigit((unsigned char)*cp) || *cp == '-') {
			acc = strtoll(lbuf, &cp, 10);
			len = strcspn(cp, " \t\n");
			valid = 0;
			if (len != 0 && (flags & DEC_SEC)) {
				if (!strncasecmp(cp, "gb", len)) {
					acc *= 1024;
					valid = 1;
				}
				if (valid || !strncasecmp(cp, "mb", len)) {
					acc *= SEC_IN_1M;
					/* round to whole number of cylinders */
					acc += ptn_alignment / 2;
					acc /= ptn_alignment;
					valid = 1;
				}
				if (valid || !strncasecmp(cp, "cyl", len)) {
					acc *= ptn_alignment;
					/* adjustments for cylinder boundary */
					if (acc == 0 && flags & DEC_RND_0)
						acc += ptn_0_offset;
					if (flags & DEC_RND)
						acc += ptn_0_offset;
					if (flags & DEC_RND_DOWN)
						acc -= ptn_0_offset;
					if (flags & DEC_RND_DOWN_2)
						acc -= ptn_0_offset;
					cp += len;
				}
			}
		}

		cp += strspn(cp, " \t");
		if (*cp != '\n') {
			lbuf[strlen(lbuf) - 1] = 0;
			printf("%s is not a valid %s number.\n", lbuf,
			    flags & DEC_SEC ? "sector" : "decimal");
			continue;
		}

		if (acc >= minval && acc <= maxval)
			return acc;
		printf("%" PRId64 " is not between %" PRId64 " and %" PRId64 ".\n", acc, minval, maxval);
	}
}

static int
ptn_id(const char *prompt, int *extended)
{
	unsigned int acc = 0;
	char *cp;

	for (;; printf("%s is not a valid partition number.\n", lbuf)) {
		printf("%s: [none] ", prompt);

		if (!fgets(lbuf, LBUF, stdin))
			errx(1, "EOF");
		lbuf[strlen(lbuf)-1] = '\0';
		cp = lbuf;

		cp += strspn(cp, " \t");
		*extended = 0;
		if (*cp == 0)
			return -1;

		if (*cp == 'E' || *cp == 'e') {
			cp++;
			*extended = 1;
		}

		acc = strtoul(cp, &cp, 10);

		cp += strspn(cp, " \t");
		if (*cp != '\0')
			continue;

		if (*extended || acc < MBR_PART_COUNT)
			return acc;
	}
}

#ifdef BOOTSEL
static void
string(const char *prompt, int length, char *buf)
{
	int len;

	for (;;) {
		printf("%s: [%.*s] (space to clear)", prompt, length, buf);

		if (!fgets(lbuf, LBUF, stdin))
			errx(1, "EOF");
		len = strlen(lbuf);
		if (len <= 1)
			/* unchanged if just <enter> */
			return;
		/* now strip trailing spaces, <space><enter> deletes string */
		do
			lbuf[--len] = 0;
		while (len != 0 && lbuf[len - 1] == ' ');
		if (len < length)
			break;
		printf("'%s' is longer than %d characters.\n",
		    lbuf, length - 1);
	}
	strncpy(buf, lbuf, length);
}
#endif

static int
type_match(const void *key, const void *item)
{
	const int *idp = key;
	const struct mbr_ptype *ptr = item;

	if (*idp < ptr->id)
		return (-1);
	if (*idp > ptr->id)
		return (1);
	return (0);
}

static const char *
get_type(int type)
{
	struct mbr_ptype *ptr;

	ptr = bsearch(&type, mbr_ptypes, KNOWN_SYSIDS,
	    sizeof(mbr_ptypes[0]), type_match);
	if (ptr == 0)
		return ("unknown");
	return (ptr->name);
}

static int
read_gpt(daddr_t offset, struct gpt_hdr *gptp)
{
	char buf[512];
	struct gpt_hdr *hdr = (void *)buf;
	const char *tabletype = GPT_TYPE(offset);

	if (read_disk(offset, buf) == -1) {
		warn("Can't read %s GPT header", tabletype);
		return -1;
	}
	(void)memcpy(gptp, buf, GPT_HDR_SIZE);

	/* GPT CRC should be calculated with CRC field preset to zero */
	hdr->hdr_crc_self = 0;

	if (memcmp(gptp->hdr_sig, GPT_HDR_SIG, sizeof(gptp->hdr_sig))
	    || gptp->hdr_lba_self != (uint64_t)offset
	    || crc32(0, (void *)hdr, gptp->hdr_size) != gptp->hdr_crc_self) {
		/* not a GPT */
		(void)memset(gptp, 0, GPT_HDR_SIZE);
	}

	if (v_flag && gptp->hdr_size != 0) {
		printf("Found %s GPT header CRC %"PRIu32" "
		    "at sector %"PRIdaddr", backup at %"PRIdaddr"\n",
		    tabletype, gptp->hdr_crc_self, offset, gptp->hdr_lba_alt);
	}
	return gptp->hdr_size;

}

static int
delete_gpt(struct gpt_hdr *gptp)
{
	char buf[512];
	struct gpt_hdr *hdr = (void *)buf;

	if (gptp->hdr_size == 0)
		return 0;

	/* don't accidently overwrite something important */
	if (gptp->hdr_lba_self != GPT_HDR_BLKNO &&
	    gptp->hdr_lba_self != (uint64_t)disksectors - 1) {
		warnx("given GPT header location doesn't seem correct");
		return -1;
	}

	(void)memcpy(buf, gptp, GPT_HDR_SIZE);
	/*
	 * Don't really delete GPT, just "disable" it, so it can
	 * be recovered later in case of mistake or something
	 */
	(void)memset(hdr->hdr_sig, 0, sizeof(gptp->hdr_sig));
	if (write_disk(gptp->hdr_lba_self, hdr) == -1) {
		warn("can't delete %s GPT header",
		    GPT_TYPE(gptp->hdr_lba_self));
		return -1;
	}
	(void)memset(gptp, 0, GPT_HDR_SIZE);
	return 1;
}
