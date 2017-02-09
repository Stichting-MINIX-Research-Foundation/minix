/*	$NetBSD: mscp_disk.c,v 1.88 2015/04/26 15:15:20 mlelstv Exp $	*/
/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
 *	@(#)uda.c	7.32 (Berkeley) 2/13/91
 */

/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)uda.c	7.32 (Berkeley) 2/13/91
 */

/*
 * RA disk device driver
 * RX MSCP floppy disk device driver
 * RRD MSCP CD device driver
 */

/*
 * TODO
 *	write bad block forwarding code
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mscp_disk.c,v 1.88 2015/04/26 15:15:20 mlelstv Exp $");

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/reboot.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/conf.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <sys/bus.h>
#include <sys/cpu.h>

#include <dev/mscp/mscp.h>
#include <dev/mscp/mscpreg.h>
#include <dev/mscp/mscpvar.h>

#include "locators.h"
#include "ioconf.h"
#include "ra.h"

/*
 * Drive status, per drive
 */
struct ra_softc {
	device_t ra_dev;	/* Autoconf struct */
	struct	disk ra_disk;
	int	ra_state;	/* open/closed state */
	u_long	ra_mediaid;	/* media id */
	int	ra_hwunit;	/* Hardware unit number */
	int	ra_havelabel;	/* true if we have a label */
	int	ra_wlabel;	/* label sector is currently writable */
};

#define rx_softc ra_softc
#define racd_softc ra_softc

void	raattach(device_t, device_t, void *);
int	rx_putonline(struct rx_softc *);
void	rrmakelabel(struct disklabel *, long);
int	ra_putonline(dev_t, struct ra_softc *);
static inline struct ra_softc *mscp_device_lookup(dev_t);

#if NRA

int	ramatch(device_t, cfdata_t, void *);

CFATTACH_DECL_NEW(ra, sizeof(struct ra_softc),
    ramatch, raattach, NULL, NULL);

#endif /* NRA */

#if NRA || NRACD || NRX

dev_type_open(raopen);
dev_type_close(raclose);
dev_type_read(raread);
dev_type_write(rawrite);
dev_type_ioctl(raioctl);
dev_type_strategy(rastrategy);
dev_type_dump(radump);
dev_type_size(rasize);

#if NRA

const struct bdevsw ra_bdevsw = {
	.d_open = raopen,
	.d_close = raclose,
	.d_strategy = rastrategy,
	.d_ioctl = raioctl,
	.d_dump = radump,
	.d_psize = rasize,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

const struct cdevsw ra_cdevsw = {
	.d_open = raopen,
	.d_close = raclose,
	.d_read = raread,
	.d_write = rawrite,
	.d_ioctl = raioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

static struct dkdriver radkdriver = {
	.d_strategy = rastrategy,
	.d_minphys = minphys
};

/*
 * More driver definitions, for generic MSCP code.
 */

int
ramatch(device_t parent, cfdata_t cf, void *aux)
{
	struct	drive_attach_args *da = aux;
	struct	mscp *mp = da->da_mp;

	if ((da->da_typ & MSCPBUS_DISK) == 0)
		return 0;
	if (cf->cf_loc[MSCPBUSCF_DRIVE] != MSCPBUSCF_DRIVE_DEFAULT &&
	    cf->cf_loc[MSCPBUSCF_DRIVE] != mp->mscp_unit)
		return 0;
	/*
	 * Check if this disk is a floppy (RX) or cd (RRD)
	 * Seems to be a safe way to test it per Chris Torek.
	 */
	if (MSCP_MID_ECH(1, mp->mscp_guse.guse_mediaid) == 'X' - '@')
		return 0;
	if (MSCP_MID_ECH(1, mp->mscp_guse.guse_mediaid) == 'R' - '@')
		return 0;
	return 1;
}

#endif /* NRA */

/*
 * Open a drive.
 */
/*ARGSUSED*/
int
raopen(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct ra_softc *ra = mscp_device_lookup(dev);
	int error, part, mask;
	/*
	 * Make sure this is a reasonable open request.
	 */
	if (!ra)
		return ENXIO;

	part = DISKPART(dev);

	mutex_enter(&ra->ra_disk.dk_openlock);

	/*
	 * If there are wedges, and this is not RAW_PART, then we
	 * need to fail.
	 */
	if (ra->ra_disk.dk_nwedges != 0 && part != RAW_PART) {
		error = EBUSY;
		goto bad1;
	}

	/*
	 * If this is the first open; we must first try to put
	 * the disk online (and read the label).
	 */
	if (ra->ra_state == DK_CLOSED) {
		if (ra_putonline(dev, ra) == MSCP_FAILED) {
			error = ENXIO;
			goto bad1;
		}
	}

	/* If the disk has no label; allow writing everywhere */
	if (ra->ra_havelabel == 0)
		ra->ra_wlabel = 1;

	if (part >= ra->ra_disk.dk_label->d_npartitions) {
		error = ENXIO;
		goto bad1;
	}

	/*
	 * Wait for the state to settle
	 */
#if notyet
	while (ra->ra_state != DK_OPEN)
		if ((error = tsleep((void *)ra, (PZERO + 1) | PCATCH,
		    devopn, 0))) {
			splx(s);
			return (error);
		}
#endif

	mask = 1 << part;

	switch (fmt) {
	case S_IFCHR:
		ra->ra_disk.dk_copenmask |= mask;
		break;
	case S_IFBLK:
		ra->ra_disk.dk_bopenmask |= mask;
		break;
	}
	ra->ra_disk.dk_openmask |= mask;
	error = 0;
 bad1:
	mutex_exit(&ra->ra_disk.dk_openlock);
	return (error);
}

/* ARGSUSED */
int
raclose(dev_t dev, int flags, int fmt, struct lwp *l)
{
	struct ra_softc *ra = mscp_device_lookup(dev);
	int mask = (1 << DISKPART(dev));

	mutex_enter(&ra->ra_disk.dk_openlock);

	switch (fmt) {
	case S_IFCHR:
		ra->ra_disk.dk_copenmask &= ~mask;
		break;
	case S_IFBLK:
		ra->ra_disk.dk_bopenmask &= ~mask;
		break;
	}
	ra->ra_disk.dk_openmask =
	    ra->ra_disk.dk_copenmask | ra->ra_disk.dk_bopenmask;

	/*
	 * Should wait for I/O to complete on this partition even if
	 * others are open, but wait for work on blkflush().
	 */
#if notyet
	if (ra->ra_openpart == 0) {
		s = spluba();
		while (bufq_peek(udautab[unit]) != NULL)
			(void) tsleep(&udautab[unit], PZERO - 1,
			    "raclose", 0);
		splx(s);
		ra->ra_state = DK_CLOSED;
		ra->ra_wlabel = 0;
	}
#endif
	mutex_exit(&ra->ra_disk.dk_openlock);
	return (0);
}

/*
 * Queue a transfer request, and if possible, hand it to the controller.
 */
void
rastrategy(struct buf *bp)
{
	struct ra_softc *ra = mscp_device_lookup(bp->b_dev);
	int b;

	/*
	 * Make sure this is a reasonable drive to use.
	 */
	if (ra == NULL) {
		bp->b_error = ENXIO;
		goto done;
	}
	/*
	 * If drive is open `raw' or reading label, let it at it.
	 */
	if (ra->ra_state == DK_RDLABEL) {
	        /* Make some statistics... /bqt */
	        b = splbio();
	        disk_busy(&ra->ra_disk);
		splx(b);
		mscp_strategy(bp, device_parent(ra->ra_dev));
		return;
	}

	/* If disk is not online, try to put it online */
	if (ra->ra_state == DK_CLOSED)
		if (ra_putonline(bp->b_dev, ra) == MSCP_FAILED) {
			bp->b_error = EIO;
			goto done;
		}

	/*
	 * Determine the size of the transfer, and make sure it is
	 * within the boundaries of the partition.
	 */
	if (bounds_check_with_label(&ra->ra_disk, bp, ra->ra_wlabel) <= 0)
		goto done;

	/* Make some statistics... /bqt */
	b = splbio();
	disk_busy(&ra->ra_disk);
	splx(b);
	mscp_strategy(bp, device_parent(ra->ra_dev));
	return;

done:
	biodone(bp);
}

int
raread(dev_t dev, struct uio *uio, int flags)
{

	return (physio(rastrategy, NULL, dev, B_READ, minphys, uio));
}

int
rawrite(dev_t dev, struct uio *uio, int flags)
{

	return (physio(rastrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 * I/O controls.
 */
int
raioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct disklabel *lp, *tp;
	struct ra_softc *ra = mscp_device_lookup(dev);
	int error;
#ifdef __HAVE_OLD_DISKLABEL
	struct disklabel newlabel;
#endif

	lp = ra->ra_disk.dk_label;

	error = disk_ioctl(&ra->ra_disk, dev, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return error;
	else
		error = 0;

	switch (cmd) {
	case DIOCWDINFO:
	case DIOCSDINFO:
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCWDINFO:
	case ODIOCSDINFO:
		if (cmd == ODIOCSDINFO || cmd == ODIOCWDINFO) {
			memset(&newlabel, 0, sizeof newlabel);
			memcpy(&newlabel, data, sizeof (struct olddisklabel));
			tp = &newlabel;
		} else
#endif
		tp = (struct disklabel *)data;

		if ((flag & FWRITE) == 0)
			error = EBADF;
		else {
			mutex_enter(&ra->ra_disk.dk_openlock);
			error = setdisklabel(lp, tp, 0, 0);
			if ((error == 0) && (cmd == DIOCWDINFO
#ifdef __HAVE_OLD_DISKLABEL
			    || cmd == ODIOCWDINFO
#endif
			    )) {
				ra->ra_wlabel = 1;
				error = writedisklabel(dev, rastrategy, lp,0);
				ra->ra_wlabel = 0;
			}
			mutex_exit(&ra->ra_disk.dk_openlock);
		}
		break;

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else
			ra->ra_wlabel = 1;
		break;

	case DIOCGDEFLABEL:
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCGDEFLABEL:
		if (cmd == ODIOCGDEFLABEL)
			tp = &newlabel;
		else
#endif
		tp = (struct disklabel *)data;
		memset(tp, 0, sizeof(struct disklabel));
		tp->d_secsize = lp->d_secsize;
		tp->d_nsectors = lp->d_nsectors;
		tp->d_ntracks = lp->d_ntracks;
		tp->d_ncylinders = lp->d_ncylinders;
		tp->d_secpercyl = lp->d_secpercyl;
		tp->d_secperunit = lp->d_secperunit;
		tp->d_type = DKTYPE_MSCP;
		tp->d_rpm = 3600;
		rrmakelabel(tp, ra->ra_mediaid);
#ifdef __HAVE_OLD_DISKLABEL
		if (cmd == ODIOCGDEFLABEL) {
			if (tp->d_npartitions > OLDMAXPARTITIONS)
				return ENOTTY;
			memcpy(data, tp, sizeof (struct olddisklabel));
		}
#endif
		break;

	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

int
radump(dev_t dev, daddr_t blkno, void *va, size_t size)
{
	return ENXIO;
}

/*
 * Return the size of a partition, if known, or -1 if not.
 */
int
rasize(dev_t dev)
{
	struct ra_softc *ra = mscp_device_lookup(dev);

	if (!ra)
		return -1;

	if (ra->ra_state == DK_CLOSED)
		if (ra_putonline(dev, ra) == MSCP_FAILED)
			return -1;

	return ra->ra_disk.dk_label->d_partitions[DISKPART(dev)].p_size *
	    (ra->ra_disk.dk_label->d_secsize / DEV_BSIZE);
}

#endif /* NRA || NRACD || NRX */

#if NRX

int	rxmatch(device_t, cfdata_t, void *);

CFATTACH_DECL_NEW(rx, sizeof(struct rx_softc),
    rxmatch, raattach, NULL, NULL);

dev_type_open(rxopen);
dev_type_read(rxread);
dev_type_write(rxwrite);
dev_type_ioctl(rxioctl);
dev_type_strategy(rxstrategy);
dev_type_dump(radump);
dev_type_size(rxsize);

const struct bdevsw rx_bdevsw = {
	.d_open = rxopen,
	.d_close = nullclose,
	.d_strategy = rxstrategy,
	.d_ioctl = rxioctl,
	.d_dump = radump,
	.d_psize = rxsize,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

const struct cdevsw rx_cdevsw = {
	.d_open = rxopen,
	.d_close = nullclose,
	.d_read = rxread,
	.d_write = rxwrite,
	.d_ioctl = rxioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

static struct dkdriver rxdkdriver = {
	rxstrategy, minphys
};

/*
 * More driver definitions, for generic MSCP code.
 */

int
rxmatch(device_t parent, cfdata_t cf, void *aux)
{
	struct	drive_attach_args *da = aux;
	struct	mscp *mp = da->da_mp;

	if ((da->da_typ & MSCPBUS_DISK) == 0)
		return 0;
	if (cf->cf_loc[MSCPBUSCF_DRIVE] != MSCPBUSCF_DRIVE_DEFAULT &&
	    cf->cf_loc[MSCPBUSCF_DRIVE] != mp->mscp_unit)
		return 0;
	/*
	 * Check if this disk is a floppy (RX)
	 * Seems to be a safe way to test it per Chris Torek.
	 */
	if (MSCP_MID_ECH(1, mp->mscp_guse.guse_mediaid) == 'X' - '@')
		return 1;
	return 0;
}

#endif /* NRX */

#if NRACD

/* Use almost all ra* routines for racd */

int	racdmatch(device_t, cfdata_t, void *);

CFATTACH_DECL_NEW(racd, sizeof(struct racd_softc),
    racdmatch, raattach, NULL, NULL);

dev_type_open(raopen);
dev_type_read(raread);
dev_type_write(rawrite);
dev_type_ioctl(raioctl);
dev_type_strategy(rastrategy);
dev_type_dump(radump);
dev_type_size(rasize);

const struct bdevsw racd_bdevsw = {
	.d_open = raopen,
	.d_close = nullclose,
	.d_strategy = rastrategy,
	.d_ioctl = raioctl,
	.d_dump = radump,
	.d_psize = rasize,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

const struct cdevsw racd_cdevsw = {
	.d_open = raopen,
	.d_close = nullclose,
	.d_read = raread,
	.d_write = rawrite,
	.d_ioctl = raioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

static struct dkdriver racddkdriver = {
	rastrategy, minphys
};

/*
 * More driver definitions, for generic MSCP code.
 */

int
racdmatch(device_t parent, cfdata_t cf, void *aux)
{
	struct	drive_attach_args *da = aux;
	struct	mscp *mp = da->da_mp;

	if ((da->da_typ & MSCPBUS_DISK) == 0)
		return 0;
	if (cf->cf_loc[MSCPBUSCF_DRIVE] != MSCPBUSCF_DRIVE_DEFAULT &&
	    cf->cf_loc[MSCPBUSCF_DRIVE] != mp->mscp_unit)
		return 0;
	/*
	 * Check if this disk is a CD (RRD)
	 */
	if (MSCP_MID_ECH(1, mp->mscp_guse.guse_mediaid) == 'R' - '@')
		return 1;
	return 0;
}

#endif /* NRACD */

/*
 * The attach routine only checks and prints drive type.
 * Bringing the disk online is done when the disk is accessed
 * the first time.
 */
void
raattach(device_t parent, device_t self, void *aux)
{
	struct	rx_softc *rx = device_private(self);
	struct	drive_attach_args *da = aux;
	struct	mscp *mp = da->da_mp;
	struct	mscp_softc *mi = device_private(parent);
	struct	disklabel *dl;

	rx->ra_dev = self;
	rx->ra_mediaid = mp->mscp_guse.guse_mediaid;
	rx->ra_state = DK_CLOSED;
	rx->ra_hwunit = mp->mscp_unit;
	mi->mi_dp[mp->mscp_unit] = self;

#if NRX
	if (MSCP_MID_ECH(1, mp->mscp_guse.guse_mediaid) == 'X' - '@')
		disk_init((struct disk *)&rx->ra_disk, device_xname(rx->ra_dev), 
		    &rxdkdriver);
#endif
#if NRACD
	if (MSCP_MID_ECH(1, mp->mscp_guse.guse_mediaid) == 'R' - '@')
		disk_init((struct disk *)&rx->ra_disk, device_xname(rx->ra_dev), 
		    &racddkdriver);
#endif
#if NRA
	if (MSCP_MID_ECH(1, mp->mscp_guse.guse_mediaid) != 'X' - '@' &&
	    MSCP_MID_ECH(1, mp->mscp_guse.guse_mediaid) != 'R' - '@')
		disk_init((struct disk *)&rx->ra_disk, device_xname(rx->ra_dev), 
		    &radkdriver);
#endif
	disk_attach(&rx->ra_disk);

	/* Fill in what we know. The actual size is gotten later */
	dl = rx->ra_disk.dk_label;

	dl->d_secsize = DEV_BSIZE;
	dl->d_nsectors = mp->mscp_guse.guse_nspt;
	dl->d_ntracks = mp->mscp_guse.guse_ngpc * mp->mscp_guse.guse_group;
	dl->d_secpercyl = dl->d_nsectors * dl->d_ntracks;
	disk_printtype(mp->mscp_unit, mp->mscp_guse.guse_mediaid);
#ifdef DEBUG
	printf("%s: nspt %d group %d ngpc %d rct %d nrpt %d nrct %d\n",
	    device_xname(self), mp->mscp_guse.guse_nspt, mp->mscp_guse.guse_group,
	    mp->mscp_guse.guse_ngpc, mp->mscp_guse.guse_rctsize,
	    mp->mscp_guse.guse_nrpt, mp->mscp_guse.guse_nrct);
#endif
	if (MSCP_MID_ECH(1, mp->mscp_guse.guse_mediaid) != 'X' - '@') {
		/*
		 * XXX We should try to discover wedges here, but
		 * XXX that would mean being able to do I/O.  Should
		 * XXX use config_defer() here.
		 */
	}
}

/*
 * (Try to) put the drive online. This is done the first time the
 * drive is opened, or if it har fallen offline.
 */
int
rx_putonline(struct rx_softc *rx)
{
	struct	mscp *mp;
	struct	mscp_softc *mi = device_private(device_parent(rx->ra_dev));

	rx->ra_state = DK_CLOSED;
	mp = mscp_getcp(mi, MSCP_WAIT);
	mp->mscp_opcode = M_OP_ONLINE;
	mp->mscp_unit = rx->ra_hwunit;
	mp->mscp_cmdref = 1;
	*mp->mscp_addr |= MSCP_OWN | MSCP_INT;

	/* Poll away */
	bus_space_read_2(mi->mi_iot, mi->mi_iph, 0);
	if (tsleep(&rx->ra_state, PRIBIO, "rxonline", 100*100))
		rx->ra_state = DK_CLOSED;

	if (rx->ra_state == DK_CLOSED)
		return MSCP_FAILED;

	return MSCP_DONE;
}

#if NRX

/*
 * Open a drive.
 */
/*ARGSUSED*/
int
rxopen(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct rx_softc *rx;
	int unit;

	/*
	 * Make sure this is a reasonable open request.
	 */
	unit = DISKUNIT(dev);
	rx = device_lookup_private(&rx_cd, unit);
	if (!rx)
		return ENXIO;

	/*
	 * If this is the first open; we must first try to put
	 * the disk online (and read the label).
	 */
	if (rx->ra_state == DK_CLOSED)
		if (rx_putonline(rx) == MSCP_FAILED)
			return ENXIO;

	return 0;
}

/*
 * Queue a transfer request, and if possible, hand it to the controller.
 *
 * This routine is broken into two so that the internal version
 * udastrat1() can be called by the (nonexistent, as yet) bad block
 * revectoring routine.
 */
void
rxstrategy(struct buf *bp)
{
	int unit;
	struct rx_softc *rx;
	int b;

	/*
	 * Make sure this is a reasonable drive to use.
	 */
	unit = DISKUNIT(bp->b_dev);
	if ((rx = device_lookup_private(&rx_cd, unit)) == NULL) {
		bp->b_error = ENXIO;
		goto done;
	}

	/* If disk is not online, try to put it online */
	if (rx->ra_state == DK_CLOSED)
		if (rx_putonline(rx) == MSCP_FAILED) {
			bp->b_error = EIO;
			goto done;
		}

	/*
	 * Determine the size of the transfer, and make sure it is
	 * within the boundaries of the partition.
	 */
	if (bp->b_blkno >= rx->ra_disk.dk_label->d_secperunit) {
		bp->b_resid = bp->b_bcount;
		goto done;
	}

	/* Make some statistics... /bqt */
	b = splbio();
	disk_busy(&rx->ra_disk);
	splx(b);
	mscp_strategy(bp, device_parent(rx->ra_dev));
	return;

done:
	biodone(bp);
}

int
rxread(dev_t dev, struct uio *uio, int flag)
{

	return (physio(rxstrategy, NULL, dev, B_READ, minphys, uio));
}

int
rxwrite(dev_t dev, struct uio *uio, int flag)
{

	return (physio(rxstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 * I/O controls.
 */
int
rxioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	int unit = DISKUNIT(dev);
	struct rx_softc *rx = device_lookup_private(&rx_cd, unit);
	int error;

        error = disk_ioctl(&rx->ra_disk, dev, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return error;
	else
		error = 0;

	switch (cmd) {
	case DIOCWDINFO:
	case DIOCSDINFO:
	case DIOCWLABEL:
		break;

	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

int
rxsize(dev_t dev)
{

	return -1;
}

#endif /* NRX */

void	rrdgram(device_t, struct mscp *, struct mscp_softc *);
void	rriodone(device_t, struct buf *);
int	rronline(device_t, struct mscp *);
int	rrgotstatus(device_t, struct mscp *);
void	rrreplace(device_t, struct mscp *);
int	rrioerror(device_t, struct mscp *, struct buf *);
void	rrfillin(struct buf *, struct mscp *);
void	rrbb(device_t, struct mscp *, struct buf *);


struct	mscp_device ra_device = {
	rrdgram,
	rriodone,
	rronline,
	rrgotstatus,
	rrreplace,
	rrioerror,
	rrbb,
	rrfillin,
};

/*
 * Handle an error datagram.
 * This can come from an unconfigured drive as well.
 */
void
rrdgram(device_t usc, struct mscp *mp, struct mscp_softc *mi)
{
	if (mscp_decodeerror(usc == NULL?"unconf disk" : device_xname(usc), mp, mi))
		return;
	/*
	 * SDI status information bytes 10 and 11 are the microprocessor
	 * error code and front panel code respectively.  These vary per
	 * drive type and are printed purely for field service information.
	 */
	if (mp->mscp_format == M_FM_SDI)
		printf("\tsdi uproc error code 0x%x, front panel code 0x%x\n",
			mp->mscp_erd.erd_sdistat[10],
			mp->mscp_erd.erd_sdistat[11]);
}


void
rriodone(device_t usc, struct buf *bp)
{
	struct ra_softc *ra = device_private(usc);

	disk_unbusy(&ra->ra_disk, bp->b_bcount, (bp->b_flags & B_READ));

	biodone(bp);
}

/*
 * A drive came on line.  Check its type and size.  Return DONE if
 * we think the drive is truly on line.	 In any case, awaken anyone
 * sleeping on the drive on-line-ness.
 */
int
rronline(device_t usc, struct mscp *mp)
{
	struct ra_softc *ra = device_private(usc);
	struct disklabel *dl;

	wakeup((void *)&ra->ra_state);
	if ((mp->mscp_status & M_ST_MASK) != M_ST_SUCCESS) {
		aprint_error_dev(usc, "attempt to bring on line failed: ");
		mscp_printevent(mp);
		return (MSCP_FAILED);
	}

	ra->ra_state = DK_OPEN;

	dl = ra->ra_disk.dk_label;
	dl->d_secperunit = (daddr_t)mp->mscp_onle.onle_unitsize;

	if (dl->d_secpercyl) {
		dl->d_ncylinders = dl->d_secperunit/dl->d_secpercyl;
		dl->d_type = DKTYPE_MSCP;
		dl->d_rpm = 3600;
	} else {
		dl->d_type = DKTYPE_FLOPPY;
		dl->d_rpm = 300;
	}
	rrmakelabel(dl, ra->ra_mediaid);

	return (MSCP_DONE);
}

void
rrmakelabel(struct disklabel *dl, long type)
{
	int n, p = 0;

	dl->d_bbsize = BBSIZE;
	dl->d_sbsize = SBLOCKSIZE;

	/* Create the disk name for disklabel. Phew... */
	dl->d_typename[p++] = MSCP_MID_CHAR(2, type);
	dl->d_typename[p++] = MSCP_MID_CHAR(1, type);
	if (MSCP_MID_ECH(0, type))
		dl->d_typename[p++] = MSCP_MID_CHAR(0, type);
	n = MSCP_MID_NUM(type);
	if (n > 99) {
		dl->d_typename[p++] = '1';
		n -= 100;
	}
	if (n > 9) {
		dl->d_typename[p++] = (n / 10) + '0';
		n %= 10;
	}
	dl->d_typename[p++] = n + '0';
	dl->d_typename[p] = 0;
	dl->d_npartitions = MAXPARTITIONS;
	dl->d_partitions[0].p_size = dl->d_partitions[2].p_size =
	    dl->d_secperunit;
	dl->d_partitions[0].p_offset = dl->d_partitions[2].p_offset = 0;
	dl->d_interleave = dl->d_headswitch = 1;
	dl->d_magic = dl->d_magic2 = DISKMAGIC;
	dl->d_checksum = dkcksum(dl);
}

/*
 * We got some (configured) unit's status.  Return DONE if it succeeded.
 */
int
rrgotstatus(device_t usc, struct mscp *mp)
{
	if ((mp->mscp_status & M_ST_MASK) != M_ST_SUCCESS) {
		aprint_error_dev(usc, "attempt to get status failed: ");
		mscp_printevent(mp);
		return (MSCP_FAILED);
	}
	/* record for (future) bad block forwarding and whatever else */
#ifdef notyet
	uda_rasave(ui->ui_unit, mp, 1);
#endif
	return (MSCP_DONE);
}

/*
 * A replace operation finished.
 */
/*ARGSUSED*/
void
rrreplace(device_t usc, struct mscp *mp)
{

	panic("udareplace");
}

/*
 * A transfer failed.  We get a chance to fix or restart it.
 * Need to write the bad block forwaring code first....
 */
/*ARGSUSED*/
int
rrioerror(device_t usc, struct mscp *mp, struct buf *bp)
{
	struct ra_softc *ra = device_private(usc);
	int code = mp->mscp_event;

	switch (code & M_ST_MASK) {
	/* The unit has fallen offline. Try to figure out why. */
	case M_ST_OFFLINE:
		bp->b_error = EIO;
		ra->ra_state = DK_CLOSED;
		if (code & M_OFFLINE_UNMOUNTED)
			aprint_error_dev(usc, "not mounted/spun down\n");
		if (code & M_OFFLINE_DUPLICATE)
			aprint_error_dev(usc, "duplicate unit number!!!\n");
		return MSCP_DONE;

	case M_ST_AVAILABLE:
		ra->ra_state = DK_CLOSED; /* Force another online */
		return MSCP_DONE;

	default:
		printf("%s:", device_xname(usc));
		break;
	}
	return (MSCP_FAILED);
}

/*
 * Fill in disk addresses in a mscp packet waiting for transfer.
 */
void
rrfillin(struct buf *bp, struct mscp *mp)
{
	struct ra_softc *ra;
	struct disklabel *lp;
	int part = DISKPART(bp->b_dev);

	ra = mscp_device_lookup(bp->b_dev);
	lp = ra->ra_disk.dk_label;

	mp->mscp_seq.seq_lbn = lp->d_partitions[part].p_offset + bp->b_blkno;
	mp->mscp_unit = ra->ra_hwunit;
	mp->mscp_seq.seq_bytecount = bp->b_bcount;
}

/*
 * A bad block related operation finished.
 */
/*ARGSUSED*/
void
rrbb(device_t usc, struct mscp *mp, struct buf *bp)
{

	panic("udabb");
}

/*
 * (Try to) put the drive online. This is done the first time the
 * drive is opened, or if it has fallen offline.
 */
int
ra_putonline(dev_t dev, struct ra_softc *ra)
{
	struct	disklabel *dl;
	const char *msg;

	if (rx_putonline(ra) != MSCP_DONE)
		return MSCP_FAILED;

	dl = ra->ra_disk.dk_label;

	ra->ra_state = DK_RDLABEL;
	printf("%s", device_xname(ra->ra_dev));
	if ((msg = readdisklabel(
	    MAKEDISKDEV(major(dev), device_unit(ra->ra_dev), RAW_PART),
	    rastrategy, dl, NULL)) == NULL) {
		ra->ra_havelabel = 1;
		ra->ra_state = DK_OPEN;
	}
#if NRACD
	else if (cdevsw_lookup(dev) == &racd_cdevsw) {
		dl->d_partitions[0].p_offset = 0;
		dl->d_partitions[0].p_size = dl->d_secperunit;
		dl->d_partitions[0].p_fstype = FS_ISO9660;
	}
#endif /* NRACD */
	else {
		printf(": %s", msg);
	}

	printf(": size %d sectors\n", dl->d_secperunit);

	return MSCP_DONE;
}


static inline struct ra_softc *
mscp_device_lookup(dev_t dev)
{
	struct ra_softc *ra;
	int unit;

	unit = DISKUNIT(dev);
#if NRA
	if (cdevsw_lookup(dev) == &ra_cdevsw)
		ra = device_lookup_private(&ra_cd, unit);
	else
#endif
#if NRACD
	if (cdevsw_lookup(dev) == &racd_cdevsw)
		ra = device_lookup_private(&racd_cd, unit);
	else
#endif
#if NRX
	if (cdevsw_lookup(dev) == &rx_cdevsw)
		ra = device_lookup_private(&rx_cd, unit);
	else
#endif
		panic("mscp_device_lookup: unexpected major %"PRIu32" unit %u",
		    major(dev), unit);
	return ra;
}
