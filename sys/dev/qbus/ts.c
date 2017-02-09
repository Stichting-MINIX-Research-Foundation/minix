/*	$NetBSD: ts.c,v 1.31 2014/07/25 08:10:38 dholland Exp $ */

/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	@(#)tmscp.c	7.16 (Berkeley) 5/9/91
 */

/*
 * sccsid = "@(#)tmscp.c	1.24	(ULTRIX)	1/21/86";
 */

/************************************************************************
 *									*
 *	  Licensed from Digital Equipment Corporation			*
 *			 Copyright (c)					*
 *		 Digital Equipment Corporation				*
 *		     Maynard, Massachusetts				*
 *			   1985, 1986					*
 *		      All rights reserved.				*
 *									*
 *	  The Information in this software is subject to change		*
 *   without notice and should not be construed as a commitment		*
 *   by	 Digital  Equipment  Corporation.   Digital   makes  no		*
 *   representations about the suitability of this software for		*
 *   any purpose.  It is supplied "As Is" without expressed  or		*
 *   implied  warranty.							*
 *									*
 *	  If the Regents of the University of California or its		*
 *   licensees modify the software in a manner creating			*
 *   derivative copyright rights, appropriate copyright			*
 *   legends may be placed on  the derivative work in addition		*
 *   to that set forth above.						*
 *									*
 ************************************************************************/

/*
 * TSV05/TS05 device driver, written by Bertram Barth.
 *
 * should be TS11 compatible (untested)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ts.c,v 1.31 2014/07/25 08:10:38 dholland Exp $");

#undef	TSDEBUG

/*
 * TODO:
 *
 * Keep track of tape position so that lseek et al works.
 * Use tprintf to inform the user, not the system console.
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/syslog.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/uio.h>
#include <sys/proc.h>

#include <sys/bus.h>

#include <dev/qbus/ubareg.h>
#include <dev/qbus/ubavar.h>

#include <dev/qbus/tsreg.h>

#include "ioconf.h"

struct ts {
	struct cmd cmd;	/* command packet */
	struct chr chr;	/* characteristics packet */
	struct status status;	/* status packet */
};

/*
 * Software status, per controller.
 * also status per tape-unit, since only one unit per controller
 * (thus we have no struct ts_info)
 */
struct ts_softc {
	device_t sc_dev;		/* Autoconf ... */
	struct uba_softc *sc_uh;	/* the parent UBA */
	struct uba_unit sc_unit;	/* Struct common for UBA to talk */
	struct evcnt sc_intrcnt;	/* Interrupt counting */
	struct ubinfo sc_ui;		/* mapping info for struct ts */
	struct uba_unit sc_uu;		/* Struct for UBA to communicate */
	bus_space_tag_t sc_iot;
	bus_addr_t sc_ioh;
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_dmam;
	volatile struct ts *sc_vts;	/* Memory address of ts struct */
	struct ts *sc_bts;		/* Unibus address of ts struct */
	int	sc_type;		/* TS11 or TS05? */
	short	sc_waddr;		/* Value to write to TSDB */
	struct bufq_state *sc_bufq;	/* pending I/O requests */

	short	sc_mapped;		/* Unibus map allocated ? */
	short	sc_state;		/* see below: ST_xxx */
	short	sc_rtc;			/* retry count for lcmd */
	short	sc_openf;		/* lock against multiple opens */
	short	sc_liowf;		/* last operation was write */
	struct buf ts_cbuf;		/* internal cmd buffer (for ioctls) */
};

#define	TS_WCSR(csr, val) \
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, csr, val)
#define TS_RCSR(csr) \
	bus_space_read_2(sc->sc_iot, sc->sc_ioh, csr)

#define LOWORD(x)	((int)(x) & 0xffff)
#define HIWORD(x)	(((int)(x) >> 16) & 0x3f)

#define	TYPE_TS11	0
#define	TYPE_TS05	1
#define	TYPE_TU80	2

#define	TS_INVALID	0
#define	TS_INIT		1
#define	TS_RUNNING	2
#define	TS_FASTREPOS	3

static	void tsintr(void *);
static	void tsinit(struct ts_softc *);
static	void tscommand(struct ts_softc *, dev_t, int, int);
static	int tsstart(struct ts_softc *, int);
static	void tswchar(struct ts_softc *);
static	bool tsreset(struct ts_softc *);
static	int tsmatch(device_t, cfdata_t, void *);
static	void tsattach(device_t, device_t, void *);
static	int tsready(struct uba_unit *);

CFATTACH_DECL_NEW(ts, sizeof(struct ts_softc),
    tsmatch, tsattach, NULL, NULL);

dev_type_open(tsopen);
dev_type_close(tsclose);
dev_type_read(tsread);
dev_type_write(tswrite);
dev_type_ioctl(tsioctl);
dev_type_strategy(tsstrategy);
dev_type_dump(tsdump);

const struct bdevsw ts_bdevsw = {
	.d_open = tsopen,
	.d_close = tsclose,
	.d_strategy = tsstrategy,
	.d_ioctl = tsioctl,
	.d_dump = tsdump,
	.d_psize = nosize,
	.d_discard = nodiscard,
	.d_flag = D_TAPE
};

const struct cdevsw ts_cdevsw = {
	.d_open = tsopen,
	.d_close = tsclose,
	.d_read = tsread,
	.d_write = tswrite,
	.d_ioctl = tsioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TAPE
};

/* Bits in minor device */
#define TS_UNIT(dev)	(minor(dev)&03)
#define TS_HIDENSITY	010

#define TS_PRI	LOG_INFO


/*
 * Probe for device. If found, try to raise an interrupt.
 */
int
tsmatch(device_t parent, cfdata_t match, void *aux)
{
	struct device tsdev;
	struct ts_softc ssc;
	struct ts_softc *sc = &ssc;
	struct uba_attach_args *ua = aux;
	int i;

	sc->sc_iot = ua->ua_iot;
	sc->sc_ioh = ua->ua_ioh;
	sc->sc_mapped = 0;
	sc->sc_dev = &tsdev;
	sc->sc_uh = device_private(parent);
	strcpy(sc->sc_dev->dv_xname, "ts");

	/* Try to reset the device */
	for (i = 0; i < 3; i++) {
		if (tsreset(sc))
			break;
	}

	if (i == 3)
		return 0;

	tsinit(sc);
	tswchar(sc);		/* write charact. to enable interrupts */
				/* completion of this will raise the intr. */

	DELAY(1000000);		/* Wait for interrupt */
	ubmemfree(sc->sc_uh, &sc->sc_ui);
	return 1;
}

/*
 */
void
tsattach(device_t parent, device_t self, void *aux)
{
	struct ts_softc *sc = device_private(self);
	struct uba_attach_args *ua = aux;
	int error;
	const char *t;

	sc->sc_dev = self;
	sc->sc_uh = device_private(parent);
	sc->sc_iot = ua->ua_iot;
	sc->sc_ioh = ua->ua_ioh;
	sc->sc_dmat = ua->ua_dmat;

	sc->sc_uu.uu_dev = self;
	sc->sc_uu.uu_ready = tsready;

	tsinit(sc);	/* reset and map */

	error = bus_dmamap_create(sc->sc_dmat, (64*1024), 16, (64*1024),
	    0, BUS_DMA_NOWAIT, &sc->sc_dmam);
	if (error) {
		aprint_error(": failed create DMA map %d\n", error);
		return;
	}

	bufq_alloc(&sc->sc_bufq, "fcfs", 0);

	/*
	 * write the characteristics (again)
	 */
	sc->sc_state = TS_INIT;		/* tsintr() checks this ... */
	tswchar(sc);
	if (tsleep(sc, PRIBIO, "tsattach", 100)) {
		aprint_error(": failed SET CHARACTERISTICS\n");
		return;
	}

	sc->sc_state = TS_RUNNING;
	if (sc->sc_uh->uh_type == UBA_UBA) {
		if (sc->sc_vts->status.xst2 & TS_SF_TU80) {
			sc->sc_type = TYPE_TU80;
			t = "TU80";
		} else {
			sc->sc_type = TYPE_TS11;
			t = "TS11";
		}
	} else {
		sc->sc_type = TYPE_TS05;
		t = "TS05";
	}

	aprint_normal(": %s\n", t);
	aprint_normal_dev(sc->sc_dev, 
	    "rev %d, extended features %s, transport %s\n",
	    (sc->sc_vts->status.xst2 & TS_SF_MCRL) >> 2,
	    (sc->sc_vts->status.xst2 & TS_SF_EFES ? "enabled" : "disabled"),
	    (TS_RCSR(TSSR) & TS_OFL ? "offline" : "online"));

	uba_intr_establish(ua->ua_icookie, ua->ua_cvec, tsintr,
	    sc, &sc->sc_intrcnt);
	evcnt_attach_dynamic(&sc->sc_intrcnt, EVCNT_TYPE_INTR, ua->ua_evcnt,
	    device_xname(sc->sc_dev), "intr");
}

/*
 * Initialize a TS device. Set up UBA mapping registers,
 * initialize data structures, what else ???
 */
void
tsinit(struct ts_softc *sc)
{
	if (sc->sc_mapped == 0) {

		/*
		 * Map the communications area and command and message
		 * buffer into Unibus address space.
		 */
		sc->sc_ui.ui_size = sizeof(struct ts);
		if (ubmemalloc(sc->sc_uh, &sc->sc_ui, UBA_CANTWAIT))
			return;
		sc->sc_vts = (void *)sc->sc_ui.ui_vaddr;
		sc->sc_bts = (void *)sc->sc_ui.ui_baddr;
		sc->sc_waddr = sc->sc_ui.ui_baddr |
		    ((sc->sc_ui.ui_baddr >> 16) & 3);
		sc->sc_mapped = 1;
	}
	tsreset(sc);
}

/*
 * Execute a (ioctl) command on the tape drive a specified number of times.
 * This routine sets up a buffer and calls the strategy routine which
 * issues the command to the controller.
 */
void
tscommand(struct ts_softc *sc, dev_t dev, int cmd, int count)
{
	struct buf *bp;

#ifdef TSDEBUG
	printf("tscommand (%x, %d)\n", cmd, count);
#endif

	bp = &sc->ts_cbuf;

	mutex_enter(&bufcache_lock);
	while (bp->b_cflags & BC_BUSY) {
		/*
		 * This special check is because BC_BUSY never
		 * gets cleared in the non-waiting rewind case. ???
		 */
		if (bp->b_bcount == 0 && (bp->b_oflags & BO_DONE))
			break;
		if (bbusy(bp, false, 0, NULL) == 0)
			break;
		/* check MOT-flag !!! */
	}
	bp->b_flags = B_READ;
	mutex_exit(&bufcache_lock);

	/*
	 * Load the buffer.  The b_count field gets used to hold the command
	 * count.  the b_resid field gets used to hold the command mneumonic.
	 * These 2 fields are "known" to be "safe" to use for this purpose.
	 * (Most other drivers also use these fields in this way.)
	 */
	bp->b_dev = dev;
	bp->b_bcount = count;
	bp->b_resid = cmd;
	bp->b_blkno = 0;
	bp->b_oflags = 0;
	bp->b_objlock = &buffer_lock;
	tsstrategy(bp);
	/*
	 * In case of rewind from close, don't wait.
	 * This is the only case where count can be 0.
	 */
	if (count == 0)
		return;
	biowait(bp);
	mutex_enter(&bufcache_lock);
	cv_broadcast(&bp->b_busy);
	bp->b_cflags = 0;
	mutex_exit(&bufcache_lock);
}

/*
 * Start an I/O operation on TS05 controller
 */
int
tsstart(struct ts_softc *sc, int isloaded)
{
	struct buf *bp;
	int cmd;

	bp = bufq_peek(sc->sc_bufq);
	if (bp == NULL) {
		return 0;
	}
#ifdef TSDEBUG
	printf("buf: %p bcount %ld blkno %d\n", bp, bp->b_bcount, bp->b_blkno);
#endif
	/*
	 * Check if command is an ioctl or not (ie. read or write).
	 * If it's an ioctl then just set the flags for later use;
	 * For other commands attempt to setup a buffer pointer.
	 */
	if (bp == &sc->ts_cbuf) {
		switch ((int)bp->b_resid) {
		case MTWEOF:
			cmd = TS_CMD_WTM;
			break;
		case MTFSF:
			cmd = TS_CMD_STMF;
			sc->sc_vts->cmd.cw1 = bp->b_bcount;
			break;
		case MTBSF:
			cmd = TS_CMD_STMR;
			sc->sc_vts->cmd.cw1 = bp->b_bcount;
			break;
		case MTFSR:
			cmd = TS_CMD_SRF;
			sc->sc_vts->cmd.cw1 = bp->b_bcount;
			break;
		case MTBSR:
			cmd = TS_CMD_SRR;
			sc->sc_vts->cmd.cw1 = bp->b_bcount;
			break;
		case MTREW:
			cmd = TS_CMD_RWND;
			break;
		case MTOFFL:
			cmd = TS_CMD_RWUL;
			break;
		case MTNOP:
			cmd = TS_CMD_STAT;
			break;
		default:
			aprint_error_dev(sc->sc_dev, "bad ioctl %d\n",
				(int)bp->b_resid);
			/* Need a no-op. get status */
			cmd = TS_CMD_STAT;
		} /* end switch (bp->b_resid) */
	} else {
		if (isloaded == 0) {
			/*
			 * now we try to map the buffer into uba map space (???)
			 */
			if (bus_dmamap_load(sc->sc_dmat, sc->sc_dmam,
			    bp->b_data,
			    bp->b_bcount, bp->b_proc, BUS_DMA_NOWAIT)) {
				uba_enqueue(&sc->sc_uu);
				return 0;
			}
			sc->sc_rtc = 0;
		}
		sc->sc_vts->cmd.cw1 = LOWORD(sc->sc_dmam->dm_segs[0].ds_addr);
		sc->sc_vts->cmd.cw2 = HIWORD(sc->sc_dmam->dm_segs[0].ds_addr);
		sc->sc_vts->cmd.cw3 = bp->b_bcount;
		bp->b_error = 0; /* Used for error count */
#ifdef TSDEBUG
		printf("tsstart-1: err %d\n", bp->b_error);
#endif
		if (bp->b_flags & B_READ)
			cmd = TS_CMD_RNF;
		else
			cmd = TS_CMD_WD;
	}

	/*
	 * Now that the command-buffer is setup, give it to the controller
	 */
	sc->sc_vts->cmd.cmdr = TS_CF_ACK | TS_CF_IE | cmd;
#ifdef TSDEBUG
	printf("tsstart: sending cmdr %x\n", sc->sc_vts->cmd.cmdr);
#endif
	TS_WCSR(TSDB, sc->sc_waddr);
	return 1;
}

/*
 * Called when there are free uba resources.
 */
int
tsready(struct uba_unit *uu)
{
	struct ts_softc *sc = device_private(uu->uu_dev);
	struct buf *bp = bufq_peek(sc->sc_bufq);

	if (bus_dmamap_load(sc->sc_dmat, sc->sc_dmam, bp->b_data,
	    bp->b_bcount, bp->b_proc, BUS_DMA_NOWAIT))
		return 0;

	tsstart(sc, 1);
	return 1;
}

/*
 * initialize the controller by sending WRITE CHARACTERISTICS command.
 * contents of command- and message-buffer are assembled during this
 * function.
 */
void
tswchar(struct ts_softc *sc)
{
	/*
	 * assemble and send "WRITE CHARACTERISTICS" command
	 */

	sc->sc_vts->cmd.cmdr = TS_CF_ACK | TS_CF_IE | TS_CMD_WCHAR;
	sc->sc_vts->cmd.cw1  = LOWORD(&sc->sc_bts->chr);
	sc->sc_vts->cmd.cw2  = HIWORD(&sc->sc_bts->chr);
	sc->sc_vts->cmd.cw3  = 010;		   /* size of charact.-data */

	sc->sc_vts->chr.sadrl = LOWORD(&sc->sc_bts->status);
	sc->sc_vts->chr.sadrh = HIWORD(&sc->sc_bts->status);
	sc->sc_vts->chr.onesix = (sc->sc_type == TYPE_TS05 ? 020 : 016);
	sc->sc_vts->chr.chrw = TS_WC_ESS;
	sc->sc_vts->chr.xchrw = TS_WCX_HSP|TS_WCX_RBUF|TS_WCX_WBUF;

	TS_WCSR(TSDB, sc->sc_waddr);
}

/*
 * Reset the TS11. Return 1 if failed, 0 if succeeded.
 */
bool
tsreset(struct ts_softc *sc)
{
	int timeout;

	/*
	 * reset ctlr by writing into TSSR, then write characteristics
	 */
	timeout = 0;		/* timeout in 10 seconds */
	TS_WCSR(TSSR, 0);	/* start initialization */
	while ((TS_RCSR(TSSR) & TS_SSR) == 0) {
		DELAY(10000);
		if (timeout++ > 1000)
			return false;
	}
	return true;
}

static void
prtstat(struct ts_softc *sc, int sr)
{
	char buf[100];

	snprintb(buf, sizeof(buf), TS_TSSR_BITS, sr);
	aprint_normal_dev(sc->sc_dev, "TSSR=%s\n", buf);
	snprintb(buf, sizeof(buf), TS_XST0_BITS, sc->sc_vts->status.xst0);
	aprint_normal_dev(sc->sc_dev, "XST0=%s\n", buf);
}

/*
 * TSV05/TS05 interrupt routine
 */
static void
tsintr(void *arg)
{
	struct ts_softc *sc = arg;
	struct buf *bp;

	unsigned short sr = TS_RCSR(TSSR);	/* save TSSR */
	unsigned short mh = sc->sc_vts->status.hdr;	/* and msg-header */
		/* clear the message header ??? */

	short ccode = sc->sc_vts->cmd.cmdr & TS_CF_CCODE;

	bp = bufq_peek(sc->sc_bufq);
#ifdef TSDEBUG
	{
		char buf[100];
		snprintb(buf, sizeof(buf), TS_TSSR_BITS, sr);
		printf("tsintr: sr %x mh %x\n", sr, mh);
		printf("srbits: %s\n", buf);
	}
#endif
	/*
	 * There are two different things which can (and should) be checked:
	 * the actual (internal) state and the device's result (tssr/msg.hdr)
	 *
	 * For each state there's only one "normal" interrupt. Anything else
	 * has to be checked more intensively. Thus in a first run according
	 * to the internal state the expected interrupt is checked/handled.
	 *
	 * In a second run the remaining (not yet handled) interrupts are
	 * checked according to the drive's result.
	 */
	switch (sc->sc_state) {

	case TS_INVALID:
		/*
		 * Ignore unsolicited interrupts.
		 */
		log(LOG_WARNING, "%s: stray intr [%x,%x]\n",
			device_xname(sc->sc_dev), sr, mh);
		return;

	case TS_INIT:
		/*
		 * Init phase ready.
		 */
		wakeup(sc);
		return;

	case TS_RUNNING:
	case TS_FASTREPOS:
		/*
		 * Here we expect interrupts indicating the end of
		 * commands or indicating problems.
		 */
		/*
		 * Anything else is handled outside this switch ...
		 */
		break;

	default:
		aprint_error_dev(sc->sc_dev,
		    "unexpected interrupt during state %d [%x,%x]\n",
		    sc->sc_state, sr, mh);
		return;
	}

	/*
	 * now we check the termination class.
	 */
	switch (sr & TS_TC) {

	case TS_TC_NORM:
		/*
		 * Normal termination -- The operation is completed
		 * witout incident.
		 */
		if (sc->sc_state == TS_FASTREPOS) {
#ifdef TSDEBUG
			printf("Fast repos interrupt\n");
#endif
			/* Fast repos succeeded, start normal data xfer */
			sc->sc_state = TS_RUNNING;
			tsstart(sc, 1);
			return;
		}
		sc->sc_liowf = (ccode == TS_CC_WRITE);
		break;

	case TS_TC_ATTN:
		/*
		 * Attention condition -- this code indicates that the
		 * drive has undergone a status change, such as going
		 * off-line or coming on-line.
		 * (Without EAI enabled, no Attention interrupts occur.
		 * drive status changes are signaled by the VCK flag.)
		 */
		return;

	case TS_TC_TSA:
		/*
		 * Tape Status Alert -- A status condition is encountered
		 * that may have significance to the program. Bits of
		 * interest in the extended status registers include
		 * TMK, EOT and RLL.
		 */
#ifdef TSDEBUG
		{
			char buf[100];
			snprintb(buf, sizeof(buf),
			    TS_XST0_BITS, sc->sc_vts->status.xst0);
			printf("TSA: sr %x bits %s\n",
			    sc->sc_vts->status.xst0, buf);
		}
#endif
		if (sc->sc_vts->status.xst0 & TS_SF_TMK) {
#ifdef TSDEBUG
			printf(("Tape Mark detected"));
#endif
			/* Read to end-of-file. No error. */
			break;
		}
		if (sc->sc_vts->status.xst0 & TS_SF_EOT) {
			/* End of tape. Bad. */
#ifdef TSDEBUG
			printf("TS_TC_TSA: EOT\n");
#endif
			if (bp != NULL)
				bp->b_error = EIO;
			break;
		}
		if (sc->sc_vts->status.xst0 & TS_SF_RLS)
			break;
#ifndef TSDEBUG
		{
			char buf[100];
			snprintb(buf, sizeof(buf),
			    TS_XST0_BITS, sc->sc_vts->status.xst0);
			printf("TSA: sr %x bits %s\n",
			    sc->sc_vts->status.xst0, buf);
		}
#endif
		break;

	case TS_TC_FR:
		/*
		 * Function Reject -- The specified function was not
		 * initiated. Bits of interest include OFL, VCK, BOT,
		 * WLE, ILC and ILA.
		 */
		if (sr & TS_OFL)
			printf("tape is off-line.\n");
#ifdef TSDEBUG
		{
			char buf[100];
			snprintb(buf, sizeof(buf),
			    TS_XST0_BITS, sc->sc_vts->status.xst0);
			printf("FR: sr %x bits %s\n",
			    sc->sc_vts->status.xst0, buf);
		}
#endif
		if (sc->sc_vts->status.xst0 & TS_SF_VCK)
			printf("Volume check\n");
		if (sc->sc_vts->status.xst0 & TS_SF_BOT)
			printf("Start of tape.\n");
		if (sc->sc_vts->status.xst0 & TS_SF_WLE)
			printf("Write Lock Error\n");
		if (sc->sc_vts->status.xst0 & TS_SF_EOT)
			printf("End of tape.\n");
		break;

	case TS_TC_TPD:
		/*
		 * Recoverable Error -- Tape position is a record beyond
		 * what its position was when the function was initiated.
		 * Suggested recovery procedure is to log the error and
		 * issue the appropriate retry command.
		 *
		 * Do a fast repositioning one record back.
		 */
		sc->sc_state = TS_FASTREPOS;
#ifdef TSDEBUG
		printf("TS_TC_TPD: errcnt %d\n", sc->sc_rtc);
#endif
		if (sc->sc_rtc++ == 8) {
			aprint_error_dev(sc->sc_dev, "failed 8 retries\n");
			prtstat(sc, sr);
			if (bp != NULL)
				bp->b_error = EIO;
			break;
		}
		sc->sc_vts->cmd.cmdr = TS_CF_ACK | TS_CF_IE | TS_CMD_SRR;
		sc->sc_vts->cmd.cw1 = 1;
		TS_WCSR(TSDB, sc->sc_waddr);
		return;

	case TS_TC_TNM:
		/*
		 * Recoverable Error -- Tape position has not changed.
		 * Suggested recovery procedure is to log the error and
		 * reissue the original command.
		 */
		if (sc->sc_rtc++ == 8) {
			aprint_error_dev(sc->sc_dev, "failed 8 retries\n");
			prtstat(sc, sr);
			if (bp != NULL)
				bp->b_error = EIO;
			break;
		}
		tsstart(sc, 1);
		return;

	case TS_TC_TPL:
		/*
		 * Unrecoverable Error -- Tape position has been lost.
		 * No valid recovery procedures exist unless the tape
		 * has labels or sequence numbers.
		 */
		aprint_error_dev(sc->sc_dev, "tape position lost\n");
		if (bp != NULL)
			bp->b_error = EIO;
		break;

	case TS_TC_FCE:
		/*
		 * Fatal subsytem Error -- The subsytem is incapable
		 * of properly performing commands, or at least its
		 * integrity is seriously questionable. Refer to the
		 * fatal class code field in the TSSR register for
		 * additional information on the type of fatal error.
		 */
		aprint_error_dev(sc->sc_dev, "fatal controller error\n");
		prtstat(sc, sr);
		break;

	default:
		aprint_error_dev(sc->sc_dev,
		    "error 0x%x, resetting controller\n", sr & TS_TC);
		tsreset(sc);
	}
	if ((bp = bufq_get(sc->sc_bufq)) != NULL) {
#ifdef TSDEBUG
		printf("tsintr2: que %p\n", bufq_peek(sc->sc_bufq));
#endif
		if (bp != &sc->ts_cbuf) {	/* no ioctl */
			bus_dmamap_unload(sc->sc_dmat, sc->sc_dmam);
			uba_done(sc->sc_uh);
		}
		bp->b_resid = sc->sc_vts->status.rbpcr;
		biodone (bp);
	}
	tsstart(sc, 0);
}


/*
 * Open a ts device and set the unit online.  If the controller is not
 * in the run state, call init to initialize the ts controller first.
 */
int
tsopen(dev_t dev, int flag, int type, struct lwp *l)
{
	struct ts_softc *sc = device_lookup_private(&ts_cd, TS_UNIT(dev));

	if (sc == NULL)
		return ENXIO;

	if (sc->sc_state < TS_RUNNING)
		return ENXIO;

	if (sc->sc_openf)
		return EBUSY;
	sc->sc_openf = 1;

	/*
	 * check if transport is really online.
	 * (without attention-interrupts enabled, we really don't know
	 * the actual state of the transport. Thus we call get-status
	 * (ie. MTNOP) once and check the actual status.)
	 */
	if (TS_RCSR(TSSR) & TS_OFL) {
		uprintf("%s: transport is offline.\n", device_xname(sc->sc_dev));
		sc->sc_openf = 0;
		return EIO;		/* transport is offline */
	}
	tscommand(sc, dev, MTNOP, 1);
	if ((flag & FWRITE) && (sc->sc_vts->status.xst0 & TS_SF_WLK)) {
		uprintf("%s: no write ring.\n", device_xname(sc->sc_dev));
		sc->sc_openf = 0;
		return EROFS;
	}
	if (sc->sc_vts->status.xst0 & TS_SF_VCK) {
		sc->sc_vts->cmd.cmdr = TS_CF_CVC|TS_CF_ACK;
		TS_WCSR(TSDB, sc->sc_waddr);
	}
	tscommand(sc, dev, MTNOP, 1);
#ifdef TSDEBUG
	{
		char buf[100];
		snprintb(buf, sizeof(buf),
		    TS_XST0_BITS, sc->sc_vts->status.xst0);
		printf("tsopen: xst0 %s\n", buf);
	}
#endif
	sc->sc_liowf = 0;
	return 0;
}


/*
 * Close tape device.
 *
 * If tape was open for writing or last operation was
 * a write, then write two EOF's and backspace over the last one.
 * Unless this is a non-rewinding special file, rewind the tape.
 *
 * Make the tape available to others, by clearing openf flag.
 */
int
tsclose(dev_t dev, int flag, int type, struct lwp *l)
{
	struct ts_softc *sc = device_lookup_private(&ts_cd, TS_UNIT(dev));

	if (flag == FWRITE || ((flag & FWRITE) && sc->sc_liowf)) {
		/*
		 * We are writing two tape marks (EOT), but place the tape
		 * before the second one, so that another write operation
		 * will overwrite the second one and leave and EOF-mark.
		 */
		tscommand(sc, dev, MTWEOF, 1);	/* Write Tape Mark */
		tscommand(sc, dev, MTWEOF, 1);	/* Write Tape Mark */
		tscommand(sc, dev, MTBSF, 1);	/* Skip Tape Marks Reverse */
	}

	if ((dev & T_NOREWIND) == 0)
		tscommand(sc, dev, MTREW, 0);

	sc->sc_openf = 0;
	sc->sc_liowf = 0;
	return 0;
}


/*
 * Manage buffers and perform block mode read and write operations.
 */
void
tsstrategy(struct buf *bp)
{
	struct ts_softc *sc = device_lookup_private(&ts_cd, TS_UNIT(bp->b_dev));
	bool empty;
	int s;

#ifdef TSDEBUG
	printf("buf: %p bcount %ld blkno %d\n", bp, bp->b_bcount, bp->b_blkno);
#endif
	s = splbio ();
	empty = (bufq_peek(sc->sc_bufq) == NULL);
	bufq_put(sc->sc_bufq, bp);
	if (empty)
		tsstart(sc, 0);
	splx(s);
}


/*
 * Catch ioctl commands, and call the "command" routine to do them.
 */
int
tsioctl(dev_t dev,
	u_long cmd,
	void *data,
	int flag,
	struct lwp *l)
{
	struct buf *bp;
	struct ts_softc * const sc = device_lookup_private(&ts_cd, TS_UNIT(dev));
	struct mtop *mtop;	/* mag tape cmd op to perform */
	struct mtget *mtget;	/* mag tape struct to get info in */
	int callcount;		/* number of times to call routine */
	int scount;			/* number of files/records to space */
	int spaceop = 0;		/* flag for skip/space operation */
	int error = 0;

#ifdef TSDEBUG
	printf("tsioctl (%x, %lx, %p, %d)\n", dev, cmd, data, flag);
#endif

	bp = &sc->ts_cbuf;

	switch (cmd) {
	case MTIOCTOP:			/* do a mag tape op */
		mtop = (struct mtop *)data;
		switch (mtop->mt_op) {
		case MTWEOF:		/* write an end-of-file record */
			callcount = mtop->mt_count;
			scount = 1;
			break;
		case MTFSR:		/* forward space record */
		case MTBSR:		/* backward space record */
			spaceop = 1;
		case MTFSF:		/* forward space file */
		case MTBSF:		/* backward space file */
			callcount = 1;
			scount = mtop->mt_count;
			break;
		case MTREW:		/* rewind */
		case MTOFFL:		/* rewind and put the drive offline */
		case MTNOP:		/* no operation, sets status only */
			callcount = 1;
			scount = 1;		/* wait for this rewind */
			break;
		case MTRETEN:		/* retension */
		case MTERASE:		/* erase entire tape */
		case MTEOM:		/* forward to end of media */
		case MTNBSF:		/* backward space to begin of file */
		case MTCACHE:		/* enable controller cache */
		case MTNOCACHE:		/* disable controller cache */
		case MTSETBSIZ:		/* set block size; 0 for variable */
		case MTSETDNSTY:	/* set density code for current mode */
			printf("ioctl %d not implemented.\n", mtop->mt_op);
			return (ENXIO);
		default:
#ifdef TSDEBUG
			printf("invalid ioctl %d\n", mtop->mt_op);
#endif
			return (ENXIO);
		}	/* switch (mtop->mt_op) */

		if (callcount <= 0 || scount <= 0) {
#ifdef TSDEBUG
			printf("invalid values %d/%d\n", callcount, scount);
#endif
			return (EINVAL);
		}
		do {
			tscommand(sc, dev, mtop->mt_op, scount);
			if (spaceop && bp->b_resid) {
#ifdef TSDEBUG
				printf(("spaceop didn't complete\n"));
#endif
				return (EIO);
			}
			if (bp->b_error != 0) {
#ifdef TSDEBUG
				printf("error in ioctl %d\n", mtop->mt_op);
#endif
				break;
			}
		} while (--callcount > 0);
		if (bp->b_error != 0)
			error = bp->b_error;
		return (error);

	case MTIOCGET:			/* get tape status */
		mtget = (struct mtget *)data;
		mtget->mt_type = MT_ISTS;
		mtget->mt_dsreg = TS_RCSR(TSSR);
		mtget->mt_erreg = sc->sc_vts->status.xst0;
		mtget->mt_resid = 0;		/* ??? */
		mtget->mt_density = 0;		/* ??? */
		break;

	case MTIOCIEOT:			/* ignore EOT error */
#ifdef TSDEBUG
		printf(("MTIOCIEOT not implemented.\n"));
#endif
		return (ENXIO);

	case MTIOCEEOT:			/* enable EOT error */
#ifdef TSDEBUG
		printf(("MTIOCEEOT not implemented.\n"));
#endif
		return (ENXIO);

	default:
#ifdef TSDEBUG
		printf("invalid ioctl cmd 0x%lx\n", cmd);
#endif
		return (ENXIO);
	}

	return (0);
}


/*
 *
 */
int
tsread(dev_t dev, struct uio *uio, int flag)
{
	return (physio (tsstrategy, NULL, dev, B_READ, minphys, uio));
}

/*
 *
 */
int
tswrite(dev_t dev, struct uio *uio, int flag)
{
	return (physio (tsstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 *
 */
int
tsdump(dev_t dev, daddr_t blkno, void *va, size_t size)
{
	return EIO;
}
