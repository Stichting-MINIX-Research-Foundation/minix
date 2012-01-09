/*	$NetBSD: bootinfo_biosgeom.c,v 1.21 2010/12/25 01:19:33 jakllsch Exp $	*/

/*
 * Copyright (c) 1997
 *	Matthias Drochner.  All rights reserved.
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

#include <sys/types.h>
#include <machine/disklabel.h>
#include <machine/cpu.h>

#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>

#include "libi386.h"
#include "biosdisk_ll.h"
#include "bootinfo.h"

#ifdef BIOSDISK_EXTINFO_V3
static struct {
	char	*name;
	int	flag;
} bus_names[] = { {"ISA", BI_GEOM_BUS_ISA},
		  {"PCI", BI_GEOM_BUS_PCI},
		  {NULL, BI_GEOM_BUS_OTHER} };
static struct {
	char	*name;
	int	flag;
} iface_names[] = { {"ATA", BI_GEOM_IFACE_ATA},
		    {"ATAPI", BI_GEOM_IFACE_ATAPI},
		    {"SCSI", BI_GEOM_IFACE_SCSI},
		    {"USB", BI_GEOM_IFACE_USB},
		    {"1394", BI_GEOM_IFACE_1394},
		    {"FIBRE", BI_GEOM_IFACE_FIBRE},
		    {NULL, BI_GEOM_IFACE_OTHER} };
#endif

void
bi_getbiosgeom(void)
{
	struct btinfo_biosgeom *bibg;
	int i, j, nvalid;
	int nhd;
	unsigned int cksum;
	struct biosdisk_ll d;
	struct biosdisk_extinfo ed;
	char buf[BIOSDISK_DEFAULT_SECSIZE];

	nhd = get_harddrives();
#ifdef GEOM_DEBUG
	printf("nhd %d\n", nhd);
#endif

	bibg = alloc(sizeof(struct btinfo_biosgeom)
		     + (nhd - 1) * sizeof(struct bi_biosgeom_entry));
	if (bibg == NULL)
		return;

	for (i = nvalid = 0; i < MAX_BIOSDISKS && nvalid < nhd; i++) {

		d.dev = 0x80 + i;

		if (set_geometry(&d, &ed))
			continue;
		memset(&bibg->disk[nvalid], 0, sizeof(bibg->disk[nvalid]));

		bibg->disk[nvalid].sec = d.sec;
		bibg->disk[nvalid].head = d.head;
		bibg->disk[nvalid].cyl = d.cyl;
		bibg->disk[nvalid].dev = d.dev;

		if (readsects(&d, 0, 1, buf, 0)) {
			bibg->disk[nvalid].flags |= BI_GEOM_INVALID;
			nvalid++;
			continue;
		}

#ifdef GEOM_DEBUG
		printf("#%d: %x: C %d H %d S %d\n", nvalid,
		       d.dev, d.cyl, d.head, d.sec);
		printf("   sz %d fl %x cyl %d head %d sec %d totsec %"PRId64" sbytes %d\n",
		       ed.size, ed.flags, ed.cyl, ed.head, ed.sec,
		       ed.totsec, ed.sbytes);
#endif

		if (d.flags & BIOSDISK_INT13EXT) {
			bibg->disk[nvalid].totsec = ed.totsec;
			bibg->disk[nvalid].flags |= BI_GEOM_EXTINT13;
		}
#ifdef BIOSDISK_EXTINFO_V3
#ifdef GEOM_DEBUG
		printf("   edd_cfg %x, sig %x, len %x, bus %s type %s\n",
		       ed.edd_cfg, ed.devpath_sig, ed.devpath_len,
		       ed.host_bus, ed.iface_type);
#endif

		/* The v3.0 stuff will help identify the disks */
		if (ed.size >= offsetof(struct biosdisk_ext13info, checksum)
		    && ed.devpath_sig == EXTINFO_DEVPATH_SIGNATURE) {
			char *cp;

			for (cp = (void *)&ed.devpath_sig, cksum = 0;
			     cp <= (char *)&ed.checksum; cp++) {
				cksum += *cp;
			}
			if ((cksum & 0xff) != 0)
				bibg->disk[nvalid].flags |= BI_GEOM_BADCKSUM;
#ifdef GEOM_DEBUG
			printf("checksum %x\n", cksum & 0xff);
#endif
			for (j = 0; ; j++) {
				cp = bus_names[j].name;
				if (cp == NULL)
					break;
				if (strncmp(cp, ed.host_bus,
					    sizeof(ed.host_bus)) == 0)
					break;
			}
#ifdef GEOM_DEBUG
			printf("bus %s (%x)\n", cp ? cp : "null",
			       bus_names[j].flag);
#endif
			bibg->disk[nvalid].flags |= bus_names[j].flag;
			for (j = 0; ; j++) {
				cp = iface_names[j].name;
				if (cp == NULL)
					break;
				if (strncmp(cp, ed.iface_type,
					    sizeof(ed.iface_type)) == 0)
					break;
			}
			bibg->disk[nvalid].flags |= iface_names[j].flag;
			/* Dump raw interface path and device path */
			bibg->disk[nvalid].interface_path =
					ed.interface_path.ip_32[0];
			bibg->disk[nvalid].device_path =
					ed.device_path.dp_64[0];
#ifdef GEOM_DEBUG
			printf("device %s (%x) interface %x path %llx\n",
			       cp ? cp : "null",
			       iface_names[j].flag,
			       ed.interface_path.ip_32[0],
			       ed.device_path.dp_64[0]);
#endif
		}
#endif

		for (j = 0, cksum = 0; j < BIOSDISK_DEFAULT_SECSIZE; j++)
			cksum += buf[j];
		bibg->disk[nvalid].cksum = cksum;
		memcpy(bibg->disk[nvalid].dosparts, &buf[MBR_PART_OFFSET],
		       sizeof(bibg->disk[nvalid].dosparts));
		nvalid++;
	}

	bibg->num = nvalid;

	BI_ADD(bibg, BTINFO_BIOSGEOM, sizeof(struct btinfo_biosgeom)
	       + nvalid * sizeof(struct bi_biosgeom_entry));
}
