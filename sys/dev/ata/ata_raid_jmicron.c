/*	$NetBSD: ata_raid_jmicron.c,v 1.5 2014/03/25 16:19:13 christos Exp $	*/

/*-
 * Copyright (c) 2000-2008 Søren Schmidt <sos@FreeBSD.org>
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
 * Support for parsing JMicron Technology RAID controller configuration blocks.
 *
 * Adapted to NetBSD by Juan Romero Pardines (xtraeme@gmail.org).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ata_raid_jmicron.c,v 1.5 2014/03/25 16:19:13 christos Exp $");

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
ata_raid_jmicron_type(int type)
{
	static char buffer[16];

	switch (type) {
	case JM_T_RAID0:
		return "RAID0";
	case JM_T_RAID1:
		return "RAID1";
	case JM_T_RAID01:
		return "RAID0+1";
	case JM_T_RAID5:
		return "RAID5";
	case JM_T_JBOD:
		return "JBOD";
	default:
		snprintf(buffer, sizeof(buffer), "UNKNOWN 0x%02x", type);
		return buffer;
	}
}

static void
ata_raid_jmicron_print_info(struct jmicron_raid_conf *info)
{
	int i;
  
	printf("****** ATA JMicron Technology Corp Metadata ******\n");
	printf("signature           %.2s\n",	info->signature);
	printf("version             0x%04x\n",	info->version);
	printf("checksum            0x%04x\n",	info->checksum);
	printf("disk_id             0x%08x\n",	info->disk_id);
	printf("offset              0x%08x\n",	info->offset);
	printf("disk_sectors_low    0x%08x\n",	info->disk_sectors_low);
	printf("disk_sectors_high   0x%08x\n",	info->disk_sectors_high);
	printf("name                %.16s\n",	info->name);
	printf("type                %s\n",
	    ata_raid_jmicron_type(info->type));
	printf("stripe_shift        %d\n",	info->stripe_shift);
	printf("flags               0x%04x\n",	info->flags);
	printf("spare:\n");
	for (i = 0; i < 2 && info->spare[i]; i++)
		printf("    %d                  0x%08x\n", i, info->spare[i]);
	printf("disks:\n");
	for (i = 0; i < 8 && info->disks[i]; i++)
		printf("    %d                  0x%08x\n", i, info->disks[i]);
	printf("=================================================\n");
}
#endif

int
ata_raid_read_config_jmicron(struct wd_softc *sc)
{
	struct atabus_softc *atabus;
	struct jmicron_raid_conf *info;
	struct vnode *vp;
	struct ataraid_array_info *aai;
	struct ataraid_disk_info *adi;
	uint64_t disk_size;
	uint32_t drive;
	uint16_t checksum, *ptr;
	int bmajor, error, count, disk, total_disks;
	dev_t dev;

	info = malloc(sizeof(*info), M_DEVBUF, M_WAITOK|M_ZERO);

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

	error = ata_raid_config_block_rw(vp, JMICRON_LBA(sc), info,
	    sizeof(*info), B_READ);
	VOP_CLOSE(vp, FREAD, NOCRED);
	vput(vp);
	if (error) {
		DPRINTF(("%s: error %d reading JMicron config block\n",
		    device_xname(sc->sc_dev), error));
		goto out;
	}

	/* Check for JMicron signature. */
	if (strncmp(info->signature, JMICRON_MAGIC, 2)) {
		DPRINTF(("%s: JMicron RAID signature check failed\n",
		    device_xname(sc->sc_dev)));
		error = ESRCH;
		goto out;
	}

	/* calculate checksum and compare for valid */
	for (checksum = 0, ptr = (uint16_t *)info, count = 0;
	     count < 64; count++)
		checksum += *ptr++;
	if (checksum) {
		DPRINTF(("%s: JMicron checksum failed\n",
		    device_xname(sc->sc_dev)));
		error = ESRCH;
		goto out;
	}

#ifdef ATA_RAID_DEBUG
	ata_raid_jmicron_print_info(info);
#endif
	/*
	 * Check that there aren't stale config blocks without
	 * any array set configured.
	 */
	for (total_disks = 0, disk = 0; disk < JM_MAX_DISKS; disk++)
		if (info->disks[disk] == info->disk_id)
			total_disks++;
	if (total_disks <= 1) {
		error = EINVAL;
		goto out;
	}

	/*
	 * Check volume's state and bail out if it's not acceptable.
	 */
	if ((info->flags & (JM_F_READY|JM_F_BOOTABLE|JM_F_ACTIVE)) == 0) {
		error = EINVAL;
		goto out;
	}

	/*
	 * Lookup or allocate a new array info structure for
	 * this array.
	 */
	aai = ata_raid_get_array_info(ATA_RAID_TYPE_JMICRON, 0); 
	aai->aai_status = AAI_S_READY;

	switch (info->type) {
	case JM_T_RAID0:
		aai->aai_level = AAI_L_RAID0;
		aai->aai_width = total_disks;
		break;
	case JM_T_RAID1:
		aai->aai_level = AAI_L_RAID1;
		aai->aai_width = 1;
		break;
	case JM_T_RAID01:
		aai->aai_level = AAI_L_RAID0 | AAI_L_RAID1;
		aai->aai_width = total_disks / 2;
		break;
	case JM_T_JBOD:
		aai->aai_level = AAI_L_SPAN;
		aai->aai_width = total_disks;
		break;
	default:
		DPRINTF(("%s: unknown JMicron RAID type 0x%02x\n",
		    device_xname(sc->sc_dev), info->type));
		error = EINVAL;
		goto out;
	}

	disk_size = (info->disk_sectors_high << 16) + info->disk_sectors_low;
	aai->aai_type = ATA_RAID_TYPE_JMICRON;
	aai->aai_generation = 0;
	aai->aai_capacity = disk_size * aai->aai_width;
	aai->aai_interleave = 2 << info->stripe_shift;
	aai->aai_ndisks = total_disks;
	aai->aai_heads = 255;
	aai->aai_sectors = 63;
	aai->aai_cylinders =
	    aai->aai_capacity / (aai->aai_heads * aai->aai_sectors);
	aai->aai_offset = info->offset * 16;
	aai->aai_reserved = 2;
	if (info->name)
		strlcpy(aai->aai_name, info->name, sizeof(aai->aai_name));

	atabus = device_private(device_parent(sc->sc_dev));
	drive = atabus->sc_chan->ch_channel;
	if (drive >= aai->aai_ndisks) {
		DPRINTF(("%s: drive number %d doesn't make sense within "
		    "%d-disk array\n", device_xname(sc->sc_dev),
		    drive, aai->aai_ndisks));
		error = EINVAL;
		goto out;
	}

	if (info->disks[drive] == info->disk_id) {
		adi = &aai->aai_disks[drive];
		adi->adi_dev = sc->sc_dev;
		adi->adi_status = ADI_S_ONLINE | ADI_S_ASSIGNED;
		adi->adi_sectors = aai->aai_capacity;
		adi->adi_compsize = disk_size - aai->aai_reserved;
	}

	error = 0;

 out:
	free(info, M_DEVBUF);
	return error;
}
