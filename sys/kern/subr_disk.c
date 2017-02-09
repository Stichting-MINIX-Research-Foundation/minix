/*	$NetBSD: subr_disk.c,v 1.113 2015/05/14 17:31:24 chs Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1999, 2000, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ufs_disksubr.c	8.5 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_disk.c,v 1.113 2015/05/14 17:31:24 chs Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/syslog.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/sysctl.h>
#include <lib/libkern/libkern.h>

/*
 * Compute checksum for disk label.
 */
u_int
dkcksum(struct disklabel *lp)
{

	return dkcksum_sized(lp, lp->d_npartitions);
}

u_int
dkcksum_sized(struct disklabel *lp, size_t npartitions)
{
	uint16_t *start, *end;
	uint16_t sum = 0;

	start = (uint16_t *)lp;
	end = (uint16_t *)&lp->d_partitions[npartitions];
	while (start < end)
		sum ^= *start++;
	return sum;
}

/*
 * Disk error is the preface to plaintive error messages
 * about failing disk transfers.  It prints messages of the form

hp0g: hard error reading fsbn 12345 of 12344-12347 (hp0 bn %d cn %d tn %d sn %d)

 * if the offset of the error in the transfer and a disk label
 * are both available.  blkdone should be -1 if the position of the error
 * is unknown; the disklabel pointer may be null from drivers that have not
 * been converted to use them.  The message is printed with printf
 * if pri is LOG_PRINTF, otherwise it uses log at the specified priority.
 * The message should be completed (with at least a newline) with printf
 * or addlog, respectively.  There is no trailing space.
 */
#ifndef PRIdaddr
#define PRIdaddr PRId64
#endif
void
diskerr(const struct buf *bp, const char *dname, const char *what, int pri,
    int blkdone, const struct disklabel *lp)
{
	int unit = DISKUNIT(bp->b_dev), part = DISKPART(bp->b_dev);
	void (*pr)(const char *, ...) __printflike(1, 2);
	char partname = 'a' + part;
	daddr_t sn;

	if (/*CONSTCOND*/0)
		/* Compiler will error this is the format is wrong... */
		printf("%" PRIdaddr, bp->b_blkno);

	if (pri != LOG_PRINTF) {
		static const char fmt[] = "";
		log(pri, fmt);
		pr = addlog;
	} else
		pr = printf;
	(*pr)("%s%d%c: %s %sing fsbn ", dname, unit, partname, what,
	    bp->b_flags & B_READ ? "read" : "writ");
	sn = bp->b_blkno;
	if (bp->b_bcount <= DEV_BSIZE)
		(*pr)("%" PRIdaddr, sn);
	else {
		if (blkdone >= 0) {
			sn += blkdone;
			(*pr)("%" PRIdaddr " of ", sn);
		}
		(*pr)("%" PRIdaddr "-%" PRIdaddr "", bp->b_blkno,
		    bp->b_blkno + (bp->b_bcount - 1) / DEV_BSIZE);
	}
	if (lp && (blkdone >= 0 || bp->b_bcount <= lp->d_secsize)) {
		sn += lp->d_partitions[part].p_offset;
		(*pr)(" (%s%d bn %" PRIdaddr "; cn %" PRIdaddr "",
		    dname, unit, sn, sn / lp->d_secpercyl);
		sn %= lp->d_secpercyl;
		(*pr)(" tn %" PRIdaddr " sn %" PRIdaddr ")",
		    sn / lp->d_nsectors, sn % lp->d_nsectors);
	}
}

/*
 * Searches the iostatlist for the disk corresponding to the
 * name provided.
 */
struct disk *
disk_find(const char *name)
{
	struct io_stats *stat;

	stat = iostat_find(name);

	if ((stat != NULL) && (stat->io_type == IOSTAT_DISK))
		return stat->io_parent;

	return (NULL);
}

void
disk_init(struct disk *diskp, const char *name, const struct dkdriver *driver)
{
	u_int blocksize = DEV_BSIZE;

	/*
	 * Initialize the wedge-related locks and other fields.
	 */
	mutex_init(&diskp->dk_rawlock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&diskp->dk_openlock, MUTEX_DEFAULT, IPL_NONE);
	LIST_INIT(&diskp->dk_wedges);
	diskp->dk_nwedges = 0;
	diskp->dk_labelsector = LABELSECTOR;
	diskp->dk_blkshift = DK_BSIZE2BLKSHIFT(blocksize);
	diskp->dk_byteshift = DK_BSIZE2BYTESHIFT(blocksize);
	diskp->dk_name = name;
	diskp->dk_driver = driver;
}

/*
 * Attach a disk.
 */
void
disk_attach(struct disk *diskp)
{

	/*
	 * Allocate and initialize the disklabel structures.
	 */
	diskp->dk_label = kmem_zalloc(sizeof(struct disklabel), KM_SLEEP);
	diskp->dk_cpulabel = kmem_zalloc(sizeof(struct cpu_disklabel),
	    KM_SLEEP);
	if ((diskp->dk_label == NULL) || (diskp->dk_cpulabel == NULL))
		panic("disk_attach: can't allocate storage for disklabel");

	/*
	 * Set up the stats collection.
	 */
	diskp->dk_stats = iostat_alloc(IOSTAT_DISK, diskp, diskp->dk_name);
}

int
disk_begindetach(struct disk *dk, int (*lastclose)(device_t),
    device_t self, int flags)
{
	int rc;

	rc = 0;
	mutex_enter(&dk->dk_openlock);
	if (dk->dk_openmask == 0)
		;	/* nothing to do */
	else if ((flags & DETACH_FORCE) == 0)
		rc = EBUSY;
	else if (lastclose != NULL)
		rc = (*lastclose)(self);
	mutex_exit(&dk->dk_openlock);

	return rc;
}

/*
 * Detach a disk.
 */
void
disk_detach(struct disk *diskp)
{

	/*
	 * Remove from the drivelist.
	 */
	iostat_free(diskp->dk_stats);

	/*
	 * Release the disk-info dictionary.
	 */
	if (diskp->dk_info) {
		prop_object_release(diskp->dk_info);
		diskp->dk_info = NULL;
	}

	/*
	 * Free the space used by the disklabel structures.
	 */
	kmem_free(diskp->dk_label, sizeof(*diskp->dk_label));
	kmem_free(diskp->dk_cpulabel, sizeof(*diskp->dk_cpulabel));
}

void
disk_destroy(struct disk *diskp)
{

	mutex_destroy(&diskp->dk_openlock);
	mutex_destroy(&diskp->dk_rawlock);
}

/*
 * Mark the disk as busy for metrics collection.
 */
void
disk_busy(struct disk *diskp)
{

	iostat_busy(diskp->dk_stats);
}

/*
 * Finished disk operations, gather metrics.
 */
void
disk_unbusy(struct disk *diskp, long bcount, int read)
{

	iostat_unbusy(diskp->dk_stats, bcount, read);
}

/*
 * Return true if disk has an I/O operation in flight.
 */
bool
disk_isbusy(struct disk *diskp)
{

	return iostat_isbusy(diskp->dk_stats);
}

/*
 * Bounds checking against the media size, used for the raw partition.
 * secsize, mediasize and b_blkno must all be the same units.
 * Possibly this has to be DEV_BSIZE (512).
 */
int
bounds_check_with_mediasize(struct buf *bp, int secsize, uint64_t mediasize)
{
	int64_t sz;

	if (bp->b_blkno < 0) {
		/* Reject negative offsets immediately. */
		bp->b_error = EINVAL;
		return 0;
	}

	sz = howmany((int64_t)bp->b_bcount, secsize);

	/*
	 * bp->b_bcount is a 32-bit value, and we rejected a negative
	 * bp->b_blkno already, so "bp->b_blkno + sz" cannot overflow.
	 */

	if (bp->b_blkno + sz > mediasize) {
		sz = mediasize - bp->b_blkno;
		if (sz == 0) {
			/* If exactly at end of disk, return EOF. */
			bp->b_resid = bp->b_bcount;
			return 0;
		}
		if (sz < 0) {
			/* If past end of disk, return EINVAL. */
			bp->b_error = EINVAL;
			return 0;
		}
		/* Otherwise, truncate request. */
		bp->b_bcount = sz * secsize;
	}

	return 1;
}

/*
 * Determine the size of the transfer, and make sure it is
 * within the boundaries of the partition. Adjust transfer
 * if needed, and signal errors or early completion.
 */
int
bounds_check_with_label(struct disk *dk, struct buf *bp, int wlabel)
{
	struct disklabel *lp = dk->dk_label;
	struct partition *p = lp->d_partitions + DISKPART(bp->b_dev);
	uint64_t p_size, p_offset, labelsector;
	int64_t sz;

	if (bp->b_blkno < 0) {
		/* Reject negative offsets immediately. */
		bp->b_error = EINVAL;
		return -1;
	}

	/* Protect against division by zero. XXX: Should never happen?!?! */
	if (lp->d_secpercyl == 0) {
		bp->b_error = EINVAL;
		return -1;
	}

	p_size = (uint64_t)p->p_size << dk->dk_blkshift;
	p_offset = (uint64_t)p->p_offset << dk->dk_blkshift;
#if RAW_PART == 3
	labelsector = lp->d_partitions[2].p_offset;
#else
	labelsector = lp->d_partitions[RAW_PART].p_offset;
#endif
	labelsector = (labelsector + dk->dk_labelsector) << dk->dk_blkshift;

	sz = howmany((int64_t)bp->b_bcount, DEV_BSIZE);

	/*
	 * bp->b_bcount is a 32-bit value, and we rejected a negative
	 * bp->b_blkno already, so "bp->b_blkno + sz" cannot overflow.
	 */

	if (bp->b_blkno + sz > p_size) {
		sz = p_size - bp->b_blkno;
		if (sz == 0) {
			/* If exactly at end of disk, return EOF. */
			bp->b_resid = bp->b_bcount;
			return 0;
		}
		if (sz < 0) {
			/* If past end of disk, return EINVAL. */
			bp->b_error = EINVAL;
			return -1;
		}
		/* Otherwise, truncate request. */
		bp->b_bcount = sz << DEV_BSHIFT;
	}

	/* Overwriting disk label? */
	if (bp->b_blkno + p_offset <= labelsector &&
	    bp->b_blkno + p_offset + sz > labelsector &&
	    (bp->b_flags & B_READ) == 0 && !wlabel) {
		bp->b_error = EROFS;
		return -1;
	}

	/* calculate cylinder for disksort to order transfers with */
	bp->b_cylinder = (bp->b_blkno + p->p_offset) /
	    (lp->d_secsize / DEV_BSIZE) / lp->d_secpercyl;
	return 1;
}

int
disk_read_sectors(void (*strat)(struct buf *), const struct disklabel *lp,
    struct buf *bp, unsigned int sector, int count)
{
	bp->b_blkno = sector;
	bp->b_bcount = count * lp->d_secsize;
	bp->b_flags = (bp->b_flags & ~B_WRITE) | B_READ;
	bp->b_oflags &= ~BO_DONE;
	bp->b_cylinder = sector / lp->d_secpercyl;
	(*strat)(bp);
	return biowait(bp);
}

const char *
convertdisklabel(struct disklabel *lp, void (*strat)(struct buf *),
    struct buf *bp, uint32_t secperunit)
{
	struct partition rp, *altp, *p;
	int geom_ok;

	memset(&rp, 0, sizeof(rp));
	rp.p_size = secperunit;
	rp.p_fstype = FS_UNUSED;

	/* If we can seek to d_secperunit - 1, believe the disk geometry. */
	if (secperunit != 0 &&
	    disk_read_sectors(strat, lp, bp, secperunit - 1, 1) == 0)
		geom_ok = 1;
	else
		geom_ok = 0;

#if 0
	printf("%s: secperunit (%" PRIu32 ") %s\n", __func__,
	    secperunit, geom_ok ? "ok" : "not ok");
#endif

	p = &lp->d_partitions[RAW_PART];
	if (RAW_PART == 'c' - 'a')
		altp = &lp->d_partitions['d' - 'a'];
	else
		altp = &lp->d_partitions['c' - 'a'];

	if (lp->d_npartitions > RAW_PART && p->p_offset == 0 && p->p_size != 0)
		;	/* already a raw partition */
	else if (lp->d_npartitions > MAX('c', 'd') - 'a' &&
		 altp->p_offset == 0 && altp->p_size != 0) {
		/* alternate partition ('c' or 'd') is suitable for raw slot,
		 * swap with 'd' or 'c'.
		 */
		rp = *p;
		*p = *altp;
		*altp = rp;
	} else if (lp->d_npartitions <= RAW_PART &&
	           lp->d_npartitions > 'c' - 'a') {
		/* No raw partition is present, but the alternate is present.
		 * Copy alternate to raw partition.
		 */
		lp->d_npartitions = RAW_PART + 1;
		*p = *altp;
	} else if (!geom_ok)
		return "no raw partition and disk reports bad geometry";
	else if (lp->d_npartitions <= RAW_PART) {
		memset(&lp->d_partitions[lp->d_npartitions], 0,
		    sizeof(struct partition) * (RAW_PART - lp->d_npartitions));
		*p = rp;
		lp->d_npartitions = RAW_PART + 1;
	} else if (lp->d_npartitions < MAXPARTITIONS) {
		memmove(p + 1, p,
		    sizeof(struct partition) * (lp->d_npartitions - RAW_PART));
		*p = rp;
		lp->d_npartitions++;
	} else
		return "no raw partition and partition table is full";
	return NULL;
}

/*
 * disk_ioctl --
 *	Generic disk ioctl handling.
 */
int
disk_ioctl(struct disk *dk, dev_t dev, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	struct dkwedge_info *dkw;
	struct partinfo *pt;
#ifdef __HAVE_OLD_DISKLABEL
	struct disklabel newlabel;
#endif

	switch (cmd) {
	case DIOCGDISKINFO:
		if (dk->dk_info == NULL)
			return ENOTSUP;
		return prop_dictionary_copyout_ioctl(data, cmd, dk->dk_info);

	case DIOCGSECTORSIZE:
		*(u_int *)data = dk->dk_geom.dg_secsize;
		return 0;

	case DIOCGMEDIASIZE:
		*(off_t *)data = (off_t)dk->dk_geom.dg_secsize *
		    dk->dk_geom.dg_secperunit;
		return 0;
	default:
		break;
	}

	if (dev == NODEV)
		return EPASSTHROUGH;

	/* The following should be moved to dk_ioctl */
	switch (cmd) {
	case DIOCGDINFO:
		memcpy(data, dk->dk_label, sizeof (*dk->dk_label));
		return 0;

#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCGDINFO:
		memcpy(&newlabel, dk->dk_label, sizeof(newlabel));
		if (newlabel.d_npartitions > OLDMAXPARTITIONS)
			return ENOTTY;
		memcpy(data, &newlabel, sizeof(struct olddisklabel));
		return 0;
#endif

	case DIOCGPART:
		if (dk->dk_label == NULL)
			return EBUSY;
		pt = data;
		pt->disklab = dk->dk_label;
		pt->part = &dk->dk_label->d_partitions[DISKPART(dev)];
		return 0;

	case DIOCAWEDGE:
		if ((flag & FWRITE) == 0)
			return EBADF;

		dkw = data;
		strlcpy(dkw->dkw_parent, dk->dk_name, sizeof(dkw->dkw_parent));
		return dkwedge_add(dkw);

	case DIOCDWEDGE:
		if ((flag & FWRITE) == 0)
			return EBADF;

		dkw = data;
		strlcpy(dkw->dkw_parent, dk->dk_name, sizeof(dkw->dkw_parent));
		return dkwedge_del(dkw);

	case DIOCLWEDGES:
		return dkwedge_list(dk, data, l);

	case DIOCMWEDGES:
		if ((flag & FWRITE) == 0)
			return EBADF;

		dkwedge_discover(dk);
		return 0;

	default:
		return EPASSTHROUGH;
	}
}

void
disk_set_info(device_t dev, struct disk *dk, const char *type)
{
	struct disk_geom *dg = &dk->dk_geom;

	if (dg->dg_secsize == 0) {
#ifdef DIAGNOSTIC
		printf("%s: fixing 0 sector size\n", dk->dk_name);
#endif
		dg->dg_secsize = DEV_BSIZE;
	}

	dk->dk_blkshift = DK_BSIZE2BLKSHIFT(dg->dg_secsize);
	dk->dk_byteshift = DK_BSIZE2BYTESHIFT(dg->dg_secsize);

	if (dg->dg_secperunit == 0 && dg->dg_ncylinders == 0) {
#ifdef DIAGNOSTIC
		printf("%s: secperunit and ncylinders are zero\n", dk->dk_name);
#endif
		return;
	}

	if (dg->dg_secperunit == 0) {
		if (dg->dg_nsectors == 0 || dg->dg_ntracks == 0) {
#ifdef DIAGNOSTIC
			printf("%s: secperunit and (sectors or tracks) "
			    "are zero\n", dk->dk_name);
#endif
			return;
		}
		dg->dg_secperunit = (int64_t) dg->dg_nsectors *
		    dg->dg_ntracks * dg->dg_ncylinders;
	}

	if (dg->dg_ncylinders == 0) {
		if (dg->dg_ntracks && dg->dg_nsectors)
			dg->dg_ncylinders = dg->dg_secperunit /
			    (dg->dg_ntracks * dg->dg_nsectors);
	}

	prop_dictionary_t disk_info, odisk_info, geom;

	disk_info = prop_dictionary_create();
	geom = prop_dictionary_create();

	prop_dictionary_set_uint64(geom, "sectors-per-unit",
	    dg->dg_secperunit);

	prop_dictionary_set_uint32(geom, "sector-size", dg->dg_secsize);

	if (dg->dg_nsectors)
		prop_dictionary_set_uint16(geom, "sectors-per-track",
		    dg->dg_nsectors);

	if (dg->dg_ntracks)
		prop_dictionary_set_uint16(geom, "tracks-per-cylinder",
		    dg->dg_ntracks);

	if (dg->dg_ncylinders)
		prop_dictionary_set_uint64(geom, "cylinders-per-unit",
		    dg->dg_ncylinders);

	prop_dictionary_set(disk_info, "geometry", geom);

	if (type)
		prop_dictionary_set_cstring_nocopy(disk_info, "type", type);

	prop_object_release(geom);

	odisk_info = dk->dk_info;
	dk->dk_info = disk_info;

	if (dev)
		prop_dictionary_set(device_properties(dev), "disk-info",
		    disk_info);

	/*
	 * Don't release disk_info here; we keep a reference to it.
	 * disk_detach() will release it when we go away.
	 */
	if (odisk_info)
		prop_object_release(odisk_info);
}
