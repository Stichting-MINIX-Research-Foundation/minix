/*	format 1.1 - format PC floppy disk		Author: Kees J. Bot
 *								5 Mar 1994
 */
#define nil 0
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <machine/diskparm.h>
#include <minix/minlib.h>

/* Constants. */
#define SECTOR_SIZE	512
#define NR_HEADS	  2
#define MAX_SECTORS	 18	/* 1.44Mb is the largest. */

/* Name error in <ibm/diskparm.h>, left over from the days floppies were
 * single sided.
 */
#define sectors_per_track	sectors_per_cylinder

/* From floppy device number to drive/type/format-bit and back.  See fd(4). */
#define isfloppy(dev)		(((dev) & 0xFF00) == 0x0200)
#define fl_drive(dev)		(((dev) & 0x0003) >> 0)
#define fl_type(dev)		(((dev) & 0x007C) >> 2)
#define fl_format(dev)		(((dev) & 0x0080) >> 7)
#define fl_makedev(drive, type, fmt)	\
	((dev_t) (0x0200 | ((fmt) << 7) | ((type) << 2) | ((drive) << 0)))

/* Recognize floppy types. */
#define NR_TYPES		7	/* # non-auto types */
#define isflauto(type)		((type) == 0)
#define isfltyped(type)		((unsigned) ((type) - 1) < NR_TYPES)
#define isflpart(type)		((unsigned) (type) >= 28)

/* Formatting parameters per type.  (Most of these parameters have no use
 * for formatting, disk_parameter_s probably matches a BIOS parameter table.)
 */
typedef struct disk_parameter_s	fmt_params_t;

typedef struct type_parameters {
	unsigned		media_size;
	unsigned		drive_size;
	fmt_params_t		fmt_params;
} type_parameters_t;

#define DC	0	/* Don't care. */

type_parameters_t parameters[NR_TYPES] = {
	/* mediasize       s1     off   sec/cyl  dlen       fill    start */
	/*       drivesize     s2   sizecode  gap    fmtgap     settle    */
  /* pc */ {  360,  360, { DC, DC, DC, 2,  9, DC, DC, 0x50, 0xF6, DC, DC }},
  /* at */ { 1200, 1200, { DC, DC, DC, 2, 15, DC, DC, 0x54, 0xF6, DC, DC }},
  /* qd */ {  360,  720, { DC, DC, DC, 2,  9, DC, DC, 0x50, 0xF6, DC, DC }},
  /* ps */ {  720,  720, { DC, DC, DC, 2,  9, DC, DC, 0x50, 0xF6, DC, DC }},
  /* pat */{  360, 1200, { DC, DC, DC, 2,  9, DC, DC, 0x50, 0xF6, DC, DC }},
  /* qh */ {  720, 1200, { DC, DC, DC, 2,  9, DC, DC, 0x50, 0xF6, DC, DC }},
  /* PS */ { 1440, 1440, { DC, DC, DC, 2, 18, DC, DC, 0x54, 0xF6, DC, DC }},
};

/* Per sector ID to be sent to the controller by the driver. */
typedef struct sector_id {
	unsigned char	cyl;
	unsigned char	head;
	unsigned char	sector;
	unsigned char	sector_size_code;
} sector_id_t;

/* Data to be "written" to the driver to format a track.  (lseek to the track
 * first.)  The first sector contains sector ID's, the second format params.
 */

typedef struct track_data {
	sector_id_t	sec_ids[SECTOR_SIZE / sizeof(sector_id_t)];
	fmt_params_t	fmt_params;
	char		padding[SECTOR_SIZE - sizeof(fmt_params_t)];
} track_data_t;

void report(const char *label)
{
	fprintf(stderr, "format: %s: %s\n", label, strerror(errno));
}

void fatal(const char *label)
{
	report(label);
	exit(1);
}

void format_track(int ffd, unsigned type, unsigned cyl, unsigned head)
/* Format a single track on a floppy. */
{
	type_parameters_t *tparams= &parameters[type - 1];
	track_data_t track_data;
	off_t track_pos;
	unsigned sector;
	unsigned nr_sectors= tparams->fmt_params.sectors_per_track;
	sector_id_t *sid;

	memset(&track_data, 0, sizeof(track_data));

	/* Set the sector id's.  (Note that sectors count from 1.) */
	for (sector= 0; sector <= nr_sectors; sector++) {
		sid= &track_data.sec_ids[sector];

		sid->cyl= cyl;
		sid->head= head;
		sid->sector= sector + 1;
		sid->sector_size_code= tparams->fmt_params.sector_size_code;
	}

	/* Format parameters. */
	track_data.fmt_params= tparams->fmt_params;

	/* Seek to the right track. */
	track_pos= (off_t) (cyl * NR_HEADS + head) * nr_sectors * SECTOR_SIZE;
	if (lseek(ffd, track_pos, SEEK_SET) == -1) {
		fprintf(stderr,
		"format: seeking to cyl %u, head %u (pos %d) failed: %s\n",
			cyl, head, track_pos, strerror(errno));
		exit(1);
	}

	/* Format track. */
	if (write(ffd, &track_data, sizeof(track_data)) < 0) {
		fprintf(stderr,
			"format: formatting cyl %d, head %d failed: %s\n",
			cyl, head, strerror(errno));
		exit(1);
	}

	/* Make sure the data is not just cached in a file system. */
	fsync(ffd);
}

void verify_track(int vfd, unsigned type, unsigned cyl, unsigned head)
/* Verify a track by reading it.  On error read sector by sector. */
{
	type_parameters_t *tparams= &parameters[type - 1];
	off_t track_pos;
	unsigned sector;
	unsigned nr_sectors= tparams->fmt_params.sectors_per_track;
	size_t track_bytes;
	static char buf[MAX_SECTORS * SECTOR_SIZE];
	static unsigned bad_count;

	/* Seek to the right track. */
	track_pos= (off_t) (cyl * NR_HEADS + head) * nr_sectors * SECTOR_SIZE;
	if (lseek(vfd, track_pos, SEEK_SET) == -1) {
		fprintf(stderr,
		"format: seeking to cyl %u, head %u (pos %d) failed: %s\n",
			cyl, head, track_pos, strerror(errno));
		exit(1);
	}

	/* Read the track whole. */
	track_bytes= nr_sectors * SECTOR_SIZE;
	if (read(vfd, buf, track_bytes) == track_bytes) return;

	/* An error occurred, retry sector by sector. */
	for (sector= 0; sector < nr_sectors; sector++) {
		if (lseek(vfd, track_pos, SEEK_SET) == -1) {
			fprintf(stderr,
	"format: seeking to cyl %u, head %u, sector %u (pos %d) failed: %s\n",
				cyl, head, sector, track_pos, strerror(errno));
			exit(1);
		}

		switch (read(vfd, buf, SECTOR_SIZE)) {
		case -1:
			fprintf(stderr,
		"format: bad sector at cyl %u, head %u, sector %u (pos %d)\n",
				cyl, head, sector, track_pos);
			bad_count++;
			break;
		case SECTOR_SIZE:
			/* Fine. */
			break;
		default:
			fprintf(stderr, "format: short read at pos %d\n",
				track_pos);
			bad_count++;
		}
		track_pos+= SECTOR_SIZE;
		if (bad_count >= nr_sectors) {
	fprintf(stderr, "format: too many bad sectors, floppy unusable\n");
			exit(1);
		}
	}
}

void format_device(unsigned drive, unsigned type, int verify)
{
	int ffd, vfd;
	char *fmt_dev, *ver_dev;
	struct stat st;
	unsigned cyl, head;
	unsigned nr_cyls;
	type_parameters_t *tparams= &parameters[type - 1];
	int verbose= isatty(1);

	fmt_dev= tmpnam(nil);

	if (mknod(fmt_dev, S_IFBLK | 0700, fl_makedev(drive, type, 1)) < 0) {
		fprintf(stderr, "format: making format device failed: %s\n",
			strerror(errno));
		exit(1);
	}

	if ((ffd= open(fmt_dev, O_WRONLY)) < 0 || fstat(ffd, &st) < 0) {
		report(fmt_dev);
		(void) unlink(fmt_dev);
		exit(1);
	}

	(void) unlink(fmt_dev);

	if (st.st_rdev != fl_makedev(drive, type, 1)) {
		/* Someone is trying to trick me. */
		exit(1);
	}

	if (verify) {
		ver_dev= tmpnam(nil);

		if (mknod(ver_dev, S_IFBLK | 0700, fl_makedev(drive, type, 0))
									< 0) {
			fprintf(stderr,
				"format: making verify device failed: %s\n",
				strerror(errno));
			exit(1);
		}

		if ((vfd= open(ver_dev, O_RDONLY)) < 0) {
			report(ver_dev);
			(void) unlink(ver_dev);
			exit(1);
		}

		(void) unlink(ver_dev);
	}

	nr_cyls= tparams->media_size * (1024 / SECTOR_SIZE) / NR_HEADS
				/ tparams->fmt_params.sectors_per_track;

	if (verbose) {
		printf("Formatting a %uk diskette in a %uk drive\n",
			tparams->media_size, tparams->drive_size);
	}

	for (cyl= 0; cyl < nr_cyls; cyl++) {
		for (head= 0; head < NR_HEADS; head++) {
			if (verbose) {
				printf(" Cyl. %2u, Head %u\r", cyl, head);
				fflush(stdout);
			}
			/* After formatting a track we are too late to format
			 * the next track.  So we can sleep at most 1/6 sec to
			 * allow the above printf to get displayed before we
			 * lock Minix into the floppy driver again.
			 */
			if (verbose) usleep(50000);	/* 1/20 sec will do. */
			format_track(ffd, type, cyl, head);
			if (verify) verify_track(vfd, type, cyl, head);
		}
	}
	if (verbose) fputc('\n', stdout);
}

void usage(void)
{
	fprintf(stderr,
		"Usage: format [-v] <device> [<media size> [<drive size>]]\n");
	exit(1);
}

int main(int argc, char **argv)
{
	char *device;
	unsigned drive;
	unsigned type;
	unsigned media_size;
	unsigned drive_size;
	int verify= 0;
	struct stat st0, st;
	char special[PATH_MAX + 1], mounted_on[PATH_MAX + 1];
	char version[MNTNAMELEN], rw_flag[MNTFLAGLEN];

	/* Option -v. */
	while (argc > 1 && argv[1][0] == '-') {
		char *p;

		for (p= argv[1]; *p == '-' || *p == 'v'; p++) {
			if (*p == 'v') verify= 1;
		}
		if (*p != 0) usage();
		argc--;
		argv++;
		if (strcmp(argv[0], "--") == 0) break;
	}

	if (argc < 2 || argc > 4) usage();

	/* Check if the caller has read-write permission.  Use the access()
	 * call to check with the real uid & gid.  This program is usually
	 * set-uid root.
	 */
	device= argv[1];
	if (stat(device, &st0) < 0
		|| access(device, R_OK|W_OK) < 0
		|| stat(device, &st) < 0
		|| (errno= EACCES, 0)	/* set errno for following tests */
		|| st.st_dev != st0.st_dev
		|| st.st_ino != st0.st_ino
	) {
		fatal(device);
	}

	if (!S_ISBLK(st.st_mode) || !isfloppy(st.st_rdev)) {
		fprintf(stderr, "format: %s: not a floppy device\n", device);
		exit(1);
	}

	drive= fl_drive(st.st_rdev);
	type= fl_type(st.st_rdev);

	/* The drive should not be mounted. */
	if (load_mtab("mkfs") < 0) exit(1);

	while (get_mtab_entry(special, mounted_on, version, rw_flag) == 0) {
		if (stat(special, &st) >= 0 && isfloppy(st.st_rdev)
					&& fl_drive(st.st_rdev) == drive) {
			fprintf(stderr, "format: %s is mounted on %s\n",
				device, mounted_on);
			exit(1);
		}
	}

	if (isflauto(type)) {
		/* Auto type 0 requires size(s). */
		unsigned long lmedia, ldrive;
		char *end;

		if (argc < 3) {
			fprintf(stderr,
			"format: no size specified for auto floppy device %s\n",
				device);
			usage();
		}

		lmedia= strtoul(argv[2], &end, 10);
		if (end == argv[2] || *end != 0 || lmedia > 20 * 1024)
			usage();

		if (argc == 4) {
			ldrive= strtoul(argv[3], &end, 10);
			if (end == argv[3] || *end != 0 || ldrive > 20 * 1024)
				usage();
		} else {
			ldrive= lmedia;
		}

		/* Silently correct wrong ordered sizes. */
		if (lmedia > ldrive) {
			media_size= ldrive;
			drive_size= lmedia;
		} else {
			media_size= lmedia;
			drive_size= ldrive;
		}

		/* A 1.44M drive can do 720k diskettes with no extra tricks.
		 * Diddle with the 720k params so it is found.
		 */
		if (media_size == 720 && drive_size == 1440)
			parameters[4 - 1].drive_size= 1440;

		/* Translate the auto type to a known type. */
		for (type= 1; type <= NR_TYPES; type++) {
			if (parameters[type - 1].media_size == media_size
				&& parameters[type - 1].drive_size == drive_size
			) break;
		}

		if (!isfltyped(type)) {
			fprintf(stderr,
			"format: can't format a %uk floppy in a %uk drive\n",
				media_size, drive_size);
			exit(1);
		}
	} else
	if (isfltyped(type)) {
		/* No sizes needed for a non-auto type. */

		if (argc > 2) {
			fprintf(stderr,
	"format: no sizes need to be specified for non-auto floppy device %s\n",
				device);
			usage();
		}
	} else
	if (isflpart(type)) {
		fprintf(stderr,
			"format: floppy partition %s can't be formatted\n",
			device);
		exit(1);
	} else {
		fprintf(stderr,
			"format: %s: can't format strange type %d\n",
			device, type);
	}

	format_device(drive, type, verify);
	exit(0);
}
