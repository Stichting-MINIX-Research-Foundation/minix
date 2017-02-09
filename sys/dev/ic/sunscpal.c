/*	$NetBSD: sunscpal.c,v 1.26 2014/03/25 16:19:13 christos Exp $	*/

/*
 * Copyright (c) 2001 Matthew Fredette
 * Copyright (c) 1995 David Jones, Gordon W. Ross
 * Copyright (c) 1994 Jarle Greipsland
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
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by
 *      David Jones and Gordon Ross
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This is a machine-independent driver for the Sun "sc"
 * SCSI Bus Controller (SBC).
 *
 * This code should work with any memory-mapped card,
 * and can be shared by multiple adapters that address
 * the card with different register offset spacings.
 * (This can happen on the atari, for example.)
 *
 * Credits, history:
 *
 * Matthew Fredette completely copied revision 1.38 of
 * ncr5380sbc.c, and then heavily modified it to match
 * the Sun sc PAL.  The remaining credits are for
 * ncr5380sbc.c:
 *
 * David Jones is the author of most of the code that now
 * appears in this file, and was the architect of the
 * current overall structure (MI/MD code separation, etc.)
 *
 * Gordon Ross integrated the message phase code, added lots of
 * comments about what happens when and why (re. SCSI spec.),
 * debugged some reentrance problems, and added several new
 * "hooks" needed for the Sun3 "si" adapters.
 *
 * The message in/out code was taken nearly verbatim from
 * the aic6360 driver by Jarle Greipsland.
 *
 * Several other NCR5380 drivers were used for reference
 * while developing this driver, including work by:
 *   The Alice Group (mac68k port) namely:
 *       Allen K. Briggs, Chris P. Caputo, Michael L. Finch,
 *       Bradley A. Grantham, and Lawrence A. Kesteloot
 *   Michael L. Hitch (amiga drivers: sci.c)
 *   Leo Weppelman (atari driver: ncr5380.c)
 * There are others too.  Thanks, everyone.
 *
 * Transliteration to bus_space() performed 9/17/98 by
 * John Ruschmeyer (jruschme@exit109.com) for i386 'nca' driver.
 * Thank you all.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sunscpal.c,v 1.26 2014/03/25 16:19:13 christos Exp $");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsipi_debug.h>
#include <dev/scsipi/scsi_message.h>
#include <dev/scsipi/scsiconf.h>

#ifdef DDB
#include <ddb/db_output.h>
#endif

#include <dev/ic/sunscpalreg.h>
#include <dev/ic/sunscpalvar.h>

static void	sunscpal_reset_scsibus(struct sunscpal_softc *);
static void	sunscpal_sched(struct sunscpal_softc *);
static void	sunscpal_done(struct sunscpal_softc *);

static int	sunscpal_select(struct sunscpal_softc *, struct sunscpal_req *);
static void	sunscpal_reselect(struct sunscpal_softc *);

static int	sunscpal_msg_in(struct sunscpal_softc *);
static int	sunscpal_msg_out(struct sunscpal_softc *);
static int	sunscpal_data_xfer(struct sunscpal_softc *, int);
static int	sunscpal_command(struct sunscpal_softc *);
static int	sunscpal_status(struct sunscpal_softc *);
static void	sunscpal_machine(struct sunscpal_softc *);

void	sunscpal_abort(struct sunscpal_softc *);
void	sunscpal_cmd_timeout(void *);
/*
 * Action flags returned by the info_transfer functions:
 * (These determine what happens next.)
 */
#define ACT_CONTINUE	0x00	/* No flags: expect another phase */
#define ACT_DISCONNECT	0x01	/* Target is disconnecting */
#define ACT_CMD_DONE	0x02	/* Need to call scsipi_done() */
#define ACT_RESET_BUS	0x04	/* Need bus reset (cmd timeout) */
#define ACT_WAIT_DMA	0x10	/* Wait for DMA to complete */

/*****************************************************************
 * Debugging stuff
 *****************************************************************/

#ifndef DDB
/* This is used only in recoverable places. */
#ifndef Debugger
#define Debugger() printf("Debug: sunscpal.c:%d\n", __LINE__)
#endif
#endif

#ifdef	SUNSCPAL_DEBUG

#define	SUNSCPAL_DBG_BREAK	1
#define	SUNSCPAL_DBG_CMDS	2
#define	SUNSCPAL_DBG_DMA	4
int sunscpal_debug = 0;
#define	SUNSCPAL_BREAK() \
	do { if (sunscpal_debug & SUNSCPAL_DBG_BREAK) Debugger(); } while (0)
static void sunscpal_show_scsi_cmd(struct scsipi_xfer *);
#ifdef DDB
void	sunscpal_clear_trace(void);
void	sunscpal_show_trace(void);
void	sunscpal_show_req(struct sunscpal_req *);
void	sunscpal_show_state(void);
#endif	/* DDB */
#else	/* SUNSCPAL_DEBUG */

#define	SUNSCPAL_BREAK() 		/* nada */
#define sunscpal_show_scsi_cmd(xs) /* nada */

#endif	/* SUNSCPAL_DEBUG */

static const char *
phase_names[8] = {
	"DATA_OUT",
	"DATA_IN",
	"COMMAND",
	"STATUS",
	"UNSPEC1",
	"UNSPEC2",
	"MSG_OUT",
	"MSG_IN",
};

#ifdef SUNSCPAL_USE_BUS_DMA
static void sunscpal_dma_alloc(struct sunscpal_softc *);
static void sunscpal_dma_free(struct sunscpal_softc *);
static void sunscpal_dma_setup(struct sunscpal_softc *);
#else
#define sunscpal_dma_alloc(sc) (*sc->sc_dma_alloc)(sc)
#define sunscpal_dma_free(sc) (*sc->sc_dma_free)(sc)
#define sunscpal_dma_setup(sc) (*sc->sc_dma_setup)(sc)
#endif
static void sunscpal_minphys(struct buf *);

/*****************************************************************
 * Actual chip control
 *****************************************************************/

/*
 * XXX: These timeouts might need to be tuned...
 */

/* This one is used when waiting for a phase change. (X100uS.) */
int sunscpal_wait_phase_timo = 1000 * 10 * 300;	/* 5 min. */

/* These are used in the following inline functions. */
int sunscpal_wait_req_timo = 1000 * 50;	/* X2 = 100 mS. */
int sunscpal_wait_nrq_timo = 1000 * 25;	/* X2 =  50 mS. */

static inline int sunscpal_wait_req(struct sunscpal_softc *);
static inline int sunscpal_wait_not_req(struct sunscpal_softc *);
static inline void sunscpal_sched_msgout(struct sunscpal_softc *, int);

/* Return zero on success. */
static inline int sunscpal_wait_req(struct sunscpal_softc *sc)
{
	int timo = sunscpal_wait_req_timo;

	for (;;) {
		if (SUNSCPAL_READ_2(sc, sunscpal_icr) & SUNSCPAL_ICR_REQUEST) {
			timo = 0;	/* return 0 */
			break;
		}
		if (--timo < 0)
			break;	/* return -1 */
		delay(2);
	}
	return timo;
}

/* Return zero on success. */
static inline int sunscpal_wait_not_req(struct sunscpal_softc *sc)
{
	int timo = sunscpal_wait_nrq_timo;

	for (;;) {
		if ((SUNSCPAL_READ_2(sc, sunscpal_icr) &
		    SUNSCPAL_ICR_REQUEST) == 0) {
			timo = 0;	/* return 0 */
			break;
		}
		if (--timo < 0)
			break;	/* return -1 */
		delay(2);
	}
	return timo;
}

/*
 * These functions control DMA functions in the chipset independent of
 * the host DMA implementation.
 */
static void sunscpal_dma_start(struct sunscpal_softc *);
static void sunscpal_dma_poll(struct sunscpal_softc *);
static void sunscpal_dma_stop(struct sunscpal_softc *);

static void
sunscpal_dma_start(struct sunscpal_softc *sc)
{
	struct sunscpal_req *sr = sc->sc_current;
	int xlen;
	uint16_t icr;

	xlen = sc->sc_reqlen;

	/* Let'er rip! */
	icr = SUNSCPAL_READ_2(sc, sunscpal_icr);
	icr |= SUNSCPAL_ICR_DMA_ENABLE |
	    ((xlen & 1) ? 0 : SUNSCPAL_ICR_WORD_MODE) |
	    ((sr->sr_flags & SR_IMMED) ? 0 : SUNSCPAL_ICR_INTERRUPT_ENABLE);
	SUNSCPAL_WRITE_2(sc, sunscpal_icr, icr);

	sc->sc_state |= SUNSCPAL_DOINGDMA;

#ifdef	SUNSCPAL_DEBUG
	if (sunscpal_debug & SUNSCPAL_DBG_DMA) {
		printf("%s: started, flags=0x%x\n",
		    __func__, sc->sc_state);
	}
#endif
}

#define	ICR_MASK (SUNSCPAL_ICR_PARITY_ERROR | SUNSCPAL_ICR_BUS_ERROR | SUNSCPAL_ICR_INTERRUPT_REQUEST)
#define	POLL_TIMO	50000	/* X100 = 5 sec. */

/*
 * Poll (spin-wait) for DMA completion.
 * Called right after xx_dma_start(), and
 * xx_dma_stop() will be called next.
 */
static void
sunscpal_dma_poll(struct sunscpal_softc *sc)
{
	struct sunscpal_req *sr = sc->sc_current;
	int tmo;

	/* Make sure DMA started successfully. */
	if (sc->sc_state & SUNSCPAL_ABORTING)
		return;

	/* Wait for any "DMA complete" or error bits. */
	tmo = POLL_TIMO;
	for (;;) {
		if (SUNSCPAL_READ_2(sc, sunscpal_icr) & ICR_MASK)
			break;
		if (--tmo <= 0) {
			printf("sc: DMA timeout (while polling)\n");
			/* Indicate timeout as MI code would. */
			sr->sr_flags |= SR_OVERDUE;
			break;
		}
		delay(100);
	}
	SUNSCPAL_TRACE("sunscpal_dma_poll: waited %d\n", POLL_TIMO - tmo);

#ifdef	SUNSCPAL_DEBUG
	if (sunscpal_debug & SUNSCPAL_DBG_DMA) {
		char buffer[64];
		snprintb(buffer, sizeof(buffer),
		    SUNSCPAL_READ_2(sc, sunscpal_icr), SUNSCPAL_ICR_BITS);
		printf("%s: done, icr=%s\n", __func__, buffer);
	}
#endif
}

static void
sunscpal_dma_stop(struct sunscpal_softc *sc)
{
	struct sunscpal_req *sr = sc->sc_current;
	struct scsipi_xfer *xs = sr->sr_xs;
	int resid, ntrans;
	uint16_t icr;

	if ((sc->sc_state & SUNSCPAL_DOINGDMA) == 0) {
#ifdef	DEBUG
		printf("%s: DMA not running\n", __func__);
#endif
		return;
	}
	sc->sc_state &= ~SUNSCPAL_DOINGDMA;

	/* First, halt the DMA engine. */
	icr = SUNSCPAL_READ_2(sc, sunscpal_icr);
	icr &= ~(SUNSCPAL_ICR_DMA_ENABLE | SUNSCPAL_ICR_WORD_MODE |
	    SUNSCPAL_ICR_INTERRUPT_ENABLE);
	SUNSCPAL_WRITE_2(sc, sunscpal_icr, icr);

#ifdef	SUNSCPAL_USE_BUS_DMA
	/*
	 * XXX - this function is supposed to be independent of
	 * the host's DMA implementation.
	 */
 {
	 sunscpal_dma_handle_t dh = sr->sr_dma_hand;

	 /* sync the DMA map: */
	 bus_dmamap_sync(sc->sunscpal_dmat, dh->dh_dmamap, 0, dh->dh_maplen,
	     ((xs->xs_control & XS_CTL_DATA_OUT) == 0 ?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE));
 }
#endif /* SUNSCPAL_USE_BUS_DMA */


	if (icr & (SUNSCPAL_ICR_BUS_ERROR)) {
		char buffer[64];
		snprintb(buffer, sizeof(buffer), SUNSCPAL_ICR_BITS, icr);
		printf("sc: DMA error, icr=%s, reset\n", buffer);
		sr->sr_xs->error = XS_DRIVER_STUFFUP;
		sc->sc_state |= SUNSCPAL_ABORTING;
		goto out;
	}

	/* Note that timeout may have set the error flag. */
	if (sc->sc_state & SUNSCPAL_ABORTING)
		goto out;

	/* XXX: Wait for DMA to actually finish? */

	/*
	 * Now try to figure out how much actually transferred
	 */

	resid = SUNSCPAL_DMA_COUNT_FLIP(SUNSCPAL_READ_2(sc,
	    sunscpal_dma_count));
	ntrans = sc->sc_reqlen - resid;

#ifdef	SUNSCPAL_DEBUG
	if (sunscpal_debug & SUNSCPAL_DBG_DMA) {
		printf("%s: resid=0x%x ntrans=0x%x\n",
		    __func__, resid, ntrans);
	}
#endif

	if (ntrans < sc->sc_min_dma_len) {
		printf("sc: DMA count: 0x%x\n", resid);
		sc->sc_state |= SUNSCPAL_ABORTING;
		goto out;
	}
	if (ntrans > sc->sc_datalen)
		panic("%s: excess transfer", __func__);

	/* Adjust data pointer */
	sc->sc_dataptr += ntrans;
	sc->sc_datalen -= ntrans;

	/*
	 * After a read, we may need to clean-up
	 * "Left-over bytes" (yuck!)
	 */
	if (((xs->xs_control & XS_CTL_DATA_OUT) == 0) &&
	    ((icr & SUNSCPAL_ICR_ODD_LENGTH) != 0)) {
#ifdef DEBUG
		printf("sc: Got Left-over bytes!\n");
#endif
		*(sc->sc_dataptr++) = SUNSCPAL_READ_1(sc, sunscpal_data);
		sc->sc_datalen--;
	}

 out:
	SUNSCPAL_WRITE_2(sc, sunscpal_dma_count, SUNSCPAL_DMA_COUNT_FLIP(0));

}

/* Ask the target for a MSG_OUT phase. */
static inline void
sunscpal_sched_msgout(struct sunscpal_softc *sc, int msg_code)
{
	/*
	 * This controller does not allow you to assert ATN, which
	 * will eventually leave us with no option other than to reset
	 * the bus.  We keep this function as a placeholder, though,
	 * and this printf will eventually go away or get #ifdef'ed:
	 */
	printf("%s: trying to schedule 0x%0x\n", __func__, msg_code);
	sc->sc_msgpriq |= msg_code;
}

int
sunscpal_pio_out(struct sunscpal_softc *sc, int phase, int count, uint8_t *data)
{
	int resid;

	resid = count;
	while (resid > 0) {
		if (!SUNSCPAL_BUSY(sc)) {
			SUNSCPAL_TRACE("pio_out: lost BSY, resid=%d\n", resid);
			break;
		}
		if (sunscpal_wait_req(sc)) {
			SUNSCPAL_TRACE("pio_out: no REQ, resid=%d\n", resid);
			break;
		}
		if (SUNSCPAL_BUS_PHASE(SUNSCPAL_READ_2(sc, sunscpal_icr)) !=
		    phase)
			break;

		/* Put the data on the bus. */
		if (data) {
			SUNSCPAL_BYTE_WRITE(sc, phase, *data++);
		} else {
			SUNSCPAL_BYTE_WRITE(sc, phase, 0);
		}

		--resid;
	}

	return count - resid;
}


int
sunscpal_pio_in(struct sunscpal_softc *sc, int phase, int count, uint8_t *data)
{
	int resid;

	resid = count;
	while (resid > 0) {
		if (!SUNSCPAL_BUSY(sc)) {
			SUNSCPAL_TRACE("pio_in: lost BSY, resid=%d\n", resid);
			break;
		}
		if (sunscpal_wait_req(sc)) {
			SUNSCPAL_TRACE("pio_in: no REQ, resid=%d\n", resid);
			break;
		}
		/* A phase change is not valid until AFTER REQ rises! */
		if (SUNSCPAL_BUS_PHASE(SUNSCPAL_READ_2(sc, sunscpal_icr)) !=
		    phase)
			break;

		/* Read the data bus. */
		if (data)
			*data++ = SUNSCPAL_BYTE_READ(sc, phase);
		else
			(void)SUNSCPAL_BYTE_READ(sc, phase);

		--resid;
	}

	return count - resid;
}


void
sunscpal_init(struct sunscpal_softc *sc)
{
	int i, j;

#ifdef	SUNSCPAL_DEBUG
	sunscpal_debug_sc = sc;
#endif

	for (i = 0; i < SUNSCPAL_OPENINGS; i++)
		sc->sc_ring[i].sr_xs = NULL;
	for (i = 0; i < 8; i++)
		for (j = 0; j < 8; j++)
			sc->sc_matrix[i][j] = NULL;

	sc->sc_prevphase = SUNSCPAL_PHASE_INVALID;
	sc->sc_state = SUNSCPAL_IDLE;

	SUNSCPAL_WRITE_2(sc, sunscpal_icr, 0);
	SUNSCPAL_WRITE_2(sc, sunscpal_dma_addr_h, 0);
	SUNSCPAL_WRITE_2(sc, sunscpal_dma_addr_l, 0);
	SUNSCPAL_WRITE_2(sc, sunscpal_dma_count, SUNSCPAL_DMA_COUNT_FLIP(0));

	SUNSCPAL_CLR_INTR(sc);

	/* Another hack (Er.. hook!) for anything that needs it: */
	if (sc->sc_intr_on) {
		SUNSCPAL_TRACE("init: intr ON\n", 0);
		sc->sc_intr_on(sc);
	}
}


static void
sunscpal_reset_scsibus(struct sunscpal_softc *sc)
{

	SUNSCPAL_TRACE("reset_scsibus, cur=0x%x\n", (long)sc->sc_current);

	SUNSCPAL_WRITE_2(sc, sunscpal_icr, SUNSCPAL_ICR_RESET);
	delay(500);
	SUNSCPAL_WRITE_2(sc, sunscpal_icr, 0);

	SUNSCPAL_CLR_INTR(sc);
	/* XXX - Need long delay here! */
	delay(100000);

	/* XXX - Need to cancel disconnected requests. */
}


/*
 * Interrupt handler for the SCSI Bus Controller (SBC)
 * This may also called for a DMA timeout (at splbio).
 */
int
sunscpal_intr(void *arg)
{
	struct sunscpal_softc *sc = arg;
	int claimed = 0;

	/*
	 * Do not touch SBC regs here unless sc_current == NULL
	 * or it will complain about "register conflict" errors.
	 * Instead, just let sunscpal_machine() deal with it.
	 */
	SUNSCPAL_TRACE("intr: top, state=%d\n", sc->sc_state);

	if (sc->sc_state == SUNSCPAL_IDLE) {
		/*
		 * Might be reselect.  sunscpal_reselect() will check,
		 * and set up the connection if so.  This will verify
		 * that sc_current == NULL at the beginning...
		 */

		/* Another hack (Er.. hook!) for anything that needs it: */
		if (sc->sc_intr_off) {
			SUNSCPAL_TRACE("intr: for reselect, intr off\n", 0);
		    sc->sc_intr_off(sc);
		}

		sunscpal_reselect(sc);
	}

	/*
	 * The remaining documented interrupt causes are a DMA complete
	 * condition.
	 *
	 * The procedure is to let sunscpal_machine() figure out what
	 * to do next.
	 */
	if (sc->sc_state & SUNSCPAL_WORKING) {
		SUNSCPAL_TRACE("intr: call machine, cur=0x%x\n",
		    (long)sc->sc_current);
		/* This will usually free-up the nexus. */
		sunscpal_machine(sc);
		SUNSCPAL_TRACE("intr: machine done, cur=0x%x\n",
		    (long)sc->sc_current);
		claimed = 1;
	}

	/* Maybe we can run some commands now... */
	if (sc->sc_state == SUNSCPAL_IDLE) {
		SUNSCPAL_TRACE("intr: call sched, cur=0x%x\n",
		    (long)sc->sc_current);
		sunscpal_sched(sc);
		SUNSCPAL_TRACE("intr: sched done, cur=0x%x\n",
		    (long)sc->sc_current);
	}

	return claimed;
}


/*
 * Abort the current command (i.e. due to timeout)
 */
void
sunscpal_abort(struct sunscpal_softc *sc)
{

	/*
	 * Finish it now.  If DMA is in progress, we
	 * can not call sunscpal_sched_msgout() because
	 * that hits the SBC (avoid DMA conflict).
	 */

	/* Another hack (Er.. hook!) for anything that needs it: */
	if (sc->sc_intr_off) {
		SUNSCPAL_TRACE("abort: intr off\n", 0);
		sc->sc_intr_off(sc);
	}

	sc->sc_state |= SUNSCPAL_ABORTING;
	if ((sc->sc_state & SUNSCPAL_DOINGDMA) == 0) {
		sunscpal_sched_msgout(sc, SEND_ABORT);
	}
	SUNSCPAL_TRACE("abort: call machine, cur=0x%x\n",
	    (long)sc->sc_current);
	sunscpal_machine(sc);
	SUNSCPAL_TRACE("abort: machine done, cur=0x%x\n",
	    (long)sc->sc_current);

	/* Another hack (Er.. hook!) for anything that needs it: */
	if (sc->sc_intr_on) {
		SUNSCPAL_TRACE("abort: intr ON\n", 0);
		sc->sc_intr_on(sc);
	}
}

/*
 * Timeout handler, scheduled for each SCSI command.
 */
void
sunscpal_cmd_timeout(void *arg)
{
	struct sunscpal_req *sr = arg;
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	struct sunscpal_softc *sc;
	int s;

	s = splbio();

	/* Get all our variables... */
	xs = sr->sr_xs;
	if (xs == NULL) {
		printf("%s: no scsipi_xfer\n", __func__);
		goto out;
	}
	periph = xs->xs_periph;
	sc = device_private(periph->periph_channel->chan_adapter->adapt_dev);

	printf("%s: cmd timeout, targ=%d, lun=%d\n",
	    device_xname(sc->sc_dev),
	    sr->sr_target, sr->sr_lun);

	/*
	 * Mark the overdue job as failed, and arrange for
	 * sunscpal_machine to terminate it.  If the victim
	 * is the current job, call sunscpal_machine() now.
	 * Otherwise arrange for sunscpal_sched() to do it.
	 */
	sr->sr_flags |= SR_OVERDUE;
	if (sc->sc_current == sr) {
		SUNSCPAL_TRACE("cmd_tmo: call abort, sr=0x%x\n", (long)sr);
		sunscpal_abort(sc);
	} else {
		/*
		 * The driver may be idle, or busy with another job.
		 * Arrange for sunscpal_sched() to do the deed.
		 */
		SUNSCPAL_TRACE("cmd_tmo: clear matrix, t/l=0x%02x\n",
		    (sr->sr_target << 4) | sr->sr_lun);
		sc->sc_matrix[sr->sr_target][sr->sr_lun] = NULL;
	}

	/*
	 * We may have aborted the current job, or may have
	 * already been idle. In either case, we should now
	 * be idle, so try to start another job.
	 */
	if (sc->sc_state == SUNSCPAL_IDLE) {
		SUNSCPAL_TRACE("cmd_tmo: call sched, cur=0x%x\n",
		    (long)sc->sc_current);
		sunscpal_sched(sc);
		SUNSCPAL_TRACE("cmd_tmo: sched done, cur=0x%x\n",
		    (long)sc->sc_current);
	}

 out:
	splx(s);
}


/*****************************************************************
 * Interface to higher level
 *****************************************************************/


/*
 * Enter a new SCSI command into the "issue" queue, and
 * if there is work to do, start it going.
 *
 * WARNING:  This can be called recursively!
 * (see comment in sunscpal_done)
 */
void
sunscpal_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
    void *arg)
{
	struct scsipi_xfer *xs;
	struct sunscpal_softc *sc;
	struct sunscpal_req *sr;
	int s, i, flags;

	sc = device_private(chan->chan_adapter->adapt_dev);

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;
		flags = xs->xs_control;

		if (flags & XS_CTL_DATA_UIO)
			panic("sunscpal: scsi data uio requested");

		s = splbio();

		if (flags & XS_CTL_POLL) {
			/* Terminate any current command. */
			sr = sc->sc_current;
			if (sr != NULL) {
				printf("%s: polled request aborting %d/%d\n",
				    device_xname(sc->sc_dev), sr->sr_target,
				    sr->sr_lun);
				sunscpal_abort(sc);
			}
			if (sc->sc_state != SUNSCPAL_IDLE) {
				panic("%s: polled request, abort failed",
				    __func__);
			}
		}

		/*
		 * Find lowest empty slot in ring buffer.
		 * XXX: What about "fairness" and cmd order?
		 */
		for (i = 0; i < SUNSCPAL_OPENINGS; i++)
			if (sc->sc_ring[i].sr_xs == NULL)
				goto new;

		xs->error = XS_RESOURCE_SHORTAGE;
		SUNSCPAL_TRACE("scsipi_cmd: no openings, rv=%d\n", rv);
		goto out;

 new:
		/* Create queue entry */
		sr = &sc->sc_ring[i];
		sr->sr_xs = xs;
		sr->sr_target = xs->xs_periph->periph_target;
		sr->sr_lun = xs->xs_periph->periph_lun;
		sr->sr_dma_hand = NULL;
		sr->sr_dataptr = xs->data;
		sr->sr_datalen = xs->datalen;
		sr->sr_flags = (flags & XS_CTL_POLL) ? SR_IMMED : 0;
		sr->sr_status = -1;	/* no value */
		sc->sc_ncmds++;

		SUNSCPAL_TRACE("scsipi_cmd: new sr=0x%x\n", (long)sr);

		if (flags & XS_CTL_POLL) {
			/* Force this new command to be next. */
			sc->sc_rr = i;
		}

		/*
		 * If we were idle, run some commands...
		 */
		if (sc->sc_state == SUNSCPAL_IDLE) {
			SUNSCPAL_TRACE("scsipi_cmd: call sched, cur=0x%x\n",
			    (long)sc->sc_current);
			sunscpal_sched(sc);
			SUNSCPAL_TRACE("scsipi_cmd: sched done, cur=0x%x\n",
			    (long)sc->sc_current);
		}

		if (flags & XS_CTL_POLL) {
			/* Make sure sunscpal_sched() finished it. */
			if ((xs->xs_status & XS_STS_DONE) == 0)
				panic("%s: poll didn't finish", __func__);
		}

 out:
		splx(s);
		return;

	case ADAPTER_REQ_GROW_RESOURCES:
		/* XXX Not supported. */
		return;

	case ADAPTER_REQ_SET_XFER_MODE:
	    {
		/*
		 * We don't support Sync, Wide, or Tagged Queueing.
		 * Just callback now, to report this.
		 */
		struct scsipi_xfer_mode *xm = arg;

		xm->xm_mode = 0;
		xm->xm_period = 0;
		xm->xm_offset = 0;
		scsipi_async_event(chan, ASYNC_EVENT_XFER_MODE, xm);
		return;
	    }
	}
}


/*
 * POST PROCESSING OF SCSI_CMD (usually current)
 * Called by sunscpal_sched(), sunscpal_machine()
 */
static void
sunscpal_done(struct sunscpal_softc *sc)
{
	struct	sunscpal_req *sr;
	struct	scsipi_xfer *xs;

#ifdef	DIAGNOSTIC
	if (sc->sc_state == SUNSCPAL_IDLE)
		panic("%s: state=idle", __func__);
	if (sc->sc_current == NULL)
		panic("%s: current=0", __func__);
#endif

	sr = sc->sc_current;
	xs = sr->sr_xs;

	SUNSCPAL_TRACE("done: top, cur=0x%x\n", (long)sc->sc_current);

	/*
	 * Clean up DMA resources for this command.
	 */
	if (sr->sr_dma_hand) {
		SUNSCPAL_TRACE("done: dma_free, dh=0x%x\n",
		    (long)sr->sr_dma_hand);
		sunscpal_dma_free(sc);
	}
#ifdef	DIAGNOSTIC
	if (sr->sr_dma_hand)
		panic("%s: DMA free did not", __func__);
#endif

	if (sc->sc_state & SUNSCPAL_ABORTING) {
		SUNSCPAL_TRACE("done: aborting, error=%d\n", xs->error);
		if (xs->error == XS_NOERROR)
			xs->error = XS_TIMEOUT;
	}

	SUNSCPAL_TRACE("done: check error=%d\n", (long)xs->error);

	/* If error is already set, ignore sr_status value. */
	if (xs->error != XS_NOERROR)
		goto finish;

	SUNSCPAL_TRACE("done: check status=%d\n", sr->sr_status);

	xs->status = sr->sr_status;
	switch (sr->sr_status) {
	case SCSI_OK:	/* 0 */
		break;

	case SCSI_CHECK:
	case SCSI_BUSY:
		xs->error = XS_BUSY;
		break;

	case -1:
		/* This is our "impossible" initial value. */
		/* fallthrough */
	default:
		printf("%s: target %d, bad status=%d\n",
		    device_xname(sc->sc_dev), sr->sr_target, sr->sr_status);
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}

 finish:

	SUNSCPAL_TRACE("done: finish, error=%d\n", xs->error);

	/*
	 * Dequeue the finished command, but don't clear sc_state until
	 * after the call to scsipi_done(), because that may call back to
	 * sunscpal_scsi_cmd() - unwanted recursion!
	 *
	 * Keeping sc->sc_state != idle terminates the recursion.
	 */
#ifdef	DIAGNOSTIC
	if ((sc->sc_state & SUNSCPAL_WORKING) == 0)
		panic("%s: bad state", __func__);
#endif

	/* Clear our pointers to the request. */
	sc->sc_current = NULL;
	sc->sc_matrix[sr->sr_target][sr->sr_lun] = NULL;
	callout_stop(&sr->sr_xs->xs_callout);

	/* Make the request free. */
	sr->sr_xs = NULL;
	sc->sc_ncmds--;

	/* Tell common SCSI code it is done. */
	scsipi_done(xs);

	sc->sc_state = SUNSCPAL_IDLE;
	/* Now sunscpal_sched() may be called again. */
}


/*
 * Schedule a SCSI operation.  This routine should return
 * only after it achieves one of the following conditions:
 *  	Busy (sc->sc_state != SUNSCPAL_IDLE)
 *  	No more work can be started.
 */
static void
sunscpal_sched(struct sunscpal_softc *sc)
{
	struct sunscpal_req	*sr;
	struct scsipi_xfer *xs;
	int	target = 0, lun = 0;
	int	error, i;

	/* Another hack (Er.. hook!) for anything that needs it: */
	if (sc->sc_intr_off) {
		SUNSCPAL_TRACE("sched: top, intr off\n", 0);
		sc->sc_intr_off(sc);
	}

 next_job:
	/*
	 * Grab the next job from queue.  Must be idle.
	 */
#ifdef	DIAGNOSTIC
	if (sc->sc_state != SUNSCPAL_IDLE)
		panic("%s: not idle", __func__);
	if (sc->sc_current)
		panic("%s: current set", __func__);
#endif

	/*
	 * Always start the search where we last looked.
	 */
	i = sc->sc_rr;
	sr = NULL;
	do {
		if (sc->sc_ring[i].sr_xs) {
			target = sc->sc_ring[i].sr_target;
			lun = sc->sc_ring[i].sr_lun;
			if (sc->sc_matrix[target][lun] == NULL) {
				/*
				 * Do not mark the  target/LUN busy yet,
				 * because reselect may cause some other
				 * job to become the current one, so we
				 * might not actually start this job.
				 * Instead, set sc_matrix later on.
				 */
				sc->sc_rr = i;
				sr = &sc->sc_ring[i];
				break;
			}
		}
		i++;
		if (i == SUNSCPAL_OPENINGS)
			i = 0;
	} while (i != sc->sc_rr);

	if (sr == NULL) {
		SUNSCPAL_TRACE("sched: no work, cur=0x%x\n",
		    (long)sc->sc_current);

		/* Another hack (Er.. hook!) for anything that needs it: */
		if (sc->sc_intr_on) {
			SUNSCPAL_TRACE("sched: ret, intr ON\n", 0);
			sc->sc_intr_on(sc);
		}

		return;		/* No more work to do. */
	}

	SUNSCPAL_TRACE("sched: select for t/l=0x%02x\n",
	    (sr->sr_target << 4) | sr->sr_lun);

	sc->sc_state = SUNSCPAL_WORKING;
	error = sunscpal_select(sc, sr);
	if (sc->sc_current) {
		/* Lost the race!  reselected out from under us! */
		/* Work with the reselected job. */
		if (sr->sr_flags & SR_IMMED) {
			printf("%s: reselected while polling (abort)\n",
			    device_xname(sc->sc_dev));
			/* Abort the reselected job. */
			sc->sc_state |= SUNSCPAL_ABORTING;
			sc->sc_msgpriq |= SEND_ABORT;
		}
		sr = sc->sc_current;
		xs = sr->sr_xs;
		SUNSCPAL_TRACE("sched: reselect, new sr=0x%x\n", (long)sr);
		goto have_nexus;
	}

	/* Normal selection result.  Target/LUN is now busy. */
	sc->sc_matrix[target][lun] = sr;
	sc->sc_current = sr;	/* connected */
	xs = sr->sr_xs;

	/*
	 * Initialize pointers, etc. for this job
	 */
	sc->sc_dataptr  = sr->sr_dataptr;
	sc->sc_datalen  = sr->sr_datalen;
	sc->sc_prevphase = SUNSCPAL_PHASE_INVALID;
	sc->sc_msgpriq = SEND_IDENTIFY;
	sc->sc_msgoutq = 0;
	sc->sc_msgout  = 0;

	SUNSCPAL_TRACE("sched: select rv=%d\n", error);

	switch (error) {
	case XS_NOERROR:
		break;

	case XS_BUSY:
		/* XXX - Reset and try again. */
		printf("%s: select found SCSI bus busy, resetting...\n",
		    device_xname(sc->sc_dev));
		sunscpal_reset_scsibus(sc);
		/* fallthrough */
	case XS_SELTIMEOUT:
	default:
		xs->error = error;	/* from select */
		SUNSCPAL_TRACE("sched: call done, sr=0x%x\n", (long)sr);
		sunscpal_done(sc);

		/* Paranoia: clear everything. */
		sc->sc_dataptr = NULL;
		sc->sc_datalen = 0;
		sc->sc_prevphase = SUNSCPAL_PHASE_INVALID;
		sc->sc_msgpriq = 0;
		sc->sc_msgoutq = 0;
		sc->sc_msgout  = 0;

		goto next_job;
	}

	/*
	 * Selection was successful.  Normally, this means
	 * we are starting a new command.  However, this
	 * might be the termination of an overdue job.
	 */
	if (sr->sr_flags & SR_OVERDUE) {
		SUNSCPAL_TRACE("sched: overdue, sr=0x%x\n", (long)sr);
		sc->sc_state |= SUNSCPAL_ABORTING;
		sc->sc_msgpriq |= SEND_ABORT;
		goto have_nexus;
	}

	/*
	 * OK, we are starting a new command.
	 * Initialize and allocate resources for the new command.
	 * Device reset is special (only uses MSG_OUT phase).
	 * Normal commands start in MSG_OUT phase where we will
	 * send and IDENDIFY message, and then expect CMD phase.
	 */
#ifdef	SUNSCPAL_DEBUG
	if (sunscpal_debug & SUNSCPAL_DBG_CMDS) {
		printf("%s: begin, target=%d, LUN=%d\n", __func__,
		    xs->xs_periph->periph_target, xs->xs_periph->periph_lun);
		sunscpal_show_scsi_cmd(xs);
	}
#endif
	if (xs->xs_control & XS_CTL_RESET) {
		SUNSCPAL_TRACE("sched: cmd=reset, sr=0x%x\n", (long)sr);
		/* Not an error, so do not set SUNSCPAL_ABORTING */
		sc->sc_msgpriq |= SEND_DEV_RESET;
		goto have_nexus;
	}

#ifdef	DIAGNOSTIC
	if ((xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT)) == 0) {
		if (sc->sc_dataptr) {
			printf("%s: ptr but no data in/out flags?\n",
			    device_xname(sc->sc_dev));
			SUNSCPAL_BREAK();
			sc->sc_dataptr = NULL;
		}
	}
#endif

	/* Allocate DMA space (maybe) */
	if (sc->sc_dataptr && (sc->sc_flags & SUNSCPAL_DISABLE_DMA) == 0 &&
		(sc->sc_datalen >= sc->sc_min_dma_len))
	{
		SUNSCPAL_TRACE("sched: dma_alloc, len=%d\n", sc->sc_datalen);
		sunscpal_dma_alloc(sc);
	}

	/*
	 * Initialization hook called just after select,
	 * at the beginning of COMMAND phase.
	 * (but AFTER the DMA allocation is done)
	 *
	 * We need to set up the DMA engine BEFORE the target puts
	 * the SCSI bus into any DATA phase.
	 */
	if (sr->sr_dma_hand) {
		SUNSCPAL_TRACE("sched: dma_setup, dh=0x%x\n",
		    (long) sr->sr_dma_hand);
	    sunscpal_dma_setup(sc);
	}

	/*
	 * Schedule a timeout for the job we are starting.
	 */
	if ((sr->sr_flags & SR_IMMED) == 0) {
		i = mstohz(xs->timeout);
		SUNSCPAL_TRACE("sched: set timeout=%d\n", i);
		callout_reset(&sr->sr_xs->xs_callout, i,
		    sunscpal_cmd_timeout, sr);
	}

 have_nexus:

	SUNSCPAL_TRACE("sched: call machine, cur=0x%x\n",
	    (long)sc->sc_current);
	sunscpal_machine(sc);
	SUNSCPAL_TRACE("sched: machine done, cur=0x%x\n",
	    (long)sc->sc_current);

	/*
	 * What state did sunscpal_machine() leave us in?
	 * Hopefully it sometimes completes a job...
	 */
	if (sc->sc_state == SUNSCPAL_IDLE)
		goto next_job;

	return; 	/* Have work in progress. */
}


/*
 *  Reselect handler: checks for reselection, and if we are being
 *	reselected, it sets up sc->sc_current.
 *
 *  We are reselected when:
 *	SEL is TRUE
 *	IO  is TRUE
 *	BSY is FALSE
 */
void
sunscpal_reselect(struct sunscpal_softc *sc)
{

	/*
	 * This controller does not implement disconnect/reselect, so
	 * we really don't have anything to do here.  We keep this
	 * function as a placeholder, though.
	 */
}

/*
 *  Select target: xs is the transfer that we are selecting for.
 *  sc->sc_current should be NULL.
 *
 *  Returns:
 *	sc->sc_current != NULL  ==> we were reselected (race!)
 *	XS_NOERROR		==> selection worked
 *	XS_BUSY 		==> lost arbitration
 *	XS_SELTIMEOUT   	==> no response to selection
 */
static int
sunscpal_select(struct sunscpal_softc *sc, struct sunscpal_req *sr)
{
	int timo, target_mask;
	u_short	mode;

	/* Check for reselect */
	sunscpal_reselect(sc);
	if (sc->sc_current) {
		SUNSCPAL_TRACE("select: reselect, cur=0x%x\n",
		    (long)sc->sc_current);
		return XS_BUSY;	/* reselected */
	}

	/*
	 * Select the target.
	 */
	target_mask = (1 << sr->sr_target);
	SUNSCPAL_WRITE_1(sc, sunscpal_data, target_mask);
	SUNSCPAL_WRITE_2(sc, sunscpal_icr, SUNSCPAL_ICR_SELECT);

	/*
	 * Wait for the target to assert BSY.
	 * SCSI spec. says wait for 250 mS.
	 */
	for (timo = 25000;;) {
		if (SUNSCPAL_READ_2(sc, sunscpal_icr) & SUNSCPAL_ICR_BUSY)
			goto success;
		if (--timo <= 0)
			break;
		delay(10);
	}

	SUNSCPAL_WRITE_1(sc, sunscpal_data, 0);
	SUNSCPAL_WRITE_2(sc, sunscpal_icr, 0);

	SUNSCPAL_TRACE("select: device down, rc=%d\n", XS_SELTIMEOUT);
	return XS_SELTIMEOUT;

 success:

	/*
	 * The target is now driving BSY, so we can stop
	 * driving SEL and the data bus.  We do set up
	 * whether or not this target needs parity.
	 */
	mode = 0;
	if ((sc->sc_parity_disable & target_mask) == 0)
		mode |= SUNSCPAL_ICR_PARITY_ENABLE;
	SUNSCPAL_WRITE_2(sc, sunscpal_icr, mode);

	return XS_NOERROR;
}

/*****************************************************************
 * Functions to handle each info. transfer phase:
 *****************************************************************/

/*
 * The message system:
 *
 * This is a revamped message system that now should easier accommodate
 * new messages, if necessary.
 *
 * Currently we accept these messages:
 * IDENTIFY (when reselecting)
 * COMMAND COMPLETE # (expect bus free after messages marked #)
 * NOOP
 * MESSAGE REJECT
 * SYNCHRONOUS DATA TRANSFER REQUEST
 * SAVE DATA POINTER
 * RESTORE POINTERS
 * DISCONNECT #
 *
 * We may send these messages in prioritized order:
 * BUS DEVICE RESET #		if XS_CTL_RESET & xs->xs_control (or in
 *				weird sits.)
 * MESSAGE PARITY ERROR		par. err. during MSGI
 * MESSAGE REJECT		If we get a message we don't know how to handle
 * ABORT #			send on errors
 * INITIATOR DETECTED ERROR	also on errors (SCSI2) (during info xfer)
 * IDENTIFY			At the start of each transfer
 * SYNCHRONOUS DATA TRANSFER REQUEST	if appropriate
 * NOOP				if nothing else fits the bill ...
 */

/*
 * Precondition:
 * The SCSI bus is already in the MSGI phase and there is a message byte
 * on the bus, along with an asserted REQ signal.
 *
 * Our return value determines whether our caller, sunscpal_machine()
 * will expect to see another REQ (and possibly phase change).
 */
static int
sunscpal_msg_in(struct sunscpal_softc *sc)
{
	struct sunscpal_req *sr = sc->sc_current;
	struct scsipi_xfer *xs = sr->sr_xs;
	int n, phase;
	int act_flags;

	act_flags = ACT_CONTINUE;

	if (sc->sc_prevphase == SUNSCPAL_PHASE_MSG_IN) {
		/* This is a continuation of the previous message. */
		n = sc->sc_imp - sc->sc_imess;
		SUNSCPAL_TRACE("msg_in: continuation, n=%d\n", n);
		goto nextbyte;
	}

	/* This is a new MESSAGE IN phase.  Clean up our state. */
	sc->sc_state &= ~SUNSCPAL_DROP_MSGIN;

 nextmsg:
	n = 0;
	sc->sc_imp = &sc->sc_imess[n];

 nextbyte:
	/*
	 * Read a whole message, but don't ack the last byte.  If we reject the
	 * message, we have to assert ATN during the message transfer phase
	 * itself.
	 */
	for (;;) {
		/*
		 * Read a message byte.
		 * First, check BSY, REQ, phase...
		 */
		if (!SUNSCPAL_BUSY(sc)) {
			SUNSCPAL_TRACE("msg_in: lost BSY, n=%d\n", n);
			/* XXX - Assume the command completed? */
			act_flags |= (ACT_DISCONNECT | ACT_CMD_DONE);
			return act_flags;
		}
		if (sunscpal_wait_req(sc)) {
			SUNSCPAL_TRACE("msg_in: BSY but no REQ, n=%d\n", n);
			/* Just let sunscpal_machine() handle it... */
			return act_flags;
		}
		phase = SUNSCPAL_BUS_PHASE(SUNSCPAL_READ_2(sc, sunscpal_icr));
		if (phase != SUNSCPAL_PHASE_MSG_IN) {
			/*
			 * Target left MESSAGE IN, probably because it
			 * a) noticed our ATN signal, or
			 * b) ran out of messages.
			 */
			return act_flags;
		}
		/* Still in MESSAGE IN phase, and REQ is asserted. */
		if ((SUNSCPAL_READ_2(sc, sunscpal_icr) &
		    SUNSCPAL_ICR_PARITY_ERROR) != 0) {
			sunscpal_sched_msgout(sc, SEND_PARITY_ERROR);
			sc->sc_state |= SUNSCPAL_DROP_MSGIN;
		}

		/* Gather incoming message bytes if needed. */
		if ((sc->sc_state & SUNSCPAL_DROP_MSGIN) == 0) {
			if (n >= SUNSCPAL_MAX_MSG_LEN) {
				sunscpal_sched_msgout(sc, SEND_REJECT);
				sc->sc_state |= SUNSCPAL_DROP_MSGIN;
			} else {
				*sc->sc_imp++ =
				    SUNSCPAL_READ_1(sc, sunscpal_cmd_stat);
				n++;
				/*
				 * This testing is suboptimal, but most
				 * messages will be of the one byte variety, so
				 * it should not affect performance
				 * significantly.
				 */
				if (n == 1 && MSG_IS1BYTE(sc->sc_imess[0]))
					goto have_msg;
				if (n == 2 && MSG_IS2BYTE(sc->sc_imess[0]))
					goto have_msg;
				if (n >= 3 && MSG_ISEXTENDED(sc->sc_imess[0]) &&
					n == sc->sc_imess[1] + 2)
					goto have_msg;
			}
		}

		/*
		 * If we reach this spot we're either:
		 * a) in the middle of a multi-byte message, or
		 * b) dropping bytes.
		 */

		if (act_flags != ACT_CONTINUE)
			return act_flags;

		/* back to nextbyte */
	}

 have_msg:
	/* We now have a complete message.  Parse it. */

	switch (sc->sc_imess[0]) {
	case MSG_CMDCOMPLETE:
		SUNSCPAL_TRACE("msg_in: CMDCOMPLETE\n", 0);
		/* Target is about to disconnect. */
		act_flags |= (ACT_DISCONNECT | ACT_CMD_DONE);
		break;

	case MSG_PARITY_ERROR:
		SUNSCPAL_TRACE("msg_in: PARITY_ERROR\n", 0);
		/* Resend the last message. */
		sunscpal_sched_msgout(sc, sc->sc_msgout);
		break;

	case MSG_MESSAGE_REJECT:
		/* The target rejects the last message we sent. */
		SUNSCPAL_TRACE("msg_in: got reject for 0x%x\n", sc->sc_msgout);
		switch (sc->sc_msgout) {
		case SEND_IDENTIFY:
			/* Really old target controller? */
			/* XXX ... */
			break;
		case SEND_INIT_DET_ERR:
			goto abort;
		}
		break;

	case MSG_NOOP:
		SUNSCPAL_TRACE("msg_in: NOOP\n", 0);
		break;

	case MSG_DISCONNECT:
		SUNSCPAL_TRACE("msg_in: DISCONNECT\n", 0);
		/* Target is about to disconnect. */
		act_flags |= ACT_DISCONNECT;
		if ((xs->xs_periph->periph_quirks & PQUIRK_AUTOSAVE) == 0)
			break;
		/*FALLTHROUGH*/

	case MSG_SAVEDATAPOINTER:
		SUNSCPAL_TRACE("msg_in: SAVE_PTRS\n", 0);
		sr->sr_dataptr = sc->sc_dataptr;
		sr->sr_datalen = sc->sc_datalen;
		break;

	case MSG_RESTOREPOINTERS:
		SUNSCPAL_TRACE("msg_in: RESTORE_PTRS\n", 0);
		sc->sc_dataptr = sr->sr_dataptr;
		sc->sc_datalen = sr->sr_datalen;
		break;

	case MSG_EXTENDED:
		switch (sc->sc_imess[2]) {
		case MSG_EXT_SDTR:
		case MSG_EXT_WDTR:
			/* This controller can not do synchronous mode. */
			goto reject;
		default:
			printf("%s: unrecognized MESSAGE EXTENDED; "
			    "sending REJECT\n",
			    device_xname(sc->sc_dev));
			SUNSCPAL_BREAK();
			goto reject;
		}
		break;

	default:
		SUNSCPAL_TRACE("msg_in: eh? imsg=0x%x\n", sc->sc_imess[0]);
		printf("%s: unrecognized MESSAGE; sending REJECT\n",
		    device_xname(sc->sc_dev));
		SUNSCPAL_BREAK();
		/* FALLTHROUGH */
	reject:
		sunscpal_sched_msgout(sc, SEND_REJECT);
		break;

	abort:
		sc->sc_state |= SUNSCPAL_ABORTING;
		sunscpal_sched_msgout(sc, SEND_ABORT);
		break;
	}

	/* Go get the next message, if any. */
	if (act_flags == ACT_CONTINUE)
		goto nextmsg;

	return act_flags;
}


/*
 * The message out (and in) stuff is a bit complicated:
 * If the target requests another message (sequence) without
 * having changed phase in between it really asks for a
 * retransmit, probably due to parity error(s).
 * The following messages can be sent:
 * IDENTIFY	   @ These 4 stem from SCSI command activity
 * SDTR		   @
 * WDTR		   @
 * DEV_RESET	   @
 * REJECT if MSGI doesn't make sense
 * PARITY_ERROR if parity error while in MSGI
 * INIT_DET_ERR if parity error while not in MSGI
 * ABORT if INIT_DET_ERR rejected
 * NOOP if asked for a message and there's nothing to send
 *
 * Note that we call this one with (sc_current == NULL)
 * when sending ABORT for unwanted reselections.
 */
static int
sunscpal_msg_out(struct sunscpal_softc *sc)
{
	/*
	 * This controller does not allow you to assert ATN, which
	 * means we will never get the opportunity to send messages to
	 * the target (the bus will never enter this MSG_OUT phase).
	 * This will eventually leave us with no option other than to
	 * reset the bus.  We keep this function as a placeholder,
	 * though, and this printf will eventually go away or get
	 * #ifdef'ed:
	 */
	printf("%s: bus is in MSG_OUT phase?\n", __func__);
	return ACT_CONTINUE | ACT_RESET_BUS;
}

/*
 * Handle command phase.
 */
static int
sunscpal_command(struct sunscpal_softc *sc)
{
	struct sunscpal_req *sr = sc->sc_current;
	struct scsipi_xfer *xs = sr->sr_xs;
	int len;

	/* Assume command can be sent in one go. */
	/* XXX: Do this using DMA, and get a phase change intr? */
	len = sunscpal_pio_out(sc, SUNSCPAL_PHASE_COMMAND, xs->cmdlen,
	    (uint8_t *)xs->cmd);

	if (len != xs->cmdlen) {
#ifdef	SUNSCPAL_DEBUG
		printf("%s: short transfer: wanted %d got %d.\n",
		    __func__, xs->cmdlen, len);
		sunscpal_show_scsi_cmd(xs);
		SUNSCPAL_BREAK();
#endif
		if (len < 6) {
			xs->error = XS_DRIVER_STUFFUP;
			sc->sc_state |= SUNSCPAL_ABORTING;
			sunscpal_sched_msgout(sc, SEND_ABORT);
		}

	}

	return ACT_CONTINUE;
}


/*
 * Handle either data_in or data_out
 */
static int
sunscpal_data_xfer(struct sunscpal_softc *sc, int phase)
{
	struct sunscpal_req *sr = sc->sc_current;
	struct scsipi_xfer *xs = sr->sr_xs;
	int expected_phase;
	int len;

	/*
	 * When aborting a command, disallow any data phase.
	 */
	if (sc->sc_state & SUNSCPAL_ABORTING) {
		printf("%s: aborting, bus phase=%s (reset)\n",
		    device_xname(sc->sc_dev), phase_names[(phase >> 8) & 7]);
		return ACT_RESET_BUS;	/* XXX */
	}

	/* Validate expected phase (data_in or data_out) */
	expected_phase = (xs->xs_control & XS_CTL_DATA_OUT) ?
	    SUNSCPAL_PHASE_DATA_OUT : SUNSCPAL_PHASE_DATA_IN;
	if (phase != expected_phase) {
		printf("%s: data phase error\n", device_xname(sc->sc_dev));
		goto abort;
	}

	/* Make sure we have some data to move. */
	if (sc->sc_datalen <= 0) {
		/* Device needs padding. */
		if (phase == SUNSCPAL_PHASE_DATA_IN)
			sunscpal_pio_in(sc, phase, 4096, NULL);
		else
			sunscpal_pio_out(sc, phase, 4096, NULL);
		/* Make sure that caused a phase change. */
		if (SUNSCPAL_BUS_PHASE(SUNSCPAL_READ_2(sc, sunscpal_icr)) ==
		    phase) {
			/* More than 4k is just too much! */
			printf("%s: too much data padding\n",
			    device_xname(sc->sc_dev));
			goto abort;
		}
		return ACT_CONTINUE;
	}

	/*
	 * Attempt DMA only if dma_alloc gave us a DMA handle AND
	 * there is enough left to transfer so DMA is worth while.
	 */
	if (sr->sr_dma_hand && (sc->sc_datalen >= sc->sc_min_dma_len)) {
		/*
		 * OK, really start DMA.  Note, the MD start function
		 * is responsible for setting the TCMD register, etc.
		 * (Acknowledge the phase change there, not here.)
		 */
		SUNSCPAL_TRACE("data_xfer: dma_start, dh=0x%x\n",
		    (long)sr->sr_dma_hand);
		sunscpal_dma_start(sc);
		return ACT_WAIT_DMA;
	}

	/*
	 * Doing PIO for data transfer.  (Possibly "Pseudo DMA")
	 * XXX:  Do PDMA functions need to set tcmd later?
	 */
	SUNSCPAL_TRACE("data_xfer: doing PIO, len=%d\n", sc->sc_datalen);
	if (phase == SUNSCPAL_PHASE_DATA_OUT) {
		len = sunscpal_pio_out(sc, phase,
		    sc->sc_datalen, sc->sc_dataptr);
	} else {
		len = sunscpal_pio_in(sc, phase,
		    sc->sc_datalen, sc->sc_dataptr);
	}
	sc->sc_dataptr += len;
	sc->sc_datalen -= len;

	SUNSCPAL_TRACE("data_xfer: did PIO, resid=%d\n", sc->sc_datalen);
	return ACT_CONTINUE;

 abort:
	sc->sc_state |= SUNSCPAL_ABORTING;
	sunscpal_sched_msgout(sc, SEND_ABORT);
	return ACT_CONTINUE;
}


static int
sunscpal_status(struct sunscpal_softc *sc)
{
	int len;
	uint8_t status;
	struct sunscpal_req *sr = sc->sc_current;

	len = sunscpal_pio_in(sc, SUNSCPAL_PHASE_STATUS, 1, &status);
	if (len) {
		sr->sr_status = status;
	} else {
		printf("%s: none?\n", __func__);
	}

	return ACT_CONTINUE;
}


/*
 * This is the big state machine that follows SCSI phase changes.
 * This is somewhat like a co-routine.  It will do a SCSI command,
 * and exit if the command is complete, or if it must wait, i.e.
 * for DMA to complete or for reselect to resume the job.
 *
 * The bus must be selected, and we need to know which command is
 * being undertaken.
 */
static void
sunscpal_machine(struct sunscpal_softc *sc)
{
	struct sunscpal_req *sr;
	struct scsipi_xfer *xs;
	int act_flags, phase, timo;

#ifdef	DIAGNOSTIC
	if (sc->sc_state == SUNSCPAL_IDLE)
		panic("%s: state=idle", __func__);
	if (sc->sc_current == NULL)
		panic("%s: no current cmd", __func__);
#endif

	sr = sc->sc_current;
	xs = sr->sr_xs;
	act_flags = ACT_CONTINUE;

	/*
	 * This will be called by sunscpal_intr() when DMA is
	 * complete.  Must stop DMA before touching the PAL or
	 * there will be "register conflict" errors.
	 */
	if ((sc->sc_state & SUNSCPAL_DOINGDMA) != 0) {
		/* Pick-up where where we left off... */
		goto dma_done;
	}

 next_phase:

	if (!SUNSCPAL_BUSY(sc)) {
		/* Unexpected disconnect */
		printf("%s: unexpected disconnect.\n", __func__);
		xs->error = XS_DRIVER_STUFFUP;
		act_flags |= (ACT_DISCONNECT | ACT_CMD_DONE);
		goto do_actions;
	}

	/*
	 * Wait for REQ before reading the phase.
	 * Need to wait longer than usual here, because
	 * some devices are just plain slow...
	 */
	timo = sunscpal_wait_phase_timo;
	for (;;) {
		if (SUNSCPAL_READ_2(sc, sunscpal_icr) & SUNSCPAL_ICR_REQUEST)
			break;
		if (--timo <= 0) {
			if (sc->sc_state & SUNSCPAL_ABORTING) {
				printf("%s: no REQ while aborting, reset\n",
				    device_xname(sc->sc_dev));
				act_flags |= ACT_RESET_BUS;
				goto do_actions;
			}
			printf("%s: no REQ for next phase, abort\n",
			    device_xname(sc->sc_dev));
			sc->sc_state |= SUNSCPAL_ABORTING;
			sunscpal_sched_msgout(sc, SEND_ABORT);
			goto next_phase;
		}
		delay(100);
	}

	phase = SUNSCPAL_BUS_PHASE(SUNSCPAL_READ_2(sc, sunscpal_icr));
	SUNSCPAL_TRACE("machine: phase=%s\n",
	    (long)phase_names[(phase >> 8) & 7]);

	/*
	 * We assume that the device knows what it's doing,
	 * so any phase is good.
	 */

	switch (phase) {

	case SUNSCPAL_PHASE_DATA_OUT:
	case SUNSCPAL_PHASE_DATA_IN:
		act_flags = sunscpal_data_xfer(sc, phase);
		break;

	case SUNSCPAL_PHASE_COMMAND:
		act_flags = sunscpal_command(sc);
		break;

	case SUNSCPAL_PHASE_STATUS:
		act_flags = sunscpal_status(sc);
		break;

	case SUNSCPAL_PHASE_MSG_OUT:
		act_flags = sunscpal_msg_out(sc);
		break;

	case SUNSCPAL_PHASE_MSG_IN:
		act_flags = sunscpal_msg_in(sc);
		break;

	default:
		printf("%s: Unexpected phase 0x%x\n", __func__, phase);
		sc->sc_state |= SUNSCPAL_ABORTING;
		sunscpal_sched_msgout(sc, SEND_ABORT);
		goto next_phase;

	} /* switch */
	sc->sc_prevphase = phase;

 do_actions:

	if (act_flags & ACT_WAIT_DMA) {
		act_flags &= ~ACT_WAIT_DMA;
		/* Wait for DMA to complete (polling, or interrupt). */
		if ((sr->sr_flags & SR_IMMED) == 0) {
			SUNSCPAL_TRACE("machine: wait for DMA intr.\n", 0);
			return; 	/* will resume at dma_done */
		}
		/* Busy-wait for it to finish. */
		SUNSCPAL_TRACE("machine: dma_poll, dh=0x%x\n",
		    (long)sr->sr_dma_hand);
		sunscpal_dma_poll(sc);
 dma_done:
		/* Return here after interrupt. */
		if (sr->sr_flags & SR_OVERDUE)
			sc->sc_state |= SUNSCPAL_ABORTING;
		SUNSCPAL_TRACE("machine: dma_stop, dh=0x%x\n",
		    (long)sr->sr_dma_hand);
		sunscpal_dma_stop(sc);
		SUNSCPAL_CLR_INTR(sc);	/* XXX */
		/*
		 * While DMA is running we can not touch the SBC,
		 * so various places just set SUNSCPAL_ABORTING and
		 * expect us the "kick it" when DMA is done.
		 */
		if (sc->sc_state & SUNSCPAL_ABORTING) {
			sunscpal_sched_msgout(sc, SEND_ABORT);
		}
	}

	/*
	 * Check for parity error.
	 * XXX - better place to check?
	 */
	if (SUNSCPAL_READ_2(sc, sunscpal_icr) & SUNSCPAL_ICR_PARITY_ERROR) {
		printf("%s: parity error!\n", device_xname(sc->sc_dev));
		/* XXX: sc->sc_state |= SUNSCPAL_ABORTING; */
		sunscpal_sched_msgout(sc, SEND_PARITY_ERROR);
	}

	if (act_flags == ACT_CONTINUE)
		goto next_phase;
	/* All other actions "break" from the loop. */

	SUNSCPAL_TRACE("machine: act_flags=0x%x\n", act_flags);

	if (act_flags & ACT_RESET_BUS) {
		act_flags |= ACT_CMD_DONE;
		/*
		 * Reset the SCSI bus, usually due to a timeout.
		 * The error code XS_TIMEOUT allows retries.
		 */
		sc->sc_state |= SUNSCPAL_ABORTING;
		printf("%s: reset SCSI bus for TID=%d LUN=%d\n",
		    device_xname(sc->sc_dev), sr->sr_target, sr->sr_lun);
		sunscpal_reset_scsibus(sc);
	}

	if (act_flags & ACT_CMD_DONE) {
		act_flags |= ACT_DISCONNECT;
		/* Need to call scsipi_done() */
		/* XXX: from the aic6360 driver, but why? */
		if (sc->sc_datalen < 0) {
			printf("%s: %d extra bytes from %d:%d\n",
			    device_xname(sc->sc_dev), -sc->sc_datalen,
			    sr->sr_target, sr->sr_lun);
			sc->sc_datalen = 0;
		}
		xs->resid = sc->sc_datalen;
		/* Note: this will clear sc_current */
		SUNSCPAL_TRACE("machine: call done, cur=0x%x\n", (long)sr);
		sunscpal_done(sc);
	}

	if (act_flags & ACT_DISCONNECT) {
		/*
		 * The device has dropped BSY (or will soon).
		 * We have to wait here for BSY to drop, otherwise
		 * the next command may decide we need a bus reset.
		 */
		timo = sunscpal_wait_req_timo;	/* XXX */
		for (;;) {
			if (!SUNSCPAL_BUSY(sc))
				goto busfree;
			if (--timo <= 0)
				break;
			delay(2);
		}
		/* Device is sitting on the bus! */
		printf("%s: Target %d LUN %d stuck busy, resetting...\n",
		    device_xname(sc->sc_dev), sr->sr_target, sr->sr_lun);
		sunscpal_reset_scsibus(sc);
 busfree:
		SUNSCPAL_TRACE("machine: discon, waited %d\n",
			sunscpal_wait_req_timo - timo);

		SUNSCPAL_WRITE_2(sc, sunscpal_icr, 0);

		if ((act_flags & ACT_CMD_DONE) == 0) {
			SUNSCPAL_TRACE("machine: discon, cur=0x%x\n", (long)sr);
		}

		/*
		 * We may be here due to a disconnect message,
		 * in which case we did NOT call sunscpal_done,
		 * and we need to clear sc_current.
		 */
		sc->sc_state = SUNSCPAL_IDLE;
		sc->sc_current = NULL;

		/* Paranoia: clear everything. */
		sc->sc_dataptr = NULL;
		sc->sc_datalen = 0;
		sc->sc_prevphase = SUNSCPAL_PHASE_INVALID;
		sc->sc_msgpriq = 0;
		sc->sc_msgoutq = 0;
		sc->sc_msgout  = 0;

		/* Our caller will re-enable interrupts. */
	}
}


#ifdef	SUNSCPAL_DEBUG

static void
sunscpal_show_scsi_cmd(struct scsipi_xfer *xs)
{
	uint8_t *b = (uint8_t *)xs->cmd;
	int i = 0;

	scsipi_printaddr(xs->xs_periph);
	if ((xs->xs_control & XS_CTL_RESET) == 0) {
		printf("-");
		while (i < xs->cmdlen) {
			if (i != 0)
				printf(",");
			printf("%x", b[i++]);
		}
		printf("-\n");
	} else {
		printf("-RESET-\n");
	}
}


int sunscpal_traceidx = 0;

#define	TRACE_MAX	1024
struct trace_ent {
	char *msg;
	long  val;
} sunscpal_tracebuf[TRACE_MAX];

void
sunscpal_trace(char *msg, long val)
{
	struct trace_ent *tr;
	int s;

	s = splbio();

	tr = &sunscpal_tracebuf[sunscpal_traceidx];

	sunscpal_traceidx++;
	if (sunscpal_traceidx >= TRACE_MAX)
		sunscpal_traceidx = 0;

	tr->msg = msg;
	tr->val = val;

	splx(s);
}

#ifdef	DDB
void
sunscpal_clear_trace(void)
{

	sunscpal_traceidx = 0;
	memset((void *)sunscpal_tracebuf, 0, sizeof(sunscpal_tracebuf));
}

void
sunscpal_show_trace(void)
{
	struct trace_ent *tr;
	int idx;

	idx = sunscpal_traceidx;
	do {
		tr = &sunscpal_tracebuf[idx];
		idx++;
		if (idx >= TRACE_MAX)
			idx = 0;
		if (tr->msg)
			db_printf(tr->msg, tr->val);
	} while (idx != sunscpal_traceidx);
}

void
sunscpal_show_req(struct sunscpal_req *sr)
{
	struct scsipi_xfer *xs = sr->sr_xs;

	db_printf("TID=%d ",	sr->sr_target);
	db_printf("LUN=%d ",	sr->sr_lun);
	db_printf("dh=%p ",	sr->sr_dma_hand);
	db_printf("dptr=%p ",	sr->sr_dataptr);
	db_printf("dlen=0x%x ",	sr->sr_datalen);
	db_printf("flags=%d ",	sr->sr_flags);
	db_printf("stat=%d ",	sr->sr_status);

	if (xs == NULL) {
		db_printf("(xs=NULL)\n");
		return;
	}
	db_printf("\n");
#ifdef	SCSIDEBUG
	show_scsipi_xs(xs);
#else
	db_printf("xs=%p\n", xs);
#endif
}

void
sunscpal_show_state(void)
{
	struct sunscpal_softc *sc;
	struct sunscpal_req *sr;
	int i, j, k;

	sc = sunscpal_debug_sc;

	if (sc == NULL) {
		db_printf("sunscpal_debug_sc == NULL\n");
		return;
	}

	db_printf("sc_ncmds=%d\n",  	sc->sc_ncmds);
	k = -1;	/* which is current? */
	for (i = 0; i < SUNSCPAL_OPENINGS; i++) {
		sr = &sc->sc_ring[i];
		if (sr->sr_xs) {
			if (sr == sc->sc_current)
				k = i;
			db_printf("req %d: (sr=%p)", i, sr);
			sunscpal_show_req(sr);
		}
	}
	db_printf("sc_rr=%d, current=%d\n", sc->sc_rr, k);

	db_printf("Active request matrix:\n");
	for(i = 0; i < 8; i++) {		/* targets */
		for (j = 0; j < 8; j++) {	/* LUN */
			sr = sc->sc_matrix[i][j];
			if (sr) {
				db_printf("TID=%d LUN=%d sr=%p\n", i, j, sr);
			}
		}
	}

	db_printf("sc_state=0x%x\n",	sc->sc_state);
	db_printf("sc_current=%p\n",	sc->sc_current);
	db_printf("sc_dataptr=%p\n",	sc->sc_dataptr);
	db_printf("sc_datalen=0x%x\n",	sc->sc_datalen);

	db_printf("sc_prevphase=%d\n",	sc->sc_prevphase);
	db_printf("sc_msgpriq=0x%x\n",	sc->sc_msgpriq);
}
#endif	/* DDB */
#endif	/* SUNSCPAL_DEBUG */

void
sunscpal_attach(struct sunscpal_softc *sc, int options)
{

	/*
	 * Handle our options.
	 */
	aprint_normal(": options=0x%x\n", options);
	sc->sc_parity_disable = (options & SUNSCPAL_OPT_NO_PARITY_CHK);
	if (options & SUNSCPAL_OPT_DISABLE_DMA)
		sc->sc_flags |= SUNSCPAL_DISABLE_DMA;

	/*
	 * Fill in the adapter.
	 */
	memset(&sc->sc_adapter, 0, sizeof(sc->sc_adapter));
	sc->sc_adapter.adapt_dev = sc->sc_dev;
	sc->sc_adapter.adapt_nchannels = 1;
	sc->sc_adapter.adapt_openings = SUNSCPAL_OPENINGS;
	sc->sc_adapter.adapt_max_periph = 1;
	sc->sc_adapter.adapt_request = sunscpal_scsipi_request;
	sc->sc_adapter.adapt_minphys = sunscpal_minphys;
	if (options & SUNSCPAL_OPT_FORCE_POLLING)
		sc->sc_adapter.adapt_flags |= SCSIPI_ADAPT_POLL_ONLY;

	sc->sc_channel.chan_adapter = &sc->sc_adapter;
	sc->sc_channel.chan_bustype = &scsi_bustype;
	sc->sc_channel.chan_channel = 0;
	sc->sc_channel.chan_ntargets = 8;
	sc->sc_channel.chan_nluns = 8;
	sc->sc_channel.chan_id = 7;

	/*
	 * Add reference to adapter so that we drop the reference after
	 * config_found() to make sure the adatper is disabled.
	 */
	if (scsipi_adapter_addref(&sc->sc_adapter) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to enable controller\n");
		return;
	}

	sunscpal_init(sc);	/* Init chip and driver */
	sunscpal_reset_scsibus(sc);

	/*
	 * Ask the adapter what subunits are present
	 */
	(void)config_found(sc->sc_dev, &sc->sc_channel, scsiprint);
	scsipi_adapter_delref(&sc->sc_adapter);
}

int
sunscpal_detach(struct sunscpal_softc *sc, int flags)
{

	return EOPNOTSUPP;
}

static void
sunscpal_minphys(struct buf *bp)
{

	if (bp->b_bcount > SUNSCPAL_MAX_DMA_LEN) {
#ifdef	SUNSCPAL_DEBUG
		if (sunscpal_debug & SUNSCPAL_DBG_DMA) {
			printf("%s: len = 0x%lx.\n", __func__, bp->b_bcount);
			Debugger();
		}
#endif
		bp->b_bcount = SUNSCPAL_MAX_DMA_LEN;
	}
	return minphys(bp);
}

#ifdef SUNSCPAL_USE_BUS_DMA

/*
 * Allocate a DMA handle and put it in sr->sr_dma_hand.  Prepare
 * for DMA transfer.
 */
static void
sunscpal_dma_alloc(struct sunscpal_softc *sc)
{
	struct sunscpal_req *sr = sc->sc_current;
	sunscpal_dma_handle_t dh;
	int i, xlen;
	u_long addr;

#ifdef	DIAGNOSTIC
	if (sr->sr_dma_hand != NULL)
		panic("%s: already have DMA handle", __func__);
#endif

	addr = (u_long)sc->sc_dataptr;
	xlen = sc->sc_datalen;

	/* If the DMA start addr is misaligned then do PIO */
	if ((addr & 1) || (xlen & 1)) {
		printf("%s: misaligned.\n", __func__);
		return;
	}

	/* Make sure our caller checked sc_min_dma_len. */
	if (xlen < sc->sc_min_dma_len)
		panic("%s: xlen=0x%x", __func__, xlen);

	/*
	 * Never attempt single transfers of more than 63k, because
	 * our count register is only 16 bits.
	 * This should never happen since already bounded by minphys().
	 * XXX - Should just segment these...
	 */
	if (xlen > SUNSCPAL_MAX_DMA_LEN) {
		printf("%s: excessive xlen=0x%x\n", __func__, xlen);
		Debugger();
		sc->sc_datalen = xlen = SUNSCPAL_MAX_DMA_LEN;
	}

	/* Find free DMA handle.  Guaranteed to find one since we have
	   as many DMA handles as the driver has processes. */
	for (i = 0; i < SUNSCPAL_OPENINGS; i++) {
		if ((sc->sc_dma_handles[i].dh_flags & SUNSCDH_BUSY) == 0)
			goto found;
	}
	panic("%s: no free DMA handles.", device_xname(sc->sc_dev));
 found:

	dh = &sc->sc_dma_handles[i];
	dh->dh_flags = SUNSCDH_BUSY;
	dh->dh_mapaddr = (uint8_t *)addr;
	dh->dh_maplen  = xlen;
	dh->dh_dvma = 0;

	/* Load the DMA map. */
	if (bus_dmamap_load(sc->sunscpal_dmat, dh->dh_dmamap,
	    dh->dh_mapaddr, dh->dh_maplen, NULL, BUS_DMA_NOWAIT) != 0) {
		/* Can't load map */
		printf("%s: can't DMA %p/0x%x\n", __func__,
		    dh->dh_mapaddr, dh->dh_maplen);
		dh->dh_flags = 0;
		return;
	}

	/* success */
	sr->sr_dma_hand = dh;
}

static void
sunscpal_dma_free(struct sunscpal_softc *sc)
{
	struct sunscpal_req *sr = sc->sc_current;
	sunscpal_dma_handle_t dh = sr->sr_dma_hand;

#ifdef	DIAGNOSTIC
	if (dh == NULL)
		panic("%s: no DMA handle", __func__);
#endif

	if (sc->sc_state & SUNSCPAL_DOINGDMA)
		panic("%s: free while in progress", __func__);

	if (dh->dh_flags & SUNSCDH_BUSY) {
		/* XXX - Should separate allocation and mapping. */
		/* Give back the DVMA space. */
		bus_dmamap_unload(sc->sunscpal_dmat, dh->dh_dmamap);
		dh->dh_flags = 0;
	}
	sr->sr_dma_hand = NULL;
}

/*
 * This function is called during the SELECT phase that
 * precedes a COMMAND phase, in case we need to setup the
 * DMA engine before the bus enters a DATA phase.
 *
 * On the sc version, setup the start address and the count.
 */
static void
sunscpal_dma_setup(struct sunscpal_softc *sc)
{
	struct sunscpal_req *sr = sc->sc_current;
	struct scsipi_xfer *xs = sr->sr_xs;
	sunscpal_dma_handle_t dh = sr->sr_dma_hand;
	long data_pa;
	int xlen;

	/*
	 * Get the DVMA mapping for this segment.
	 * XXX - Should separate allocation and mapin.
	 */
	data_pa = dh->dh_dvma;
	data_pa += (sc->sc_dataptr - dh->dh_mapaddr);
	if (data_pa & 1)
		panic("%s: bad pa=0x%lx", __func__, data_pa);
	xlen = sc->sc_datalen;
	if (xlen & 1)
		panic("%s: bad xlen=0x%x", __func__, xlen);
	sc->sc_reqlen = xlen; 	/* XXX: or less? */

#ifdef	SUNSCPAL_DEBUG
	if (sunscpal_debug & SUNSCPAL_DBG_DMA) {
		printf("%s: dh=%p, pa=0x%lx, xlen=0x%x\n",
		    __func__, dh, data_pa, xlen);
	}
#endif

	/* sync the DMA map: */
	bus_dmamap_sync(sc->sunscpal_dmat, dh->dh_dmamap, 0, dh->dh_maplen,
	    ((xs->xs_control & XS_CTL_DATA_OUT) == 0 ?
	    BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE));

	/* Load the start address and the count. */
	SUNSCPAL_WRITE_2(sc, sunscpal_dma_addr_h, (data_pa >> 16) & 0xFFFF);
	SUNSCPAL_WRITE_2(sc, sunscpal_dma_addr_l, (data_pa >> 0) & 0xFFFF);
	SUNSCPAL_WRITE_2(sc, sunscpal_dma_count, SUNSCPAL_DMA_COUNT_FLIP(xlen));
}

#endif /* SUNSCPAL_USE_BUS_DMA */
