/*	$NetBSD: ata_raid_promise.c,v 1.11 2008/03/18 20:46:36 cube Exp $	*/

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
 * Support for parsing Promise ATA RAID controller configuration blocks.
 *
 * Adapted to NetBSD by Jason R. Thorpe of Wasabi Systems, Inc.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ata_raid_promise.c,v 1.11 2008/03/18 20:46:36 cube Exp $");

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
ata_raid_read_config_promise(struct wd_softc *sc)
{
	struct promise_raid_conf *info;
	struct vnode *vp;
	int bmajor, error, count;
	u_int disk;
	dev_t dev;
	uint32_t cksum, *ckptr;
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

	error = ata_raid_config_block_rw(vp, PR_LBA(sc), info,
	    sizeof(*info), B_READ);
	VOP_CLOSE(vp, FREAD, NOCRED);
	vput(vp);
	if (error) {
		aprint_error_dev(sc->sc_dev,
		    "error %d reading Promise config block\n", error);
		goto out;
	}

	/* Check the signature. */
	if (strncmp(info->promise_id, PR_MAGIC, sizeof(PR_MAGIC)) != 0) {
		DPRINTF(("%s: Promise signature check failed\n",
		    device_xname(sc->sc_dev)));
		error = ESRCH;
		goto out;
	}

	/* Verify the checksum. */
	for (cksum = 0, ckptr = (uint32_t *) info, count = 0; count < 511;
	     count++)
		cksum += *ckptr++;
	if (cksum != *ckptr) {
		DPRINTF(("%s: Promise checksum failed\n",
		    device_xname(sc->sc_dev)));
		error = ESRCH;
		goto out;
	}

	if (info->raid.integrity != PR_I_VALID) {
		DPRINTF(("%s: Promise config block marked invalid\n",
		    device_xname(sc->sc_dev)));
		error = ESRCH;
		goto out;
	}

	/*
	 * Lookup or allocate a new array info structure for
	 * this array.
	 */
	aai = ata_raid_get_array_info(ATA_RAID_TYPE_PROMISE,
	    info->raid.array_number);

	if (info->raid.generation == 0 ||
	    info->raid.generation > aai->aai_generation) {
		aai->aai_generation = info->raid.generation;
		/* aai_type and aai_arrayno filled in already */
		if ((info->raid.status &
		     (PR_S_VALID | PR_S_ONLINE | PR_S_INITED | PR_S_READY)) ==
		    (PR_S_VALID | PR_S_ONLINE | PR_S_INITED | PR_S_READY)) {
			aai->aai_status |= AAI_S_READY;
			if (info->raid.status & PR_S_DEGRADED)
				aai->aai_status |= AAI_S_DEGRADED;
		} else
			aai->aai_status &= ~AAI_S_READY;

		switch (info->raid.type) {
		case PR_T_RAID0:
			aai->aai_level = AAI_L_RAID0;
			break;

		case PR_T_RAID1:
			aai->aai_level = AAI_L_RAID1;
			if (info->raid.array_width > 1)
				aai->aai_level |= AAI_L_RAID0;
			break;

		case PR_T_SPAN:
			aai->aai_level = AAI_L_SPAN;
			break;

		default:
			aprint_error_dev(sc->sc_dev,
			    "unknown Promise RAID type 0x%02x\n",
			    info->raid.type);
			error = EINVAL;
			goto out;
		}

		aai->aai_interleave = 1U << info->raid.stripe_shift;
		aai->aai_width = info->raid.array_width;
		aai->aai_ndisks = info->raid.total_disks;
		aai->aai_heads = info->raid.heads + 1;
		aai->aai_sectors = info->raid.sectors;
		aai->aai_cylinders = info->raid.cylinders + 1;
		aai->aai_capacity = info->raid.total_sectors;
		aai->aai_offset = 0;
		aai->aai_reserved = 63;

		for (disk = 0; disk < aai->aai_ndisks; disk++) {
			adi = &aai->aai_disks[disk];
			adi->adi_status = 0;
			if (info->raid.disk[disk].flags & PR_F_ONLINE)
				adi->adi_status |= ADI_S_ONLINE;
			if (info->raid.disk[disk].flags & PR_F_ASSIGNED)
				adi->adi_status |= ADI_S_ASSIGNED;
			if (info->raid.disk[disk].flags & PR_F_SPARE) {
				adi->adi_status &= ~ADI_S_ONLINE;
				adi->adi_status |= ADI_S_SPARE;
			}
			if (info->raid.disk[disk].flags &
			    (PR_F_REDIR | PR_F_DOWN))
				adi->adi_status &= ~ADI_S_ONLINE;
		}
	}
	adi = &aai->aai_disks[info->raid.disk_number];
	if (adi->adi_status) {
		adi->adi_dev = sc->sc_dev;
		adi->adi_sectors = info->raid.disk_sectors;
		adi->adi_compsize = sc->sc_capacity - aai->aai_reserved;
	}

	error = 0;

 out:
	free(info, M_DEVBUF);
	return (error);
}
