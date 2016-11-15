/*	$NetBSD: ata_raid_nvidia.c,v 1.2 2014/03/25 16:19:13 christos Exp $	*/

/*-
 * Copyright (c) 2000 - 2008 Søren Schmidt <sos@FreeBSD.org>
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
 * Support for parsing nVIDIA MediaShield RAID controller configuration blocks.
 *
 * Adapted to NetBSD by Tatoku Ogaito (tacha@NetBSD.org)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ata_raid_nvidia.c,v 1.2 2014/03/25 16:19:13 christos Exp $");

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
ata_raid_nvidia_type(int type)
{
	static char buffer[16];

	switch (type) {
	case NV_T_SPAN:     return "SPAN";
	case NV_T_RAID0:    return "RAID0";
	case NV_T_RAID1:    return "RAID1";
	case NV_T_RAID3:    return "RAID3";
	case NV_T_RAID5:    return "RAID5";
	case NV_T_RAID01:   return "RAID0+1";
	default:
		snprintf(buffer, sizeof(buffer), "UNKNOWN 0x%02x", type);
		return buffer;
    }
}

static void
ata_raid_nvidia_print_info(struct nvidia_raid_conf *info)
{
    printf("******** ATA nVidia MediaShield Metadata ********\n");
    printf("nvidia_id           <%.8s>\n", info->nvidia_id);
    printf("config_size         %d\n", info->config_size);
    printf("checksum            0x%08x\n", info->checksum);
    printf("version             0x%04x\n", info->version);
    printf("disk_number         %d\n", info->disk_number);
    printf("dummy_0             0x%02x\n", info->dummy_0);
    printf("total_sectors       %d\n", info->total_sectors);
    printf("sectors_size        %d\n", info->sector_size);
    printf("serial              %.16s\n", info->serial);
    printf("revision            %.4s\n", info->revision);
    printf("dummy_1             0x%08x\n", info->dummy_1);
    printf("magic_0             0x%08x\n", info->magic_0);
    printf("magic_1             0x%016jx\n", info->magic_1);
    printf("magic_2             0x%016jx\n", info->magic_2);
    printf("flags               0x%02x\n", info->flags);
    printf("array_width         %d\n", info->array_width);
    printf("total_disks         %d\n", info->total_disks);
    printf("dummy_2             0x%02x\n", info->dummy_2);
    printf("type                %s\n", ata_raid_nvidia_type(info->type));
    printf("dummy_3             0x%04x\n", info->dummy_3);
    printf("stripe_sectors      %d\n", info->stripe_sectors);
    printf("stripe_bytes        %d\n", info->stripe_bytes);
    printf("stripe_shift        %d\n", info->stripe_shift);
    printf("stripe_mask         0x%08x\n", info->stripe_mask);
    printf("stripe_sizesectors  %d\n", info->stripe_sizesectors);
    printf("stripe_sizebytes    %d\n", info->stripe_sizebytes);
    printf("rebuild_lba         %d\n", info->rebuild_lba);
    printf("dummy_4             0x%08x\n", info->dummy_4);
    printf("dummy_5             0x%08x\n", info->dummy_5);
    printf("status              0x%08x\n", info->status);
    printf("=================================================\n");
}
#endif

int
ata_raid_read_config_nvidia(struct wd_softc *sc)
{
	struct nvidia_raid_conf *info;
	struct vnode *vp;
	int bmajor, error, count;
	dev_t dev;
	uint32_t cksum, *ckptr;
	struct ataraid_array_info *aai;
	struct ataraid_disk_info *adi;
	static struct _arrayno {
	  uint64_t magic1;
	  uint64_t magic2;
	  struct _arrayno *next;
	} arrayno = { 0, 0, NULL}, *anptr;

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

	error = ata_raid_config_block_rw(vp, NVIDIA_LBA(sc), info,
	    sizeof(*info), B_READ);
	VOP_CLOSE(vp, FREAD, NOCRED);
	vput(vp);
	if (error) {
		aprint_error_dev(sc->sc_dev,
		    "error %d reading nVidia MediaShield config block\n", error);
		goto out;
	}

#ifdef ATA_RAID_DEBUG
	ata_raid_nvidia_print_info(info);
#endif

	/* Check the signature. */
	if (strncmp(info->nvidia_id, NV_MAGIC, strlen(NV_MAGIC)) != 0) {
		DPRINTF(("%s: nVidia signature check failed\n",
		    device_xname(sc->sc_dev)));
		error = ESRCH;
		goto out;
	}

	/* check if the checksum is OK */
	for (cksum = 0, ckptr = (uint32_t *)info, count = 0; count < info->config_size;
	     count++)
		cksum += *ckptr++;
	if (cksum) {
		DPRINTF(("%s: nVidia checksum failed (0x%02x)\n",
			 device_xname(sc->sc_dev), cksum));
		error = ESRCH;
		goto out;
	}

	/*
	 * Lookup or allocate a new array info structure for
	 * this array. Since nVidia raid information does not
	 * provides array# directory, we must count the number.
	 * The available traces are magic_1 and magic_2.
	 */
	for (anptr = &arrayno, count = 0; anptr->next; anptr = anptr->next, count++) {
		if (anptr->magic1 == info->magic_1 &&
		    anptr->magic2 == info->magic_2)
	    break;
	}
	if (anptr->next == NULL) {
		/* new array */
		anptr->magic1 = info->magic_1;
		anptr->magic2 = info->magic_2;
		anptr->next = malloc(sizeof(arrayno), M_DEVBUF, M_WAITOK);
	}
	aai = ata_raid_get_array_info(ATA_RAID_TYPE_NVIDIA, count);

	aai->aai_status = AAI_S_READY;
	if (info->status & NV_S_DEGRADED)
		aai->aai_status |= AAI_S_DEGRADED;

	switch (info->type) {
	case NV_T_RAID0:
		aai->aai_level = AAI_L_RAID0;
		break;

	case NV_T_RAID1:
		aai->aai_level = AAI_L_RAID1;
		break;

	case NV_T_RAID5:
		aai->aai_level = AAI_L_RAID5;
		break;

	case NV_T_RAID01:
		aai->aai_level = AAI_L_RAID0 | AAI_L_RAID1;
		break;

	case NV_T_SPAN:
		aai->aai_level = AAI_L_SPAN;
		break;

	default:
		aprint_error_dev(sc->sc_dev,
			 "unknown nVidia type 0x%02x\n", info->type);
		error = EINVAL;
		goto out;
	}

	aai->aai_type = ATA_RAID_TYPE_NVIDIA;
	aai->aai_interleave = info->stripe_sectors;
	aai->aai_width = info->array_width;

	aai->aai_ndisks = info->total_disks;
	aai->aai_capacity = info->total_sectors;
	aai->aai_heads = 255;
	aai->aai_sectors = 63;
	aai->aai_cylinders = aai->aai_capacity / (aai->aai_heads * aai->aai_sectors);
	aai->aai_offset = 0;
	aai->aai_reserved = 2;

	adi = &aai->aai_disks[info->disk_number];
	adi->adi_dev = sc->sc_dev;
	adi->adi_status = ADI_S_ONLINE | ADI_S_ASSIGNED;
	adi->adi_sectors = aai->aai_capacity;
	adi->adi_compsize = aai->aai_capacity / info->array_width;

	error = 0;

 out:
	free(info, M_DEVBUF);
	return (error);
}
