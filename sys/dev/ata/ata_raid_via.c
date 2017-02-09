/*	$NetBSD: ata_raid_via.c,v 1.7 2014/03/25 16:19:13 christos Exp $	*/

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

/*
 * Support for parsing VIA V-RAID ATA RAID controller configuration blocks.
 *
 * Adapted to NetBSD by Tim Rightnour (garbled@netbsd.org)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ata_raid_via.c,v 1.7 2014/03/25 16:19:13 christos Exp $");

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/kauth.h>

#include <miscfs/specfs/specdev.h>

#include <dev/ata/atareg.h>
#include <dev/ata/atavar.h>
#include <dev/ata/wdvar.h>

#include <dev/ata/ata_raidreg.h>
#include <dev/ata/ata_raidvar.h>

#ifdef ATA_RAID_DEBUG
#define	DPRINTF(x)	printf x
#else
#define	DPRINTF(x)	/* nothing */
#endif

#ifdef ATA_RAID_DEBUG
static const char *
ata_raid_via_type(int type)
{
	static char buffer[16];

	switch (type) {
	case VIA_T_RAID0:   return "RAID0";
	case VIA_T_RAID1:   return "RAID1";
	case VIA_T_RAID5:   return "RAID5";
	case VIA_T_RAID01:  return "RAID0+1";
	case VIA_T_SPAN:    return "SPAN";
	default:
		snprintf(buffer, sizeof(buffer), "UNKNOWN 0x%02x", type);
		return buffer;
	}
}

static void
ata_raid_via_print_info(struct via_raid_conf *info)
{
	int i;
  
	printf("*************** ATA VIA Metadata ****************\n");
	printf("magic               0x%02x\n", info->magic);
	printf("dummy_0             0x%02x\n", info->dummy_0);
	printf("type                %s\n",
	    ata_raid_via_type(info->type & VIA_T_MASK));
	printf("bootable            %d\n", info->type & VIA_T_BOOTABLE);
	printf("unknown             %d\n", info->type & VIA_T_UNKNOWN);
	printf("disk_index          0x%02x\n", info->disk_index);
	printf("stripe_layout       0x%02x\n", info->stripe_layout);
	printf(" stripe_disks       %d\n", info->stripe_layout & VIA_L_DISKS);
	printf(" stripe_sectors     %d\n",
	    0x08 << ((info->stripe_layout & VIA_L_MASK) >> VIA_L_SHIFT));
	printf("disk_sectors        %ju\n", info->disk_sectors);
	printf("disk_id             0x%08x\n", info->disk_id);
	printf("DISK#   disk_id\n");
	for (i = 0; i < 8; i++) {
	if (info->disks[i])
		printf("  %d    0x%08x\n", i, info->disks[i]);
	}    
	printf("checksum            0x%02x\n", info->checksum);
	printf("=================================================\n");
}
#endif

int
ata_raid_read_config_via(struct wd_softc *sc)
{
	struct via_raid_conf *info;
	struct atabus_softc *atabus;
	struct vnode *vp;
	int bmajor, error;
	dev_t dev;
	uint32_t drive;
	uint8_t checksum, checksum_alt, byte3, *ptr;
	int count, disk;
	struct ataraid_array_info *aai;
	struct ataraid_disk_info *adi;

	info = malloc(sizeof(*info), M_DEVBUF, M_WAITOK);

	bmajor = devsw_name2blk(device_xname(sc->sc_dev), NULL, 0);

	/* Get a vnode for the raw partition of this disk. */
	dev = MAKEDISKDEV(bmajor, device_unit(sc->sc_dev), RAW_PART);
	error = bdevvp(dev, &vp);
	if (error)
		goto out;

	error = VOP_OPEN(vp, FREAD, NOCRED);
	if (error) {
		vput(vp);
		goto out;
	}

	error = ata_raid_config_block_rw(vp, VIA_LBA(sc), info,
	    sizeof(*info), B_READ);
	VOP_CLOSE(vp, FREAD, NOCRED);
	vput(vp);
	if (error) {
		aprint_error_dev(sc->sc_dev,
		    "error %d reading VIA V-RAID config block\n", error);
		goto out;
	}

#ifdef ATA_RAID_DEBUG
	ata_raid_via_print_info(info);
	printf("MAGIC == 0x%02x\n", info->magic);
#endif

	/* Check the signature. */
	if (info->magic != VIA_MAGIC) {
		DPRINTF(("%s: VIA V-RAID signature check failed\n",
		    device_xname(sc->sc_dev)));
		error = ESRCH;
		goto out;
	}

	/* calculate checksum and compare for valid */
	for (byte3 = 0, checksum = 0, ptr = (uint8_t *)info, count = 0;
	    count < 50; count++)
		if (count == 3)
			byte3 = *ptr++;
		else
			checksum += *ptr++;
	checksum_alt = checksum + (byte3 & ~VIA_T_BOOTABLE);
	checksum += byte3;
	if (checksum != info->checksum && checksum_alt != info->checksum) {
		DPRINTF(("%s: VIA V-RAID checksum failed 0x%02x != "
		    "0x%02x or 0x%02x\n", device_xname(sc->sc_dev),
		    info->checksum, checksum, checksum_alt));
		error = ESRCH;
		goto out;
	}
	
	/*
	 * Lookup or allocate a new array info structure for
	 * this array. Use the serial number of disk0 as the array#
	 */
	aai = ata_raid_get_array_info(ATA_RAID_TYPE_VIA, info->disks[0]);

	aai->aai_status = AAI_S_READY;

	switch (info->type & VIA_T_MASK) {
	case VIA_T_RAID0:
		aai->aai_level = AAI_L_RAID0;
		aai->aai_width = info->stripe_layout & VIA_L_DISKS;
		aai->aai_capacity = aai->aai_width * info->disk_sectors;
		break;

	case VIA_T_RAID1:
		aai->aai_level = AAI_L_RAID1;
		aai->aai_width = 1;
		aai->aai_capacity = aai->aai_width * info->disk_sectors;
		break;

	case VIA_T_RAID5:
		aai->aai_level = AAI_L_RAID5;
		aai->aai_width = info->stripe_layout & VIA_L_DISKS;
		aai->aai_capacity = (aai->aai_width - 1) * info->disk_sectors;
		break;

	case VIA_T_SPAN:
		aai->aai_level = AAI_L_SPAN;
		aai->aai_width = 1;
		aai->aai_capacity += info->disk_sectors; /* XXX ??? */
		break;

	default:
		aprint_error_dev(sc->sc_dev,
		    "unknown VIA V-RAID type 0x%02x\n", info->type);
		error = EINVAL;
		goto out;
	}

	aai->aai_type = ATA_RAID_TYPE_VIA;
	for (count = 0, disk = 0; disk < 8; disk++)
		if (info->disks[disk])
			count++;
	aai->aai_interleave =
		0x08 << ((info->stripe_layout & VIA_L_MASK) >> VIA_L_SHIFT);
	aai->aai_ndisks = count;
	aai->aai_heads = 255;
	aai->aai_sectors = 63;
	aai->aai_cylinders = aai->aai_capacity / (63 * 255);
	aai->aai_offset = 0;
	aai->aai_reserved = 1;

	/* XXX - bogus.  RAID1 shouldn't really have an interleave */
	if (aai->aai_interleave == 0)
		aai->aai_interleave = aai->aai_capacity;

	atabus = device_private(device_parent(sc->sc_dev));
	drive = atabus->sc_chan->ch_channel;
	if (drive >= aai->aai_ndisks) {
		aprint_error_dev(sc->sc_dev,
		    "drive number %d doesn't make sense within %d-disk "
		    "array\n", drive, aai->aai_ndisks);
		error = EINVAL;
		goto out;
	}

	adi = &aai->aai_disks[drive];
	adi->adi_dev = sc->sc_dev;
	adi->adi_status = ADI_S_ONLINE | ADI_S_ASSIGNED;
	adi->adi_sectors = aai->aai_capacity;
	adi->adi_compsize = info->disk_sectors;

	error = 0;

 out:
	free(info, M_DEVBUF);
	return (error);
}
