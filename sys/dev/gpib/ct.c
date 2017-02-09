/*	$NetBSD: ct.c,v 1.27 2014/07/25 08:10:36 dholland Exp $ */

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
 * CS/80 cartridge tape driver (HP9144, HP88140, HP9145)
 *
 * Reminder:
 *	C_CC bit (character count option) when used in the CS/80 command
 *	'set options' will cause the tape not to stream.
 *
 * TODO:
 *	make filesystem compatible
 *	make block mode work according to mtio(4) spec. (if possible)
 *	merge with CS/80 disk driver
 *	finish support of HP9145
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ct.c,v 1.27 2014/07/25 08:10:36 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/proc.h>
#include <sys/tprintf.h>

#include <dev/gpib/ctreg.h>	/* XXX must be before cs80busvar.h ATM */

#include <dev/gpib/gpibvar.h>
#include <dev/gpib/cs80busvar.h>

/* number of eof marks to remember */
#define EOFS	128

#ifdef DEBUG
int ctdebug = 0xff;
#define CDB_FILES	0x01
#define CDB_BSF		0x02
#define CDB_IDENT	0x04
#define CDB_FAIL	0x08
#define CDB_FOLLOW	0x10
#define DPRINTF(mask, str)	if (ctdebug & (mask)) printf str
#else
#define DPRINTF(mask, str)	/* nothing */
#endif

struct	ct_softc {
	device_t sc_dev;

	gpib_chipset_tag_t sc_ic;
	gpib_handle_t sc_hdl;

	int	sc_slave;		/* GPIB slave ID */
	int	sc_punit;		/* physical unit */
	struct	ct_iocmd sc_ioc;
	struct	ct_rscmd sc_rsc;
	struct	cs80_stat sc_stat;
	struct	bufq_state *sc_tab;
	int	sc_active;
	struct	buf *sc_bp;
	struct	buf sc_bufstore;	/* XXX */
	int	sc_blkno;
	int	sc_cmd;
	int	sc_resid;
	char	*sc_addr;
	int	sc_flags;
#define	CTF_OPEN	0x01
#define	CTF_ALIVE	0x02
#define	CTF_WRT		0x04
#define	CTF_CMD		0x08
#define	CTF_IO		0x10
#define	CTF_BEOF	0x20
#define	CTF_AEOF	0x40
#define	CTF_EOT		0x80
#define	CTF_STATWAIT	0x100
#define CTF_CANSTREAM	0x200
#define	CTF_WRTTN	0x400
	short	sc_type;
	tpr_t	sc_tpr;
	int	sc_eofp;
	int	sc_eofs[EOFS];
};

int	ctmatch(device_t, cfdata_t, void *);
void	ctattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(ct, sizeof(struct ct_softc),
	ctmatch, ctattach, NULL, NULL);

int	ctident(device_t, struct ct_softc *,
	    struct cs80bus_attach_args *);

int	ctlookup(int, int, int);
void	ctaddeof(struct ct_softc *);
void	ctustart(struct ct_softc *);
void	cteof(struct ct_softc *, struct buf *);
void	ctdone(struct ct_softc *, struct buf *);

void	ctcallback(void *, int);
void	ctstart(struct ct_softc *);
void	ctintr(struct ct_softc *);

void	ctcommand(dev_t, int, int);

dev_type_open(ctopen);
dev_type_close(ctclose);
dev_type_read(ctread);
dev_type_write(ctwrite);
dev_type_ioctl(ctioctl);
dev_type_strategy(ctstrategy);

const struct bdevsw ct_bdevsw = {
	.d_open = ctopen,
	.d_close = ctclose,
	.d_strategy = ctstrategy,
	.d_ioctl = ctioctl,
	.d_dump = nodump,
	.d_psize = nosize,
	.d_discard = nodiscard,
	.d_flag = D_TAPE
};

const struct cdevsw ct_cdevsw = {
	.d_open = ctopen,
	.d_close = ctclose,
	.d_read = ctread,
	.d_write = ctwrite,
	.d_ioctl = ctioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TAPE
};

extern struct cfdriver ct_cd;

struct	ctinfo {
	short	hwid;
	short	punit;
	const char	*desc;
} ctinfo[] = {
	{ CT7946ID,	1,	"7946A"	},
	{ CT7912PID,	1,	"7912P"	},
	{ CT7914PID,	1,	"7914P"	},
	{ CT9144ID,	0,	"9144"	},
	{ CT9145ID,	0,	"9145"	},
	{ CT35401ID,	0,	"35401A"},
};
int	nctinfo = sizeof(ctinfo) / sizeof(ctinfo[0]);

#define	CT_NOREW	4
#define	CT_STREAM	8
#define	CTUNIT(x)	(minor(x) & 0x03)

int
ctlookup(int id, int slave, int punit)
{
	int i;

	for (i = 0; i < nctinfo; i++)
		if (ctinfo[i].hwid == id)
			break;
	if (i == nctinfo)
		return (-1);
	return (i);
}

int
ctmatch(device_t parent, cfdata_t match, void *aux)
{
	struct cs80bus_attach_args *ca = aux;
	int i;

	if ((i = ctlookup(ca->ca_id, ca->ca_slave, ca->ca_punit)) < 0)
		return (0);
	ca->ca_punit = ctinfo[i].punit;
	return (1);
}

void
ctattach(device_t parent, device_t self, void *aux)
{
	struct ct_softc *sc = device_private(self);
	struct cs80bus_attach_args *ca = aux;
	struct cs80_description csd;
	char name[7];
	int type, i, n, canstream = 0;

	sc->sc_ic = ca->ca_ic;
	sc->sc_slave = ca->ca_slave;
	sc->sc_punit = ca->ca_punit;

	if ((type = ctlookup(ca->ca_id, ca->ca_slave, ca->ca_punit)) < 0)
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
	if (ctdebug & CDB_IDENT) {
		printf("\n%s: name: ('%s')\n",
		    device_xname(sc->sc_dev),name);
		printf("  iuw %x, maxxfr %d, ctype %d\n",
		    csd.d_iuw, csd.d_cmaxxfr, csd.d_ctype);
		printf("  utype %d, bps %d, blkbuf %d, burst %d, blktime %d\n",
		    csd.d_utype, csd.d_sectsize,
		    csd.d_blkbuf, csd.d_burstsize, csd.d_blocktime);
		printf("  avxfr %d, ort %d, atp %d, maxint %d, fv %x, rv %x\n",
		    csd.d_uavexfr, csd.d_retry, csd.d_access,
		    csd.d_maxint, csd.d_fvbyte, csd.d_rvbyte);
		printf("  maxcyl/head/sect %d/%d/%d, maxvsect %d, inter %d\n",
		    csd.d_maxcylhead >> 8 , csd.d_maxcylhead & 0xff,
		    csd.d_maxsect, csd.d_maxvsectl, csd.d_interleave);
		printf("%s", device_xname(sc->sc_dev));
	}
#endif

	switch (ca->ca_id) {
	case CT7946ID:
		if (memcmp(name, "079450", 6) == 0)
			return;			/* not really a 7946 */
		/* fall into... */
	case CT9144ID:
	case CT9145ID:
	case CT35401ID:
		sc->sc_type = CT9144;
		canstream = 1;
		break;

	case CT7912PID:
	case CT7914PID:
		sc->sc_type = CT88140;
		break;
	default:
		sc->sc_type = type;
		break;
	}

	sc->sc_type = type;
	sc->sc_flags = canstream ? CTF_CANSTREAM : 0;
	printf(": %s %stape\n", ctinfo[type].desc,
	    canstream ? "streaming " : "");

	bufq_alloc(&sc->sc_tab, "fcfs", 0);

	if (gpibregister(sc->sc_ic, sc->sc_slave, ctcallback, sc,
	    &sc->sc_hdl)) {
		aprint_error_dev(sc->sc_dev, "can't register callback\n");
		return;
	}

	sc->sc_flags |= CTF_ALIVE;
}

/*ARGSUSED*/
int
ctopen(dev_t dev, int flag, int type, struct lwp *l)
{
	struct ct_softc *sc;
	u_int8_t opt;

	sc = device_lookup_private(&ct_cd, CTUNIT(dev));
	if (sc == NULL || (sc->sc_flags & CTF_ALIVE) == 0)
		return (ENXIO);

	if (sc->sc_flags & CTF_OPEN)
		return (EBUSY);

	if ((dev & CT_STREAM) && (sc->sc_flags & CTF_CANSTREAM))
		opt = C_SPAR | C_IMRPT;
	else
		opt = C_SPAR;

	if (cs80setoptions(device_parent(sc->sc_dev), sc->sc_slave,
	    sc->sc_punit, opt))
		return (EBUSY);

	sc->sc_tpr = tprintf_open(l->l_proc);
	sc->sc_flags |= CTF_OPEN;

	return (0);
}

/*ARGSUSED*/
int
ctclose(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct ct_softc *sc;

	sc = device_lookup_private(&ct_cd, CTUNIT(dev));
	if (sc == NULL)
		return (ENXIO);

	if ((sc->sc_flags & (CTF_WRT|CTF_WRTTN)) == (CTF_WRT|CTF_WRTTN) &&
	    (sc->sc_flags & CTF_EOT) == 0 ) { /* XXX return error if EOT ?? */
		ctcommand(dev, MTWEOF, 2);
		ctcommand(dev, MTBSR, 1);
		if (sc->sc_eofp == EOFS - 1)
			sc->sc_eofs[EOFS - 1]--;
		else
			sc->sc_eofp--;
		DPRINTF(CDB_BSF, ("%s: ctclose backup eofs prt %d blk %d\n",
		    device_xname(sc->sc_dev), sc->sc_eofp,
		    sc->sc_eofs[sc->sc_eofp]));
	}

	if ((minor(dev) & CT_NOREW) == 0)
		ctcommand(dev, MTREW, 1);
	sc->sc_flags &= ~(CTF_OPEN | CTF_WRT | CTF_WRTTN);
	tprintf_close(sc->sc_tpr);
	DPRINTF(CDB_FILES, ("ctclose: flags %x\n", sc->sc_flags));

	return (0);	/* XXX */
}

void
ctcommand(dev_t dev, int cmd, int cnt)
{
	struct ct_softc *sc;
	struct buf *bp;
	struct buf *nbp = 0;

	sc = device_lookup_private(&ct_cd, CTUNIT(dev));
	bp = &sc->sc_bufstore;

	DPRINTF(CDB_FOLLOW, ("ctcommand: called\n"));

	if (cmd == MTBSF && sc->sc_eofp == EOFS - 1) {
		cnt = sc->sc_eofs[EOFS - 1] - cnt;
		ctcommand(dev, MTREW, 1);
		ctcommand(dev, MTFSF, cnt);
		cnt = 2;
		cmd = MTBSR;
	}

	if (cmd == MTBSF && sc->sc_eofp - cnt < 0) {
		cnt = 1;
		cmd = MTREW;
	}

	sc->sc_flags |= CTF_CMD;
	sc->sc_bp = bp;
	sc->sc_cmd = cmd;
	bp->b_dev = dev;
	bp->b_objlock = &buffer_lock;
	if (cmd == MTFSF) {
		nbp = (struct buf *)geteblk(MAXBSIZE);
		bp->b_data = nbp->b_data;
		bp->b_bcount = MAXBSIZE;
	}

	while (cnt-- > 0) {
		bp->b_flags = 0;
		bp->b_cflags = BC_BUSY;
		bp->b_oflags = 0;
		if (cmd == MTBSF) {
			sc->sc_blkno = sc->sc_eofs[sc->sc_eofp];
			sc->sc_eofp--;
			DPRINTF(CDB_BSF, ("%s: backup eof pos %d blk %d\n",
			    device_xname(sc->sc_dev), sc->sc_eofp,
			    sc->sc_eofs[sc->sc_eofp]));
		}
		ctstrategy(bp);
		biowait(bp);
	}
	bp->b_flags = 0;
	sc->sc_flags &= ~CTF_CMD;
	if (nbp)
		brelse(nbp, 0);
}

void
ctstrategy(struct buf *bp)
{
	struct ct_softc *sc;
	int s;

	DPRINTF(CDB_FOLLOW, ("cdstrategy(%p): dev %" PRIx64 ", bn %" PRIx64
	    ", bcount %x, %c\n",
	    bp, bp->b_dev, bp->b_blkno, bp->b_bcount,
	    (bp->b_flags & B_READ) ? 'R' : 'W'));

	sc = device_lookup_private(&ct_cd, CTUNIT(bp->b_dev));

	s = splbio();
	bufq_put(sc->sc_tab, bp);
	if (sc->sc_active == 0) {
		sc->sc_active = 1;
		ctustart(sc);
	}
	splx(s);
}

void
ctustart(struct ct_softc *sc)
{
	struct buf *bp;

	bp = bufq_peek(sc->sc_tab);
	sc->sc_addr = bp->b_data;
	sc->sc_resid = bp->b_bcount;
	if (gpibrequest(sc->sc_ic, sc->sc_hdl))
		ctstart(sc);
}

void
ctstart(struct ct_softc *sc)
{
	struct buf *bp;
	struct ct_ulcmd ul;
	struct ct_wfmcmd wfm;
	int i, slave, punit;

	slave = sc->sc_slave;
	punit = sc->sc_punit;

	bp = bufq_peek(sc->sc_tab);
	if ((sc->sc_flags & CTF_CMD) && sc->sc_bp == bp) {
		switch(sc->sc_cmd) {
		case MTFSF:
			bp->b_flags |= B_READ;
			goto mustio;

		case MTBSF:
			goto gotaddr;

		case MTOFFL:
			sc->sc_blkno = 0;
			ul.unit = CS80CMD_SUNIT(punit);
			ul.cmd = CS80CMD_UNLOAD;
			(void) cs80send(device_parent(sc->sc_dev), slave,
			    punit, CS80CMD_SCMD, &ul, sizeof(ul));
			break;

		case MTWEOF:
			sc->sc_blkno++;
			sc->sc_flags |= CTF_WRT;
			wfm.unit = CS80CMD_SUNIT(sc->sc_punit);
			wfm.cmd = CS80CMD_WFM;
			(void) cs80send(device_parent(sc->sc_dev), slave,
			    punit, CS80CMD_SCMD, &wfm, sizeof(wfm));
			ctaddeof(sc);
			break;

		case MTBSR:
			sc->sc_blkno--;
			goto gotaddr;

		case MTFSR:
			sc->sc_blkno++;
			goto gotaddr;

		case MTREW:
			sc->sc_blkno = 0;
			DPRINTF(CDB_BSF, ("%s: clearing eofs\n",
			    device_xname(sc->sc_dev)));
			for (i=0; i<EOFS; i++)
				sc->sc_eofs[i] = 0;
			sc->sc_eofp = 0;

gotaddr:
			sc->sc_ioc.unit = CS80CMD_SUNIT(sc->sc_punit);
			sc->sc_ioc.saddr = CS80CMD_SADDR;
			sc->sc_ioc.addr0 = 0;
			sc->sc_ioc.addr = htobe32(sc->sc_blkno);
			sc->sc_ioc.nop2 = CS80CMD_NOP;
			sc->sc_ioc.slen = CS80CMD_SLEN;
			sc->sc_ioc.len = htobe32(0);
			sc->sc_ioc.nop3 = CS80CMD_NOP;
			sc->sc_ioc.cmd = CS80CMD_READ;
			(void) cs80send(device_parent(sc->sc_dev), slave,
			    punit, CS80CMD_SCMD, &sc->sc_ioc,
			    sizeof(sc->sc_ioc));
			break;
		}
	} else {
mustio:
		if ((bp->b_flags & B_READ) &&
		    sc->sc_flags & (CTF_BEOF|CTF_EOT)) {
			DPRINTF(CDB_FILES, ("ctstart: before %x\n",
				    sc->sc_flags));
			if (sc->sc_flags & CTF_BEOF) {
				sc->sc_flags &= ~CTF_BEOF;
				sc->sc_flags |= CTF_AEOF;
				DPRINTF(CDB_FILES, ("ctstart: after %x\n",
				    sc->sc_flags));
			}
			bp->b_resid = bp->b_bcount;
			ctdone(sc, bp);
			return;
		}
		sc->sc_flags |= CTF_IO;
		sc->sc_ioc.unit = CS80CMD_SUNIT(sc->sc_punit);
		sc->sc_ioc.saddr = CS80CMD_SADDR;
		sc->sc_ioc.addr0 = 0;
		sc->sc_ioc.addr = htobe32(sc->sc_blkno);
		sc->sc_ioc.nop2 = CS80CMD_NOP;
		sc->sc_ioc.slen = CS80CMD_SLEN;
		sc->sc_ioc.len = htobe32(sc->sc_resid);
		sc->sc_ioc.nop3 = CS80CMD_NOP;
		if (bp->b_flags & B_READ)
			sc->sc_ioc.cmd = CS80CMD_READ;
		else {
			sc->sc_ioc.cmd = CS80CMD_WRITE;
			sc->sc_flags |= (CTF_WRT | CTF_WRTTN);
		}
		(void) cs80send(device_parent(sc->sc_dev), slave, punit,
		    CS80CMD_SCMD, &sc->sc_ioc, sizeof(sc->sc_ioc));
	}
	gpibawait(sc->sc_ic);
}

/*
 * Hideous grue to handle EOF/EOT (mostly for reads)
 */
void
cteof(struct ct_softc *sc, struct buf *bp)
{
	long blks;

	/*
	 * EOT on a write is an error.
	 */
	if ((bp->b_flags & B_READ) == 0) {
		bp->b_resid = bp->b_bcount;
		bp->b_error = ENOSPC;
		sc->sc_flags |= CTF_EOT;
		return;
	}
	/*
	 * Use returned block position to determine how many blocks
	 * we really read and update b_resid.
	 */
	blks = sc->sc_stat.c_blk - sc->sc_blkno - 1;
	DPRINTF(CDB_FILES, ("cteof: bc %d oblk %d nblk %d read %ld, resid %ld\n",
	    bp->b_bcount, sc->sc_blkno, sc->sc_stat.c_blk,
	    blks, bp->b_bcount - CTKTOB(blks)));
	if (blks == -1) { /* 9145 on EOF does not change sc_stat.c_blk */
		blks = 0;
		sc->sc_blkno++;
	}
	else {
		sc->sc_blkno = sc->sc_stat.c_blk;
	}
	bp->b_resid = bp->b_bcount - CTKTOB(blks);
	/*
	 * If we are at physical EOV or were after an EOF,
	 * we are now at logical EOT.
	 */
	if ((sc->sc_stat.c_aef & AEF_EOV) ||
	    (sc->sc_flags & CTF_AEOF)) {
		sc->sc_flags |= CTF_EOT;
		sc->sc_flags &= ~(CTF_AEOF|CTF_BEOF);
	}
	/*
	 * If we were before an EOF or we have just completed a FSF,
	 * we are now after EOF.
	 */
	else if ((sc->sc_flags & CTF_BEOF) ||
		 ((sc->sc_flags & CTF_CMD) && sc->sc_cmd == MTFSF)) {
		sc->sc_flags |= CTF_AEOF;
		sc->sc_flags &= ~CTF_BEOF;
	}
	/*
	 * Otherwise if we read something we are now before EOF
	 * (and no longer after EOF).
	 */
	else if (blks) {
		sc->sc_flags |= CTF_BEOF;
		sc->sc_flags &= ~CTF_AEOF;
	}
	/*
	 * Finally, if we didn't read anything we just passed an EOF
	 */
	else
		sc->sc_flags |= CTF_AEOF;
	DPRINTF(CDB_FILES, ("cteof: leaving flags %x\n", sc->sc_flags));
}


void
ctcallback(void *v, int action)
{
	struct ct_softc *sc = v;

	DPRINTF(CDB_FOLLOW, ("ctcallback: v=%p, action=%d\n", v, action));

	switch (action) {
	case GPIBCBF_START:
		ctstart(sc);
		break;
	case GPIBCBF_INTR:
		ctintr(sc);
		break;
#ifdef DEBUG
	default:
		DPRINTF(CDB_FAIL, ("ctcallback: unknown action %d\n", action));
		break;
#endif
	}
}

void
ctintr(struct ct_softc *sc)
{
	struct buf *bp;
	u_int8_t stat;
	int slave, punit;
	int dir;

	slave = sc->sc_slave;
	punit = sc->sc_punit;

	bp = bufq_peek(sc->sc_tab);
	if (bp == NULL) {
		aprint_error_dev(sc->sc_dev, "bp == NULL\n");
		return;
	}
	if (sc->sc_flags & CTF_IO) {
		sc->sc_flags &= ~CTF_IO;
		dir = (bp->b_flags & B_READ ? GPIB_READ : GPIB_WRITE);
		gpibxfer(sc->sc_ic, slave, CS80CMD_EXEC, sc->sc_addr,
		    sc->sc_resid, dir, dir == GPIB_READ);
		return;
	}
	if ((sc->sc_flags & CTF_STATWAIT) == 0) {
		if (gpibpptest(sc->sc_ic, slave) == 0) {
			sc->sc_flags |= CTF_STATWAIT;
			gpibawait(sc->sc_ic);
			return;
		}
	} else
		sc->sc_flags &= ~CTF_STATWAIT;
	(void) gpibrecv(sc->sc_ic, slave, CS80CMD_QSTAT, &stat, 1);
	DPRINTF(CDB_FILES, ("ctintr: before flags %x\n", sc->sc_flags));
	if (stat) {
		sc->sc_rsc.unit = CS80CMD_SUNIT(punit);
		sc->sc_rsc.cmd = CS80CMD_STATUS;
		(void) gpibsend(sc->sc_ic, slave, CS80CMD_SCMD, &sc->sc_rsc,
		    sizeof(sc->sc_rsc));
		(void) gpibrecv(sc->sc_ic, slave, CS80CMD_EXEC, &sc->sc_stat,
		    sizeof(sc->sc_stat));
		(void) gpibrecv(sc->sc_ic, slave, CS80CMD_QSTAT, &stat, 1);
		DPRINTF(CDB_FILES, ("ctintr: return stat 0x%x, A%x F%x blk %d\n",
			       stat, sc->sc_stat.c_aef,
			       sc->sc_stat.c_fef, sc->sc_stat.c_blk));
		if (stat == 0) {
			if (sc->sc_stat.c_aef & (AEF_EOF | AEF_EOV)) {
				cteof(sc, bp);
				ctaddeof(sc);
				goto done;
			}
			if (sc->sc_stat.c_fef & FEF_PF) {
				cs80reset(sc, slave, punit);
				ctstart(sc);
				return;
			}
			if (sc->sc_stat.c_fef & FEF_REXMT) {
				ctstart(sc);
				return;
			}
			if (sc->sc_stat.c_aef & 0x5800) {
				if (sc->sc_stat.c_aef & 0x4000)
					tprintf(sc->sc_tpr,
					    "%s: uninitialized media\n",
					    device_xname(sc->sc_dev));
				if (sc->sc_stat.c_aef & 0x1000)
					tprintf(sc->sc_tpr,
					    "%s: not ready\n",
					    device_xname(sc->sc_dev));
				if (sc->sc_stat.c_aef & 0x0800)
					tprintf(sc->sc_tpr,
					    "%s: write protect\n",
					    device_xname(sc->sc_dev));
			} else {
				printf("%s err: v%d u%d ru%d bn%d, ",
				    device_xname(sc->sc_dev),
				    (sc->sc_stat.c_vu>>4)&0xF,
				    sc->sc_stat.c_vu&0xF,
				    sc->sc_stat.c_pend,
				    sc->sc_stat.c_blk);
				printf("R0x%x F0x%x A0x%x I0x%x\n",
				    sc->sc_stat.c_ref,
				    sc->sc_stat.c_fef,
				    sc->sc_stat.c_aef,
				    sc->sc_stat.c_ief);
			}
		} else
			aprint_error_dev(sc->sc_dev, "request status failed\n");
		bp->b_error = EIO;
		goto done;
	} else
		bp->b_resid = 0;
	if (sc->sc_flags & CTF_CMD) {
		switch (sc->sc_cmd) {
		case MTFSF:
			sc->sc_flags &= ~(CTF_BEOF|CTF_AEOF);
			sc->sc_blkno += CTBTOK(sc->sc_resid);
			ctstart(sc);
			return;
		case MTBSF:
			sc->sc_flags &= ~(CTF_AEOF|CTF_BEOF|CTF_EOT);
			break;
		case MTBSR:
			sc->sc_flags &= ~CTF_BEOF;
			if (sc->sc_flags & CTF_EOT) {
				sc->sc_flags |= CTF_AEOF;
				sc->sc_flags &= ~CTF_EOT;
			} else if (sc->sc_flags & CTF_AEOF) {
				sc->sc_flags |= CTF_BEOF;
				sc->sc_flags &= ~CTF_AEOF;
			}
			break;
		case MTWEOF:
			sc->sc_flags &= ~CTF_BEOF;
			if (sc->sc_flags & (CTF_AEOF|CTF_EOT)) {
				sc->sc_flags |= CTF_EOT;
				sc->sc_flags &= ~CTF_AEOF;
			} else
				sc->sc_flags |= CTF_AEOF;
			break;
		case MTREW:
		case MTOFFL:
			sc->sc_flags &= ~(CTF_BEOF|CTF_AEOF|CTF_EOT);
			break;
		}
	} else {
		sc->sc_flags &= ~CTF_AEOF;
		sc->sc_blkno += CTBTOK(sc->sc_resid);
	}
done:
	DPRINTF(CDB_FILES, ("ctintr: after flags %x\n", sc->sc_flags));
	ctdone(sc, bp);
}

void
ctdone(struct ct_softc *sc, struct buf *bp)
{

	(void)bufq_get(sc->sc_tab);
	biodone(bp);
	gpibrelease(sc->sc_ic, sc->sc_hdl);
	if (bufq_peek(sc->sc_tab) == NULL) {
		sc->sc_active = 0;
		return;
	}
	ctustart(sc);
}

int
ctread(dev_t dev, struct uio *uio, int flags)
{
	return (physio(ctstrategy, NULL, dev, B_READ, minphys, uio));
}

int
ctwrite(dev_t dev, struct uio *uio, int flags)
{
	/* XXX: check for hardware write-protect? */
	return (physio(ctstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*ARGSUSED*/
int
ctioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct mtop *op;
	int cnt;

	switch (cmd) {

	case MTIOCTOP:
		op = (struct mtop *)data;
		switch(op->mt_op) {

		case MTWEOF:
		case MTFSF:
		case MTBSR:
		case MTBSF:
		case MTFSR:
			cnt = op->mt_count;
			break;

		case MTREW:
		case MTOFFL:
			cnt = 1;
			break;

		default:
			return (EINVAL);
		}
		ctcommand(dev, op->mt_op, cnt);
		break;

	case MTIOCGET:
		break;

	default:
		return (EINVAL);
	}
	return (0);
}

void
ctaddeof(struct ct_softc *sc)
{

	if (sc->sc_eofp == EOFS - 1)
		sc->sc_eofs[EOFS - 1]++;
	else {
		sc->sc_eofp++;
		if (sc->sc_eofp == EOFS - 1)
			sc->sc_eofs[EOFS - 1] = EOFS;
		else
			/* save blkno */
			sc->sc_eofs[sc->sc_eofp] = sc->sc_blkno - 1;
	}
	DPRINTF(CDB_BSF, ("%s: add eof pos %d blk %d\n",
		       device_xname(sc->sc_dev), sc->sc_eofp,
		       sc->sc_eofs[sc->sc_eofp]));
}
