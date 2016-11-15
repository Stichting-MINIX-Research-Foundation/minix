/*	$NetBSD: rd.c,v 1.40 2015/04/13 16:33:24 riastradh Exp $ */

/*-
 * Copyright (c) 1996-2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: rd.c 1.44 92/12/26$
 *
 *	@(#)rd.c	8.2 (Berkeley) 5/19/94
 */

/*
 * CS80/SS80 disk driver
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rd.c,v 1.40 2015/04/13 16:33:24 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/callout.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/endian.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/stat.h>

#include <sys/rndsource.h>

#include <dev/gpib/gpibvar.h>
#include <dev/gpib/cs80busvar.h>

#include <dev/gpib/rdreg.h>

#ifdef DEBUG
int	rddebug = 0xff;
#define RDB_FOLLOW	0x01
#define RDB_STATUS	0x02
#define RDB_IDENT	0x04
#define RDB_IO		0x08
#define RDB_ASYNC	0x10
#define RDB_ERROR	0x80
#define DPRINTF(mask, str)	if (rddebug & (mask)) printf str
#else
#define DPRINTF(mask, str)	/* nothing */
#endif

struct	rd_softc {
	device_t sc_dev;
	gpib_chipset_tag_t sc_ic;
	gpib_handle_t sc_hdl;

	struct	disk sc_dk;

	int	sc_slave;		/* GPIB slave */
	int	sc_punit;		/* physical unit on slave */

	int	sc_flags;
#define	RDF_ALIVE	0x01
#define	RDF_SEEK	0x02
#define RDF_SWAIT	0x04
#define RDF_OPENING	0x08
#define RDF_CLOSING	0x10
#define RDF_WANTED	0x20
#define RDF_WLABEL	0x40

	u_int16_t sc_type;
	u_int8_t *sc_addr;
	int	sc_resid;
	struct	rd_iocmd sc_ioc;
	struct	bufq_state *sc_tab;
	int	sc_active;
	int	sc_errcnt;

	struct	callout sc_restart_ch;

	krndsource_t rnd_source;
};

#define RDUNIT(dev)			DISKUNIT(dev)
#define RDPART(dev)			DISKPART(dev)
#define RDMAKEDEV(maj, unit, part)	MAKEDISKDEV(maj, unit, part)
#define RDLABELDEV(dev)	(RDMAKEDEV(major(dev), RDUNIT(dev), RAW_PART))

#define	RDRETRY		5
#define RDWAITC		1	/* min time for timeout in seconds */

int	rderrthresh = RDRETRY-1;	/* when to start reporting errors */

/*
 * Misc. HW description, indexed by sc_type.
 * Used for mapping 256-byte sectors for 512-byte sectors
 */
const struct rdidentinfo {
	u_int16_t ri_hwid;		/* 2 byte HW id */
	u_int16_t ri_maxunum;		/* maximum allowed unit number */
	const char *ri_desc;		/* drive type description */
	int	ri_nbpt;		/* DEV_BSIZE blocks per track */
	int	ri_ntpc;		/* tracks per cylinder */
	int	ri_ncyl;		/* cylinders per unit */
	int	ri_nblocks;		/* DEV_BSIZE blocks on disk */
} rdidentinfo[] = {
	{ RD7946AID,	0,	"7945A",	NRD7945ABPT,
	  NRD7945ATRK,	968,	 108416 },

	{ RD9134DID,	1,	"9134D",	NRD9134DBPT,
	  NRD9134DTRK,	303,	  29088 },

	{ RD9134LID,	1,	"9122S",	NRD9122SBPT,
	  NRD9122STRK,	77,	   1232 },

	{ RD7912PID,	0,	"7912P",	NRD7912PBPT,
	  NRD7912PTRK,	572,	 128128 },

	{ RD7914PID,	0,	"7914P",	NRD7914PBPT,
	  NRD7914PTRK,	1152,	 258048 },

	{ RD7958AID,	0,	"7958A",	NRD7958ABPT,
	  NRD7958ATRK,	1013,	 255276 },

	{ RD7957AID,	0,	"7957A",	NRD7957ABPT,
	  NRD7957ATRK,	1036,	 159544 },

	{ RD7933HID,	0,	"7933H",	NRD7933HBPT,
	  NRD7933HTRK,	1321,	 789958 },

	{ RD9134LID,	1,	"9134L",	NRD9134LBPT,
	  NRD9134LTRK,	973,	  77840 },

	{ RD7936HID,	0,	"7936H",	NRD7936HBPT,
	  NRD7936HTRK,	698,	 600978 },

	{ RD7937HID,	0,	"7937H",	NRD7937HBPT,
	  NRD7937HTRK,	698,	1116102 },

	{ RD7914CTID,	0,	"7914CT",	NRD7914PBPT,
	  NRD7914PTRK,	1152,	 258048 },

	{ RD7946AID,	0,	"7946A",	NRD7945ABPT,
	  NRD7945ATRK,	968,	 108416 },

	{ RD9134LID,	1,	"9122D",	NRD9122SBPT,
	  NRD9122STRK,	77,	   1232 },

	{ RD7957BID,	0,	"7957B",	NRD7957BBPT,
	  NRD7957BTRK,	1269,	 159894 },

	{ RD7958BID,	0,	"7958B",	NRD7958BBPT,
	  NRD7958BTRK,	786,	 297108 },

	{ RD7959BID,	0,	"7959B",	NRD7959BBPT,
	  NRD7959BTRK,	1572,	 594216 },

	{ RD2200AID,	0,	"2200A",	NRD2200ABPT,
	  NRD2200ATRK,	1449,	 654948 },

	{ RD2203AID,	0,	"2203A",	NRD2203ABPT,
	  NRD2203ATRK,	1449,	1309896 }
};
int numrdidentinfo = sizeof(rdidentinfo) / sizeof(rdidentinfo[0]);

int	rdlookup(int, int, int);
int	rdgetinfo(struct rd_softc *);
void	rdrestart(void *);
struct buf *rdfinish(struct rd_softc *, struct buf *);

void	rdgetcompatlabel(struct rd_softc *, struct disklabel *);
void	rdgetdefaultlabel(struct rd_softc *, struct disklabel *);
void	rdrestart(void *);
void	rdustart(struct rd_softc *);
struct buf *rdfinish(struct rd_softc *, struct buf *);
void	rdcallback(void *, int);
void	rdstart(struct rd_softc *);
void	rdintr(struct rd_softc *);
int	rderror(struct rd_softc *);

int	rdmatch(device_t, cfdata_t, void *);
void	rdattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(rd, sizeof(struct rd_softc),
	rdmatch, rdattach, NULL, NULL);


dev_type_open(rdopen);
dev_type_close(rdclose);
dev_type_read(rdread);
dev_type_write(rdwrite);
dev_type_ioctl(rdioctl);
dev_type_strategy(rdstrategy);
dev_type_dump(rddump);
dev_type_size(rdsize);

const struct bdevsw rd_bdevsw = {
	.d_open = rdopen,
	.d_close = rdclose,
	.d_strategy = rdstrategy,
	.d_ioctl = rdioctl,
	.d_dump = rddump,
	.d_psize = rdsize,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

const struct cdevsw rd_cdevsw = {
	.d_open = rdopen,
	.d_close = rdclose,
	.d_read = rdread,
	.d_write = rdwrite,
	.d_ioctl = rdioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

extern struct cfdriver rd_cd;

int
rdlookup(int id, int slave, int punit)
{
	int i;

	for (i = 0; i < numrdidentinfo; i++) {
		if (rdidentinfo[i].ri_hwid == id)
			break;
	}
	if (i == numrdidentinfo || punit > rdidentinfo[i].ri_maxunum)
		return (-1);
	return (i);
}

int
rdmatch(device_t parent, cfdata_t match, void *aux)
{
	struct cs80bus_attach_args *ca = aux;

	if (rdlookup(ca->ca_id, ca->ca_slave, ca->ca_punit) < 0)
		return (0);
	return (1);
}

void
rdattach(device_t parent, device_t self, void *aux)
{
	struct rd_softc *sc = device_private(self);
	struct cs80bus_attach_args *ca = aux;
	struct cs80_description csd;
	char name[7];
	int type, i, n;

	sc->sc_dev = self;
	sc->sc_ic = ca->ca_ic;
	sc->sc_slave = ca->ca_slave;
	sc->sc_punit = ca->ca_punit;

	if ((type = rdlookup(ca->ca_id, ca->ca_slave, ca->ca_punit)) < 0)
		return;

	if (cs80reset(parent, sc->sc_slave, sc->sc_punit)) {
		aprint_normal("\n");
		aprint_error_dev(sc->sc_dev, "can't reset device\n");
		return;
	}

	if (cs80describe(parent, sc->sc_slave, sc->sc_punit, &csd)) {
		aprint_normal("\n");
		aprint_error_dev(sc->sc_dev, "didn't respond to describe command\n");
		return;
	}
	memset(name, 0, sizeof(name));
	for (i=0, n=0; i<3; i++) {
		name[n++] = (csd.d_name[i] >> 4) + '0';
		name[n++] = (csd.d_name[i] & 0x0f) + '0';
	}

#ifdef DEBUG
	if (rddebug & RDB_IDENT) {
		printf("\n%s: name: ('%s')\n",
		    device_xname(sc->sc_dev), name);
		printf("  iuw %x, maxxfr %d, ctype %d\n",
		    csd.d_iuw, csd.d_cmaxxfr, csd.d_ctype);
		printf("  utype %d, bps %d, blkbuf %d, burst %d, blktime %d\n",
		    csd.d_utype, csd.d_sectsize,
		    csd.d_blkbuf, csd.d_burstsize, csd.d_blocktime);
		printf("  avxfr %d, ort %d, atp %d, maxint %d, fv %x, rv %x\n",
		    csd.d_uavexfr, csd.d_retry, csd.d_access,
		    csd.d_maxint, csd.d_fvbyte, csd.d_rvbyte);
		printf("  maxcyl/head/sect %d/%d/%d, maxvsect %d, inter %d\n",
		    csd.d_maxcylhead >> 8, csd.d_maxcylhead & 0xff,
		    csd.d_maxsect, csd.d_maxvsectl, csd.d_interleave);
		printf("%s", device_xname(sc->sc_dev));
	}
#endif

	/*
	 * Take care of a couple of anomolies:
	 * 1. 7945A and 7946A both return same HW id
	 * 2. 9122S and 9134D both return same HW id
	 * 3. 9122D and 9134L both return same HW id
	 */
	switch (ca->ca_id) {
	case RD7946AID:
		if (memcmp(name, "079450", 6) == 0)
			type = RD7945A;
		else
			type = RD7946A;
		break;

	case RD9134LID:
		if (memcmp(name, "091340", 6) == 0)
			type = RD9134L;
		else
			type = RD9122D;
		break;

	case RD9134DID:
		if (memcmp(name, "091220", 6) == 0)
			type = RD9122S;
		else
			type = RD9134D;
		break;
	}

	sc->sc_type = type;

	/*
	 * XXX We use DEV_BSIZE instead of the sector size value pulled
	 * XXX off the driver because all of this code assumes 512 byte
	 * XXX blocks.  ICK!
	 */
	printf(": %s\n", rdidentinfo[type].ri_desc);
	printf("%s: %d cylinders, %d heads, %d blocks, %d bytes/block\n",
	    device_xname(sc->sc_dev), rdidentinfo[type].ri_ncyl,
	    rdidentinfo[type].ri_ntpc, rdidentinfo[type].ri_nblocks,
	    DEV_BSIZE);

	bufq_alloc(&sc->sc_tab, "fcfs", 0);

	/*
	 * Initialize and attach the disk structure.
	 */
	memset(&sc->sc_dk, 0, sizeof(sc->sc_dk));
	disk_init(&sc->sc_dk, device_xname(sc->sc_dev), NULL);
	disk_attach(&sc->sc_dk);

	callout_init(&sc->sc_restart_ch, 0);

	if (gpibregister(sc->sc_ic, sc->sc_slave, rdcallback, sc,
	    &sc->sc_hdl)) {
		aprint_error_dev(sc->sc_dev, "can't register callback\n");
		return;
	}

	sc->sc_flags = RDF_ALIVE;
#ifdef DEBUG
	/* always report errors */
	if (rddebug & RDB_ERROR)
		rderrthresh = 0;
#endif
	/*
	 * attach the device into the random source list
	 */
	rnd_attach_source(&sc->rnd_source, device_xname(sc->sc_dev),
			  RND_TYPE_DISK, RND_FLAG_DEFAULT);
}

/*
 * Read or construct a disklabel
 */
int
rdgetinfo(struct rd_softc *sc)
{
	struct disklabel *lp = sc->sc_dk.dk_label;
	struct partition *pi;
	const char *msg;

	memset(sc->sc_dk.dk_cpulabel, 0, sizeof(struct cpu_disklabel));

	rdgetdefaultlabel(sc, lp);

	/*
	 * Call the generic disklabel extraction routine
	 */
	msg = readdisklabel(RDMAKEDEV(0, device_unit(sc->sc_dev), RAW_PART),
	    rdstrategy, lp, NULL);
	if (msg == NULL)
		return (0);

	pi = lp->d_partitions;
	printf("%s: WARNING: %s\n", device_xname(sc->sc_dev), msg);

	pi[RAW_PART].p_size = rdidentinfo[sc->sc_type].ri_nblocks;
	lp->d_npartitions = RAW_PART+1;
	pi[0].p_size = 0;

	return (0);
}

int
rdopen(dev_t dev, int flags, int mode, struct lwp *l)
{
	struct rd_softc *sc;
	int error, mask, part;

	sc = device_lookup_private(&rd_cd, RDUNIT(dev));
	if (sc == NULL || (sc->sc_flags & RDF_ALIVE) ==0)
		return (ENXIO);

	/*
	 * Wait for any pending opens/closes to complete
	 */
	while (sc->sc_flags & (RDF_OPENING | RDF_CLOSING))
		(void) tsleep(sc, PRIBIO, "rdopen", 0);

	/*
	 * On first open, get label and partition info.
	 * We may block reading the label, so be careful
	 * to stop any other opens.
	 */
	if (sc->sc_dk.dk_openmask == 0) {
		sc->sc_flags |= RDF_OPENING;
		error = rdgetinfo(sc);
		sc->sc_flags &= ~RDF_OPENING;
		wakeup((void *)sc);
		if (error)
			return (error);
	}

	part = RDPART(dev);
	mask = 1 << part;

	/* Check that the partition exists. */
	if (part != RAW_PART && (part > sc->sc_dk.dk_label->d_npartitions ||
	    sc->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED))
		return (ENXIO);

	/* Ensure only one open at a time. */
	switch (mode) {
	case S_IFCHR:
		sc->sc_dk.dk_copenmask |= mask;
		break;
	case S_IFBLK:
		sc->sc_dk.dk_bopenmask |= mask;
		break;
	}
	sc->sc_dk.dk_openmask =
	    sc->sc_dk.dk_copenmask | sc->sc_dk.dk_bopenmask;

	return (0);
}

int
rdclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct rd_softc *sc;
	struct disk *dk;
	int mask, s;

	sc = device_lookup_private(&rd_cd, RDUNIT(dev));
	if (sc == NULL)
		return (ENXIO);

	dk = &sc->sc_dk;

	mask = 1 << RDPART(dev);
	if (mode == S_IFCHR)
		dk->dk_copenmask &= ~mask;
	else
		dk->dk_bopenmask &= ~mask;
	dk->dk_openmask = dk->dk_copenmask | dk->dk_bopenmask;
	/*
	 * On last close, we wait for all activity to cease since
	 * the label/parition info will become invalid.  Since we
	 * might sleep, we must block any opens while we are here.
	 * Note we don't have to about other closes since we know
	 * we are the last one.
	 */
	if (dk->dk_openmask == 0) {
		sc->sc_flags |= RDF_CLOSING;
		s = splbio();
		while (sc->sc_active) {
			sc->sc_flags |= RDF_WANTED;
			(void) tsleep(&sc->sc_tab, PRIBIO, "rdclose", 0);
		}
		splx(s);
		sc->sc_flags &= ~(RDF_CLOSING | RDF_WLABEL);
		wakeup((void *)sc);
	}
	return (0);
}

void
rdstrategy(struct buf *bp)
{
	struct rd_softc *sc;
	struct partition *pinfo;
	daddr_t bn;
	int sz, s;
	int offset;

	sc = device_lookup_private(&rd_cd, RDUNIT(bp->b_dev));

	DPRINTF(RDB_FOLLOW,
	    ("rdstrategy(%p): dev %" PRIx64 ", bn %" PRId64 ", bcount %d, %c\n",
	    bp, bp->b_dev, bp->b_blkno, bp->b_bcount,
	    (bp->b_flags & B_READ) ? 'R' : 'W'));

	bn = bp->b_blkno;
	sz = howmany(bp->b_bcount, DEV_BSIZE);
	pinfo = &sc->sc_dk.dk_label->d_partitions[RDPART(bp->b_dev)];

	/* Don't perform partition translation on RAW_PART. */
	offset = (RDPART(bp->b_dev) == RAW_PART) ? 0 : pinfo->p_offset;

	if (RDPART(bp->b_dev) != RAW_PART) {
		/*
		 * XXX This block of code belongs in
		 * XXX bounds_check_with_label()
		 */

		if (bn < 0 || bn + sz > pinfo->p_size) {
			sz = pinfo->p_size - bn;
			if (sz == 0) {
				bp->b_resid = bp->b_bcount;
				goto done;
			}
			if (sz < 0) {
				bp->b_error = EINVAL;
				goto done;
			}
			bp->b_bcount = dbtob(sz);
		}
		/*
		 * Check for write to write protected label
		 */
		if (bn + offset <= LABELSECTOR &&
#if LABELSECTOR != 0
		    bn + offset + sz > LABELSECTOR &&
#endif
		    !(bp->b_flags & B_READ) && !(sc->sc_flags & RDF_WLABEL)) {
			bp->b_error = EROFS;
			goto done;
		}
	}
	bp->b_rawblkno = bn + offset;
	s = splbio();
	bufq_put(sc->sc_tab, bp);
	if (sc->sc_active == 0) {
		sc->sc_active = 1;
		rdustart(sc);
	}
	splx(s);
	return;
done:
	biodone(bp);
}

/*
 * Called from timeout() when handling maintenance releases
 * callout from timeouts
 */
void
rdrestart(void *arg)
{
	int s = splbio();
	rdustart((struct rd_softc *)arg);
	splx(s);
}


/* called by rdstrategy() to start a block transfer */
/* called by rdrestart() when handingly timeouts */
/* called by rdintr() */
void
rdustart(struct rd_softc *sc)
{
	struct buf *bp;

	bp = bufq_peek(sc->sc_tab);
	sc->sc_addr = bp->b_data;
	sc->sc_resid = bp->b_bcount;
	if (gpibrequest(sc->sc_ic, sc->sc_hdl))
		rdstart(sc);
}

struct buf *
rdfinish(struct rd_softc *sc, struct buf *bp)
{

	sc->sc_errcnt = 0;
	(void)bufq_get(sc->sc_tab);
	bp->b_resid = 0;
	biodone(bp);
	gpibrelease(sc->sc_ic, sc->sc_hdl);
	if ((bp = bufq_peek(sc->sc_tab)) != NULL)
		return (bp);
	sc->sc_active = 0;
	if (sc->sc_flags & RDF_WANTED) {
		sc->sc_flags &= ~RDF_WANTED;
		wakeup((void *)&sc->sc_tab);
	}
	return (NULL);
}

void
rdcallback(void *v, int action)
{
	struct rd_softc *sc = v;

	DPRINTF(RDB_FOLLOW, ("rdcallback: v=%p, action=%d\n", v, action));

	switch (action) {
	case GPIBCBF_START:
		rdstart(sc);
		break;
	case GPIBCBF_INTR:
		rdintr(sc);
		break;
#ifdef DEBUG
	default:
		DPRINTF(RDB_ERROR, ("rdcallback: unknown action %d\n",
		    action));
		break;
#endif
	}
}


/* called from rdustart() to start a transfer */
/* called from gpib interface as the initiator */
void
rdstart(struct rd_softc *sc)
{
	struct buf *bp = bufq_peek(sc->sc_tab);
	int slave, punit;

	slave = sc->sc_slave;
	punit = sc->sc_punit;

	DPRINTF(RDB_FOLLOW, ("rdstart(%s): bp %p, %c\n",
	    device_xname(sc->sc_dev), bp, (bp->b_flags & B_READ) ? 'R' : 'W'));

again:

	sc->sc_flags |= RDF_SEEK;
	sc->sc_ioc.c_unit = CS80CMD_SUNIT(punit);
	sc->sc_ioc.c_volume = CS80CMD_SVOL(0);
	sc->sc_ioc.c_saddr = CS80CMD_SADDR;
	sc->sc_ioc.c_hiaddr = htobe16(0);
	sc->sc_ioc.c_addr = htobe32(RDBTOS(bp->b_rawblkno));
	sc->sc_ioc.c_nop2 = CS80CMD_NOP;
	sc->sc_ioc.c_slen = CS80CMD_SLEN;
	sc->sc_ioc.c_len = htobe32(sc->sc_resid);
	sc->sc_ioc.c_cmd = bp->b_flags & B_READ ? CS80CMD_READ : CS80CMD_WRITE;

	if (gpibsend(sc->sc_ic, slave, CS80CMD_SCMD, &sc->sc_ioc.c_unit,
	    sizeof(sc->sc_ioc)-1) == sizeof(sc->sc_ioc)-1) {
		/* Instrumentation. */
		disk_busy(&sc->sc_dk);
		iostat_seek(sc->sc_dk.dk_stats);
		gpibawait(sc->sc_ic);
		return;
	}
	/*
	 * Experience has shown that the gpibwait in this gpibsend will
	 * occasionally timeout.  It appears to occur mostly on old 7914
	 * drives with full maintenance tracks.  We should probably
	 * integrate this with the backoff code in rderror.
	 */

	DPRINTF(RDB_ERROR,
	    ("rdstart: cmd %x adr %ul blk %" PRId64 " len %d ecnt %d\n",
	    sc->sc_ioc.c_cmd, sc->sc_ioc.c_addr, bp->b_blkno, sc->sc_resid,
	     sc->sc_errcnt));

	sc->sc_flags &= ~RDF_SEEK;
	cs80reset(device_parent(sc->sc_dev), slave, punit);
	if (sc->sc_errcnt++ < RDRETRY)
		goto again;
	printf("%s: rdstart err: cmd 0x%x sect %uld blk %" PRId64 " len %d\n",
	       device_xname(sc->sc_dev), sc->sc_ioc.c_cmd, sc->sc_ioc.c_addr,
	       bp->b_blkno, sc->sc_resid);
	bp->b_error = EIO;
	bp = rdfinish(sc, bp);
	if (bp) {
		sc->sc_addr = bp->b_data;
		sc->sc_resid = bp->b_bcount;
		if (gpibrequest(sc->sc_ic, sc->sc_hdl))
			goto again;
	}
}

void
rdintr(struct rd_softc *sc)
{
	struct buf *bp;
	u_int8_t stat = 13;	/* in case gpibrecv fails */
	int rv, dir, restart, slave;

	slave = sc->sc_slave;
	bp = bufq_peek(sc->sc_tab);

	DPRINTF(RDB_FOLLOW, ("rdintr(%s): bp %p, %c, flags %x\n",
	    device_xname(sc->sc_dev), bp, (bp->b_flags & B_READ) ? 'R' : 'W',
	    sc->sc_flags));

	disk_unbusy(&sc->sc_dk, (bp->b_bcount - bp->b_resid),
		(bp->b_flags & B_READ));

	if (sc->sc_flags & RDF_SEEK) {
		sc->sc_flags &= ~RDF_SEEK;
		dir = (bp->b_flags & B_READ ? GPIB_READ : GPIB_WRITE);
		gpibxfer(sc->sc_ic, slave, CS80CMD_EXEC, sc->sc_addr,
		    sc->sc_resid, dir, dir == GPIB_READ);
		disk_busy(&sc->sc_dk);
		return;
	}
	if ((sc->sc_flags & RDF_SWAIT) == 0) {
		if (gpibpptest(sc->sc_ic, slave) == 0) {
			/* Instrumentation. */
			disk_busy(&sc->sc_dk);
			sc->sc_flags |= RDF_SWAIT;
			gpibawait(sc->sc_ic);
			return;
		}
	} else
		sc->sc_flags &= ~RDF_SWAIT;
	rv = gpibrecv(sc->sc_ic, slave, CS80CMD_QSTAT, &stat, 1);
	if (rv != 1 || stat) {
		DPRINTF(RDB_ERROR,
		    ("rdintr: receive failed (rv=%d) or bad stat %d\n", rv,
		     stat));
		restart = rderror(sc);
		if (sc->sc_errcnt++ < RDRETRY) {
			if (restart)
				rdstart(sc);
			return;
		}
		bp->b_error = EIO;
	}
	if (rdfinish(sc, bp) != NULL)
		rdustart(sc);
	rnd_add_uint32(&sc->rnd_source, bp->b_blkno);
}

/*
 * Deal with errors.
 * Returns 1 if request should be restarted,
 * 0 if we should just quietly give up.
 */
int
rderror(struct rd_softc *sc)
{
	struct cs80_stat css;
	struct buf *bp;
	daddr_t hwbn, pbn;

	DPRINTF(RDB_FOLLOW, ("rderror: sc=%p\n", sc));

	if (cs80status(device_parent(sc->sc_dev), sc->sc_slave,
	    sc->sc_punit, &css)) {
		cs80reset(device_parent(sc->sc_dev), sc->sc_slave,
		    sc->sc_punit);
		return (1);
	}
#ifdef DEBUG
	if (rddebug & RDB_ERROR) {			/* status info */
		printf("\n    volume: %d, unit: %d\n",
		       (css.c_vu>>4)&0xF, css.c_vu&0xF);
		printf("    reject 0x%x\n", css.c_ref);
		printf("    fault 0x%x\n", css.c_fef);
		printf("    access 0x%x\n", css.c_aef);
		printf("    info 0x%x\n", css.c_ief);
		printf("    block,  P1-P10: ");
		printf("0x%x", *(u_int32_t *)&css.c_raw[0]);
		printf("0x%x", *(u_int32_t *)&css.c_raw[4]);
		printf("0x%x\n", *(u_int16_t *)&css.c_raw[8]);
	}
#endif
	if (css.c_fef & FEF_REXMT)
		return (1);
	if (css.c_fef & FEF_PF) {
		cs80reset(device_parent(sc->sc_dev), sc->sc_slave,
		    sc->sc_punit);
		return (1);
	}
	/*
	 * Unit requests release for internal maintenance.
	 * We just delay awhile and try again later.  Use expontially
	 * increasing backoff ala ethernet drivers since we don't really
	 * know how long the maintenance will take.  With RDWAITC and
	 * RDRETRY as defined, the range is 1 to 32 seconds.
	 */
	if (css.c_fef & FEF_IMR) {
		extern int hz;
		int rdtimo = RDWAITC << sc->sc_errcnt;
		DPRINTF(RDB_STATUS,
		    ("%s: internal maintenance, %d-second timeout\n",
		    device_xname(sc->sc_dev), rdtimo));
		gpibrelease(sc->sc_ic, sc->sc_hdl);
		callout_reset(&sc->sc_restart_ch, rdtimo * hz, rdrestart, sc);
		return (0);
	}
	/*
	 * Only report error if we have reached the error reporting
	 * threshhold.  By default, this will only report after the
	 * retry limit has been exceeded.
	 */
	if (sc->sc_errcnt < rderrthresh)
		return (1);

	/*
	 * First conjure up the block number at which the error occurred.
 	 */
	bp = bufq_peek(sc->sc_tab);
	pbn = sc->sc_dk.dk_label->d_partitions[RDPART(bp->b_dev)].p_offset;
	if ((css.c_fef & FEF_CU) || (css.c_fef & FEF_DR) ||
	    (css.c_ief & IEF_RRMASK)) {
		/*
		 * Not all errors report a block number, just use b_blkno.
		 */
		hwbn = RDBTOS(pbn + bp->b_blkno);
		pbn = bp->b_blkno;
	} else {
		hwbn = css.c_blk;
		pbn = RDSTOB(hwbn) - pbn;
	}
#ifdef DEBUG
	if (rddebug & RDB_ERROR) {			/* status info */
		printf("\n    volume: %d, unit: %d\n",
		       (css.c_vu>>4)&0xF, css.c_vu&0xF);
		printf("    reject 0x%x\n", css.c_ref);
		printf("    fault 0x%x\n", css.c_fef);
		printf("    access 0x%x\n", css.c_aef);
		printf("    info 0x%x\n", css.c_ief);
		printf("    block,  P1-P10: ");
		printf("    block: %" PRId64 ", P1-P10: ", hwbn);
		printf("0x%x", *(u_int32_t *)&css.c_raw[0]);
		printf("0x%x", *(u_int32_t *)&css.c_raw[4]);
		printf("0x%x\n", *(u_int16_t *)&css.c_raw[8]);
	}
#endif
#ifdef DEBUG
	if (rddebug & RDB_ERROR) {			/* command */
		printf("    ioc: ");
		printf("0x%x", *(u_int32_t *)&sc->sc_ioc.c_pad);
		printf("0x%x", *(u_int16_t *)&sc->sc_ioc.c_hiaddr);
		printf("0x%x", *(u_int32_t *)&sc->sc_ioc.c_addr);
		printf("0x%x", *(u_int16_t *)&sc->sc_ioc.c_nop2);
		printf("0x%x", *(u_int32_t *)&sc->sc_ioc.c_len);
		printf("0x%x\n", *(u_int16_t *)&sc->sc_ioc.c_cmd);
		return (1);
	}
#endif
	/*
	 * Now output a generic message suitable for badsect.
	 * Note that we don't use harderr because it just prints
	 * out b_blkno which is just the beginning block number
	 * of the transfer, not necessary where the error occurred.
	 */
	printf("%s%c: hard error, sector number %" PRId64 "\n",
	    device_xname(sc->sc_dev), 'a'+RDPART(bp->b_dev), pbn);
	/*
	 * Now report the status as returned by the hardware with
	 * attempt at interpretation.
	 */
	printf("%s %s error:", device_xname(sc->sc_dev),
	    (bp->b_flags & B_READ) ? "read" : "write");
	printf(" unit %d, volume %d R0x%x F0x%x A0x%x I0x%x\n",
	       css.c_vu&0xF, (css.c_vu>>4)&0xF,
	       css.c_ref, css.c_fef, css.c_aef, css.c_ief);
	printf("P1-P10: ");
	printf("0x%x ", *(u_int32_t *)&css.c_raw[0]);
	printf("0x%x ", *(u_int32_t *)&css.c_raw[4]);
	printf("0x%x\n", *(u_int16_t *)&css.c_raw[8]);

	return (1);
}

int
rdread(dev_t dev, struct uio *uio, int flags)
{

	return (physio(rdstrategy, NULL, dev, B_READ, minphys, uio));
}

int
rdwrite(dev_t dev, struct uio *uio, int flags)
{

	return (physio(rdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

int
rdioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct rd_softc *sc;
	struct disklabel *lp;
	int error, flags;

	sc = device_lookup_private(&rd_cd, RDUNIT(dev));
	if (sc == NULL)
		return (ENXIO);
	lp = sc->sc_dk.dk_label;

	DPRINTF(RDB_FOLLOW, ("rdioctl: sc=%p\n", sc));

	error = disk_ioctl(&sc->sc_dk, dev, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return error;

	switch (cmd) {
	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return (EBADF);
		if (*(int *)data)
			sc->sc_flags |= RDF_WLABEL;
		else
			sc->sc_flags &= ~RDF_WLABEL;
		return (0);

	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			return (EBADF);
		return (setdisklabel(lp, (struct disklabel *)data,
		    (sc->sc_flags & RDF_WLABEL) ? 0 : sc->sc_dk.dk_openmask,
		    (struct cpu_disklabel *)0));

	case DIOCWDINFO:
		if ((flag & FWRITE) == 0)
			return (EBADF);
		error = setdisklabel(lp, (struct disklabel *)data,
		    (sc->sc_flags & RDF_WLABEL) ? 0 : sc->sc_dk.dk_openmask,
		    (struct cpu_disklabel *)0);
		if (error)
			return (error);
		flags = sc->sc_flags;
		sc->sc_flags = RDF_ALIVE | RDF_WLABEL;
		error = writedisklabel(RDLABELDEV(dev), rdstrategy, lp,
		    (struct cpu_disklabel *)0);
		sc->sc_flags = flags;
		return (error);

	case DIOCGDEFLABEL:
		rdgetdefaultlabel(sc, (struct disklabel *)data);
		return (0);
	}
	return (EINVAL);
}

void
rdgetdefaultlabel(struct rd_softc *sc, struct disklabel *lp)
{
	int type = sc->sc_type;

	memset((void *)lp, 0, sizeof(struct disklabel));

	lp->d_type = DKTYPE_HPIB /* DKTYPE_GPIB */;
	lp->d_secsize = DEV_BSIZE;
	lp->d_nsectors = rdidentinfo[type].ri_nbpt;
	lp->d_ntracks = rdidentinfo[type].ri_ntpc;
	lp->d_ncylinders = rdidentinfo[type].ri_ncyl;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
	lp->d_secperunit = lp->d_ncylinders * lp->d_secpercyl;

	strncpy(lp->d_typename, rdidentinfo[type].ri_desc, 16);
	strncpy(lp->d_packname, "fictitious", 16);
	lp->d_rpm = 3000;
	lp->d_interleave = 1;
	lp->d_flags = 0;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size =
	    lp->d_secperunit * (lp->d_secsize / DEV_BSIZE);
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);
}

int
rdsize(dev_t dev)
{
	struct rd_softc *sc;
	int psize, didopen = 0;

	sc = device_lookup_private(&rd_cd, RDUNIT(dev));
	if (sc == NULL || (sc->sc_flags & RDF_ALIVE) == 0)
		return (-1);

	/*
	 * We get called very early on (via swapconf)
	 * without the device being open so we may need
	 * to handle it here.
	 */
	if (sc->sc_dk.dk_openmask == 0) {
		if (rdopen(dev, FREAD | FWRITE, S_IFBLK, NULL))
			return (-1);
		didopen = 1;
	}
	psize = sc->sc_dk.dk_label->d_partitions[RDPART(dev)].p_size *
	    (sc->sc_dk.dk_label->d_secsize / DEV_BSIZE);
	if (didopen)
		(void) rdclose(dev, FREAD | FWRITE, S_IFBLK, NULL);
	return (psize);
}


static int rddoingadump;	/* simple mutex */

/*
 * Non-interrupt driven, non-dma dump routine.
 */
int
rddump(dev_t dev, daddr_t blkno, void *va, size_t size)
{
	struct rd_softc *sc;
	int sectorsize;		/* size of a disk sector */
	int nsects;		/* number of sectors in partition */
	int sectoff;		/* sector offset of partition */
	int totwrt;		/* total number of sectors left to write */
	int nwrt;		/* current number of sectors to write */
	int slave;
	struct disklabel *lp;
	u_int8_t stat;

	/* Check for recursive dump; if so, punt. */
	if (rddoingadump)
		return (EFAULT);
	rddoingadump = 1;

	sc = device_lookup_private(&rd_cd, RDUNIT(dev));
	if (sc == NULL || (sc->sc_flags & RDF_ALIVE) == 0)
		return (ENXIO);

	DPRINTF(RDB_FOLLOW, ("rddump: sc=%p\n", sc));

	slave = sc->sc_slave;

	/*
	 * Convert to disk sectors.  Request must be a multiple of size.
	 */
	lp = sc->sc_dk.dk_label;
	sectorsize = lp->d_secsize;
	if ((size % sectorsize) != 0)
		return (EFAULT);
	totwrt = size / sectorsize;
	blkno = dbtob(blkno) / sectorsize;	/* blkno in DEV_BSIZE units */

	nsects = lp->d_partitions[RDPART(dev)].p_size;
	sectoff = lp->d_partitions[RDPART(dev)].p_offset;

	/* Check transfer bounds against partition size. */
	if ((blkno < 0) || (blkno + totwrt) > nsects)
		return (EINVAL);

	/* Offset block number to start of partition. */
	blkno += sectoff;

	while (totwrt > 0) {
		nwrt = totwrt;		/* XXX */
#ifndef RD_DUMP_NOT_TRUSTED
		/*
		 * Fill out and send GPIB command.
		 */
		sc->sc_ioc.c_unit = CS80CMD_SUNIT(sc->sc_punit);
		sc->sc_ioc.c_volume = CS80CMD_SVOL(0);
		sc->sc_ioc.c_saddr = CS80CMD_SADDR;
		sc->sc_ioc.c_hiaddr = 0;
		sc->sc_ioc.c_addr = RDBTOS(blkno);
		sc->sc_ioc.c_nop2 = CS80CMD_NOP;
		sc->sc_ioc.c_slen = CS80CMD_SLEN;
		sc->sc_ioc.c_len = nwrt * sectorsize;
		sc->sc_ioc.c_cmd = CS80CMD_WRITE;
		(void) gpibsend(sc->sc_ic, slave, CS80CMD_SCMD,
		    &sc->sc_ioc.c_unit, sizeof(sc->sc_ioc)-3);
		if (gpibswait(sc->sc_ic, slave))
			return (EIO);
		/*
		 * Send the data.
		 */
		(void) gpibsend(sc->sc_ic, slave, CS80CMD_EXEC, va,
		    nwrt * sectorsize);
		(void) gpibswait(sc->sc_ic, slave);
		(void) gpibrecv(sc->sc_ic, slave, CS80CMD_QSTAT, &stat, 1);
		if (stat)
			return (EIO);
#else /* RD_DUMP_NOT_TRUSTED */
		/* Let's just talk about this first... */
		printf("%s: dump addr %p, blk %d\n", device_xname(sc->sc_dev),
		    va, blkno);
		delay(500 * 1000);	/* half a second */
#endif /* RD_DUMP_NOT_TRUSTED */

		/* update block count */
		totwrt -= nwrt;
		blkno += nwrt;
		va = (char *)va + sectorsize * nwrt;
	}
	rddoingadump = 0;
	return (0);
}
