/*	$NetBSD: ata_raidvar.h,v 1.12 2010/07/06 18:03:21 bsh Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_ATA_ATA_RAIDVAR_H_
#define	_DEV_ATA_ATA_RAIDVAR_H_

#include <sys/queue.h>

/*
 * Types of RAID configurations we support.  Do not change the order
 * of this list, as it will change the order in which the arrays are
 * sorted.
 *
 * If this list is updated, ensure the array in 
 * ata_raid.c:ata_raid_type_name() is also updated.
 */
#define	ATA_RAID_TYPE_PROMISE	0
#define	ATA_RAID_TYPE_ADAPTEC	1
#define	ATA_RAID_TYPE_VIA	2
#define	ATA_RAID_TYPE_NVIDIA	3
#define ATA_RAID_TYPE_JMICRON	4
#define	ATA_RAID_TYPE_INTEL	5
#define	ATA_RAID_TYPE_MAX	5

/*
 * Max # of disks supported by a single array.  This is limited by
 * the number that the individual controller config blocks can support:
 *
 *	Promise		8
 */
#define	ATA_RAID_MAX_DISKS	8

struct ataraid_disk_info {
	device_t adi_dev;		/* disk's device */
	int	adi_status;		/* disk's status */
	uint64_t	adi_sectors;
	uint64_t	adi_compsize;		/* in sectors */
};

/* adi_status */
#define	ADI_S_ONLINE		0x01
#define	ADI_S_ASSIGNED		0x02
#define	ADI_S_SPARE		0x04

struct ataraid_array_info {
	TAILQ_ENTRY(ataraid_array_info) aai_list;

	device_t aai_ld;		/* associated logical disk */

	u_int	aai_type;		/* array type */
	u_int	aai_arrayno;		/* array number */
	int	aai_level;		/* RAID level */
	int	aai_generation;		/* config generation # */
	int	aai_status;		/* array status */

	/* Geometry info. */
	u_int	aai_interleave;		/* stripe size */
	u_int	aai_width;		/* array width */
	u_int	aai_ndisks;		/* number of disks */
	u_int	aai_heads;		/* tracks/cyl */
	u_int	aai_sectors;		/* secs/track */
	u_int	aai_cylinders;		/* cyl/unit */
	uint64_t	aai_capacity;		/* in sectors */
	daddr_t		aai_offset;		/* component start offset */
	uint64_t	aai_reserved;		/* component reserved sectors */

	char	aai_name[32];		/* array volume name */

	uint aai_curdisk;	/* to enumerate component disks */
	struct ataraid_disk_info aai_disks[ATA_RAID_MAX_DISKS];
};

/* aai_level */
#define	AAI_L_SPAN		0x01
#define	AAI_L_RAID0		0x02
#define	AAI_L_RAID1		0x04
#define	AAI_L_RAID5		0x08

/* aai_status */
#define	AAI_S_READY		0x01
#define	AAI_S_DEGRADED		0x02

struct vnode;
struct wd_softc;

typedef TAILQ_HEAD(, ataraid_array_info) ataraid_array_info_list_t;
extern ataraid_array_info_list_t ataraid_array_info_list;

void	ata_raid_check_component(device_t);
const char *ata_raid_type_name(u_int);

struct ataraid_array_info *ata_raid_get_array_info(u_int, u_int);
int	ata_raid_config_block_rw(struct vnode *, daddr_t, void *,
	    size_t, int);

struct vnode *ata_raid_disk_vnode_find(struct ataraid_disk_info *);

/* Promise RAID support */
int	ata_raid_read_config_promise(struct wd_softc *);

/* Adaptec HostRAID support */
int	ata_raid_read_config_adaptec(struct wd_softc *);

/* VIA V-RAID support */
int	ata_raid_read_config_via(struct wd_softc *);

/* nVidia MediaShield support */
int	ata_raid_read_config_nvidia(struct wd_softc *);

/* JMicron RAID support */
int	ata_raid_read_config_jmicron(struct wd_softc *);

/* Intel MatrixRAID support */
int	ata_raid_read_config_intel(struct wd_softc *);

#endif /* _DEV_ATA_ATA_RAIDVAR_H_ */
