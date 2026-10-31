/*	$NetBSD: biosdisk_ll.h,v 1.16 2018/03/08 10:34:33 nonaka Exp $	 */

/*
 * Copyright (c) 1996
 * 	Matthias Drochner.  All rights reserved.
 * Copyright (c) 1996
 * 	Perry E. Metzger.  All rights reserved.
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
 *    must display the following acknowledgements:
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 *	This product includes software developed for the NetBSD Project
 *	by Perry E. Metzger.
 * 4. The names of the authors may not be used to endorse or promote products
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

/*
 * shared by bootsector startup (bootsectmain) and biosdisk.c needs lowlevel
 * parts from bios_disk.S
 */

/*
 * Beware that bios_disk.S relies on the offsets of the structure
 * members.
 */
struct biosdisk_ll {
	int             dev;		/* BIOS device number */
	int		type;		/* device type; see below */
	int             sec, head, cyl;	/* geometry */
	int		flags;		/* see below */
	int		chs_sectors;	/* # of sectors addressable by CHS */
	int		secsize;	/* bytes per sector */
};
#define	BIOSDISK_INT13EXT	1	/* BIOS supports int13 extension */

#define BIOSDISK_TYPE_FD	0
#define BIOSDISK_TYPE_HD	1
#define BIOSDISK_TYPE_CD	2

/*
 * Version 1.x drive parameters from int13 extensions
 * - should be supported by every BIOS that supports the extensions.
 * Version 3.x parameters allow the drives to be matched properly
 * - but are much less likely to be supported.
 */

struct biosdisk_extinfo {
	uint16_t	size;		/* size of buffer, set on call */
	uint16_t	flags;		/* flags, see below */
	uint32_t	cyl;		/* # of physical cylinders */
	uint32_t	head;		/* # of physical heads */
	uint32_t	sec;		/* # of physical sectors per track */
	uint64_t	totsec;		/* total number of sectors */
	uint16_t	sbytes;		/* # of bytes per sector */
#if defined(BIOSDISK_EXTINFO_V2) || defined(BIOSDISK_EXTINFO_V3)
	/* v2.0 extensions */
	uint32_t	edd_cfg;	/* EDD configuration parameters */
#if defined(BIOSDISK_EXTINFO_V3)
	/* v3.0 extensions */
	uint16_t	devpath_sig;	/* 0xbedd if path info present */
#define EXTINFO_DEVPATH_SIGNATURE	0xbedd
	uint8_t		devpath_len;	/* length from devpath_sig */
	uint8_t		fill21[3];
	char		host_bus[4];	/* Probably "ISA" or "PCI" */
	char		iface_type[8];	/* "ATA", "ATAPI", "SCSI" etc */
	union {
		uint8_t		ip_8[8];
		uint16_t	ip_16[4];
		uint32_t	ip_32[2];
		uint64_t	ip_64[1];
	} interface_path;
#define	ip_isa_iobase	ip_16[0];	/* iobase for ISA bus */
#define	ip_pci_bus	ip_8[0];	/* PCI bus number */
#define	ip_pci_device	ip_8[1];	/* PCI device number */
#define	ip_pci_function	ip_8[2];	/* PCI function number */
	union {
		uint8_t		dp_8[8];
		uint16_t	dp_16[4];
		uint32_t	dp_32[2];
		uint64_t	dp_64[1];
	} device_path;
#define	dp_ata_slave	dp_8[0];
#define	dp_atapi_slave	dp_8[0];
#define	dp_atapi_lun	dp_8[1];
#define	dp_scsi_lun	dp_8[0];
#define	dp_firewire_guid dp_64[0];
#define	dp_fibrechnl_wwn dp_64[0];
	uint8_t		fill40[1];
	uint8_t		checksum;	/* byte sum from dev_path_sig is 0 */
#endif /* BIOSDISK_EXTINFO_V3 */
#endif /* BIOSDISK_EXTINFO_V2 */
} __packed;

#define EXTINFO_DMA_TRANS	0x0001	/* transparent DMA boundary errors */
#define EXTINFO_GEOM_VALID	0x0002	/* geometry in c/h/s in struct valid */
#define EXTINFO_REMOVABLE	0x0004	/* removable device */
#define EXTINFO_WRITEVERF	0x0008	/* supports write with verify */
#define EXTINFO_CHANGELINE	0x0010	/* changeline support */
#define EXTINFO_LOCKABLE	0x0020	/* device is lockable */
#define EXTINFO_MAXGEOM		0x0040	/* geometry set to max; no media */

#ifndef BIOSDISK_DEFAULT_SECSIZE
#define BIOSDISK_DEFAULT_SECSIZE	512
#endif

int set_geometry(struct biosdisk_ll *, struct biosdisk_extinfo *);
int readsects(struct biosdisk_ll *, daddr_t, int, char *, int);
