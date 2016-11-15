/*	$NetBSD: ata_raid_adaptec.c,v 1.9 2008/09/15 11:44:50 tron Exp $	*/

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
 * Support for parsing Adaptec ATA RAID controller configuration blocks.
 *
 * Adapted to NetBSD by Allen K. Briggs
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ata_raid_adaptec.c,v 1.9 2008/09/15 11:44:50 tron Exp $");

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

int
ata_raid_read_config_adaptec(struct wd_softc *sc)
{
	struct adaptec_raid_conf *info;
	struct atabus_softc *atabus;
	struct vnode *vp;
	int bmajor, error;
	dev_t dev;
	uint32_t gen, drive;
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

	error = ata_raid_config_block_rw(vp, ADP_LBA(sc), info,
	    sizeof(*info), B_READ);
	VOP_CLOSE(vp, FREAD, NOCRED);
	vput(vp);
	if (error) {
		aprint_error_dev(sc->sc_dev,
		    "error %d reading Adaptec config block\n", error);
		goto out;
	}

	info->magic_0 = be32toh(info->magic_0);
	info->magic_1 = be32toh(info->magic_1);
	info->magic_2 = be32toh(info->magic_2);
	info->magic_3 = be32toh(info->magic_3);
	info->magic_4 = be32toh(info->magic_4);
	/* Check the signature. */
	if (info->magic_0 != ADP_MAGIC_0 || info->magic_3 != ADP_MAGIC_3) {
		DPRINTF(("%s: Adaptec signature check failed\n",
		    device_xname(sc->sc_dev)));
		error = ESRCH;
		goto out;
	}

	/*
	 * Lookup or allocate a new array info structure for
	 * this array.
	 */
	aai = ata_raid_get_array_info(ATA_RAID_TYPE_ADAPTEC,
		be32toh(info->configs[0].disk_number));

	gen = be32toh(info->generation);

	if (gen == 0 || gen > aai->aai_generation) {
		aai->aai_generation = gen;

		aai->aai_status = AAI_S_READY;

		switch (info->configs[0].type) {
		case ADP_T_RAID0:
			aai->aai_level = AAI_L_RAID0;
			aai->aai_interleave = 
			    (be16toh(info->configs[0].stripe_sectors) >> 1);
			aai->aai_width = be16toh(info->configs[0].total_disks);
			break;

		case ADP_T_RAID1:
			aai->aai_level = AAI_L_RAID1;
			aai->aai_interleave = 0;
			aai->aai_width = be16toh(info->configs[0].total_disks)
			    / 2;
			break;

		default:
			aprint_error_dev(sc->sc_dev,
			    "unknown Adaptec RAID type 0x%02x\n",
			    info->configs[0].type);
			error = EINVAL;
			goto out;
		}

		aai->aai_type = ATA_RAID_TYPE_ADAPTEC;
		aai->aai_ndisks = be16toh(info->configs[0].total_disks);
		aai->aai_capacity = be32toh(info->configs[0].sectors);
		aai->aai_heads = 255;
		aai->aai_sectors = 63;
		aai->aai_cylinders = aai->aai_capacity / (63 * 255);
		aai->aai_offset = 0;
		aai->aai_reserved = 17;
		if (info->configs[0].name)
			strlcpy(aai->aai_name, info->configs[0].name,
			    sizeof(aai->aai_name));

		/* XXX - bogus.  RAID1 shouldn't really have an interleave */
		if (aai->aai_interleave == 0)
			aai->aai_interleave = aai->aai_capacity;
	}

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
	adi->adi_compsize = be32toh(info->configs[drive+1].sectors);

	error = 0;

 out:
	free(info, M_DEVBUF);
	return (error);
}
