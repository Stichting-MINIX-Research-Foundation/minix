/*	$NetBSD: ata_raid_intel.c,v 1.7 2014/03/25 16:19:13 christos Exp $	*/

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
 * Support for parsing Intel MatrixRAID controller configuration blocks.
 *
 * Adapted to NetBSD by Juan Romero Pardines (xtraeme@gmail.org).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ata_raid_intel.c,v 1.7 2014/03/25 16:19:13 christos Exp $");

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

static int find_volume_id(struct intel_raid_conf *);


#ifdef ATA_RAID_DEBUG
static const char *
ata_raid_intel_type(int type)
{
	static char buffer[16];

	switch (type) {
	case INTEL_T_RAID0:
		return "RAID0";
	case INTEL_T_RAID1:
		return "RAID1";
	case INTEL_T_RAID5:
		return "RAID5";
	default:
		snprintf(buffer, sizeof(buffer), "UNKNOWN 0x%02x", type);
		return buffer;
	}
}

static void
ata_raid_intel_print_info(struct intel_raid_conf *info)
{
	struct intel_raid_mapping *map;
	int i, j;
 
	printf("********* ATA Intel MatrixRAID Metadata *********\n");
	printf("intel_id		<%.24s>\n", info->intel_id);
	printf("version			<%.6s>\n", info->version);
	printf("checksum		0x%08x\n", info->checksum);
	printf("config_size		0x%08x\n", info->config_size);
	printf("config_id		0x%08x\n", info->config_id);
	printf("generation		0x%08x\n", info->generation);
	printf("total_disks		%u\n", info->total_disks);
	printf("total_volumes		%u\n", info->total_volumes);
	printf("DISK#	serial	disk	sectors	disk_id	flags\n");
	for (i = 0; i < info->total_disks; i++) {
		printf("    %d <%.16s> %u 0x%08x 0x%08x\n",
		    i, info->disk[i].serial, info->disk[i].sectors,
		    info->disk[i].id, info->disk[i].flags);
	}

	map = (struct intel_raid_mapping *)&info->disk[info->total_disks];
	for (j = 0; j < info->total_volumes; j++) {
		printf("name		%.16s\n", map->name);
		printf("total_sectors	%ju\n", map->total_sectors);
		printf("state		%u\n", map->state);
		printf("reserved	%u\n", map->reserved);
		printf("offset		%u\n", map->offset);
		printf("disk_sectors	%u\n", map->disk_sectors);
		printf("stripe_count	%u\n", map->stripe_count);
		printf("stripe_sectors	%u\n", map->stripe_sectors);
		printf("status		%u\n", map->status);
		printf("type		%s\n", ata_raid_intel_type(map->type));
		printf("total_disks	%u\n", map->total_disks);
		printf("magic[0]	0x%02x\n", map->magic[0]);
		printf("magic[1]	0x%02x\n", map->magic[1]);
		printf("magic[2]	0x%02x\n", map->magic[2]);
		for (i = 0; i < map->total_disks; i++)
			printf("    disk %d at disk_idx 0x%08x\n",
			    i, map->disk_idx[i]);

		map = (struct intel_raid_mapping *)
		    &map->disk_idx[map->total_disks];
	}
	printf("=================================================\n");
}
#endif

int
ata_raid_read_config_intel(struct wd_softc *sc)
{
	struct intel_raid_conf *info;
	struct intel_raid_mapping *map;
	struct ataraid_array_info *aai;
	struct ataraid_disk_info *adi;
	struct vnode *vp;
	uint32_t checksum, *ptr;
	int bmajor, count, curvol = 0, error = 0;
	char *tmp;
	dev_t dev;
	int volumeid, diskidx;

	info = malloc(1536, M_DEVBUF, M_WAITOK|M_ZERO);

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

	error = ata_raid_config_block_rw(vp, INTEL_LBA(sc), info,
	    1024, B_READ);
	VOP_CLOSE(vp, FREAD, NOCRED);
	vput(vp);
	if (error) {
		DPRINTF(("%s: error %d reading Intel MatrixRAID config block\n",
		    device_xname(sc->sc_dev), error));
		goto out;
	}

	tmp = (char *)info;
	(void)memcpy(tmp + 1024, tmp, 512);
	(void)memcpy(tmp, tmp + 512, 1024);
	(void)memset(tmp + 1024, 0, 512);

	/* Check if this is a Intel RAID struct */
	if (strncmp(info->intel_id, INTEL_MAGIC, strlen(INTEL_MAGIC))) {
		DPRINTF(("%s: Intel MatrixRAID signature check failed\n",
		    device_xname(sc->sc_dev)));
		error = ESRCH;
		goto out;
	}

	/* calculate checksum and compare for valid */
	for (checksum = 0, ptr = (uint32_t *)info, count = 0;
	     count < (info->config_size / sizeof(uint32_t)); count++)
		checksum += *ptr++;

	checksum -= info->checksum;
	if (checksum != info->checksum) {
		DPRINTF(("%s: Intel MatrixRAID checksum failed 0x%x != 0x%x\n",
		    device_xname(sc->sc_dev), checksum, info->checksum));
		error = ESRCH;
		goto out;
	}

#ifdef ATA_RAID_DEBUG
	ata_raid_intel_print_info(info);
#endif

	/* This one points to the first volume */
	map = (struct intel_raid_mapping *)&info->disk[info->total_disks];

	volumeid = find_volume_id(info);
	if (volumeid < 0) {
		aprint_error_dev(sc->sc_dev,
				 "too many RAID arrays\n");
		error = ENOMEM;
		goto out;
	}

findvol:
	/*
	 * Lookup or allocate a new array info structure for this array.
	 */
	aai = ata_raid_get_array_info(ATA_RAID_TYPE_INTEL, volumeid + curvol); 

	/* Fill in array info */
	aai->aai_generation = info->generation;
	aai->aai_status = AAI_S_READY;

	switch (map->type) {
	case INTEL_T_RAID0:
		aai->aai_level = AAI_L_RAID0;
		aai->aai_width = map->total_disks;
		break;
	case INTEL_T_RAID1:
		aai->aai_level = AAI_L_RAID1;
		aai->aai_width = 1;
		break;
	default:
		DPRINTF(("%s: unknown Intel MatrixRAID type 0x%02x\n",
		    device_xname(sc->sc_dev), map->type));
		error = EINVAL;
		goto out;
	}

	switch (map->state) {
	case INTEL_S_DEGRADED:
		aai->aai_status |= AAI_S_DEGRADED;
		break;
	case INTEL_S_DISABLED:
	case INTEL_S_FAILURE:
		aai->aai_status &= ~AAI_S_READY;
		break;
	}

	aai->aai_type = ATA_RAID_TYPE_INTEL;
	aai->aai_capacity = map->total_sectors;
	aai->aai_interleave = map->stripe_sectors;
	aai->aai_ndisks = map->total_disks;
	aai->aai_heads = 255;
	aai->aai_sectors = 63;
	aai->aai_cylinders =
	    aai->aai_capacity / (aai->aai_heads * aai->aai_sectors);
	aai->aai_offset = map->offset;
	aai->aai_reserved = 3;
	if (map->name)
		strlcpy(aai->aai_name, map->name, sizeof(aai->aai_name));

	/* Fill in disk info */
	diskidx = aai->aai_curdisk++;
	adi = &aai->aai_disks[diskidx];
	adi->adi_status = 0;

	if (info->disk[diskidx].flags & INTEL_F_ONLINE)
		adi->adi_status |= ADI_S_ONLINE;
	if (info->disk[diskidx].flags & INTEL_F_ASSIGNED)
		adi->adi_status |= ADI_S_ASSIGNED;
	if (info->disk[diskidx].flags & INTEL_F_SPARE) {
		adi->adi_status &= ~ADI_S_ONLINE;
		adi->adi_status |= ADI_S_SPARE;
	}
	if (info->disk[diskidx].flags & INTEL_F_DOWN)
		adi->adi_status &= ~ADI_S_ONLINE;

	if (adi->adi_status) {
		adi->adi_dev = sc->sc_dev;
		adi->adi_sectors = info->disk[diskidx].sectors;
		adi->adi_compsize = adi->adi_sectors - aai->aai_reserved;

		/*
		 * Check if that is the only volume, otherwise repeat
		 * the process to find more.
		 */
		if ((curvol + 1) < info->total_volumes) {
			curvol++;
			map = (struct intel_raid_mapping *)
			    &map->disk_idx[map->total_disks];
			goto findvol;
		}
	}

 out:
	free(info, M_DEVBUF);
	return error;
}


/*
 * Assign `volume id' to RAID volumes.
 */
static struct {
	/* We assume disks are on the same array if these three values
	   are same. */
	uint32_t config_id;
	uint32_t generation;
	uint32_t checksum;

	int id;
} array_note[10]; /* XXX: this array is not used after ld_ataraid is
		   * configured. */

static int n_array = 0;
static int volume_id = 0;

static int
find_volume_id(struct intel_raid_conf *info)
{
	int i, ret;

	for (i=0; i < n_array; ++i) {
		if (info->checksum == array_note[i].checksum &&
		    info->config_id == array_note[i].config_id &&
		    info->generation == array_note[i].generation) {
			/* we have already seen this array */
			return array_note[i].id;
		}
	}

	if (n_array >= __arraycount(array_note)) {
		/* Too many arrays */
		return -1;
	}

	array_note[n_array].checksum = info->checksum;
	array_note[n_array].config_id = info->config_id;
	array_note[n_array].generation = info->generation;
	array_note[n_array].id = ret = volume_id;

	/* Allocate volume ids for all volumes in this array */
	volume_id += info->total_volumes;
	++n_array;
	return ret;
}
