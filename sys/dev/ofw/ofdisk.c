/*	$NetBSD: ofdisk.c,v 1.51 2015/04/26 15:15:20 mlelstv Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
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
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ofdisk.c,v 1.51 2015/04/26 15:15:20 mlelstv Exp $");

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <dev/ofw/openfirm.h>

struct ofdisk_softc {
	device_t sc_dev;
	int sc_phandle;
	int sc_unit;
	int sc_flags;
	struct disk sc_dk;
	int sc_ihandle;
	u_long max_transfer;
};

/* sc_flags */
#define OFDF_ISFLOPPY	0x01		/* we are a floppy drive */

#define	OFDISK_FLOPPY_P(of)		((of)->sc_flags & OFDF_ISFLOPPY)

static int ofdisk_match (device_t, cfdata_t, void *);
static void ofdisk_attach (device_t, device_t, void *);

CFATTACH_DECL_NEW(ofdisk, sizeof(struct ofdisk_softc),
    ofdisk_match, ofdisk_attach, NULL, NULL);

extern struct cfdriver ofdisk_cd;

dev_type_open(ofdisk_open);
dev_type_close(ofdisk_close);
dev_type_read(ofdisk_read);
dev_type_write(ofdisk_write);
dev_type_ioctl(ofdisk_ioctl);
dev_type_strategy(ofdisk_strategy);
dev_type_dump(ofdisk_dump);
dev_type_size(ofdisk_size);

const struct bdevsw ofdisk_bdevsw = {
	.d_open = ofdisk_open,
	.d_close = ofdisk_close,
	.d_strategy = ofdisk_strategy,
	.d_ioctl = ofdisk_ioctl,
	.d_dump = ofdisk_dump,
	.d_psize = ofdisk_size,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

const struct cdevsw ofdisk_cdevsw = {
	.d_open = ofdisk_open,
	.d_close = ofdisk_close,
	.d_read = ofdisk_read,
	.d_write = ofdisk_write,
	.d_ioctl = ofdisk_ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

static void ofminphys(struct buf *);

struct dkdriver ofdisk_dkdriver = {
	.d_strategy = ofdisk_strategy,
	.d_minphys = ofminphys
};

void ofdisk_getdefaultlabel (struct ofdisk_softc *, struct disklabel *);
void ofdisk_getdisklabel (dev_t);

static int
ofdisk_match(device_t parent, cfdata_t match, void *aux)
{
	struct ofbus_attach_args *oba = aux;
	char type[8];
	int l;

	if (strcmp(oba->oba_busname, "ofw"))
		return (0);
	if ((l = OF_getprop(oba->oba_phandle, "device_type", type,
	    sizeof type - 1)) < 0)
		return 0;
	if (l >= sizeof type)
		return 0;
	type[l] = 0;
	return !strcmp(type, "block");
}

static void
ofdisk_attach(device_t parent, device_t self, void *aux)
{
	struct ofdisk_softc *of = device_private(self);
	struct ofbus_attach_args *oba = aux;
	char child[64];
	int l;

	of->sc_dev = self;
	if ((l = OF_getprop(oba->oba_phandle, "name", child,
	    sizeof child - 1)) < 0)
		panic("device without name?");
	if (l >= sizeof child)
		l = sizeof child - 1;
	child[l] = 0;

	of->sc_flags = 0;
	of->sc_phandle = oba->oba_phandle;
	of->sc_unit = oba->oba_unit;
	of->sc_ihandle = 0;
	disk_init(&of->sc_dk, device_xname(of->sc_dev), &ofdisk_dkdriver);
	disk_attach(&of->sc_dk);
	printf("\n");

	if (strcmp(child, "floppy") == 0)
		of->sc_flags |= OFDF_ISFLOPPY;
	else {
		/* Discover wedges on this disk. */
		dkwedge_discover(&of->sc_dk);
	}
}

int
ofdisk_open(dev_t dev, int flags, int fmt, struct lwp *lwp)
{
	struct ofdisk_softc *of;
	char path[256];
	int error, l, part;

	of = device_lookup_private(&ofdisk_cd, DISKUNIT(dev));
	if (of == NULL)
		return ENXIO;

	part = DISKPART(dev);

	mutex_enter(&of->sc_dk.dk_openlock);

	/*
	 * If there are wedges, and this is not RAW_PART, then we
	 * need to fail.
	 */
	if (of->sc_dk.dk_nwedges != 0 && part != RAW_PART) {
		error = EBUSY;
		goto bad1;
	}

	if (!of->sc_ihandle) {
		if ((l = OF_package_to_path(of->sc_phandle, path,
		    sizeof path - 3)) < 0 ||
		    l >= sizeof path - 3) {
			error = ENXIO;
			goto bad1;
		}
		path[l] = 0;

		/*
		 * XXX This is for the benefit of SCSI/IDE disks that don't
		 * XXX have all their childs in the device tree.
		 * XXX YES, I DO THINK THIS IS A BUG IN OPENFIRMWARE!!!
		 * XXX And yes, this is a very gross hack!
		 * XXX See also ofscsi.c
		 */
		if (!strcmp(path + l - 4, "disk")) {
			path[l++] = '@';
			path[l++] = '0' + of->sc_unit;
			path[l] = 0;
		}

		strlcat(path, ":0", sizeof(path));

		if ((of->sc_ihandle = OF_open(path)) == -1) {
			error = ENXIO;
			goto bad1;
		}

		/*
		 * Try to get characteristics of the disk.
		 */
		of->max_transfer = OF_call_method_1("max-transfer",
		    of->sc_ihandle, 0);
		if (of->max_transfer > MAXPHYS)
			of->max_transfer = MAXPHYS;

		ofdisk_getdisklabel(dev);
	}

	switch (fmt) {
	case S_IFCHR:
		of->sc_dk.dk_copenmask |= 1 << part;
		break;
	case S_IFBLK:
		of->sc_dk.dk_bopenmask |= 1 << part;
		break;
	}
	of->sc_dk.dk_openmask =
	    of->sc_dk.dk_copenmask | of->sc_dk.dk_bopenmask;


	error = 0;
 bad1:
	mutex_exit(&of->sc_dk.dk_openlock);
	return (error);
}

int
ofdisk_close(dev_t dev, int flags, int fmt, struct lwp *l)
{
	struct ofdisk_softc *of =
		device_lookup_private(&ofdisk_cd, DISKUNIT(dev));

	mutex_enter(&of->sc_dk.dk_openlock);

	switch (fmt) {
	case S_IFCHR:
		of->sc_dk.dk_copenmask &= ~(1 << DISKPART(dev));
		break;
	case S_IFBLK:
		of->sc_dk.dk_bopenmask &= ~(1 << DISKPART(dev));
		break;
	}
	of->sc_dk.dk_openmask = of->sc_dk.dk_copenmask | of->sc_dk.dk_bopenmask;

#ifdef	FIRMWORKSBUGS
	/*
	 * This is a hack to get the firmware to flush its buffers.
	 */
	OF_seek(of->sc_ihandle, 0);
#endif
	if (!of->sc_dk.dk_openmask) {
		OF_close(of->sc_ihandle);
		of->sc_ihandle = 0;
	}

	mutex_exit(&of->sc_dk.dk_openlock);
	return 0;
}

void
ofdisk_strategy(struct buf *bp)
{
	struct ofdisk_softc *of =
		device_lookup_private(&ofdisk_cd, DISKUNIT(bp->b_dev));
	struct partition *p;
	u_quad_t off;
	int read;
	int (*OF_io)(int, void *, int);
	daddr_t blkno = bp->b_blkno;

	bp->b_resid = 0;
	if (bp->b_bcount == 0)
		goto done;

	OF_io = bp->b_flags & B_READ ? OF_read : 
		(int(*)(int, void*, int))OF_write;

	if (DISKPART(bp->b_dev) != RAW_PART) {
		if (bounds_check_with_label(&of->sc_dk, bp, 0) <= 0) {
			bp->b_resid = bp->b_bcount;
			goto done;
		}
		p = &of->sc_dk.dk_label->d_partitions[DISKPART(bp->b_dev)];
		blkno = bp->b_blkno + p->p_offset;
	}

	disk_busy(&of->sc_dk);

	off = (u_quad_t)blkno * DEV_BSIZE;
	read = -1;
	do {
		if (OF_seek(of->sc_ihandle, off) < 0)
			break;
		read = OF_io(of->sc_ihandle, bp->b_data, bp->b_bcount);
	} while (read == -2);

	if (read < 0) {
		bp->b_error = EIO;
		bp->b_resid = bp->b_bcount;
	} else
		bp->b_resid = bp->b_bcount - read;

	disk_unbusy(&of->sc_dk, bp->b_bcount - bp->b_resid,
	    (bp->b_flags & B_READ));

done:
	biodone(bp);
}

static void
ofminphys(struct buf *bp)
{
	struct ofdisk_softc *of =
		device_lookup_private(&ofdisk_cd, DISKUNIT(bp->b_dev));

	if (bp->b_bcount > of->max_transfer)
		bp->b_bcount = of->max_transfer;
}

int
ofdisk_read(dev_t dev, struct uio *uio, int flags)
{
	return physio(ofdisk_strategy, NULL, dev, B_READ, ofminphys, uio);
}

int
ofdisk_write(dev_t dev, struct uio *uio, int flags)
{
	return physio(ofdisk_strategy, NULL, dev, B_WRITE, ofminphys, uio);
}

int
ofdisk_ioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct ofdisk_softc *of =
		device_lookup_private(&ofdisk_cd, DISKUNIT(dev));
	int error;
#ifdef __HAVE_OLD_DISKLABEL
	struct disklabel newlabel;
#endif
	/* XXX: Why not allow wedges on floppy? */
	switch (cmd) {
	case DIOCDWEDGE:
	case DIOCAWEDGE:
	case DIOCLWEDGES:
	case DIOCMWEDGES:
		if (OFDISK_FLOPPY_P(of))
			return ENOTTY;
	}

	error = disk_ioctl(&of->sc_dk, dev, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return error;

	switch (cmd) {
	case DIOCWDINFO:
	case DIOCSDINFO:
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCWDINFO:
	case ODIOCSDINFO:
#endif
	{
		struct disklabel *lp;

#ifdef __HAVE_OLD_DISKLABEL
		if (cmd == ODIOCSDINFO || cmd == ODIOCWDINFO) {
			memset(&newlabel, 0, sizeof newlabel);
			memcpy(&newlabel, data, sizeof (struct olddisklabel));
			lp = &newlabel;
		} else
#endif
		lp = (struct disklabel *)data;

		if ((flag & FWRITE) == 0)
			return EBADF;

		mutex_enter(&of->sc_dk.dk_openlock);

		error = setdisklabel(of->sc_dk.dk_label,
		    lp, /*of->sc_dk.dk_openmask */0,
		    of->sc_dk.dk_cpulabel);
		if (error == 0 && cmd == DIOCWDINFO
#ifdef __HAVE_OLD_DISKLABEL
		    || xfer == ODIOCWDINFO
#endif
		    )
			error = writedisklabel(MAKEDISKDEV(major(dev),
			    DISKUNIT(dev), RAW_PART), ofdisk_strategy,
			    of->sc_dk.dk_label, of->sc_dk.dk_cpulabel);

		mutex_exit(&of->sc_dk.dk_openlock);

		return error;
	}

	case DIOCGDEFLABEL:
		ofdisk_getdefaultlabel(of, (struct disklabel *)data);
		return 0;
#ifdef __HAVE_OLD_DISKLABEL
	case DIOCGDEFLABEL:
		ofdisk_getdefaultlabel(of, &newlabel);
		if (newlabel.d_npartitions > OLDMAXPARTITIONS)
			return ENOTTY;
		memcpy(data, &newlabel, sizeof (struct olddisklabel));
		return 0;
#endif

	default:
		return ENOTTY;
	}
}

int
ofdisk_dump(dev_t dev, daddr_t blkno, void *va, size_t size)
{
	return EINVAL;
}

int
ofdisk_size(dev_t dev)
{
	struct ofdisk_softc *of;
	struct disklabel *lp;
	int size, part, omask;

	of = device_lookup_private(&ofdisk_cd, DISKUNIT(dev));
	if (of == NULL)
		return ENXIO;

	part = DISKPART(dev);
	omask = of->sc_dk.dk_openmask & (1 << part);
	lp = of->sc_dk.dk_label;

	if (omask == 0 && ofdisk_open(dev, 0, S_IFBLK, curlwp) != 0)
		return -1;

	if (lp->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = lp->d_partitions[part].p_size *
		    (lp->d_secsize / DEV_BSIZE);

	if (omask == 0 && ofdisk_close(dev, 0, S_IFBLK, curlwp) != 0)
		return -1;

	return size;
}

void
ofdisk_getdefaultlabel(struct ofdisk_softc *of, struct disklabel *lp)
{

	memset(lp, 0, sizeof *lp);

	/*
	 * XXX Firmware bug?  Asking for block size gives a
	 * XXX ridiculous number!  So we use what the boot program
	 * XXX uses.
	 */
	lp->d_secsize = DEV_BSIZE;

	lp->d_secperunit = OF_call_method_1("#blocks",
	    of->sc_ihandle, 0);
	if (lp->d_secperunit == (u_int32_t)-1)
		lp->d_secperunit = 0x7fffffff;

	lp->d_secpercyl = 1;
	lp->d_nsectors = 1;
	lp->d_ntracks = 1;
	lp->d_ncylinders = lp->d_secperunit;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);
}

void
ofdisk_getdisklabel(dev_t dev)
{
	int unit = DISKUNIT(dev);
	struct ofdisk_softc *of =
		device_lookup_private(&ofdisk_cd, unit);
	struct disklabel *lp = of->sc_dk.dk_label;
	const char *errmes;
	int l;

	ofdisk_getdefaultlabel(of, lp);

	/*
	 * Don't read the disklabel on a floppy; simply
	 * assign all partitions the same size/offset as
	 * RAW_PART.  (This is essentially what the ISA
	 * floppy driver does, but we don't deal with
	 * density stuff.)
	 */
	if (OFDISK_FLOPPY_P(of)) {
		lp->d_npartitions = MAXPARTITIONS;
		for (l = 0; l < lp->d_npartitions; l++) {
			if (l == RAW_PART)
				continue;
			/* struct copy */
			lp->d_partitions[l] =
			    lp->d_partitions[RAW_PART];
		}
		lp->d_checksum = dkcksum(lp);
	} else {
		errmes = readdisklabel(MAKEDISKDEV(major(dev),
		    unit, RAW_PART), ofdisk_strategy, lp,
		    of->sc_dk.dk_cpulabel);
		if (errmes != NULL)
			printf("%s: %s\n", device_xname(of->sc_dev), errmes);
	}
}
