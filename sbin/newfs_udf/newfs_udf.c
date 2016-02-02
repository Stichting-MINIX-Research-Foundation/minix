/* $NetBSD: newfs_udf.c,v 1.18 2013/08/09 15:11:08 reinoud Exp $ */

/*
 * Copyright (c) 2006, 2008, 2013 Reinoud Zandijk
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

/*
 * TODO
 * - implement metadata formatting for BD-R
 * - implement support for a read-only companion partition?
 */

#define _EXPOSE_MMC
#if 0
# define DEBUG
#endif

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <util.h>
#include <time.h>
#include <assert.h>
#include <err.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/cdio.h>
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <sys/param.h>
#include <sys/queue.h>

#include <fs/udf/ecma167-udf.h>
#include <fs/udf/udf_mount.h>

#include "mountprog.h"
#include "udf_create.h"
#include "udf_write.h"
#include "newfs_udf.h"

/* prototypes */
int newfs_udf(int argc, char **argv);
static void usage(void) __attribute__((__noreturn__));


/* queue for temporary storage of sectors to be written out */
struct wrsect {
	uint64_t  sectornr;
	uint8_t	 *sector_data;
	TAILQ_ENTRY(wrsect) next;
};

/* write queue and track blocking skew */
TAILQ_HEAD(wrsect_list, wrsect) write_queue;


/* global variables describing disc and format requests */
int	 fd;				/* device: file descriptor */
char	*dev;				/* device: name		   */
struct mmc_discinfo mmc_discinfo;	/* device: disc info	   */

char	*format_str;			/* format: string representation */
int	 format_flags;			/* format: attribute flags	 */
int	 media_accesstype;		/* derived from current mmc cap  */
int	 check_surface;			/* for rewritables               */
int	 imagefile_secsize;		/* for files			 */
int	 emul_packetsize;		/* for discs and files		 */

int	 wrtrack_skew;
int	 meta_perc = UDF_META_PERC;
float	 meta_fract = (float) UDF_META_PERC / 100.0;


/* --------------------------------------------------------------------- */

/*
 * write queue implementation
 */

int
udf_write_sector(void *sector, uint64_t location)
{
	struct wrsect *pos, *seekpos;


	/* search location */
	TAILQ_FOREACH_REVERSE(seekpos, &write_queue, wrsect_list, next) {
		if (seekpos->sectornr <= location)
			break;
	}
	if ((seekpos == NULL) || (seekpos->sectornr != location)) {
		pos = calloc(1, sizeof(struct wrsect));
		if (pos == NULL)
			return ENOMEM;
		/* allocate space for copy of sector data */
		pos->sector_data = calloc(1, context.sector_size);
		if (pos->sector_data == NULL)
			return ENOMEM;
		pos->sectornr = location;

		if (seekpos) {
			TAILQ_INSERT_AFTER(&write_queue, seekpos, pos, next);
		} else {
			TAILQ_INSERT_HEAD(&write_queue, pos, next);
		}	
	} else {
		pos = seekpos;
	}
	memcpy(pos->sector_data, sector, context.sector_size);

	return 0;
}


/*
 * Now all write requests are queued in the TAILQ, write them out to the
 * disc/file image. Special care needs to be taken for devices that are only
 * strict overwritable i.e. only in packet size chunks
 *
 * XXX support for growing vnd?
 */

int
writeout_write_queue(void)
{
	struct wrsect *pos;
	uint64_t offset;
	uint64_t line_start, new_line_start;
	uint32_t line_len, line_offset, relpos;
	uint32_t blockingnr;
	uint8_t *linebuf, *adr;

	blockingnr  = layout.blockingnr;
	line_len    = blockingnr   * context.sector_size;
	line_offset = wrtrack_skew * context.sector_size;

	linebuf     = malloc(line_len);
	if (linebuf == NULL)
		return ENOMEM;

	pos = TAILQ_FIRST(&write_queue);
	bzero(linebuf, line_len);

	/*
	 * Always writing out in whole lines now; this is slightly wastefull
	 * on logical overwrite volumes but it reduces complexity and the loss
	 * is near zero compared to disc size.
	 */
	line_start = (pos->sectornr - wrtrack_skew) / blockingnr;
	TAILQ_FOREACH(pos, &write_queue, next) {
		new_line_start = (pos->sectornr - wrtrack_skew) / blockingnr;
		if (new_line_start != line_start) {
			/* write out */
			offset = (uint64_t) line_start * line_len + line_offset;
#ifdef DEBUG
			printf("WRITEOUT %08"PRIu64" + %02d -- "
				"[%08"PRIu64"..%08"PRIu64"]\n",
				offset / context.sector_size, blockingnr,
				offset / context.sector_size,
				offset / context.sector_size + blockingnr-1);
#endif
			if (pwrite(fd, linebuf, line_len, offset) < 0) {
				perror("Writing failed");
				return errno;
			}
			line_start = new_line_start;
			bzero(linebuf, line_len);
		}

		relpos = (pos->sectornr - wrtrack_skew) % blockingnr;
		adr = linebuf + relpos * context.sector_size;
		memcpy(adr, pos->sector_data, context.sector_size);
	}
	/* writeout last chunk */
	offset = (uint64_t) line_start * line_len + line_offset;
#ifdef DEBUG
	printf("WRITEOUT %08"PRIu64" + %02d -- [%08"PRIu64"..%08"PRIu64"]\n",
		offset / context.sector_size, blockingnr,
		offset / context.sector_size,
		offset / context.sector_size + blockingnr-1);
#endif
	if (pwrite(fd, linebuf, line_len, offset) < 0) {
		perror("Writing failed");
		return errno;
	}

	/* success */
	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * mmc_discinfo and mmc_trackinfo readers modified from origional in udf main
 * code in sys/fs/udf/
 */

#ifdef DEBUG
static void
udf_dump_discinfo(struct mmc_discinfo *di)
{
	char bits[128];

	printf("Device/media info  :\n");
	printf("\tMMC profile        0x%02x\n", di->mmc_profile);
	printf("\tderived class      %d\n", di->mmc_class);
	printf("\tsector size        %d\n", di->sector_size);
	printf("\tdisc state         %d\n", di->disc_state);
	printf("\tlast ses state     %d\n", di->last_session_state);
	printf("\tbg format state    %d\n", di->bg_format_state);
	printf("\tfrst track         %d\n", di->first_track);
	printf("\tfst on last ses    %d\n", di->first_track_last_session);
	printf("\tlst on last ses    %d\n", di->last_track_last_session);
	printf("\tlink block penalty %d\n", di->link_block_penalty);
	snprintb(bits, sizeof(bits), MMC_DFLAGS_FLAGBITS, (uint64_t) di->disc_flags);
	printf("\tdisc flags         %s\n", bits);
	printf("\tdisc id            %x\n", di->disc_id);
	printf("\tdisc barcode       %"PRIx64"\n", di->disc_barcode);

	printf("\tnum sessions       %d\n", di->num_sessions);
	printf("\tnum tracks         %d\n", di->num_tracks);

	snprintb(bits, sizeof(bits), MMC_CAP_FLAGBITS, di->mmc_cur);
	printf("\tcapabilities cur   %s\n", bits);
	snprintb(bits, sizeof(bits), MMC_CAP_FLAGBITS, di->mmc_cap);
	printf("\tcapabilities cap   %s\n", bits);
	printf("\n");
	printf("\tlast_possible_lba  %d\n", di->last_possible_lba);
	printf("\n");
}
#else
#define udf_dump_discinfo(a);
#endif

/* --------------------------------------------------------------------- */

static int
udf_update_discinfo(struct mmc_discinfo *di)
{
	struct stat st;
	struct disklabel  disklab;
	struct partition *dp;
	off_t size, sectors, secsize;
	int partnr, error;

	memset(di, 0, sizeof(struct mmc_discinfo));

	/* check if we're on a MMC capable device, i.e. CD/DVD */
	error = ioctl(fd, MMCGETDISCINFO, di);
	if (error == 0)
		return 0;

	/* (re)fstat the file */
	fstat(fd, &st);

	if (S_ISREG(st.st_mode)) {
		/* file support; we pick the minimum sector size allowed */
		size = st.st_size;
		secsize = imagefile_secsize;
		sectors = size / secsize;
	} else {
		/*
		 * disc partition support; note we can't use DIOCGPART in
		 * userland so get disc label and use the stat info to get the
		 * partition number.
		 */
		if (ioctl(fd, DIOCGDINFO, &disklab) == -1) {
			/* failed to get disclabel! */
			perror("disklabel");
			return errno;
		}

		/* get disk partition it refers to */
		fstat(fd, &st);
		partnr = DISKPART(st.st_rdev);
		dp = &disklab.d_partitions[partnr];

		/* TODO problem with last_possible_lba on resizable VND */
		if (dp->p_size == 0) {
			perror("faulty disklabel partition returned, "
				"check label\n");
			return EIO;
		}

		sectors = dp->p_size;
		secsize = disklab.d_secsize;
	}

	/* set up a disc info profile for partitions */
	di->mmc_profile		= 0x01;	/* disc type */
	di->mmc_class		= MMC_CLASS_DISC;
	di->disc_state		= MMC_STATE_CLOSED;
	di->last_session_state	= MMC_STATE_CLOSED;
	di->bg_format_state	= MMC_BGFSTATE_COMPLETED;
	di->link_block_penalty	= 0;

	di->mmc_cur     = MMC_CAP_RECORDABLE | MMC_CAP_REWRITABLE |
		MMC_CAP_ZEROLINKBLK | MMC_CAP_HW_DEFECTFREE;
	di->mmc_cap    = di->mmc_cur;
	di->disc_flags = MMC_DFLAGS_UNRESTRICTED;

	di->last_possible_lba = sectors - 1;
	di->sector_size       = secsize;

	di->num_sessions = 1;
	di->num_tracks   = 1;

	di->first_track  = 1;
	di->first_track_last_session = di->last_track_last_session = 1;

	return 0;
}


int
udf_update_trackinfo(struct mmc_discinfo *di, struct mmc_trackinfo *ti)
{
	int error, class;

	class = di->mmc_class;
	if (class != MMC_CLASS_DISC) {
		/* tracknr specified in struct ti */
		error = ioctl(fd, MMCGETTRACKINFO, ti);
		return error;
	}

	/* discs partition support */
	if (ti->tracknr != 1)
		return EIO;

	/* create fake ti (TODO check for resized vnds) */
	ti->sessionnr  = 1;

	ti->track_mode = 0;	/* XXX */
	ti->data_mode  = 0;	/* XXX */
	ti->flags = MMC_TRACKINFO_LRA_VALID | MMC_TRACKINFO_NWA_VALID;

	ti->track_start    = 0;
	ti->packet_size    = emul_packetsize;

	/* TODO support for resizable vnd */
	ti->track_size    = di->last_possible_lba;
	ti->next_writable = di->last_possible_lba;
	ti->last_recorded = ti->next_writable;
	ti->free_blocks   = 0;

	return 0;
}


static int
udf_setup_writeparams(struct mmc_discinfo *di)
{
	struct mmc_writeparams mmc_writeparams;
	int error;

	if (di->mmc_class == MMC_CLASS_DISC)
		return 0;

	/*
	 * only CD burning normally needs setting up, but other disc types
	 * might need other settings to be made. The MMC framework will set up
	 * the nessisary recording parameters according to the disc
	 * characteristics read in. Modifications can be made in the discinfo
	 * structure passed to change the nature of the disc.
	 */
	memset(&mmc_writeparams, 0, sizeof(struct mmc_writeparams));
	mmc_writeparams.mmc_class  = di->mmc_class;
	mmc_writeparams.mmc_cur    = di->mmc_cur;

	/*
	 * UDF dictates first track to determine track mode for the whole
	 * disc. [UDF 1.50/6.10.1.1, UDF 1.50/6.10.2.1]
	 * To prevent problems with a `reserved' track in front we start with
	 * the 2nd track and if that is not valid, go for the 1st.
	 */
	mmc_writeparams.tracknr = 2;
	mmc_writeparams.data_mode  = MMC_DATAMODE_DEFAULT;	/* XA disc */
	mmc_writeparams.track_mode = MMC_TRACKMODE_DEFAULT;	/* data */

	error = ioctl(fd, MMCSETUPWRITEPARAMS, &mmc_writeparams);
	if (error) {
		mmc_writeparams.tracknr = 1;
		error = ioctl(fd, MMCSETUPWRITEPARAMS, &mmc_writeparams);
	}
	return error;
}


static void
udf_synchronise_caches(void)
{
	struct mmc_op mmc_op;

	bzero(&mmc_op, sizeof(struct mmc_op));
	mmc_op.operation = MMC_OP_SYNCHRONISECACHE;

	/* this device might not know this ioct, so just be ignorant */
	(void) ioctl(fd, MMCOP, &mmc_op);
}

/* --------------------------------------------------------------------- */

static int
udf_prepare_disc(void)
{
	struct mmc_trackinfo ti;
	struct mmc_op        op;
	int tracknr, error;

	/* If the last track is damaged, repair it */
	ti.tracknr = mmc_discinfo.last_track_last_session;
	error = udf_update_trackinfo(&mmc_discinfo, &ti);
	if (error)
		return error;

	if (ti.flags & MMC_TRACKINFO_DAMAGED) {
		/*
		 * Need to repair last track before anything can be done.
		 * this is an optional command, so ignore its error but report
		 * warning.
		 */
		memset(&op, 0, sizeof(op));
		op.operation   = MMC_OP_REPAIRTRACK;
		op.mmc_profile = mmc_discinfo.mmc_profile;
		op.tracknr     = ti.tracknr;
		error = ioctl(fd, MMCOP, &op);

		if (error)
			(void)printf("Drive can't explicitly repair last "
				"damaged track, but it might autorepair\n");
	}
	/* last track (if any) might not be damaged now, operations are ok now */

	/* setup write parameters from discinfo */
	error = udf_setup_writeparams(&mmc_discinfo);
	if (error)
		return error;

	/* if the drive is not sequential, we're done */
	if ((mmc_discinfo.mmc_cur & MMC_CAP_SEQUENTIAL) == 0)
		return 0;

#ifdef notyet
	/* if last track is not the reserved but an empty track, unreserve it */
	if (ti.flags & MMC_TRACKINFO_BLANK) {
		if (ti.flags & MMC_TRACKINFO_RESERVED == 0) {
			memset(&op, 0, sizeof(op));
			op.operation   = MMC_OP_UNRESERVETRACK;
			op.mmc_profile = mmc_discinfo.mmc_profile;
			op.tracknr     = ti.tracknr;
			error = ioctl(fd, MMCOP, &op);
			if (error)
				return error;

			/* update discinfo since it changed by the operation */
			error = udf_update_discinfo(&mmc_discinfo);
			if (error)
				return error;
		}
	}
#endif

	/* close the last session if its still open */
	if (mmc_discinfo.last_session_state == MMC_STATE_INCOMPLETE) {
		printf("Closing last open session if present\n");
		/* close all associated tracks */
		tracknr = mmc_discinfo.first_track_last_session;
		while (tracknr <= mmc_discinfo.last_track_last_session) {
			ti.tracknr = tracknr;
			error = udf_update_trackinfo(&mmc_discinfo, &ti);
			if (error)
				return error;
			printf("\tClosing open track %d\n", tracknr);
			memset(&op, 0, sizeof(op));
			op.operation   = MMC_OP_CLOSETRACK;
			op.mmc_profile = mmc_discinfo.mmc_profile;
			op.tracknr     = tracknr;
			error = ioctl(fd, MMCOP, &op);
			if (error)
				return error;
			tracknr ++;
		}
		printf("Closing session\n");
		memset(&op, 0, sizeof(op));
		op.operation   = MMC_OP_CLOSESESSION;
		op.mmc_profile = mmc_discinfo.mmc_profile;
		op.sessionnr   = mmc_discinfo.num_sessions;
		error = ioctl(fd, MMCOP, &op);
		if (error)
			return error;

		/* update discinfo since it changed by the operations */
		error = udf_update_discinfo(&mmc_discinfo);
		if (error)
			return error;
	}

	if (format_flags & FORMAT_TRACK512) {
		/* get last track again */
		ti.tracknr = mmc_discinfo.last_track_last_session;
		error = udf_update_trackinfo(&mmc_discinfo, &ti);
		if (error)
			return error;

		/* Split up the space at 512 for iso cd9660 hooking */
		memset(&op, 0, sizeof(op));
		op.operation   = MMC_OP_RESERVETRACK_NWA;	/* UPTO nwa */
		op.mmc_profile = mmc_discinfo.mmc_profile;
		op.extent      = 512;				/* size */
		error = ioctl(fd, MMCOP, &op);
		if (error)
			return error;
	}

	return 0;
}

/* --------------------------------------------------------------------- */

int
udf_surface_check(void)
{
	uint32_t loc, block_bytes;
	uint32_t sector_size, blockingnr, bpos;
	uint8_t *buffer;
	int error, num_errors;

	sector_size = context.sector_size;
	blockingnr  = layout.blockingnr;

	block_bytes = layout.blockingnr * sector_size;
	if ((buffer = malloc(block_bytes)) == NULL)
		return ENOMEM;

	/* set all one to not kill Flash memory? */
	for (bpos = 0; bpos < block_bytes; bpos++)
		buffer[bpos] = 0x00;

	printf("\nChecking disc surface : phase 1 - writing\n");
	num_errors = 0;
	loc = layout.first_lba;
	while (loc <= layout.last_lba) {
		/* write blockingnr sectors */
		error = pwrite(fd, buffer, block_bytes, loc*sector_size);
		printf("   %08d + %d (%02d %%)\r", loc, blockingnr,
			(int)((100.0 * loc)/layout.last_lba));
		fflush(stdout);
		if (error == -1) {
			/* block is bad */
			printf("BAD block at %08d + %d         \n",
				loc, layout.blockingnr);
			if ((error = udf_register_bad_block(loc))) {
				free(buffer);
				return error;
			}
			num_errors ++;
		}
		loc += layout.blockingnr;
	}

	printf("\nChecking disc surface : phase 2 - reading\n");
	num_errors = 0;
	loc = layout.first_lba;
	while (loc <= layout.last_lba) {
		/* read blockingnr sectors */
		error = pread(fd, buffer, block_bytes, loc*sector_size);
		printf("   %08d + %d (%02d %%)\r", loc, blockingnr,
			(int)((100.0 * loc)/layout.last_lba));
		fflush(stdout);
		if (error == -1) {
			/* block is bad */
			printf("BAD block at %08d + %d         \n",
				loc, layout.blockingnr);
			if ((error = udf_register_bad_block(loc))) {
				free(buffer);
				return error;
			}
			num_errors ++;
		}
		loc += layout.blockingnr;
	}
	printf("Scan complete : %d bad blocks found\n", num_errors);
	free(buffer);

	return 0;
}


/* --------------------------------------------------------------------- */

static int
udf_do_newfs(void)
{
	int error;

	error = udf_do_newfs_prefix();
	if (error)
		return error;
	error = udf_do_rootdir();
	if (error)
		return error;
	error = udf_do_newfs_postfix();

	return error;
}



/* --------------------------------------------------------------------- */

static void
usage(void)
{
	(void)fprintf(stderr, "Usage: %s [-cFM] [-L loglabel] "
	    "[-P discid] [-S sectorsize] [-s size] [-p perc] "
	    "[-t gmtoff] [-v min_udf] [-V max_udf] special\n", getprogname());
	exit(EXIT_FAILURE);
}


int
main(int argc, char **argv)
{
	struct tm *tm;
	struct stat st;
	time_t now;
	off_t setsize;
	char  scrap[255], *colon;
	int ch, req_enable, req_disable, force;
	int error;

	setprogname(argv[0]);

	/* initialise */
	format_str    = strdup("");
	req_enable    = req_disable = 0;
	format_flags  = FORMAT_INVALID;
	force         = 0;
	check_surface = 0;
	setsize       = 0;
	imagefile_secsize = 512;	/* minimum allowed sector size */
	emul_packetsize   = 32;		/* reasonable default */

	srandom((unsigned long) time(NULL));
	udf_init_create_context();
	context.app_name         = "*NetBSD newfs";
	context.app_version_main = APP_VERSION_MAIN;
	context.app_version_sub  = APP_VERSION_SUB;
	context.impl_name        = IMPL_NAME;

	/* minimum and maximum UDF versions we advise */
	context.min_udf = 0x201;
	context.max_udf = 0x201;

	/* use user's time zone as default */
	(void)time(&now);
	tm = localtime(&now);
	context.gmtoff = tm->tm_gmtoff;

	/* process options */
	while ((ch = getopt(argc, argv, "cFL:Mp:P:s:S:B:t:v:V:")) != -1) {
		switch (ch) {
		case 'c' :
			check_surface = 1;
			break;
		case 'F' :
			force = 1;
			break;
		case 'L' :
			if (context.logvol_name) free(context.logvol_name);
			context.logvol_name = strdup(optarg);
			break;
		case 'M' :
			req_disable |= FORMAT_META;
			break;
		case 'p' :
			meta_perc = a_num(optarg, "meta_perc");
			/* limit to `sensible` values */
			meta_perc = MIN(meta_perc, 99);
			meta_perc = MAX(meta_perc, 1);
			meta_fract = (float) meta_perc/100.0;
			break;
		case 'v' :
			context.min_udf = a_udf_version(optarg, "min_udf");
			if (context.min_udf > context.max_udf)
				context.max_udf = context.min_udf;
			break;
		case 'V' :
			context.max_udf = a_udf_version(optarg, "max_udf");
			if (context.min_udf > context.max_udf)
				context.min_udf = context.max_udf;
			break;
		case 'P' :
			/* check if there is a ':' in the name */
			if ((colon = strstr(optarg, ":"))) {
				if (context.volset_name)
					free(context.volset_name);
				*colon = 0;
				context.volset_name = strdup(optarg);
				optarg = colon+1;
			}
			if (context.primary_name)
				free(context.primary_name);
			if ((strstr(optarg, ":"))) {
				perror("primary name can't have ':' in its name");
				return EXIT_FAILURE;
			}
			context.primary_name = strdup(optarg);
			break;
		case 's' :
			/* support for files, set file size */
			/* XXX support for formatting recordables on vnd/file? */
			if (dehumanize_number(optarg, &setsize) < 0) {
				perror("can't parse size argument");
				return EXIT_FAILURE;
			}
			setsize = MAX(0, setsize);
			break;
		case 'S' :
			imagefile_secsize = a_num(optarg, "secsize");
			imagefile_secsize = MAX(512, imagefile_secsize);
			break;
		case 'B' :
			emul_packetsize = a_num(optarg,
				"blockingnr, packetsize");
			emul_packetsize = MAX(emul_packetsize, 1);
			emul_packetsize = MIN(emul_packetsize, 32);
			break;
		case 't' :
			/* time zone overide */
			context.gmtoff = a_num(optarg, "gmtoff");
			break;
		default  :
			usage();
			/* NOTREACHED */
		}
	}

	if (optind + 1 != argc)
		usage();

	/* get device and directory specifier */
	dev = argv[optind];

	/* open device */
	if ((fd = open(dev, O_RDWR, 0)) == -1) {
		/* check if we need to create a file */
		fd = open(dev, O_RDONLY, 0);
		if (fd > 0) {
			perror("device is there but can't be opened for read/write");
			return EXIT_FAILURE;
		}
		if (!force) {
			perror("can't open device");
			return EXIT_FAILURE;
		}
		if (setsize == 0) {
			perror("need to create image file but no size specified");
			return EXIT_FAILURE;
		}
		/* need to create a file */
		fd = open(dev, O_RDWR | O_CREAT | O_TRUNC, 0777);
		if (fd == -1) {
			perror("can't create image file");
			return EXIT_FAILURE;
		}
	}

	/* stat the device */
	if (fstat(fd, &st) != 0) {
		perror("can't stat the device");
		close(fd);
		return EXIT_FAILURE;
	}

	if (S_ISREG(st.st_mode)) {
		if (setsize == 0)
			setsize = st.st_size;
		/* sanitise arguments */
		imagefile_secsize &= ~511;
		setsize &= ~(imagefile_secsize-1);

		if (ftruncate(fd, setsize)) {
			perror("can't resize file");
			return EXIT_FAILURE;
		}
	}

	/* formatting can only be done on raw devices */
	if (!S_ISREG(st.st_mode) && !S_ISCHR(st.st_mode)) {
		printf("%s is not a raw device\n", dev);
		close(fd);
		return EXIT_FAILURE;
	}

	/* just in case something went wrong, synchronise the drive's cache */
	udf_synchronise_caches();

	/* get 'disc' information */
	error = udf_update_discinfo(&mmc_discinfo);
	if (error) {
		perror("can't retrieve discinfo");
		close(fd);
		return EXIT_FAILURE;
	}

	/* derive disc identifiers when not specified and check given */
	error = udf_proces_names();
	if (error) {
		/* error message has been printed */
		close(fd);
		return EXIT_FAILURE;
	}

	/* derive newfs disc format from disc profile */
	error = udf_derive_format(req_enable, req_disable, force);
	if (error)  {
		/* error message has been printed */
		close(fd);
		return EXIT_FAILURE;
	}

	udf_dump_discinfo(&mmc_discinfo);
	printf("Formatting disc compatible with UDF version %x to %x\n\n",
		context.min_udf, context.max_udf);
	(void)snprintb(scrap, sizeof(scrap), FORMAT_FLAGBITS,
	    (uint64_t) format_flags);
	printf("UDF properties       %s\n", scrap);
	printf("Volume set          `%s'\n", context.volset_name);
	printf("Primary volume      `%s`\n", context.primary_name);
	printf("Logical volume      `%s`\n", context.logvol_name);
	if (format_flags & FORMAT_META)
		printf("Metadata percentage  %d %%\n", meta_perc);
	printf("\n");

	/* prepare disc if nessisary (recordables mainly) */
	error = udf_prepare_disc();
	if (error) {
		perror("preparing disc failed");
		close(fd);
		return EXIT_FAILURE;
	};

	/* setup sector writeout queue's */
	TAILQ_INIT(&write_queue);

	/* perform the newfs itself */
	error = udf_do_newfs();
	if (!error) {
		/* write out sectors */
		error = writeout_write_queue();
	}

	/* in any case, synchronise the drive's cache to prevent lockups */
	udf_synchronise_caches();

	close(fd);
	if (error)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

/* --------------------------------------------------------------------- */

