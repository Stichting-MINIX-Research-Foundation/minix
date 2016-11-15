/*	$NetBSD: stvar.h,v 1.25 2015/04/13 16:33:25 riastradh Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Originally written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 * major changes by Julian Elischer (julian@jules.dialix.oz.au) May 1993
 *
 * A lot of rewhacking done by mjacob (mjacob@nas.nasa.gov).
 */

#include <sys/rndsource.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#define	ST_IO_TIME	(3 * 60 * 1000)		/* 3 minutes */
#define	ST_CTL_TIME	(30 * 1000)		/* 30 seconds */
#define	ST_SPC_TIME	(4 * 60 * 60 * 1000)	/* 4 hours */

#define	ST_RETRIES	4	/* only on non IO commands */

struct modes {
	u_int quirks;			/* same definitions as in quirkdata */
	int blksize;
	uint8_t density;
};

struct quirkdata {
	u_int quirks;
#define	ST_Q_FORCE_BLKSIZE	0x0001
#define	ST_Q_SENSE_HELP		0x0002	/* must do READ for good MODE SENSE */
#define	ST_Q_IGNORE_LOADS	0x0004
#define	ST_Q_BLKSIZE		0x0008	/* variable-block media_blksize > 0 */
#define	ST_Q_UNIMODAL		0x0010	/* unimode drive rejects mode select */
#define	ST_Q_NOPREVENT		0x0020	/* does not support PREVENT */
#define	ST_Q_ERASE_NOIMM	0x0040	/* drive rejects ERASE/w Immed bit */
#define	ST_Q_NOFILEMARKS	0x0080	/* can only write 0 filemarks */
	u_int page_0_size;
#define	MAX_PAGE_0_SIZE	64
	struct modes modes[4];
};

struct st_quirk_inquiry_pattern {
	struct scsipi_inquiry_pattern pattern;
	struct quirkdata quirkdata;
};

struct st_softc {
	device_t sc_dev;
/*--------------------callback to bus-specific code--------------------------*/
	int (*ops)(struct st_softc *, int, int);
#define ST_OPS_RBL		0x00	/* read block limit */
#define ST_OPS_MODESENSE	0x01	/* mode sense */
#define ST_OPS_MODESELECT	0x02	/* mode select */
#define ST_OPS_CMPRSS_ON 	0x03	/* turn on compression */
#define ST_OPS_CMPRSS_OFF 	0x04	/* turn off compression */
/*--------------------present operating parameters, flags etc.---------------*/
	int flags;		/* see below                         */
	u_int quirks;		/* quirks for the open mode          */
	int blksize;		/* blksize we are using              */
	uint8_t density;	/* present density                   */
	u_int page_0_size;	/* size of page 0 data		     */
	u_int last_dsty;	/* last density opened               */
	short mt_resid;		/* last (short) resid                */
	short mt_erreg;		/* last error (sense key) seen       */
	/* relative to BOT location */
	daddr_t fileno;
	daddr_t blkno;
	int32_t last_io_resid;
	int32_t last_ctl_resid;
#define	mt_key	mt_erreg
	uint8_t asc;		/* last asc code seen		     */
	uint8_t ascq;		/* last asc code seen		     */
/*--------------------device/scsi parameters---------------------------------*/
	struct scsipi_periph *sc_periph;/* our link to the adpter etc.       */
/*--------------------parameters reported by the device ---------------------*/
	int blkmin;		/* min blk size                       */
	int blkmax;		/* max blk size                       */
	const struct quirkdata *quirkdata;	/* if we have a rogue entry  */
/*--------------------parameters reported by the device for this media-------*/
	u_long numblks;		/* nominal blocks capacity            */
	int media_blksize;	/* 0 if not ST_FIXEDBLOCKS            */
	uint8_t media_density;	/* this is what it said when asked    */
/*--------------------quirks for the whole drive-----------------------------*/
	u_int drive_quirks;	/* quirks of this drive               */
/*--------------------How we should set up when opening each minor device----*/
	struct modes modes[4];	/* plus more for each mode            */
	uint8_t  modeflags[4];	/* flags for the modes                */
#define DENSITY_SET_BY_USER	0x01
#define DENSITY_SET_BY_QUIRK	0x02
#define BLKSIZE_SET_BY_USER	0x04
#define BLKSIZE_SET_BY_QUIRK	0x08
/*--------------------storage for sense data returned by the drive-----------*/
	u_char sense_data[MAX_PAGE_0_SIZE];	/*
						 * additional sense data needed
						 * for mode sense/select.
						 */
	struct bufq_state *buf_queue;	/* the queue of pending IO */
					/* operations */
	struct callout sc_callout;	/* restarting the queue after */
					/* transient error */

	struct io_stats *stats;		/* statistics for the drive */

	krndsource_t	rnd_source;
};

#define	ST_INFO_VALID	0x0001
#define	ST_BLOCK_SET	0x0002	/* block size, mode set by ioctl      */
#define	ST_WRITTEN	0x0004	/* data has been written, EOD needed */
#define	ST_FIXEDBLOCKS	0x0008
#define	ST_AT_FILEMARK	0x0010
#define	ST_EIO_PENDING	0x0020	/* error reporting deferred until next op */
#define	ST_NEW_MOUNT	0x0040	/* still need to decide mode             */
#define	ST_READONLY	0x0080	/* st_mode_sense says write protected */
#define	ST_FM_WRITTEN	0x0100	/*
				 * EOF file mark written  -- used with
				 * ~ST_WRITTEN to indicate that multiple file
				 * marks have been written
				 */
#define	ST_BLANK_READ	0x0200	/* BLANK CHECK encountered already */
#define	ST_2FM_AT_EOD	0x0400	/* write 2 file marks at EOD */
#define	ST_MOUNTED	0x0800	/* Device is presently mounted */
#define	ST_DONTBUFFER	0x1000	/* Disable buffering/caching */
#define	ST_EARLYWARN	0x2000	/* Do (deferred) EOM for variable mode */
#define	ST_EOM_PENDING	0x4000	/* EOM reporting deferred until next op */
#define	ST_POSUPDATED	0x8000	/* tape position already updated */

#define	ST_PER_ACTION	(ST_AT_FILEMARK | ST_EIO_PENDING | ST_EOM_PENDING | \
			 ST_BLANK_READ)
#define	ST_PER_MOUNT	(ST_INFO_VALID | ST_BLOCK_SET | ST_WRITTEN |	\
			 ST_FIXEDBLOCKS | ST_READONLY | ST_FM_WRITTEN |	\
			 ST_2FM_AT_EOD | ST_PER_ACTION | ST_POSUPDATED)

void	stattach(device_t, device_t, void *);
int	stdetach(device_t, int);
int	st_mode_select(struct st_softc *, int);

extern struct cfdriver st_cd;
