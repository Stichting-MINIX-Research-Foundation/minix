/*	$NetBSD: ata_raidreg.h,v 1.9 2014/09/10 07:04:48 matt Exp $	*/

/*-
 * Copyright (c) 2000,2001,2002 Søren Schmidt <sos@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#ifndef _DEV_PCI_PCIIDE_PROMISE_RAID_H_
#define	_DEV_PCI_PCIIDE_PROMISE_RAID_H_

/*
 * Macro to compute the LBA of the Promise RAID configuration structure,
 * using the disk's softc structure.
 */
#define	PR_LBA(wd)							\
	((((wd)->sc_capacity /						\
	   ((wd)->sc_params.atap_heads * (wd)->sc_params.atap_sectors)) * \
	  (wd)->sc_params.atap_heads * (wd)->sc_params.atap_sectors) -	\
	 (wd)->sc_params.atap_sectors)

struct promise_raid_conf {
	char		promise_id[24];
#define	PR_MAGIC	"Promise Technology, Inc."

	uint32_t	dummy_0;

	uint64_t	magic_0;
	uint16_t	magic_1;
	uint32_t	magic_2;
	uint8_t		filler1[470];
	struct {				/* 0x200 */
		uint32_t	integrity;
#define	PR_I_VALID	0x00000080

		uint8_t		flags;
#define	PR_F_VALID	0x01
#define	PR_F_ONLINE	0x02
#define	PR_F_ASSIGNED	0x04
#define	PR_F_SPARE	0x08
#define	PR_F_DUPLICATE	0x10
#define	PR_F_REDIR	0x20
#define	PR_F_DOWN	0x40
#define	PR_F_READY	0x80

		uint8_t		disk_number;
		uint8_t		channel;
		uint8_t		device;
		uint64_t	magic_0 __packed;
		uint32_t	disk_offset;	/* 0x210 */
		uint32_t	disk_sectors;
		uint32_t	rebuild_lba;
		uint16_t	generation;
		uint8_t		status;
#define	PR_S_VALID	0x01
#define	PR_S_ONLINE	0x02
#define	PR_S_INITED	0x04
#define	PR_S_READY	0x08
#define	PR_S_DEGRADED	0x10
#define	PR_S_MARKED	0x20
#define	PR_S_FUNCTIONAL	0x80

		uint8_t		type;
#define	PR_T_RAID0	0x00
#define	PR_T_RAID1	0x01
#define	PR_T_RAID3	0x02
#define	PR_T_RAID5	0x04
#define	PR_T_SPAN	0x08

		uint8_t		total_disks;	/* 0x220 */
		uint8_t		stripe_shift;
		uint8_t		array_width;
		uint8_t		array_number;
		uint32_t	total_sectors;
		uint16_t	cylinders;
		uint8_t		heads;
		uint8_t		sectors;
		uint64_t	magic_1 __packed;
		struct {
			uint8_t		flags;
			uint8_t		dummy_0;
			uint8_t		channel;
			uint8_t		device;
			uint64_t	magic_0 __packed;
		} disk[8];
	} raid;
	uint32_t	filler2[346];
	uint32_t	checksum;
} __packed;

/*
 * Macro to compute the LBA of the Adaptec HostRAID configuration structure,
 * using the disk's softc structure.
 */
#define	ADP_LBA(wd)							\
	((wd)->sc_capacity - 17)

struct adaptec_raid_conf {
	uint32_t	magic_0;
#define	ADP_MAGIC_0	0x900765c4

	uint32_t	generation;
	uint16_t	dummy_0;
	uint16_t	total_configs;
	uint16_t	dummy_1;
	uint16_t	checksum;
	uint32_t	dummy_2;
	uint32_t	dummy_3;
	uint32_t	flags;
	uint32_t	timestamp;
	uint32_t	dummy_4[4];
	uint32_t	dummy_5[4];
	struct {
		uint16_t	total_disks;
		uint16_t	generation;
		uint32_t	magic_0;
		uint8_t		dummy_0;
		uint8_t		type;
#define ADP_T_RAID0			0x00
#define ADP_T_RAID1			0x01
		uint8_t		dummy_1;
		uint8_t		flags;

		uint8_t		dummy_2;
		uint8_t		dummy_3;
		uint8_t		dummy_4;
		uint8_t		dummy_5;

		uint32_t	disk_number;
		uint32_t	dummy_6;
		uint32_t	sectors;
		uint16_t	stripe_sectors;
		uint16_t	dummy_7;

		uint32_t	dummy_8[4];
		uint8_t		name[16];
	} configs[127];
	uint32_t	dummy_6[13];
	uint32_t	magic_1;
#define ADP_MAGIC_1		0x0950f89f
	uint32_t	dummy_7[3];
	uint32_t	magic_2;
	uint32_t	dummy_8[46];
	uint32_t	magic_3;
#define ADP_MAGIC_3		0x4450544d
	uint32_t	magic_4;
#define ADP_MAGIC_4		0x0950f89f
	uint32_t	dummy_9[62];
} __packed;

/* VIA Tech V-RAID Metadata */
/* Derrived from FreeBSD ata-raid.h 1.46 */
#define VIA_LBA(wd) ((wd)->sc_capacity - 1)

struct via_raid_conf {
	uint16_t	magic;
#define VIA_MAGIC		0xaa55
	uint8_t		dummy_0;
	uint8_t		type;
#define VIA_T_MASK		0x7e
#define VIA_T_BOOTABLE		0x01
#define VIA_T_RAID0		0x04
#define VIA_T_RAID1		0x0c
#define VIA_T_RAID01		0x4c
#define VIA_T_RAID5		0x2c
#define VIA_T_SPAN		0x44
#define VIA_T_UNKNOWN		0x80
	uint8_t		disk_index;
#define VIA_D_MASK		0x0f
#define VIA_D_DEGRADED		0x10
#define VIA_D_HIGH_IDX		0x20
	uint8_t		stripe_layout;
#define VIA_L_DISKS		0x07
#define VIA_L_MASK		0xf0
#define VIA_L_SHIFT		4
	uint64_t		disk_sectors;
	uint32_t		disk_id;
	uint32_t		disks[8];
	uint8_t			checksum;
	uint8_t			pad_0[461];
} __packed;

/* nVidia MediaShield Metadata */
/* taken from FreeBSD ata-raid.h 1.47 */
#define NVIDIA_LBA(wd) ((wd)->sc_capacity - 2)

struct nvidia_raid_conf {
    uint8_t            nvidia_id[8];
#define NV_MAGIC                "NVIDIA  "

    uint32_t           config_size;
    uint32_t           checksum;
    uint16_t           version;
    uint8_t            disk_number;
    uint8_t            dummy_0;
    uint32_t           total_sectors;
    uint32_t           sector_size;
    uint8_t            serial[16];
    uint8_t            revision[4];
    uint32_t           dummy_1;

    uint32_t           magic_0;
#define NV_MAGIC0               0x00640044

    uint64_t           magic_1;
    uint64_t           magic_2;
    uint8_t            flags;
    uint8_t            array_width;
    uint8_t            total_disks;
    uint8_t            dummy_2;
    uint16_t           type;
#define NV_T_RAID0              0x00000080
#define NV_T_RAID1              0x00000081
#define NV_T_RAID3              0x00000083
#define NV_T_RAID5              0x00000085
#define NV_T_RAID01             0x00008180
#define NV_T_SPAN               0x000000ff

    uint16_t           dummy_3;
    uint32_t           stripe_sectors;
    uint32_t           stripe_bytes;
    uint32_t           stripe_shift;
    uint32_t           stripe_mask;
    uint32_t           stripe_sizesectors;
    uint32_t           stripe_sizebytes;
    uint32_t           rebuild_lba;
    uint32_t           dummy_4;
    uint32_t           dummy_5;
    uint32_t           status;
#define NV_S_BOOTABLE           0x00000001
#define NV_S_DEGRADED           0x00000002

    uint32_t           filler[98];
} __packed;

/* JMicron Technology Corp Metadata */
#define JMICRON_LBA(wd) 	((wd)->sc_capacity - 1)
#define JM_MAX_DISKS            8

struct jmicron_raid_conf {
	uint8_t 	signature[2];
#define JMICRON_MAGIC 		"JM"
	uint16_t 	version;
#define JMICRON_VERSION 	0x0001
	uint16_t 	checksum;
	uint8_t 	filler_1[10];
	uint32_t 	disk_id;
	uint32_t 	offset;
	uint32_t 	disk_sectors_high;
	uint16_t 	disk_sectors_low;
	uint8_t 	filler_2[2];
	uint8_t 	name[16];
	uint8_t 	type;
#define JM_T_RAID0 		0
#define JM_T_RAID1 		1
#define JM_T_RAID01 		2
#define JM_T_JBOD 		3
#define JM_T_RAID5 		5
	uint8_t 	stripe_shift;
	uint16_t 	flags;
#define JM_F_READY 		0x0001
#define JM_F_BOOTABLE 		0x0002
#define JM_F_BAD 		0x0004
#define JM_F_ACTIVE 		0x0010
#define JM_F_UNSYNC 		0x0020
#define JM_F_NEWEST 		0x0040
	uint8_t 	filler_3[4];
	uint32_t 	spare[2];
	uint32_t 	disks[JM_MAX_DISKS];
	uint8_t 	filler_4[32];
	uint8_t 	filler_5[384];
};

/* Intel MatrixRAID metadata */
#define INTEL_LBA(wd)		((wd)->sc_capacity - 3)

struct intel_raid_conf {
	uint8_t		intel_id[24];
#define INTEL_MAGIC		"Intel Raid ISM Cfg Sig. "

	uint8_t		version[6];
#define INTEL_VERSION_1100	"1.1.00"
#define INTEL_VERSION_1201	"1.2.01"
#define INTEL_VERSION_1202	"1.2.02"

	uint8_t		dummy_0[2];
	uint32_t	checksum;
	uint32_t	config_size;
	uint32_t	config_id;
	uint32_t	generation;
	uint32_t	dummy_1[2];
	uint8_t		total_disks;
	uint8_t		total_volumes;
	uint8_t 	dummy_2[2];
	uint32_t	filler_0[39];
	struct {
		uint8_t		serial[16];
		uint32_t	sectors;
		uint32_t	id;
		uint32_t	flags;
#define INTEL_F_SPARE			0x01
#define INTEL_F_ASSIGNED		0x02
#define INTEL_F_DOWN			0x04
#define INTEL_F_ONLINE			0x08
		uint32_t	filler[5];
	} __packed disk[1];
	uint32_t	filler_1[62];
} __packed;

struct intel_raid_mapping {
	uint8_t		name[16];
	uint64_t	total_sectors __packed;
	uint32_t	state;
	uint32_t	reserved;
	uint32_t	filler_0[20];
	uint32_t	offset;
	uint32_t	disk_sectors;
	uint32_t	stripe_count;
	uint16_t	stripe_sectors;
	uint8_t		status;
#define INTEL_S_READY		0x00
#define INTEL_S_DISABLED	0x01
#define INTEL_S_DEGRADED	0x02
#define INTEL_S_FAILURE		0x03

	uint8_t		type;
#define INTEL_T_RAID0		0x00
#define INTEL_T_RAID1		0x01
#define INTEL_T_RAID5		0x05

	uint8_t		total_disks;
	uint8_t		magic[3];
	uint32_t	filler_1[7];
	uint32_t	disk_idx[1];
} __packed;

#endif /* _DEV_PCI_PCIIDE_PROMISE_RAID_H_ */
