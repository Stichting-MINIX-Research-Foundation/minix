/*	$NetBSD: ukfs_int_disklabel.h,v 1.3 2011/02/22 15:42:15 pooka Exp $	*/

/*
 * Modified copy of disklabel.h so that ukfs doesn't have to depend
 * on NetBSD headers.  Contains just about only "struct disklabel".
 * The on-disk disklabel doesn't really change format, so no need
 * to track this against NetBSD changes.
 */

/*
 * Copyright (c) 1987, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)disklabel.h	8.2 (Berkeley) 7/10/94
 */

#ifndef LIB_UKFS_DISKLABEL_H_
#define	LIB_UKFS_DISKLABEL_H_

#include <sys/types.h>

#include <stdint.h>

#define	UKFS_MAXPARTITIONS	22
#define	UKFS_DISKMAGIC	((uint32_t)0x82564557)	/* The disk magic number */

struct ukfs__disklabel {
	uint32_t d_magic;		/* the magic number */
	uint16_t d_type;		/* drive type */
	uint16_t d_subtype;		/* controller/d_type specific */
	char	  d_typename[16];	/* type name, e.g. "eagle" */

	/*
	 * d_packname contains the pack identifier and is returned when
	 * the disklabel is read off the disk or in-core copy.
	 * d_boot0 and d_boot1 are the (optional) names of the
	 * primary (block 0) and secondary (block 1-15) bootstraps
	 * as found in /usr/mdec.  These are returned when using
	 * getdiskbyname(3) to retrieve the values from /etc/disktab.
	 */
	union {
		char	un_d_packname[16];	/* pack identifier */
		struct {
			char *un_d_boot0;	/* primary bootstrap name */
			char *un_d_boot1;	/* secondary bootstrap name */
		} un_b;
	} d_un;
#define	d_packname	d_un.un_d_packname
#define	d_boot0		d_un.un_b.un_d_boot0
#define	d_boot1		d_un.un_b.un_d_boot1

			/* disk geometry: */
	uint32_t d_secsize;		/* # of bytes per sector */
	uint32_t d_nsectors;		/* # of data sectors per track */
	uint32_t d_ntracks;		/* # of tracks per cylinder */
	uint32_t d_ncylinders;		/* # of data cylinders per unit */
	uint32_t d_secpercyl;		/* # of data sectors per cylinder */
	uint32_t d_secperunit;		/* # of data sectors per unit */

	/*
	 * Spares (bad sector replacements) below are not counted in
	 * d_nsectors or d_secpercyl.  Spare sectors are assumed to
	 * be physical sectors which occupy space at the end of each
	 * track and/or cylinder.
	 */
	uint16_t d_sparespertrack;	/* # of spare sectors per track */
	uint16_t d_sparespercyl;	/* # of spare sectors per cylinder */
	/*
	 * Alternative cylinders include maintenance, replacement,
	 * configuration description areas, etc.
	 */
	uint32_t d_acylinders;		/* # of alt. cylinders per unit */

			/* hardware characteristics: */
	/*
	 * d_interleave, d_trackskew and d_cylskew describe perturbations
	 * in the media format used to compensate for a slow controller.
	 * Interleave is physical sector interleave, set up by the
	 * formatter or controller when formatting.  When interleaving is
	 * in use, logically adjacent sectors are not physically
	 * contiguous, but instead are separated by some number of
	 * sectors.  It is specified as the ratio of physical sectors
	 * traversed per logical sector.  Thus an interleave of 1:1
	 * implies contiguous layout, while 2:1 implies that logical
	 * sector 0 is separated by one sector from logical sector 1.
	 * d_trackskew is the offset of sector 0 on track N relative to
	 * sector 0 on track N-1 on the same cylinder.  Finally, d_cylskew
	 * is the offset of sector 0 on cylinder N relative to sector 0
	 * on cylinder N-1.
	 */
	uint16_t d_rpm;		/* rotational speed */
	uint16_t d_interleave;		/* hardware sector interleave */
	uint16_t d_trackskew;		/* sector 0 skew, per track */
	uint16_t d_cylskew;		/* sector 0 skew, per cylinder */
	uint32_t d_headswitch;		/* head switch time, usec */
	uint32_t d_trkseek;		/* track-to-track seek, usec */
	uint32_t d_flags;		/* generic flags */
#define	NDDATA 5
	uint32_t d_drivedata[NDDATA];	/* drive-type specific information */
#define	NSPARE 5
	uint32_t d_spare[NSPARE];	/* reserved for future use */
	uint32_t d_magic2;		/* the magic number (again) */
	uint16_t d_checksum;		/* xor of data incl. partitions */

			/* filesystem and partition information: */
	uint16_t d_npartitions;	/* number of partitions in following */
	uint32_t d_bbsize;		/* size of boot area at sn0, bytes */
	uint32_t d_sbsize;		/* max size of fs superblock, bytes */
	struct	ukfs__partition {	/* the partition table */
		uint32_t p_size;	/* number of sectors in partition */
		uint32_t p_offset;	/* starting sector */
		union {
			uint32_t fsize; /* FFS, ADOS:
					    filesystem basic fragment size */
			uint32_t cdsession; /* ISO9660: session offset */
		} __partition_u2;
#define	p_fsize		__partition_u2.fsize
#define	p_cdsession	__partition_u2.cdsession
		uint8_t p_fstype;	/* filesystem type, see below */
		uint8_t p_frag;	/* filesystem fragments per block */
		union {
			uint16_t cpg;	/* UFS: FS cylinders per group */
			uint16_t sgs;	/* LFS: FS segment shift */
		} __partition_u1;
#define	p_cpg	__partition_u1.cpg
#define	p_sgs	__partition_u1.sgs
	} d_partitions[UKFS_MAXPARTITIONS];	/* actually may be more */
};

uint16_t        ukfs__disklabel_dkcksum(struct ukfs__disklabel *, int);
int             ukfs__disklabel_scan(struct ukfs__disklabel *, int *,
				     char *, size_t);

#endif /* !LIB_UKFS_DISKLABEL_H_ */
