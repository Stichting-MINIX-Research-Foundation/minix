/*	$NetBSD: rl.c,v 1.50 2015/04/26 15:15:20 mlelstv Exp $	*/

/*
 * Copyright (c) 2000 Ludd, University of Lule}, Sweden. All rights reserved.
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
 *      This product includes software developed at Ludd, University of
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * RL11/RLV11/RLV12 disk controller driver and
 * RL01/RL02 disk device driver.
 *
 * TODO:
 *	Handle disk errors more gracefully
 *	Do overlapping seeks on multiple drives
 *
 * Implementation comments:
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rl.c,v 1.50 2015/04/26 15:15:20 mlelstv Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/stat.h>
#include <sys/dkio.h>
#include <sys/fcntl.h>
#include <sys/event.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <sys/bus.h>

#include <dev/qbus/ubavar.h>
#include <dev/qbus/rlreg.h>
#include <dev/qbus/rlvar.h>

#include "ioconf.h"
#include "locators.h"

static	int rlcmatch(device_t, cfdata_t, void *);
static	void rlcattach(device_t, device_t, void *);
static	int rlcprint(void *, const char *);
static	void rlcintr(void *);
static	int rlmatch(device_t, cfdata_t, void *);
static	void rlattach(device_t, device_t, void *);
static	void rlcstart(struct rlc_softc *, struct buf *);
static	void waitcrdy(struct rlc_softc *);
static	void rlcreset(device_t);

CFATTACH_DECL_NEW(rlc, sizeof(struct rlc_softc),
    rlcmatch, rlcattach, NULL, NULL);

CFATTACH_DECL_NEW(rl, sizeof(struct rl_softc),
    rlmatch, rlattach, NULL, NULL);

static dev_type_open(rlopen);
static dev_type_close(rlclose);
static dev_type_read(rlread);
static dev_type_write(rlwrite);
static dev_type_ioctl(rlioctl);
static dev_type_strategy(rlstrategy);
static dev_type_dump(rldump);
static dev_type_size(rlpsize);

const struct bdevsw rl_bdevsw = {
	.d_open = rlopen,
	.d_close = rlclose,
	.d_strategy = rlstrategy,
	.d_ioctl = rlioctl,
	.d_dump = rldump,
	.d_psize = rlpsize,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

const struct cdevsw rl_cdevsw = {
	.d_open = rlopen,
	.d_close = rlclose,
	.d_read = rlread,
	.d_write = rlwrite,
	.d_ioctl = rlioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

#define	MAXRLXFER (RL_BPS * RL_SPT)

#define	RL_WREG(reg, val) \
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, (reg), (val))
#define RL_RREG(reg) \
	bus_space_read_2(sc->sc_iot, sc->sc_ioh, (reg))

static const char * const rlstates[] = {
	"drive not loaded",
	"drive spinning up",
	"drive brushes out",
	"drive loading heads",
	"drive seeking",
	"drive ready",
	"drive unloading heads",
	"drive spun down",
};

static const struct dkdriver rldkdriver = {
	.d_strategy = rlstrategy,
	.d_minphys = minphys
};

static const char *
rlstate(struct rlc_softc *sc, int unit)
{
	int i = 0;

	do {
		RL_WREG(RL_DA, RLDA_GS);
		RL_WREG(RL_CS, RLCS_GS|(unit << RLCS_USHFT));
		waitcrdy(sc);
	} while (((RL_RREG(RL_CS) & RLCS_ERR) != 0) && i++ < 10);
	if (i == 10)
		return NULL;
	i = RL_RREG(RL_MP) & RLMP_STATUS;
	return rlstates[i];
}

void
waitcrdy(struct rlc_softc *sc)
{
	int i;

	for (i = 0; i < 1000; i++) {
		DELAY(10000);
		if (RL_RREG(RL_CS) & RLCS_CRDY)
			return;
	}
	aprint_error_dev(sc->sc_dev, "never got ready\n"); /* ?panic? */
}

int
rlcprint(void *aux, const char *name)
{
	struct rlc_attach_args *ra = aux;

	if (name)
		aprint_normal("RL0%d at %s",
		    ra->type & RLMP_DT ? '2' : '1', name);
	aprint_normal(" drive %d", ra->hwid);
	return UNCONF;
}

/*
 * Force the controller to interrupt.
 */
int
rlcmatch(device_t parent, cfdata_t cf, void *aux)
{
	struct uba_attach_args *ua = aux;
	struct rlc_softc ssc, *sc = &ssc;
	int i;

	sc->sc_iot = ua->ua_iot;
	sc->sc_ioh = ua->ua_ioh;
	/* Force interrupt by issuing a "Get Status" command */
	RL_WREG(RL_DA, RLDA_GS);
	RL_WREG(RL_CS, RLCS_GS|RLCS_IE);

	for (i = 0; i < 100; i++) {
		DELAY(100000);
		if (RL_RREG(RL_CS) & RLCS_CRDY)
			return 1;
	}
	return 0;
}

void
rlcattach(device_t parent, device_t self, void *aux)
{
	struct rlc_softc *sc = device_private(self);
	struct uba_attach_args *ua = aux;
	struct rlc_attach_args ra;
	int i, error;

	sc->sc_dev = self;
	sc->sc_uh = device_private(parent);
	sc->sc_iot = ua->ua_iot;
	sc->sc_ioh = ua->ua_ioh;
	sc->sc_dmat = ua->ua_dmat;
	uba_intr_establish(ua->ua_icookie, ua->ua_cvec,
		rlcintr, sc, &sc->sc_intrcnt);
	evcnt_attach_dynamic(&sc->sc_intrcnt, EVCNT_TYPE_INTR, ua->ua_evcnt,
		device_xname(sc->sc_dev), "intr");
	uba_reset_establish(rlcreset, self);

	printf("\n");

	/*
	 * The RL11 can only have one transfer going at a time,
	 * and max transfer size is one track, so only one dmamap
	 * is needed.
	 */
	error = bus_dmamap_create(sc->sc_dmat, MAXRLXFER, 1, MAXRLXFER, 0,
	    BUS_DMA_ALLOCNOW, &sc->sc_dmam);
	if (error) {
		aprint_error(": Failed to allocate DMA map, error %d\n", error);
		return;
	}
	bufq_alloc(&sc->sc_q, "disksort", BUFQ_SORT_CYLINDER);
	for (i = 0; i < RL_MAXDPC; i++) {
		waitcrdy(sc);
		RL_WREG(RL_DA, RLDA_GS|RLDA_RST);
		RL_WREG(RL_CS, RLCS_GS|(i << RLCS_USHFT));
		waitcrdy(sc);
		ra.type = RL_RREG(RL_MP);
		ra.hwid = i;
		if ((RL_RREG(RL_CS) & RLCS_ERR) == 0)
			config_found(sc->sc_dev, &ra, rlcprint);
	}
}

int
rlmatch(device_t parent, cfdata_t cf, void *aux)
{
	struct rlc_attach_args *ra = aux;

	if (cf->cf_loc[RLCCF_DRIVE] != RLCCF_DRIVE_DEFAULT &&
	    cf->cf_loc[RLCCF_DRIVE] != ra->hwid)
		return 0;
	return 1;
}

void
rlattach(device_t parent, device_t self, void *aux)
{
	struct rl_softc *rc = device_private(self);
	struct rlc_attach_args *ra = aux;
	struct disklabel *dl;

	rc->rc_dev = self;
	rc->rc_rlc = device_private(parent);
	rc->rc_hwid = ra->hwid;
	disk_init(&rc->rc_disk, device_xname(rc->rc_dev), &rldkdriver);
	disk_attach(&rc->rc_disk);
	dl = rc->rc_disk.dk_label;
	dl->d_npartitions = 3;
	strcpy(dl->d_typename, "RL01");
	if (ra->type & RLMP_DT)
		dl->d_typename[3] = '2';
	dl->d_secsize = DEV_BSIZE; /* XXX - wrong, but OK for now */
	dl->d_nsectors = RL_SPT/2;
	dl->d_ntracks = RL_SPD;
	dl->d_ncylinders = ra->type & RLMP_DT ? RL_TPS02 : RL_TPS01;
	dl->d_secpercyl = dl->d_nsectors * dl->d_ntracks;
	dl->d_secperunit = dl->d_ncylinders * dl->d_secpercyl;
	dl->d_partitions[0].p_size = dl->d_partitions[2].p_size =
	    dl->d_secperunit;
	dl->d_partitions[0].p_offset = dl->d_partitions[2].p_offset = 0;
	dl->d_interleave = dl->d_headswitch = 1;
	dl->d_bbsize = BBSIZE;
	dl->d_sbsize = SBLOCKSIZE;
	dl->d_rpm = 2400;
	dl->d_type = DKTYPE_DEC;
	printf(": %s, %s\n", dl->d_typename, rlstate(rc->rc_rlc, ra->hwid));

	/*
	 * XXX We should try to discovery wedges here, but
	 * XXX that would mean loading up the pack and being
	 * XXX able to do I/O.  Should use config_defer() here.
	 */
}

int
rlopen(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct rl_softc * const rc = device_lookup_private(&rl_cd, DISKUNIT(dev));
	struct rlc_softc *sc;
	int error, part, mask;
	struct disklabel *dl;
	const char *msg;

	/*
	 * Make sure this is a reasonable open request.
	 */
	if (rc == NULL)
		return ENXIO;

	sc = rc->rc_rlc;
	part = DISKPART(dev);

	mutex_enter(&rc->rc_disk.dk_openlock);

	/*
	 * If there are wedges, and this is not RAW_PART, then we
	 * need to fail.
	 */
	if (rc->rc_disk.dk_nwedges != 0 && part != RAW_PART) {
		error = EBUSY;
		goto bad1;
	}

	/* Check that the disk actually is useable */
	msg = rlstate(sc, rc->rc_hwid);
	if (msg == NULL || msg == rlstates[RLMP_UNLOAD] ||
	    msg == rlstates[RLMP_SPUNDOWN]) {
		error = ENXIO;
		goto bad1;
	}
	/*
	 * If this is the first open; read in where on the disk we are.
	 */
	dl = rc->rc_disk.dk_label;
	if (rc->rc_state == DK_CLOSED) {
		u_int16_t mp;
		int maj;
		RL_WREG(RL_CS, RLCS_RHDR|(rc->rc_hwid << RLCS_USHFT));
		waitcrdy(sc);
		mp = RL_RREG(RL_MP);
		rc->rc_head = ((mp & RLMP_HS) == RLMP_HS);
		rc->rc_cyl = (mp >> 7) & 0777;
		rc->rc_state = DK_OPEN;
		/* Get disk label */
		maj = cdevsw_lookup_major(&rl_cdevsw);
		if ((msg = readdisklabel(MAKEDISKDEV(maj,
		    device_unit(rc->rc_dev), RAW_PART), rlstrategy, dl, NULL)))
			aprint_normal_dev(rc->rc_dev, "%s", msg);
		aprint_normal_dev(rc->rc_dev, "size %d sectors\n",
		    dl->d_secperunit);
	}
	if (part >= dl->d_npartitions) {
		error = ENXIO;
		goto bad1;
	}

	mask = 1 << part;
	switch (fmt) {
	case S_IFCHR:
		rc->rc_disk.dk_copenmask |= mask;
		break;
	case S_IFBLK:
		rc->rc_disk.dk_bopenmask |= mask;
		break;
	}
	rc->rc_disk.dk_openmask |= mask;
	error = 0;
 bad1:
	mutex_exit(&rc->rc_disk.dk_openlock);
	return (error);
}

int
rlclose(dev_t dev, int flag, int fmt, struct lwp *l)
{
	int unit = DISKUNIT(dev);
	struct rl_softc *rc = device_lookup_private(&rl_cd, unit);
	int mask = (1 << DISKPART(dev));

	mutex_enter(&rc->rc_disk.dk_openlock);

	switch (fmt) {
	case S_IFCHR:
		rc->rc_disk.dk_copenmask &= ~mask;
		break;
	case S_IFBLK:
		rc->rc_disk.dk_bopenmask &= ~mask;
		break;
	}
	rc->rc_disk.dk_openmask =
	    rc->rc_disk.dk_copenmask | rc->rc_disk.dk_bopenmask;

	if (rc->rc_disk.dk_openmask == 0)
		rc->rc_state = DK_CLOSED; /* May change pack */
	mutex_exit(&rc->rc_disk.dk_openlock);
	return 0;
}

void
rlstrategy(struct buf *bp)
{
	struct rl_softc * const rc = device_lookup_private(&rl_cd, DISKUNIT(bp->b_dev));
	struct disklabel *lp;
	int s;

	if (rc == NULL || rc->rc_state != DK_OPEN) /* How did we end up here at all? */
		panic("rlstrategy: state impossible");

	lp = rc->rc_disk.dk_label;
	if (bounds_check_with_label(&rc->rc_disk, bp, 1) <= 0)
		goto done;

	if (bp->b_bcount == 0)
		goto done;

	bp->b_rawblkno =
	    bp->b_blkno + lp->d_partitions[DISKPART(bp->b_dev)].p_offset;
	bp->b_cylinder = bp->b_rawblkno / lp->d_secpercyl;

	s = splbio();
	bufq_put(rc->rc_rlc->sc_q, bp);
	rlcstart(rc->rc_rlc, 0);
	splx(s);
	return;

done:	biodone(bp);
}

int
rlioctl(dev_t dev, u_long cmd, void *addr, int flag, struct lwp *l)
{
	struct rl_softc *rc = device_lookup_private(&rl_cd, DISKUNIT(dev));
	struct disklabel *lp = rc->rc_disk.dk_label;
	int error;
#ifdef __HAVE_OLD_DISKLABEL
	struct disklabel newlabel;
#endif

	error = disk_ioctl(&rc->rc_disk, dev, cmd, addr, flag, l);
	if (error != EPASSTHROUGH)
		return error;
	else
		error = 0;

	switch (cmd) {
	case DIOCSDINFO:
	case DIOCWDINFO:
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCWDINFO:
	case ODIOCSDINFO:
#endif
	{
		struct disklabel *tp;

#ifdef __HAVE_OLD_DISKLABEL
		if (cmd == ODIOCSDINFO || cmd == ODIOCWDINFO) {
			memset(&newlabel, 0, sizeof newlabel);
			memcpy(&newlabel, addr, sizeof (struct olddisklabel));
			tp = &newlabel;
		} else
#endif
		tp = (struct disklabel *)addr;

		if ((flag & FWRITE) == 0)
			error = EBADF;
		else {
			mutex_enter(&rc->rc_disk.dk_openlock);
			error = ((
#ifdef __HAVE_OLD_DISKLABEL
			       cmd == ODIOCSDINFO ||
#endif
			       cmd == DIOCSDINFO) ?
			    setdisklabel(lp, tp, 0, 0) :
			    writedisklabel(dev, rlstrategy, lp, 0));
			mutex_exit(&rc->rc_disk.dk_openlock);
		}
		break;
	}

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		break;

	default:
		error = ENOTTY;
		break;
	}
	return error;
}

int
rlpsize(dev_t dev)
{
	struct rl_softc * const rc = device_lookup_private(&rl_cd, DISKUNIT(dev));
	struct disklabel *dl;
	int size;

	if (rc == NULL)
		return -1;
	dl = rc->rc_disk.dk_label;
	size = dl->d_partitions[DISKPART(dev)].p_size *
	    (dl->d_secsize / DEV_BSIZE);
	return size;
}

int
rldump(dev_t dev, daddr_t blkno, void *va, size_t size)
{
	/* Not likely... */
	return 0;
}

int
rlread(dev_t dev, struct uio *uio, int ioflag)
{
	return (physio(rlstrategy, NULL, dev, B_READ, minphys, uio));
}

int
rlwrite(dev_t dev, struct uio *uio, int ioflag)
{
	return (physio(rlstrategy, NULL, dev, B_WRITE, minphys, uio));
}

static const char * const rlerr[] = {
	"no",
	"operation incomplete",
	"read data CRC",
	"header CRC",
	"data late",
	"header not found",
	"",
	"",
	"non-existent memory",
	"memory parity error",
	"",
	"",
	"",
	"",
	"",
	"",
};

void
rlcintr(void *arg)
{
	struct rlc_softc *sc = arg;
	struct buf *bp;
	u_int16_t cs;

	bp = sc->sc_active;
	if (bp == 0) {
		aprint_error_dev(sc->sc_dev, "strange interrupt\n");
		return;
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_dmam);
	sc->sc_active = 0;
	cs = RL_RREG(RL_CS);
	if (cs & RLCS_ERR) {
		int error = (cs & RLCS_ERRMSK) >> 10;

		aprint_error_dev(sc->sc_dev, "%s\n", rlerr[error]);
		bp->b_error = EIO;
		bp->b_resid = bp->b_bcount;
		sc->sc_bytecnt = 0;
	}
	if (sc->sc_bytecnt == 0) /* Finished transfer */
		biodone(bp);
	rlcstart(sc, sc->sc_bytecnt ? bp : 0);
}

/*
 * Start routine. First position the disk to the given position,
 * then start reading/writing. An optimization would be to be able
 * to handle overlapping seeks between disks.
 */
void
rlcstart(struct rlc_softc *sc, struct buf *ob)
{
	struct disklabel *lp;
	struct rl_softc *rc;
	struct buf *bp;
	int bn, cn, sn, tn, blks, err;

	if (sc->sc_active)
		return;	/* Already doing something */

	if (ob == 0) {
		bp = bufq_get(sc->sc_q);
		if (bp == NULL)
			return;	/* Nothing to do */
		sc->sc_bufaddr = bp->b_data;
		sc->sc_diskblk = bp->b_rawblkno;
		sc->sc_bytecnt = bp->b_bcount;
		bp->b_resid = 0;
	} else
		bp = ob;
	sc->sc_active = bp;

	rc = device_lookup_private(&rl_cd, DISKUNIT(bp->b_dev));
	bn = sc->sc_diskblk;
	lp = rc->rc_disk.dk_label;
	if (bn) {
		cn = bn / lp->d_secpercyl;
		sn = bn % lp->d_secpercyl;
		tn = sn / lp->d_nsectors;
		sn = sn % lp->d_nsectors;
	} else
		cn = sn = tn = 0;

	/*
	 * Check if we have to position disk first.
	 */
	if (rc->rc_cyl != cn || rc->rc_head != tn) {
		u_int16_t da = RLDA_SEEK;
		if (cn > rc->rc_cyl)
			da |= ((cn - rc->rc_cyl) << RLDA_CYLSHFT) | RLDA_DIR;
		else
			da |= ((rc->rc_cyl - cn) << RLDA_CYLSHFT);
		if (tn)
			da |= RLDA_HSSEEK;
		waitcrdy(sc);
		RL_WREG(RL_DA, da);
		RL_WREG(RL_CS, RLCS_SEEK | (rc->rc_hwid << RLCS_USHFT));
		waitcrdy(sc);
		rc->rc_cyl = cn;
		rc->rc_head = tn;
	}
	RL_WREG(RL_DA, (cn << RLDA_CYLSHFT) | (tn ? RLDA_HSRW : 0) | (sn << 1));
	blks = sc->sc_bytecnt/DEV_BSIZE;

	if (sn + blks > RL_SPT/2)
		blks = RL_SPT/2 - sn;
	RL_WREG(RL_MP, -(blks*DEV_BSIZE)/2);
	err = bus_dmamap_load(sc->sc_dmat, sc->sc_dmam, sc->sc_bufaddr,
	    (blks*DEV_BSIZE), (bp->b_flags & B_PHYS ? bp->b_proc : 0),
	    BUS_DMA_NOWAIT);
	if (err)
		panic("%s: bus_dmamap_load failed: %d",
		    device_xname(sc->sc_dev), err);
	RL_WREG(RL_BA, (sc->sc_dmam->dm_segs[0].ds_addr & 0xffff));

	/* Count up vars */
	sc->sc_bufaddr = (char *)sc->sc_bufaddr + (blks*DEV_BSIZE);
	sc->sc_diskblk += blks;
	sc->sc_bytecnt -= (blks*DEV_BSIZE);

	if (bp->b_flags & B_READ)
		RL_WREG(RL_CS, RLCS_IE|RLCS_RD|(rc->rc_hwid << RLCS_USHFT));
	else
		RL_WREG(RL_CS, RLCS_IE|RLCS_WD|(rc->rc_hwid << RLCS_USHFT));
}

/*
 * Called once per controller when an ubareset occurs.
 * Retracts all disks and restarts active transfers.
 */
void
rlcreset(device_t dev)
{
	struct rlc_softc *sc = device_private(dev);
	struct rl_softc *rc;
	int i;
	u_int16_t mp;

	for (i = 0; i < rl_cd.cd_ndevs; i++) {
		if ((rc = device_lookup_private(&rl_cd, i)) == NULL)
			continue;
		if (rc->rc_state != DK_OPEN)
			continue;
		if (rc->rc_rlc != sc)
			continue;

		RL_WREG(RL_CS, RLCS_RHDR|(rc->rc_hwid << RLCS_USHFT));
		waitcrdy(sc);
		mp = RL_RREG(RL_MP);
		rc->rc_head = ((mp & RLMP_HS) == RLMP_HS);
		rc->rc_cyl = (mp >> 7) & 0777;
	}
	if (sc->sc_active == 0)
		return;

	bufq_put(sc->sc_q, sc->sc_active);
	sc->sc_active = 0;
	rlcstart(sc, 0);
}
