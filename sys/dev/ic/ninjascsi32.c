/*	$NetBSD: ninjascsi32.c,v 1.24 2013/09/14 21:52:49 martin Exp $	*/

/*-
 * Copyright (c) 2004, 2006, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by ITOH Yasufumi.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ninjascsi32.c,v 1.24 2013/09/14 21:52:49 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/scsiio.h>
#include <sys/proc.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_message.h>

/*
 * DualEdge transfer support
 */
/* #define NJSC32_DUALEDGE */	/* XXX untested */

/*
 * Auto param loading does not work properly (it partially works (works on
 * start, doesn't on restart) on rev 0x54, it doesn't work at all on rev 0x51),
 * and it doesn't improve the performance so much,
 * forget about it.
 */
#undef NJSC32_AUTOPARAM

#include <dev/ic/ninjascsi32reg.h>
#include <dev/ic/ninjascsi32var.h>

/* #define NJSC32_DEBUG */
/* #define NJSC32_TRACE */

#ifdef NJSC32_DEBUG
#define DPRINTF(x)	printf x
#define DPRINTC(cmd, x)	PRINTC(cmd, x)
#else
#define DPRINTF(x)
#define DPRINTC(cmd, x)
#endif
#ifdef NJSC32_TRACE
#define TPRINTF(x)	printf x
#define TPRINTC(cmd, x)	PRINTC(cmd, x)
#else
#define TPRINTF(x)
#define TPRINTC(cmd, x)
#endif

#define PRINTC(cmd, x)	do {					\
		scsi_print_addr((cmd)->c_xs->xs_periph);	\
		printf x;					\
	} while (/* CONSTCOND */ 0)

static void	njsc32_scsipi_request(struct scsipi_channel *,
		    scsipi_adapter_req_t, void *);
static void	njsc32_scsipi_minphys(struct buf *);
static int	njsc32_scsipi_ioctl(struct scsipi_channel *, u_long, void *,
		    int, struct proc *);

static void	njsc32_init(struct njsc32_softc *, int nosleep);
static int	njsc32_init_cmds(struct njsc32_softc *);
static void	njsc32_target_async(struct njsc32_softc *,
		    struct njsc32_target *);
static void	njsc32_init_targets(struct njsc32_softc *);
static void	njsc32_add_msgout(struct njsc32_softc *, int);
static u_int32_t njsc32_get_auto_msgout(struct njsc32_softc *);
#ifdef NJSC32_DUALEDGE
static void	njsc32_msgout_wdtr(struct njsc32_softc *, int);
#endif
static void	njsc32_msgout_sdtr(struct njsc32_softc *, int period,
		    int offset);
static void	njsc32_negotiate_xfer(struct njsc32_softc *,
		    struct njsc32_target *);
static void	njsc32_arbitration_failed(struct njsc32_softc *);
static void	njsc32_start(struct njsc32_softc *);
static void	njsc32_run_xfer(struct njsc32_softc *, struct scsipi_xfer *);
static void	njsc32_end_cmd(struct njsc32_softc *, struct njsc32_cmd *,
		    scsipi_xfer_result_t);
static void	njsc32_wait_reset_release(void *);
static void	njsc32_reset_bus(struct njsc32_softc *);
static void	njsc32_clear_cmds(struct njsc32_softc *,
		    scsipi_xfer_result_t);
static void	njsc32_set_ptr(struct njsc32_softc *, struct njsc32_cmd *,
		    u_int32_t);
static void	njsc32_assert_ack(struct njsc32_softc *);
static void	njsc32_negate_ack(struct njsc32_softc *);
static void	njsc32_wait_req_negate(struct njsc32_softc *);
static void	njsc32_reconnect(struct njsc32_softc *, struct njsc32_cmd *);
enum njsc32_reselstat {
	NJSC32_RESEL_ERROR,		/* to be rejected */
	NJSC32_RESEL_COMPLETE,		/* reselection is just complete */
	NJSC32_RESEL_THROUGH		/* this message is OK (no reply) */
};
static enum njsc32_reselstat njsc32_resel_identify(struct njsc32_softc *,
		    int lun, struct njsc32_cmd **);
static enum njsc32_reselstat njsc32_resel_tag(struct njsc32_softc *,
		    int tag, struct njsc32_cmd **);
static void	njsc32_cmd_reload(struct njsc32_softc *, struct njsc32_cmd *,
		    int);
static void	njsc32_update_xfer_mode(struct njsc32_softc *,
		    struct njsc32_target *);
static void	njsc32_msgin(struct njsc32_softc *);
static void	njsc32_msgout(struct njsc32_softc *);
static void	njsc32_cmdtimeout(void *);
static void	njsc32_reseltimeout(void *);

static inline unsigned
njsc32_read_1(struct njsc32_softc *sc, int no)
{

	return bus_space_read_1(sc->sc_regt, sc->sc_regh, no);
}

static inline unsigned
njsc32_read_2(struct njsc32_softc *sc, int no)
{

	return bus_space_read_2(sc->sc_regt, sc->sc_regh, no);
}

static inline u_int32_t
njsc32_read_4(struct njsc32_softc *sc, int no)
{

	return bus_space_read_4(sc->sc_regt, sc->sc_regh, no);
}

static inline void
njsc32_write_1(struct njsc32_softc *sc, int no, int val)
{

	bus_space_write_1(sc->sc_regt, sc->sc_regh, no, val);
}

static inline void
njsc32_write_2(struct njsc32_softc *sc, int no, int val)
{

	bus_space_write_2(sc->sc_regt, sc->sc_regh, no, val);
}

static inline void
njsc32_write_4(struct njsc32_softc *sc, int no, u_int32_t val)
{

	bus_space_write_4(sc->sc_regt, sc->sc_regh, no, val);
}

static inline unsigned
njsc32_ireg_read_1(struct njsc32_softc *sc, int no)
{

	bus_space_write_1(sc->sc_regt, sc->sc_regh, NJSC32_REG_INDEX, no);
	return bus_space_read_1(sc->sc_regt, sc->sc_regh, NJSC32_REG_DATA_LOW);
}

static inline void
njsc32_ireg_write_1(struct njsc32_softc *sc, int no, int val)
{

	bus_space_write_1(sc->sc_regt, sc->sc_regh, NJSC32_REG_INDEX, no);
	bus_space_write_1(sc->sc_regt, sc->sc_regh, NJSC32_REG_DATA_LOW, val);
}

static inline void
njsc32_ireg_write_2(struct njsc32_softc *sc, int no, int val)
{

	bus_space_write_1(sc->sc_regt, sc->sc_regh, NJSC32_REG_INDEX, no);
	bus_space_write_2(sc->sc_regt, sc->sc_regh, NJSC32_REG_DATA_LOW, val);
}

#define NS(ns)	((ns) / 4)	/* nanosecond (>= 50) -> sync value */
#ifdef __STDC__
# define ACKW(n)	NJSC32_ACK_WIDTH_ ## n ## CLK
# define SMPL(n)	(NJSC32_SREQ_SAMPLING_ ## n ## CLK |	\
			 NJSC32_SREQ_SAMPLING_ENABLE)
#else
# define ACKW(n)	NJSC32_ACK_WIDTH_/**/n/**/CLK
# define SMPL(n)	(NJSC32_SREQ_SAMPLING_/**/n/**/CLK |	\
			 NJSC32_SREQ_SAMPLING_ENABLE)
#endif

#define NJSC32_NSYNCT_MAXSYNC	1
#define NJSC32_NSYNCT		16

/* 40MHz (25ns) */
static const struct njsc32_sync_param njsc32_synct_40M[NJSC32_NSYNCT] = {
	{ 0, 0, 0 },			/* dummy for async */
	{ NS( 50), ACKW(1), 0       },	/* 20.0 :  50ns,  25ns */
	{ NS( 75), ACKW(1), SMPL(1) },	/* 13.3 :  75ns,  25ns */
	{ NS(100), ACKW(2), SMPL(1) },	/* 10.0 : 100ns,  50ns */
	{ NS(125), ACKW(2), SMPL(2) },	/*  8.0 : 125ns,  50ns */
	{ NS(150), ACKW(3), SMPL(2) },	/*  6.7 : 150ns,  75ns */
	{ NS(175), ACKW(3), SMPL(2) },	/*  5.7 : 175ns,  75ns */
	{ NS(200), ACKW(4), SMPL(2) },	/*  5.0 : 200ns, 100ns */
	{ NS(225), ACKW(4), SMPL(4) },	/*  4.4 : 225ns, 100ns */
	{ NS(250), ACKW(4), SMPL(4) },	/*  4.0 : 250ns, 100ns */
	{ NS(275), ACKW(4), SMPL(4) },	/*  3.64: 275ns, 100ns */
	{ NS(300), ACKW(4), SMPL(4) },	/*  3.33: 300ns, 100ns */
	{ NS(325), ACKW(4), SMPL(4) },	/*  3.01: 325ns, 100ns */
	{ NS(350), ACKW(4), SMPL(4) },	/*  2.86: 350ns, 100ns */
	{ NS(375), ACKW(4), SMPL(4) },	/*  2.67: 375ns, 100ns */
	{ NS(400), ACKW(4), SMPL(4) }	/*  2.50: 400ns, 100ns */
};

#ifdef NJSC32_SUPPORT_OTHER_CLOCKS
/* 20MHz (50ns) */
static const struct njsc32_sync_param njsc32_synct_20M[NJSC32_NSYNCT] = {
	{ 0, 0, 0 },			/* dummy for async */
	{ NS(100), ACKW(1), 0       },	/* 10.0 : 100ns,  50ns */
	{ NS(150), ACKW(1), SMPL(2) },	/*  6.7 : 150ns,  50ns */
	{ NS(200), ACKW(2), SMPL(2) },	/*  5.0 : 200ns, 100ns */
	{ NS(250), ACKW(2), SMPL(4) },	/*  4.0 : 250ns, 100ns */
	{ NS(300), ACKW(3), SMPL(4) },	/*  3.3 : 300ns, 150ns */
	{ NS(350), ACKW(3), SMPL(4) },	/*  2.8 : 350ns, 150ns */
	{ NS(400), ACKW(4), SMPL(4) },	/*  2.5 : 400ns, 200ns */
	{ NS(450), ACKW(4), SMPL(4) },	/*  2.2 : 450ns, 200ns */
	{ NS(500), ACKW(4), SMPL(4) },	/*  2.0 : 500ns, 200ns */
	{ NS(550), ACKW(4), SMPL(4) },	/*  1.82: 550ns, 200ns */
	{ NS(600), ACKW(4), SMPL(4) },	/*  1.67: 600ns, 200ns */
	{ NS(650), ACKW(4), SMPL(4) },	/*  1.54: 650ns, 200ns */
	{ NS(700), ACKW(4), SMPL(4) },	/*  1.43: 700ns, 200ns */
	{ NS(750), ACKW(4), SMPL(4) },	/*  1.33: 750ns, 200ns */
	{ NS(800), ACKW(4), SMPL(4) }	/*  1.25: 800ns, 200ns */
};

/* 33.3MHz (30ns) */
static const struct njsc32_sync_param njsc32_synct_pci[NJSC32_NSYNCT] = {
	{ 0, 0, 0 },			/* dummy for async */
	{ NS( 60), ACKW(1), 0       },	/* 16.6 :  60ns,  30ns */
	{ NS( 90), ACKW(1), SMPL(1) },	/* 11.1 :  90ns,  30ns */
	{ NS(120), ACKW(2), SMPL(2) },	/*  8.3 : 120ns,  60ns */
	{ NS(150), ACKW(2), SMPL(2) },	/*  6.7 : 150ns,  60ns */
	{ NS(180), ACKW(3), SMPL(2) },	/*  5.6 : 180ns,  90ns */
	{ NS(210), ACKW(3), SMPL(4) },	/*  4.8 : 210ns,  90ns */
	{ NS(240), ACKW(4), SMPL(4) },	/*  4.2 : 240ns, 120ns */
	{ NS(270), ACKW(4), SMPL(4) },	/*  3.7 : 270ns, 120ns */
	{ NS(300), ACKW(4), SMPL(4) },	/*  3.3 : 300ns, 120ns */
	{ NS(330), ACKW(4), SMPL(4) },	/*  3.0 : 330ns, 120ns */
	{ NS(360), ACKW(4), SMPL(4) },	/*  2.8 : 360ns, 120ns */
	{ NS(390), ACKW(4), SMPL(4) },	/*  2.6 : 390ns, 120ns */
	{ NS(420), ACKW(4), SMPL(4) },	/*  2.4 : 420ns, 120ns */
	{ NS(450), ACKW(4), SMPL(4) },	/*  2.2 : 450ns, 120ns */
	{ NS(480), ACKW(4), SMPL(4) }	/*  2.1 : 480ns, 120ns */
};
#endif	/* NJSC32_SUPPORT_OTHER_CLOCKS */

#undef NS
#undef ACKW
#undef SMPL

/* initialize device */
static void
njsc32_init(struct njsc32_softc *sc, int nosleep)
{
	u_int16_t intstat;
	int i;

	/* block all interrupts */
	njsc32_write_2(sc, NJSC32_REG_IRQ, NJSC32_IRQ_MASK_ALL);

	/* clear transfer */
	njsc32_write_2(sc, NJSC32_REG_TRANSFER, 0);
	njsc32_write_4(sc, NJSC32_REG_BM_CNT, 0);

	/* make sure interrupts are cleared */
	for (i = 0; ((intstat = njsc32_read_2(sc, NJSC32_REG_IRQ))
	    & NJSC32_IRQ_INTR_PENDING) && i < 5 /* just not forever */; i++) {
		DPRINTF(("%s: njsc32_init: intr pending: %#x\n",
		    device_xname(sc->sc_dev), intstat));
	}

	/* FIFO threshold */
	njsc32_ireg_write_1(sc, NJSC32_IREG_FIFO_THRESHOLD_FULL,
	    NJSC32_FIFO_FULL_BUSMASTER);
	njsc32_ireg_write_1(sc, NJSC32_IREG_FIFO_THRESHOLD_EMPTY,
	    NJSC32_FIFO_EMPTY_BUSMASTER);

	/* clock source */
	njsc32_ireg_write_1(sc, NJSC32_IREG_CLOCK, sc->sc_clk);

	/* memory read multiple */
	njsc32_ireg_write_1(sc, NJSC32_IREG_BM,
	    NJSC32_BM_MEMRD_CMD1 | NJSC32_BM_SGT_AUTO_PARA_MEMRD_CMD);

	/* clear parity error and enable parity detection */
	njsc32_write_1(sc, NJSC32_REG_PARITY_CONTROL,
	    NJSC32_PARITYCTL_CHECK_ENABLE | NJSC32_PARITYCTL_CLEAR_ERROR);

	/* misc configuration */
	njsc32_ireg_write_2(sc, NJSC32_IREG_MISC,
	    NJSC32_MISC_SCSI_DIRECTION_DETECTOR_SELECT |
	    NJSC32_MISC_DELAYED_BMSTART |
	    NJSC32_MISC_MASTER_TERMINATION_SELECT |
	    NJSC32_MISC_BMREQ_NEGATE_TIMING_SEL |
	    NJSC32_MISC_AUTOSEL_TIMING_SEL |
	    NJSC32_MISC_BMSTOP_CHANGE2_NONDATA_PHASE);

	/*
	 * Check for termination power (32Bi and some versions of 32UDE).
	 */
	if (!nosleep || cold) {
		DPRINTF(("%s: njsc32_init: checking TERMPWR\n",
		    device_xname(sc->sc_dev)));

		/* First, turn termination power off */
		njsc32_ireg_write_1(sc, NJSC32_IREG_TERM_PWR, 0);

		/* give 0.5s to settle */
		if (nosleep)
			delay(500000);
		else
			tsleep(sc, PWAIT, "njs_t1", hz / 2);
	}

	/* supply termination power if not supplied by other devices */
	if ((njsc32_ireg_read_1(sc, NJSC32_IREG_TERM_PWR) &
	    NJSC32_TERMPWR_SENSE) == 0) {
		/* termination power is not present on the bus */
		if (sc->sc_flags & NJSC32_CANNOT_SUPPLY_TERMPWR) {
			/*
			 * CardBus device must not supply termination power
			 * to avoid excessive power consumption.
			 */
			printf("%s: no termination power present\n",
			    device_xname(sc->sc_dev));
		} else {
			/* supply termination power */
			njsc32_ireg_write_1(sc, NJSC32_IREG_TERM_PWR,
			    NJSC32_TERMPWR_BPWR);

			DPRINTF(("%s: supplying termination power\n",
			    device_xname(sc->sc_dev)));

			/* give 0.5s to settle */
			if (!nosleep)
				tsleep(sc, PWAIT, "njs_t2", hz / 2);
		}
	}

	/* stop timer */
	njsc32_write_2(sc, NJSC32_REG_TIMER, NJSC32_TIMER_STOP);
	njsc32_write_2(sc, NJSC32_REG_TIMER, NJSC32_TIMER_STOP);

	/* default transfer parameter */
	njsc32_write_1(sc, NJSC32_REG_SYNC, 0);
	njsc32_write_1(sc, NJSC32_REG_ACK_WIDTH, NJSC32_ACK_WIDTH_1CLK);
	njsc32_write_2(sc, NJSC32_REG_SEL_TIMEOUT,
	    NJSC32_SEL_TIMEOUT_TIME);

	/* select interrupt source */
	njsc32_ireg_write_2(sc, NJSC32_IREG_IRQ_SELECT,
	    NJSC32_IRQSEL_RESELECT |
	    NJSC32_IRQSEL_PHASE_CHANGE |
	    NJSC32_IRQSEL_SCSIRESET |
	    NJSC32_IRQSEL_TIMER |
	    NJSC32_IRQSEL_FIFO_THRESHOLD |
	    NJSC32_IRQSEL_TARGET_ABORT |
	    NJSC32_IRQSEL_MASTER_ABORT |
	/* XXX not yet
	    NJSC32_IRQSEL_SERR |
	    NJSC32_IRQSEL_PERR |
	    NJSC32_IRQSEL_BMCNTERR |
	*/
	    NJSC32_IRQSEL_AUTO_SCSI_SEQ);

	/* interrupts will be unblocked later after bus reset */

	/* turn LED off */
	njsc32_ireg_write_1(sc, NJSC32_IREG_EXT_PORT_DDR,
	    NJSC32_EXTPORT_LED_OFF);
	njsc32_ireg_write_1(sc, NJSC32_IREG_EXT_PORT,
	    NJSC32_EXTPORT_LED_OFF);

	/* reset SCSI bus so the targets become known state */
	njsc32_reset_bus(sc);
}

static int
njsc32_init_cmds(struct njsc32_softc *sc)
{
	struct njsc32_cmd *cmd;
	bus_addr_t dmaaddr;
	int i, error;

	/*
	 * allocate DMA area for command
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof(struct njsc32_dma_page), PAGE_SIZE, 0,
	    &sc->sc_cmdpg_seg, 1, &sc->sc_cmdpg_nsegs, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to allocate cmd page, error = %d\n",
		    error);
		return 0;
	}
	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->sc_cmdpg_seg,
	    sc->sc_cmdpg_nsegs, sizeof(struct njsc32_dma_page),
	    (void **)&sc->sc_cmdpg,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to map cmd page, error = %d\n",
		    error);
		goto fail1;
	}
	if ((error = bus_dmamap_create(sc->sc_dmat,
	    sizeof(struct njsc32_dma_page), 1,
	    sizeof(struct njsc32_dma_page), 0, BUS_DMA_NOWAIT,
	    &sc->sc_dmamap_cmdpg)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to create cmd DMA map, error = %d\n",
		    error);
		goto fail2;
	}
	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap_cmdpg,
	    sc->sc_cmdpg, sizeof(struct njsc32_dma_page),
	    NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to load cmd DMA map, error = %d\n",
		    error);
		goto fail3;
	}

	memset(sc->sc_cmdpg, 0, sizeof(struct njsc32_dma_page));
	dmaaddr = sc->sc_dmamap_cmdpg->dm_segs[0].ds_addr;

#ifdef NJSC32_AUTOPARAM
	sc->sc_ap_dma = dmaaddr + offsetof(struct njsc32_dma_page, dp_ap);
#endif

	for (i = 0; i < NJSC32_NUM_CMD; i++) {
		cmd = &sc->sc_cmds[i];
		cmd->c_sc = sc;
		cmd->c_sgt = sc->sc_cmdpg->dp_sg[i];
		cmd->c_sgt_dma = dmaaddr +
		    offsetof(struct njsc32_dma_page, dp_sg[i]);
		cmd->c_flags = 0;

		error = bus_dmamap_create(sc->sc_dmat,
		    NJSC32_MAX_XFER,		/* max total map size */
		    NJSC32_NUM_SG,		/* max number of segments */
		    NJSC32_SGT_MAXSEGLEN,	/* max size of a segment */
		    0,				/* boundary */
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &cmd->c_dmamap_xfer);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "only %d cmd descs available (error = %d)\n",
			    i, error);
			break;
		}
		TAILQ_INSERT_TAIL(&sc->sc_freecmd, cmd, c_q);
	}

	if (i > 0)
		return i;

	bus_dmamap_unload(sc->sc_dmat, sc->sc_dmamap_cmdpg);
fail3:	bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmamap_cmdpg);
fail2:	bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_cmdpg,
	    sizeof(struct njsc32_dma_page));
fail1:	bus_dmamem_free(sc->sc_dmat, &sc->sc_cmdpg_seg, sc->sc_cmdpg_nsegs);

	return 0;
}

static void
njsc32_target_async(struct njsc32_softc *sc, struct njsc32_target *target)
{

	target->t_sync =
	    NJSC32_SYNC_VAL(sc->sc_sync_max, NJSC32_SYNCOFFSET_ASYNC);
	target->t_ackwidth = NJSC32_ACK_WIDTH_1CLK;
	target->t_sample = 0;		/* disable */
	target->t_syncoffset = NJSC32_SYNCOFFSET_ASYNC;
	target->t_syncperiod = NJSC32_SYNCPERIOD_ASYNC;
}

static void
njsc32_init_targets(struct njsc32_softc *sc)
{
	int id, lun;
	struct njsc32_lu *lu;

	for (id = 0; id <= NJSC32_MAX_TARGET_ID; id++) {
		/* cancel negotiation status */
		sc->sc_targets[id].t_state = NJSC32_TARST_INIT;

		/* default to async mode */
		njsc32_target_async(sc, &sc->sc_targets[id]);

#ifdef NJSC32_DUALEDGE
		sc->sc_targets[id].t_xferctl = 0;
#endif

		sc->sc_targets[id].t_targetid =
		    (1 << id) | (1 << NJSC32_INITIATOR_ID);

		/* init logical units */
		for (lun = 0; lun < NJSC32_NLU; lun++) {
			lu = &sc->sc_targets[id].t_lus[lun];
			lu->lu_cmd = NULL;
			TAILQ_INIT(&lu->lu_q);
		}
	}
}

void
njsc32_attach(struct njsc32_softc *sc)
{
	const char *str;
#if 1	/* test */
	int reg;
	njsc32_model_t detected_model;
#endif

	/* init */
	TAILQ_INIT(&sc->sc_freecmd);
	TAILQ_INIT(&sc->sc_reqcmd);
	callout_init(&sc->sc_callout, 0);

#if 1	/* test */
	/*
	 * try to distinguish 32Bi and 32UDE
	 */
	/* try to set DualEdge bit (exists on 32UDE only) and read it back */
	njsc32_write_2(sc, NJSC32_REG_TRANSFER, NJSC32_XFR_DUALEDGE_ENABLE);
	if ((reg = njsc32_read_2(sc, NJSC32_REG_TRANSFER)) == 0xffff) {
		/* device was removed? */
		aprint_error_dev(sc->sc_dev, "attach failed\n");
		return;
	} else if (reg & NJSC32_XFR_DUALEDGE_ENABLE) {
		detected_model = NJSC32_MODEL_32UDE | NJSC32_FLAG_DUALEDGE;
	} else {
		detected_model = NJSC32_MODEL_32BI;
	}
	njsc32_write_2(sc, NJSC32_REG_TRANSFER, 0);	/* restore */

#if 1/*def DIAGNOSTIC*/
	/* compare what is configured with what is detected */
	if ((sc->sc_model & NJSC32_MODEL_MASK) !=
	    (detected_model & NJSC32_MODEL_MASK)) {
		/*
		 * Please report this error if it happens.
		 */
		aprint_error_dev(sc->sc_dev, "model mismatch: %#x vs %#x\n",
		    sc->sc_model, detected_model);
		return;
	}
#endif
#endif

	/* check model */
	switch (sc->sc_model & NJSC32_MODEL_MASK) {
	case NJSC32_MODEL_32BI:
		str = "Bi";
		/* 32Bi doesn't support DualEdge transfer */
		KASSERT((sc->sc_model & NJSC32_FLAG_DUALEDGE) == 0);
		break;
	case NJSC32_MODEL_32UDE:
		str = "UDE";
		break;
	default:
		aprint_error_dev(sc->sc_dev, "unknown model!\n");
		return;
	}
	aprint_normal_dev(sc->sc_dev, "NJSC-32%s", str);

	switch (sc->sc_clk) {
	default:
#ifdef DIAGNOSTIC
		panic("njsc32_attach: unknown clk %d", sc->sc_clk);
#endif
	case NJSC32_CLOCK_DIV_4:
		sc->sc_synct = njsc32_synct_40M;
		str = "40MHz";
		break;
#ifdef NJSC32_SUPPORT_OTHER_CLOCKS
	case NJSC32_CLOCK_DIV_2:
		sc->sc_synct = njsc32_synct_20M;
		str = "20MHz";
		break;
	case NJSC32_CLOCK_PCICLK:
		sc->sc_synct = njsc32_synct_pci;
		str = "PCI";
		break;
#endif
	}
	aprint_normal(", G/A rev %#x, clk %s%s\n",
	    NJSC32_INDEX_GAREV(njsc32_read_2(sc, NJSC32_REG_INDEX)), str,
	    (sc->sc_model & NJSC32_FLAG_DUALEDGE) ?
#ifdef NJSC32_DUALEDGE
		", DualEdge"
#else
		", DualEdge (no driver support)"
#endif
	    : "");

	/* allocate DMA resource */
	if ((sc->sc_ncmd = njsc32_init_cmds(sc)) == 0) {
		aprint_error_dev(sc->sc_dev, "no usable DMA map\n");
		return;
	}
	sc->sc_flags |= NJSC32_CMDPG_MAPPED;

	sc->sc_curcmd = NULL;
	sc->sc_nusedcmds = 0;

	sc->sc_sync_max = 1;	/* XXX look up EEPROM configuration? */

	/* initialize hardware and target structure */
	njsc32_init(sc, cold);

	/* setup adapter */
	sc->sc_adapter.adapt_dev = sc->sc_dev;
	sc->sc_adapter.adapt_nchannels = 1;
	sc->sc_adapter.adapt_request = njsc32_scsipi_request;
	sc->sc_adapter.adapt_minphys = njsc32_scsipi_minphys;
	sc->sc_adapter.adapt_ioctl = njsc32_scsipi_ioctl;

	sc->sc_adapter.adapt_max_periph = sc->sc_adapter.adapt_openings =
	    sc->sc_ncmd;

	/* setup channel */
	sc->sc_channel.chan_adapter = &sc->sc_adapter;
	sc->sc_channel.chan_bustype = &scsi_bustype;
	sc->sc_channel.chan_channel = 0;
	sc->sc_channel.chan_ntargets = NJSC32_NTARGET;
	sc->sc_channel.chan_nluns = NJSC32_NLU;
	sc->sc_channel.chan_id = NJSC32_INITIATOR_ID;

	sc->sc_scsi = config_found(sc->sc_dev, &sc->sc_channel, scsiprint);
}

int
njsc32_detach(struct njsc32_softc *sc, int flags)
{
	int rv = 0;
	int i, s;
	struct njsc32_cmd *cmd;

	callout_stop(&sc->sc_callout);

	s = splbio();

	/* clear running/disconnected commands */
	njsc32_clear_cmds(sc, XS_DRIVER_STUFFUP);

	sc->sc_stat = NJSC32_STAT_DETACH;

	/* clear pending commands */
	while ((cmd = TAILQ_FIRST(&sc->sc_reqcmd)) != NULL) {
		TAILQ_REMOVE(&sc->sc_reqcmd, cmd, c_q);
		njsc32_end_cmd(sc, cmd, XS_RESET);
	}

	if (sc->sc_scsi != NULL)
		rv = config_detach(sc->sc_scsi, flags);

	splx(s);

	/* free DMA resource */
	if (sc->sc_flags & NJSC32_CMDPG_MAPPED) {
		for (i = 0; i < sc->sc_ncmd; i++) {
			cmd = &sc->sc_cmds[i];
			if (cmd->c_flags & NJSC32_CMD_DMA_MAPPED)
				bus_dmamap_unload(sc->sc_dmat,
				    cmd->c_dmamap_xfer);
			bus_dmamap_destroy(sc->sc_dmat, cmd->c_dmamap_xfer);
		}

		bus_dmamap_unload(sc->sc_dmat, sc->sc_dmamap_cmdpg);
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmamap_cmdpg);
		bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_cmdpg,
		    sizeof(struct njsc32_dma_page));
		bus_dmamem_free(sc->sc_dmat, &sc->sc_cmdpg_seg,
		    sc->sc_cmdpg_nsegs);
	}

	return rv;
}

static inline void
njsc32_cmd_init(struct njsc32_cmd *cmd)
{

	cmd->c_flags = 0;

	/* scatter/gather table */
	cmd->c_sgtdmaaddr = NJSC32_CMD_DMAADDR_SGT(cmd, 0);
	cmd->c_sgoffset = 0;
	cmd->c_sgfixcnt = 0;

	/* data pointer */
	cmd->c_dp_cur = cmd->c_dp_saved = cmd->c_dp_max = 0;
}

static inline void
njsc32_init_msgout(struct njsc32_softc *sc)
{

	sc->sc_msgoutlen = 0;
	sc->sc_msgoutidx = 0;
}

static void
njsc32_add_msgout(struct njsc32_softc *sc, int byte)
{

	if (sc->sc_msgoutlen >= NJSC32_MSGOUT_LEN) {
		printf("njsc32_add_msgout: too many\n");
		return;
	}
	sc->sc_msgout[sc->sc_msgoutlen++] = byte;
}

static u_int32_t
njsc32_get_auto_msgout(struct njsc32_softc *sc)
{
	u_int32_t val;
	u_int8_t *p;

	val = 0;
	p = sc->sc_msgout;
	switch (sc->sc_msgoutlen) {
		/* 31-24 23-16 15-8 7 ... 1 0 */
	case 3:	/* MSG3  MSG2  MSG1 V --- cnt */
		val |= *p++ << NJSC32_MSGOUT_MSG1_SHIFT;
		/* FALLTHROUGH */

	case 2:	/* MSG2  MSG1  ---  V --- cnt */
		val |= *p++ << NJSC32_MSGOUT_MSG2_SHIFT;
		/* FALLTHROUGH */

	case 1:	/* MSG1  ---   ---  V --- cnt */
		val |= *p++ << NJSC32_MSGOUT_MSG3_SHIFT;
		val |= NJSC32_MSGOUT_VALID | sc->sc_msgoutlen;
		break;

	default:
		break;
	}
	return val;
}

#ifdef NJSC32_DUALEDGE
/* add Wide Data Transfer Request to the next Message Out */
static void
njsc32_msgout_wdtr(struct njsc32_softc *sc, int width)
{

	njsc32_add_msgout(sc, MSG_EXTENDED);
	njsc32_add_msgout(sc, MSG_EXT_WDTR_LEN);
	njsc32_add_msgout(sc, MSG_EXT_WDTR);
	njsc32_add_msgout(sc, width);
}
#endif

/* add Synchronous Data Transfer Request to the next Message Out */
static void
njsc32_msgout_sdtr(struct njsc32_softc *sc, int period, int offset)
{

	njsc32_add_msgout(sc, MSG_EXTENDED);
	njsc32_add_msgout(sc, MSG_EXT_SDTR_LEN);
	njsc32_add_msgout(sc, MSG_EXT_SDTR);
	njsc32_add_msgout(sc, period);
	njsc32_add_msgout(sc, offset);
}

static void
njsc32_negotiate_xfer(struct njsc32_softc *sc, struct njsc32_target *target)
{

	/* initial negotiation state */
	if (target->t_state == NJSC32_TARST_INIT) {
#ifdef NJSC32_DUALEDGE
		if (target->t_flags & NJSC32_TARF_DE)
			target->t_state = NJSC32_TARST_DE;
		else
#endif
		if (target->t_flags & NJSC32_TARF_SYNC)
			target->t_state = NJSC32_TARST_SDTR;
		else
			target->t_state = NJSC32_TARST_DONE;
	}

	switch (target->t_state) {
	default:
	case NJSC32_TARST_INIT:
#ifdef DIAGNOSTIC
		panic("njsc32_negotiate_xfer");
		/* NOTREACHED */
#endif
		/* FALLTHROUGH */
	case NJSC32_TARST_DONE:
		/* no more work */
		break;

#ifdef NJSC32_DUALEDGE
	case NJSC32_TARST_DE:
		njsc32_msgout_wdtr(sc, 0xde /* XXX? */);
		break;

	case NJSC32_TARST_WDTR:
		njsc32_msgout_wdtr(sc, MSG_EXT_WDTR_BUS_8_BIT);
		break;
#endif

	case NJSC32_TARST_SDTR:
		njsc32_msgout_sdtr(sc, sc->sc_synct[sc->sc_sync_max].sp_period,
		    NJSC32_SYNCOFFSET_MAX);
		break;

	case NJSC32_TARST_ASYNC:
		njsc32_msgout_sdtr(sc, NJSC32_SYNCPERIOD_ASYNC,
		    NJSC32_SYNCOFFSET_ASYNC);
		break;
	}
}

/* turn LED on */
static inline void
njsc32_led_on(struct njsc32_softc *sc)
{

	njsc32_ireg_write_1(sc, NJSC32_IREG_EXT_PORT, NJSC32_EXTPORT_LED_ON);
}

/* turn LED off */
static inline void
njsc32_led_off(struct njsc32_softc *sc)
{

	njsc32_ireg_write_1(sc, NJSC32_IREG_EXT_PORT, NJSC32_EXTPORT_LED_OFF);
}

static void
njsc32_arbitration_failed(struct njsc32_softc *sc)
{
	struct njsc32_cmd *cmd;

	if ((cmd = sc->sc_curcmd) == NULL || sc->sc_stat != NJSC32_STAT_ARBIT)
		return;

	if ((cmd->c_xs->xs_control & XS_CTL_POLL) == 0)
		callout_stop(&cmd->c_xs->xs_callout);

	sc->sc_stat = NJSC32_STAT_IDLE;
	sc->sc_curcmd = NULL;

	/* the command is no longer active */
	if (--sc->sc_nusedcmds == 0)
		njsc32_led_off(sc);
}

static inline void
njsc32_cmd_load(struct njsc32_softc *sc, struct njsc32_cmd *cmd)
{
	struct njsc32_target *target;
	struct scsipi_xfer *xs;
	int i, control, lun;
	u_int32_t msgoutreg;
#ifdef NJSC32_AUTOPARAM
	struct njsc32_autoparam *ap;
#endif

	xs = cmd->c_xs;
#ifdef NJSC32_AUTOPARAM
	ap = &sc->sc_cmdpg->dp_ap;
#else
	/* reset CDB pointer */
	njsc32_write_2(sc, NJSC32_REG_COMMAND_CONTROL, NJSC32_CMD_CLEAR_CDB_FIFO_PTR);
#endif

	/* CDB */
	TPRINTC(cmd, ("njsc32_cmd_load: CDB"));
	for (i = 0; i < xs->cmdlen; i++) {
#ifdef NJSC32_AUTOPARAM
		ap->ap_cdb[i].cdb_data = ((u_int8_t *)xs->cmd)[i];
#else
		njsc32_write_1(sc, NJSC32_REG_COMMAND_DATA,
		    ((u_int8_t *)xs->cmd)[i]);
#endif
		TPRINTF((" %02x", ((u_int8_t *)cmd->c_xs->cmd)[i]));
	}
#ifdef NJSC32_AUTOPARAM	/* XXX needed? */
	for ( ; i < NJSC32_AUTOPARAM_CDBLEN; i++)
		ap->ap_cdb[i].cdb_data = 0;
#endif

	control = xs->xs_control;

	/*
	 * Message Out
	 */
	njsc32_init_msgout(sc);

	/* Identify */
	lun = xs->xs_periph->periph_lun;
	njsc32_add_msgout(sc, (control & XS_CTL_REQSENSE) ?
	    MSG_IDENTIFY(lun, 0) : MSG_IDENTIFY(lun, 1));

	/* tagged queueing */
	if (control & XS_CTL_TAGMASK) {
		njsc32_add_msgout(sc, xs->xs_tag_type);
		njsc32_add_msgout(sc, xs->xs_tag_id);
		TPRINTF((" (tag %#x %#x)\n", xs->xs_tag_type, xs->xs_tag_id));
	}
	TPRINTF(("\n"));

	target = cmd->c_target;

	/* transfer negotiation */
	if (control & XS_CTL_REQSENSE)
		target->t_state = NJSC32_TARST_INIT;
	njsc32_negotiate_xfer(sc, target);

	msgoutreg = njsc32_get_auto_msgout(sc);

#ifdef NJSC32_AUTOPARAM
	ap->ap_msgout = htole32(msgoutreg);

	ap->ap_sync	= target->t_sync;
	ap->ap_ackwidth	= target->t_ackwidth;
	ap->ap_targetid	= target->t_targetid;
	ap->ap_sample	= target->t_sample;

	ap->ap_cmdctl = htole16(NJSC32_CMD_CLEAR_CDB_FIFO_PTR |
	    NJSC32_CMD_AUTO_COMMAND_PHASE |
	    NJSC32_CMD_AUTO_SCSI_START | NJSC32_CMD_AUTO_ATN |
	    NJSC32_CMD_AUTO_MSGIN_00_04 | NJSC32_CMD_AUTO_MSGIN_02);
#ifdef NJSC32_DUALEDGE
	ap->ap_xferctl = htole16(cmd->c_xferctl | target->t_xferctl);
#else
	ap->ap_xferctl = htole16(cmd->c_xferctl);
#endif
	ap->ap_sgtdmaaddr = htole32(cmd->c_sgtdmaaddr);

	/* sync njsc32_autoparam */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_cmdpg,
	    offsetof(struct njsc32_dma_page, dp_ap),	/* offset */
	    sizeof(struct njsc32_autoparam),
	    BUS_DMASYNC_PREWRITE);

	/* autoparam DMA address */
	njsc32_write_4(sc, NJSC32_REG_SGT_ADR, sc->sc_ap_dma);

	/* start command (autoparam) */
	njsc32_write_2(sc, NJSC32_REG_COMMAND_CONTROL,
	    NJSC32_CMD_CLEAR_CDB_FIFO_PTR | NJSC32_CMD_AUTO_PARAMETER);

#else	/* not NJSC32_AUTOPARAM */

	njsc32_write_4(sc, NJSC32_REG_SCSI_MSG_OUT, msgoutreg);

	/* load parameters */
	njsc32_write_1(sc, NJSC32_REG_TARGET_ID, target->t_targetid);
	njsc32_write_1(sc, NJSC32_REG_SYNC, target->t_sync);
	njsc32_write_1(sc, NJSC32_REG_ACK_WIDTH, target->t_ackwidth);
	njsc32_write_1(sc, NJSC32_REG_SREQ_SAMPLING, target->t_sample);
	njsc32_write_4(sc, NJSC32_REG_SGT_ADR, cmd->c_sgtdmaaddr);
#ifdef NJSC32_DUALEDGE
	njsc32_write_2(sc, NJSC32_REG_TRANSFER,
	    cmd->c_xferctl | target->t_xferctl);
#else
	njsc32_write_2(sc, NJSC32_REG_TRANSFER, cmd->c_xferctl);
#endif
	/* start AutoSCSI */
	njsc32_write_2(sc, NJSC32_REG_COMMAND_CONTROL,
	    NJSC32_CMD_CLEAR_CDB_FIFO_PTR | NJSC32_CMD_AUTO_COMMAND_PHASE |
	    NJSC32_CMD_AUTO_SCSI_START | NJSC32_CMD_AUTO_ATN |
	    NJSC32_CMD_AUTO_MSGIN_00_04 | NJSC32_CMD_AUTO_MSGIN_02);
#endif	/* not NJSC32_AUTOPARAM */
}

/* Note: must be called at splbio() */
static void
njsc32_start(struct njsc32_softc *sc)
{
	struct njsc32_cmd *cmd;

	/* get a command to issue */
	TAILQ_FOREACH(cmd, &sc->sc_reqcmd, c_q) {
		if (cmd->c_lu->lu_cmd == NULL &&
		    ((cmd->c_flags & NJSC32_CMD_TAGGED) ||
		     TAILQ_EMPTY(&cmd->c_lu->lu_q)))
			break;	/* OK, the logical unit is free */
	}
	if (!cmd)
		goto out;	/* no work to do */

	/* request will always fail if not in bus free phase */
	if (njsc32_read_1(sc, NJSC32_REG_SCSI_BUS_MONITOR) !=
	    NJSC32_BUSMON_BUSFREE)
		goto busy;

	/* clear parity error and enable parity detection */
	njsc32_write_1(sc, NJSC32_REG_PARITY_CONTROL,
	    NJSC32_PARITYCTL_CHECK_ENABLE | NJSC32_PARITYCTL_CLEAR_ERROR);

	njsc32_cmd_load(sc, cmd);

	if (sc->sc_nusedcmds++ == 0)
		njsc32_led_on(sc);

	sc->sc_curcmd = cmd;
	sc->sc_stat = NJSC32_STAT_ARBIT;

	if ((cmd->c_xs->xs_control & XS_CTL_POLL) == 0) {
		callout_reset(&cmd->c_xs->xs_callout,
		    mstohz(cmd->c_xs->timeout),
		    njsc32_cmdtimeout, cmd);
	}

	return;

busy:	/* XXX retry counter */
	TPRINTF(("%s: njsc32_start: busy\n", device_xname(sc->sc_dev)));
	njsc32_write_2(sc, NJSC32_REG_TIMER, NJSC32_ARBITRATION_RETRY_TIME);
out:	njsc32_write_2(sc, NJSC32_REG_TRANSFER, 0);
}

static void
njsc32_run_xfer(struct njsc32_softc *sc, struct scsipi_xfer *xs)
{
	struct scsipi_periph *periph;
	int control;
	int lun;
	struct njsc32_cmd *cmd;
	int s, i, error;

	periph = xs->xs_periph;
	KASSERT((unsigned)periph->periph_target <= NJSC32_MAX_TARGET_ID);

	control = xs->xs_control;
	lun = periph->periph_lun;

	/*
	 * get a free cmd
	 * (scsipi layer knows the number of cmds, so this shall never fail)
	 */
	s = splbio();
	cmd = TAILQ_FIRST(&sc->sc_freecmd);
	KASSERT(cmd);
	TAILQ_REMOVE(&sc->sc_freecmd, cmd, c_q);
	splx(s);

	/*
	 * build a request
	 */
	njsc32_cmd_init(cmd);
	cmd->c_xs = xs;
	cmd->c_target = &sc->sc_targets[periph->periph_target];
	cmd->c_lu = &cmd->c_target->t_lus[lun];

	/* tagged queueing */
	if (control & XS_CTL_TAGMASK) {
		cmd->c_flags |= NJSC32_CMD_TAGGED;
		if (control & XS_CTL_HEAD_TAG)
			cmd->c_flags |= NJSC32_CMD_TAGGED_HEAD;
	}

	/* map DMA buffer */
	cmd->c_datacnt = xs->datalen;
	if (xs->datalen) {
		/* Is XS_CTL_DATA_UIO ever used anywhere? */
		KASSERT((control & XS_CTL_DATA_UIO) == 0);

		error = bus_dmamap_load(sc->sc_dmat, cmd->c_dmamap_xfer,
		    xs->data, xs->datalen, NULL,
		    ((control & XS_CTL_NOSLEEP) ?
			BUS_DMA_NOWAIT : BUS_DMA_WAITOK) |
		    BUS_DMA_STREAMING |
		    ((control & XS_CTL_DATA_IN) ?
			BUS_DMA_READ : BUS_DMA_WRITE));

		switch (error) {
		case 0:
			break;
		case ENOMEM:
		case EAGAIN:
			xs->error = XS_RESOURCE_SHORTAGE;
			goto map_failed;
		default:
			xs->error = XS_DRIVER_STUFFUP;
		map_failed:
			printf("%s: njsc32_run_xfer: map failed, error %d\n",
			    device_xname(sc->sc_dev), error);
			/* put it back to free command list */
			s = splbio();
			TAILQ_INSERT_HEAD(&sc->sc_freecmd, cmd, c_q);
			splx(s);
			/* abort this transfer */
			scsipi_done(xs);
			return;
		}

		bus_dmamap_sync(sc->sc_dmat, cmd->c_dmamap_xfer,
		    0, cmd->c_dmamap_xfer->dm_mapsize,
		    (control & XS_CTL_DATA_IN) ?
			BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

		for (i = 0; i < cmd->c_dmamap_xfer->dm_nsegs; i++) {
			cmd->c_sgt[i].sg_addr =
			    htole32(cmd->c_dmamap_xfer->dm_segs[i].ds_addr);
			cmd->c_sgt[i].sg_len =
			    htole32(cmd->c_dmamap_xfer->dm_segs[i].ds_len);
		}
		/* end mark */
		cmd->c_sgt[i - 1].sg_len |= htole32(NJSC32_SGT_ENDMARK);

		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_cmdpg,
		    (char *)cmd->c_sgt - (char *)sc->sc_cmdpg, /* offset */
		    NJSC32_SIZE_SGT,
		    BUS_DMASYNC_PREWRITE);

		cmd->c_flags |= NJSC32_CMD_DMA_MAPPED;

		/* enable transfer */
		cmd->c_xferctl =
		    NJSC32_XFR_TRANSFER_GO | NJSC32_XFR_BM_START |
		    NJSC32_XFR_ALL_COUNT_CLR;

		/* XXX How can we specify the DMA direction? */

#if 0	/* faster write mode? (doesn't work) */
		if ((control & XS_CTL_DATA_IN) == 0)
			cmd->c_xferctl |= NJSC32_XFR_ADVANCED_BM_WRITE;
#endif
	} else {
		/* no data transfer */
		cmd->c_xferctl = 0;
	}

	/* queue request */
	s = splbio();
	TAILQ_INSERT_TAIL(&sc->sc_reqcmd, cmd, c_q);

	/* start the controller if idle */
	if (sc->sc_stat == NJSC32_STAT_IDLE)
		njsc32_start(sc);

	splx(s);

	if (control & XS_CTL_POLL) {
		/* wait for completion */
		/* XXX should handle timeout? */
		while ((xs->xs_status & XS_STS_DONE) == 0) {
			delay(1000);
			njsc32_intr(sc);
		}
	}
}

static void
njsc32_end_cmd(struct njsc32_softc *sc, struct njsc32_cmd *cmd,
    scsipi_xfer_result_t result)
{
	struct scsipi_xfer *xs;
	int s;
#ifdef DIAGNOSTIC
	struct njsc32_cmd *c;
#endif

	KASSERT(cmd);

#ifdef DIAGNOSTIC
	s = splbio();
	TAILQ_FOREACH(c, &sc->sc_freecmd, c_q) {
		if (cmd == c)
			panic("njsc32_end_cmd: already in free list");
	}
	splx(s);
#endif
	xs = cmd->c_xs;

	if (cmd->c_flags & NJSC32_CMD_DMA_MAPPED) {
		if (cmd->c_datacnt) {
			bus_dmamap_sync(sc->sc_dmat, cmd->c_dmamap_xfer,
			    0, cmd->c_dmamap_xfer->dm_mapsize,
			    (xs->xs_control & XS_CTL_DATA_IN) ?
				BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_cmdpg,
			    (char *)cmd->c_sgt - (char *)sc->sc_cmdpg,
			    NJSC32_SIZE_SGT, BUS_DMASYNC_POSTWRITE);
		}

		bus_dmamap_unload(sc->sc_dmat, cmd->c_dmamap_xfer);
		cmd->c_flags &= ~NJSC32_CMD_DMA_MAPPED;
	}

	s = splbio();
	if ((xs->xs_control & XS_CTL_POLL) == 0)
		callout_stop(&xs->xs_callout);

	TAILQ_INSERT_HEAD(&sc->sc_freecmd, cmd, c_q);
	splx(s);

	xs->error = result;
	scsipi_done(xs);

	if (--sc->sc_nusedcmds == 0)
		njsc32_led_off(sc);
}

/*
 * request from scsipi layer
 */
static void
njsc32_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
    void *arg)
{
	struct njsc32_softc *sc;
	struct scsipi_xfer_mode *xm;
	struct njsc32_target *target;

	sc = device_private(chan->chan_adapter->adapt_dev);

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		njsc32_run_xfer(sc, arg);
		break;

	case ADAPTER_REQ_GROW_RESOURCES:
		/* not supported */
		break;

	case ADAPTER_REQ_SET_XFER_MODE:
		xm = arg;
		target = &sc->sc_targets[xm->xm_target];

		target->t_flags = 0;
		if (xm->xm_mode & PERIPH_CAP_TQING)
			target->t_flags |= NJSC32_TARF_TAG;
		if (xm->xm_mode & PERIPH_CAP_SYNC) {
			target->t_flags |= NJSC32_TARF_SYNC;
#ifdef NJSC32_DUALEDGE
			if (sc->sc_model & NJSC32_FLAG_DUALEDGE)
				target->t_flags |= NJSC32_TARF_DE;
#endif
		}
#ifdef NJSC32_DUALEDGE
		target->t_xferctl = 0;
#endif
		target->t_state = NJSC32_TARST_INIT;
		njsc32_target_async(sc, target);

		break;
	default:
		break;
	}
}

static void
njsc32_scsipi_minphys(struct buf *bp)
{

	if (bp->b_bcount > NJSC32_MAX_XFER)
		bp->b_bcount = NJSC32_MAX_XFER;
	minphys(bp);
}

/*
 * On some versions of 32UDE (probably the earlier ones), the controller
 * detects continuous bus reset when the termination power is absent.
 * Make sure the system won't hang on such situation.
 */
static void
njsc32_wait_reset_release(void *arg)
{
	struct njsc32_softc *sc = arg;
	struct njsc32_cmd *cmd;

	/* clear pending commands */
	while ((cmd = TAILQ_FIRST(&sc->sc_reqcmd)) != NULL) {
		TAILQ_REMOVE(&sc->sc_reqcmd, cmd, c_q);
		njsc32_end_cmd(sc, cmd, XS_RESET);
	}

	/* If Bus Reset is not released yet, schedule recheck. */
	if (njsc32_read_2(sc, NJSC32_REG_IRQ) & NJSC32_IRQ_SCSIRESET) {
		switch (sc->sc_stat) {
		case NJSC32_STAT_RESET:
			sc->sc_stat = NJSC32_STAT_RESET1;
			break;
		case NJSC32_STAT_RESET1:
			/* print message if Bus Reset is detected twice */
			sc->sc_stat = NJSC32_STAT_RESET2;
			printf("%s: detected excessive bus reset "
			    "--- missing termination power?\n",
			    device_xname(sc->sc_dev));
			break;
		default:
			break;
		}
		callout_reset(&sc->sc_callout,
		    hz * 2	/* poll every 2s */,
		    njsc32_wait_reset_release, sc);
		return;
	}

	if (sc->sc_stat == NJSC32_STAT_RESET2)
		printf("%s: bus reset is released\n", device_xname(sc->sc_dev));

	/* unblock interrupts */
	njsc32_write_2(sc, NJSC32_REG_IRQ, 0);

	sc->sc_stat = NJSC32_STAT_IDLE;
}

static void
njsc32_reset_bus(struct njsc32_softc *sc)
{
	int s;

	DPRINTF(("%s: njsc32_reset_bus:\n", device_xname(sc->sc_dev)));

	/* block interrupts */
	njsc32_write_2(sc, NJSC32_REG_IRQ, NJSC32_IRQ_MASK_ALL);

	sc->sc_stat = NJSC32_STAT_RESET;

	/* hold SCSI bus reset */
	njsc32_write_1(sc, NJSC32_REG_SCSI_BUS_CONTROL, NJSC32_SBCTL_RST);
	delay(NJSC32_RESET_HOLD_TIME);

	/* clear transfer */
	njsc32_clear_cmds(sc, XS_RESET);

	/* initialize target structure */
	njsc32_init_targets(sc);

	/* XXXSMP scsipi */
	KERNEL_LOCK(1, curlwp);
	s = splbio();
	scsipi_async_event(&sc->sc_channel, ASYNC_EVENT_RESET, NULL);
	splx(s);
	/* XXXSMP scsipi */
	KERNEL_UNLOCK_ONE(curlwp);

	/* release SCSI bus reset */
	njsc32_write_1(sc, NJSC32_REG_SCSI_BUS_CONTROL, 0);

	njsc32_wait_reset_release(sc);
}

/*
 * clear running/disconnected commands
 */
static void
njsc32_clear_cmds(struct njsc32_softc *sc, scsipi_xfer_result_t cmdresult)
{
	struct njsc32_cmd *cmd;
	int id, lun;
	struct njsc32_lu *lu;

	njsc32_arbitration_failed(sc);

	/* clear current transfer */
	if ((cmd = sc->sc_curcmd) != NULL) {
		sc->sc_curcmd = NULL;
		njsc32_end_cmd(sc, cmd, cmdresult);
	}

	/* clear disconnected transfers */
	for (id = 0; id <= NJSC32_MAX_TARGET_ID; id++) {
		for (lun = 0; lun < NJSC32_NLU; lun++) {
			lu = &sc->sc_targets[id].t_lus[lun];

			if ((cmd = lu->lu_cmd) != NULL) {
				lu->lu_cmd = NULL;
				njsc32_end_cmd(sc, cmd, cmdresult);
			}
			while ((cmd = TAILQ_FIRST(&lu->lu_q)) != NULL) {
				TAILQ_REMOVE(&lu->lu_q, cmd, c_q);
				njsc32_end_cmd(sc, cmd, cmdresult);
			}
		}
	}
}

static int
njsc32_scsipi_ioctl(struct scsipi_channel *chan, u_long cmd,
    void *addr, int flag, struct proc *p)
{
	struct njsc32_softc *sc;

	sc = device_private(chan->chan_adapter->adapt_dev);

	switch (cmd) {
	case SCBUSIORESET:
		njsc32_init(sc, 0);
		return 0;
	default:
		break;
	}

	return ENOTTY;
}

/*
 * set current data pointer
 */
static inline void
njsc32_set_cur_ptr(struct njsc32_cmd *cmd, u_int32_t pos)
{

	/* new current data pointer */
	cmd->c_dp_cur = pos;

	/* update number of bytes transferred */
	if (pos > cmd->c_dp_max)
		cmd->c_dp_max = pos;
}

/*
 * set data pointer for the next transfer
 */
static void
njsc32_set_ptr(struct njsc32_softc *sc, struct njsc32_cmd *cmd, u_int32_t pos)
{
	struct njsc32_sgtable *sg;
	unsigned sgte;
	u_int32_t len;

	/* set current pointer */
	njsc32_set_cur_ptr(cmd, pos);

	/* undo previous fix if any */
	if (cmd->c_sgfixcnt != 0) {
		sg = &cmd->c_sgt[cmd->c_sgoffset];
		sg->sg_addr = htole32(le32toh(sg->sg_addr) - cmd->c_sgfixcnt);
		sg->sg_len = htole32(le32toh(sg->sg_len) + cmd->c_sgfixcnt);
		cmd->c_sgfixcnt = 0;
	}

	if (pos >= cmd->c_datacnt) {
		/* transfer done */
#if 1 /*def DIAGNOSTIC*/
		if (pos > cmd->c_datacnt)
			printf("%s: pos %u too large\n",
			    device_xname(sc->sc_dev), pos - cmd->c_datacnt);
#endif
		cmd->c_xferctl = 0;	/* XXX correct? */

		return;
	}

	for (sgte = 0, sg = cmd->c_sgt;
	    sgte < NJSC32_NUM_SG && pos > 0; sgte++, sg++) {
		len = le32toh(sg->sg_len) & ~NJSC32_SGT_ENDMARK;
		if (pos < len) {
			sg->sg_addr = htole32(le32toh(sg->sg_addr) + pos);
			sg->sg_len = htole32(le32toh(sg->sg_len) - pos);
			cmd->c_sgfixcnt = pos;
			break;
		}
		pos -= len;
#ifdef DIAGNOSTIC
		if (sg->sg_len & htole32(NJSC32_SGT_ENDMARK)) {
			panic("njsc32_set_ptr: bad pos");
		}
#endif
	}
#ifdef DIAGNOSTIC
	if (sgte >= NJSC32_NUM_SG)
		panic("njsc32_set_ptr: bad sg");
#endif
	if (cmd->c_sgoffset != sgte) {
		cmd->c_sgoffset = sgte;
		cmd->c_sgtdmaaddr = NJSC32_CMD_DMAADDR_SGT(cmd, sgte);
	}

	/* XXX overkill */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_cmdpg,
	    (char *)cmd->c_sgt - (char *)sc->sc_cmdpg,	/* offset */
	    NJSC32_SIZE_SGT,
	    BUS_DMASYNC_PREWRITE);
}

/*
 * save data pointer
 */
static inline void
njsc32_save_ptr(struct njsc32_cmd *cmd)
{

	cmd->c_dp_saved = cmd->c_dp_cur;
}

static void
njsc32_assert_ack(struct njsc32_softc *sc)
{
	u_int8_t reg;

	reg = njsc32_read_1(sc, NJSC32_REG_SCSI_BUS_CONTROL);
	reg |= NJSC32_SBCTL_ACK | NJSC32_SBCTL_ACK_ENABLE;
#if 0	/* needed? */
	reg |= NJSC32_SBCTL_AUTODIRECTION;
#endif
	njsc32_write_1(sc, NJSC32_REG_SCSI_BUS_CONTROL, reg);
}

static void
njsc32_negate_ack(struct njsc32_softc *sc)
{
	u_int8_t reg;

	reg = njsc32_read_1(sc, NJSC32_REG_SCSI_BUS_CONTROL);
#if 0	/* needed? */
	reg |= NJSC32_SBCTL_ACK_ENABLE;
	reg |= NJSC32_SBCTL_AUTODIRECTION;
#endif
	reg &= ~NJSC32_SBCTL_ACK;
	njsc32_write_1(sc, NJSC32_REG_SCSI_BUS_CONTROL, reg);
}

static void
njsc32_wait_req_negate(struct njsc32_softc *sc)
{
	int cnt;

	for (cnt = 0; cnt < NJSC32_REQ_TIMEOUT; cnt++) {
		if ((njsc32_read_1(sc, NJSC32_REG_SCSI_BUS_MONITOR) &
		    NJSC32_BUSMON_REQ) == 0)
			return;
		delay(1);
	}
	printf("%s: njsc32_wait_req_negate: timed out\n",
	    device_xname(sc->sc_dev));
}

static void
njsc32_reconnect(struct njsc32_softc *sc, struct njsc32_cmd *cmd)
{
	struct scsipi_xfer *xs;

	xs = cmd->c_xs;
	if ((xs->xs_control & XS_CTL_POLL) == 0) {
		callout_stop(&xs->xs_callout);
		callout_reset(&xs->xs_callout,
		    mstohz(xs->timeout),
		    njsc32_cmdtimeout, cmd);
	}

	/* Reconnection implies Restore Pointers */
	njsc32_set_ptr(sc, cmd, cmd->c_dp_saved);
}

static enum njsc32_reselstat
njsc32_resel_identify(struct njsc32_softc *sc, int lun,
    struct njsc32_cmd **pcmd)
{
	int targetid;
	struct njsc32_lu *plu;
	struct njsc32_cmd *cmd;

	switch (sc->sc_stat) {
	case NJSC32_STAT_RESEL:
		break;	/* OK */

	case NJSC32_STAT_RESEL_LUN:
	case NJSC32_STAT_RECONNECT:
		/*
		 * accept and ignore if the LUN is the same as the current one,
		 * reject otherwise.
		 */
		return sc->sc_resellun == lun ?
		    NJSC32_RESEL_THROUGH : NJSC32_RESEL_ERROR;

	default:
		printf("%s: njsc32_resel_identify: not in reselection\n",
		    device_xname(sc->sc_dev));
		return NJSC32_RESEL_ERROR;
	}

	targetid = sc->sc_reselid;
	TPRINTF(("%s: njsc32_resel_identify: reselection lun %d\n",
	    device_xname(sc->sc_dev), lun));

	if (targetid > NJSC32_MAX_TARGET_ID || lun >= NJSC32_NLU)
		return NJSC32_RESEL_ERROR;

	sc->sc_resellun = lun;
	plu = &sc->sc_targets[targetid].t_lus[lun];

	if ((cmd = plu->lu_cmd) != NULL) {
		sc->sc_stat = NJSC32_STAT_RECONNECT;
		plu->lu_cmd = NULL;
		*pcmd = cmd;
		TPRINTC(cmd, ("njsc32_resel_identify: I_T_L nexus\n"));
		njsc32_reconnect(sc, cmd);
		return NJSC32_RESEL_COMPLETE;
	} else if (!TAILQ_EMPTY(&plu->lu_q)) {
		/* wait for tag */
		sc->sc_stat = NJSC32_STAT_RESEL_LUN;
		return NJSC32_RESEL_THROUGH;
	}

	/* no disconnected commands */
	return NJSC32_RESEL_ERROR;
}

static enum njsc32_reselstat
njsc32_resel_tag(struct njsc32_softc *sc, int tag, struct njsc32_cmd **pcmd)
{
	struct njsc32_cmd_head *head;
	struct njsc32_cmd *cmd;

	TPRINTF(("%s: njsc32_resel_tag: reselection tag %d\n",
	    device_xname(sc->sc_dev), tag));
	if (sc->sc_stat != NJSC32_STAT_RESEL_LUN)
		return NJSC32_RESEL_ERROR;

	head = &sc->sc_targets[sc->sc_reselid].t_lus[sc->sc_resellun].lu_q;

	/* XXX slow? */
	/* search for the command of the tag */
	TAILQ_FOREACH(cmd, head, c_q) {
		if (cmd->c_xs->xs_tag_id == tag) {
			sc->sc_stat = NJSC32_STAT_RECONNECT;
			TAILQ_REMOVE(head, cmd, c_q);
			*pcmd = cmd;
			TPRINTC(cmd, ("njsc32_resel_tag: I_T_L_Q nexus\n"));
			njsc32_reconnect(sc, cmd);
			return NJSC32_RESEL_COMPLETE;
		}
	}

	/* no disconnected commands */
	return NJSC32_RESEL_ERROR;
}

/*
 * Reload parameters and restart AutoSCSI.
 *
 * XXX autoparam doesn't work as expected and we can't use it here.
 */
static void
njsc32_cmd_reload(struct njsc32_softc *sc, struct njsc32_cmd *cmd, int cctl)
{
	struct njsc32_target *target;

	target = cmd->c_target;

	/* clear parity error and enable parity detection */
	njsc32_write_1(sc, NJSC32_REG_PARITY_CONTROL,
	    NJSC32_PARITYCTL_CHECK_ENABLE | NJSC32_PARITYCTL_CLEAR_ERROR);

	/* load parameters */
	njsc32_write_1(sc, NJSC32_REG_SYNC, target->t_sync);
	njsc32_write_1(sc, NJSC32_REG_ACK_WIDTH, target->t_ackwidth);
	njsc32_write_1(sc, NJSC32_REG_SREQ_SAMPLING, target->t_sample);
	njsc32_write_4(sc, NJSC32_REG_SGT_ADR, cmd->c_sgtdmaaddr);
#ifdef NJSC32_DUALEDGE
	njsc32_write_2(sc, NJSC32_REG_TRANSFER,
	    cmd->c_xferctl | target->t_xferctl);
#else
	njsc32_write_2(sc, NJSC32_REG_TRANSFER, cmd->c_xferctl);
#endif
	/* start AutoSCSI */
	njsc32_write_2(sc, NJSC32_REG_COMMAND_CONTROL, cctl);

	sc->sc_curcmd = cmd;
}

static void
njsc32_update_xfer_mode(struct njsc32_softc *sc, struct njsc32_target *target)
{
	struct scsipi_xfer_mode xm;

	xm.xm_target = target - sc->sc_targets;	/* target ID */
	xm.xm_mode = 0;
	xm.xm_period = target->t_syncperiod;
	xm.xm_offset = target->t_syncoffset;
	if (xm.xm_offset != 0)
		xm.xm_mode |= PERIPH_CAP_SYNC;
	if (target->t_flags & NJSC32_TARF_TAG)
		xm.xm_mode |= PERIPH_CAP_TQING;

	scsipi_async_event(&sc->sc_channel, ASYNC_EVENT_XFER_MODE, &xm);
}

static void
njsc32_msgin(struct njsc32_softc *sc)
{
	u_int8_t msg0, msg;
	int msgcnt;
	struct njsc32_cmd *cmd;
	enum njsc32_reselstat rstat;
	int cctl = 0;
	u_int32_t ptr;	/* unsigned type ensures 2-complement calculation */
	u_int32_t msgout = 0;
	bool reload_params = FALSE;
	struct njsc32_target *target;
	int idx, period, offset;

	/*
	 * we are in Message In, so the previous Message Out should have
	 * been done.
	 */
	njsc32_init_msgout(sc);

	/* get a byte of Message In */
	msg = njsc32_read_1(sc, NJSC32_REG_DATA_IN);
	TPRINTF(("%s: njsc32_msgin: got %#x\n", device_xname(sc->sc_dev), msg));
	if ((msgcnt = sc->sc_msgincnt) < NJSC32_MSGIN_LEN)
		sc->sc_msginbuf[sc->sc_msgincnt] = msg;

	njsc32_assert_ack(sc);

	msg0 = sc->sc_msginbuf[0];
	cmd = sc->sc_curcmd;

	/* check for parity error */
	if (njsc32_read_1(sc, NJSC32_REG_PARITY_STATUS) &
	    NJSC32_PARITYSTATUS_ERROR_LSB) {

		printf("%s: msgin: parity error\n", device_xname(sc->sc_dev));

		/* clear parity error */
		njsc32_write_1(sc, NJSC32_REG_PARITY_CONTROL,
		    NJSC32_PARITYCTL_CHECK_ENABLE |
		    NJSC32_PARITYCTL_CLEAR_ERROR);

		/* respond as Message Parity Error */
		njsc32_add_msgout(sc, MSG_PARITY_ERROR);

		/* clear Message In */
		sc->sc_msgincnt = 0;
		goto reply;
	}

#define WAITNEXTMSG	do { sc->sc_msgincnt++; goto restart; } while (0)
#define MSGCOMPLETE	do { sc->sc_msgincnt = 0; goto restart; } while (0)
	if (MSG_ISIDENTIFY(msg0)) {
		/*
		 * Got Identify message from target.
		 */
		if ((msg0 & ~MSG_IDENTIFY_LUNMASK) != MSG_IDENTIFYFLAG ||
		    (rstat = njsc32_resel_identify(sc, msg0 &
			MSG_IDENTIFY_LUNMASK, &cmd)) == NJSC32_RESEL_ERROR) {
			/*
			 * invalid Identify -> Reject
			 */
			goto reject;
		}
		if (rstat == NJSC32_RESEL_COMPLETE)
			reload_params = TRUE;
		MSGCOMPLETE;
	}

	if (msg0 == MSG_SIMPLE_Q_TAG) {
		if (msgcnt == 0)
			WAITNEXTMSG;

		/* got whole message */
		sc->sc_msgincnt = 0;

		if ((rstat = njsc32_resel_tag(sc, sc->sc_msginbuf[1], &cmd))
		    == NJSC32_RESEL_ERROR) {
			/*
			 * invalid Simple Queue Tag -> Abort Tag
			 */
			printf("%s: msgin: invalid tag\n",
			    device_xname(sc->sc_dev));
			njsc32_add_msgout(sc, MSG_ABORT_TAG);
			goto reply;
		}
		if (rstat == NJSC32_RESEL_COMPLETE)
			reload_params = TRUE;
		MSGCOMPLETE;
	}

	/* I_T_L or I_T_L_Q nexus should be established now */
	if (cmd == NULL) {
		printf("%s: msgin %#x without nexus -- sending abort\n",
		    device_xname(sc->sc_dev), msg0);
		njsc32_add_msgout(sc, MSG_ABORT);
		goto reply;
	}

	/*
	 * extended message
	 * 0x01 <length (0 stands for 256)> <length bytes>
	 *                                 (<code> [<parameter> ...])
	 */
#define EXTLENOFF	1
#define EXTCODEOFF	2
	if (msg0 == MSG_EXTENDED) {
		if (msgcnt < EXTLENOFF ||
		    msgcnt < EXTLENOFF + 1 +
		    (u_int8_t)(sc->sc_msginbuf[EXTLENOFF] - 1))
			WAITNEXTMSG;

		/* got whole message */
		sc->sc_msgincnt = 0;

		switch (sc->sc_msginbuf[EXTCODEOFF]) {
		case 0:	/* Modify Data Pointer */
			if (msgcnt != 5 + EXTCODEOFF - 1)
				break;
			/*
			 * parameter is 32bit big-endian signed (2-complement)
			 * value
			 */
			ptr = (sc->sc_msginbuf[EXTCODEOFF + 1] << 24) |
			      (sc->sc_msginbuf[EXTCODEOFF + 2] << 16) |
			      (sc->sc_msginbuf[EXTCODEOFF + 3] << 8) |
			      sc->sc_msginbuf[EXTCODEOFF + 4];

			/* new pointer */
			ptr += cmd->c_dp_cur;	/* ignore overflow */

			/* reject if ptr is not in data buffer */
			if (ptr > cmd->c_datacnt)
				break;

			njsc32_set_ptr(sc, cmd, ptr);
			goto restart;

		case MSG_EXT_SDTR:	/* Synchronous Data Transfer Request */
			DPRINTC(cmd, ("SDTR %#x %#x\n",
			    sc->sc_msginbuf[EXTCODEOFF + 1],
			    sc->sc_msginbuf[EXTCODEOFF + 2]));
			if (msgcnt != MSG_EXT_SDTR_LEN + EXTCODEOFF-1)
				break;	/* reject */

			target = cmd->c_target;

			/* lookup sync period parameters */
			period = sc->sc_msginbuf[EXTCODEOFF + 1];
			for (idx = sc->sc_sync_max; idx < NJSC32_NSYNCT; idx++)
				if (sc->sc_synct[idx].sp_period >= period) {
					period = sc->sc_synct[idx].sp_period;
					break;
				}
			if (idx >= NJSC32_NSYNCT) {
				/*
				 * We can't meet the timing condition that
				 * the target requests -- use async.
				 */
				njsc32_target_async(sc, target);
				njsc32_update_xfer_mode(sc, target);
				if (target->t_state == NJSC32_TARST_SDTR) {
					/*
					 * We started SDTR exchange -- start
					 * negotiation again and request async.
					 */
					target->t_state = NJSC32_TARST_ASYNC;
					njsc32_negotiate_xfer(sc, target);
					goto reply;
				} else {
					/*
					 * The target started SDTR exchange
					 * -- just reject and fallback
					 * to async.
					 */
					goto reject;
				}
			}

			/* check sync offset */
			offset = sc->sc_msginbuf[EXTCODEOFF + 2];
			if (offset > NJSC32_SYNCOFFSET_MAX) {
				if (target->t_state == NJSC32_TARST_SDTR) {
					printf("%s: wrong sync offset: %d\n",
					    device_xname(sc->sc_dev), offset);
					/* XXX what to do? */
				}
				offset = NJSC32_SYNCOFFSET_MAX;
			}

			target->t_ackwidth = sc->sc_synct[idx].sp_ackw;
			target->t_sample   = sc->sc_synct[idx].sp_sample;
			target->t_syncperiod = period;
			target->t_syncoffset = offset;
			target->t_sync = NJSC32_SYNC_VAL(idx, offset);
			njsc32_update_xfer_mode(sc, target);

			if (target->t_state == NJSC32_TARST_SDTR) {
				target->t_state = NJSC32_TARST_DONE;
			} else {
				njsc32_msgout_sdtr(sc, period, offset);
				goto reply;
			}
			goto restart;

		case MSG_EXT_WDTR:	/* Wide Data Transfer Request */
			DPRINTC(cmd,
			    ("WDTR %#x\n", sc->sc_msginbuf[EXTCODEOFF + 1]));
#ifdef NJSC32_DUALEDGE
			if (msgcnt != MSG_EXT_WDTR_LEN + EXTCODEOFF-1)
				break;	/* reject */

			/*
			 * T->I of this message is not used for
			 * DualEdge negotiation, so the device
			 * must not be a DualEdge device.
			 *
			 * XXX correct?
			 */
			target = cmd->c_target;
			target->t_xferctl = 0;

			switch (target->t_state) {
			case NJSC32_TARST_DE:
				if (sc->sc_msginbuf[EXTCODEOFF + 1] !=
				    MSG_EXT_WDTR_BUS_8_BIT) {
					/*
					 * Oops, we got unexpected WDTR.
					 * Negotiate for 8bit.
					 */
					target->t_state = NJSC32_TARST_WDTR;
				} else {
					target->t_state = NJSC32_TARST_SDTR;
				}
				njsc32_negotiate_xfer(sc, target);
				goto reply;

			case NJSC32_TARST_WDTR:
				if (sc->sc_msginbuf[EXTCODEOFF + 1] !=
				    MSG_EXT_WDTR_BUS_8_BIT) {
					printf("%s: unexpected transfer width:"
					    " %#x\n", device_xname(sc->sc_dev),
					    sc->sc_msginbuf[EXTCODEOFF + 1]);
					/* XXX what to do? */
				}
				target->t_state = NJSC32_TARST_SDTR;
				njsc32_negotiate_xfer(sc, target);
				goto reply;

			default:
				/* the target started WDTR exchange */
				DPRINTC(cmd, ("WDTR from target\n"));

				target->t_state = NJSC32_TARST_SDTR;
				njsc32_target_async(sc, target);

				break;	/* reject the WDTR (8bit transfer) */
			}
#endif	/* NJSC32_DUALEDGE */
			break;	/* reject */
		}
		DPRINTC(cmd, ("njsc32_msgin: reject ext msg %#x msgincnt %d\n",
		    sc->sc_msginbuf[EXTCODEOFF], msgcnt));
		goto reject;
	}

	/* 2byte messages */
	if (MSG_IS2BYTE(msg0)) {
		if (msgcnt == 0)
			WAITNEXTMSG;

		/* got whole message */
		sc->sc_msgincnt = 0;
	}

	switch (msg0) {
	case MSG_CMDCOMPLETE:		/* 0x00 */
	case MSG_SAVEDATAPOINTER:	/* 0x02 */
	case MSG_DISCONNECT:		/* 0x04 */
		/* handled by AutoSCSI */
		PRINTC(cmd, ("msgin: unexpected msg: %#x\n", msg0));
		break;

	case MSG_RESTOREPOINTERS:	/* 0x03 */
		/* restore data pointer to what was saved */
		DPRINTC(cmd, ("njsc32_msgin: Restore Pointers\n"));
		njsc32_set_ptr(sc, cmd, cmd->c_dp_saved);
		reload_params = TRUE;
		MSGCOMPLETE;
		/* NOTREACHED */
		break;

#if 0	/* handled above */
	case MSG_EXTENDED:		/* 0x01 */
#endif
	case MSG_MESSAGE_REJECT:	/* 0x07 */
		target = cmd->c_target;
		DPRINTC(cmd, ("Reject tarst %d\n", target->t_state));
		switch (target->t_state) {
#ifdef NJSC32_DUALEDGE
		case NJSC32_TARST_WDTR:
		case NJSC32_TARST_DE:
			target->t_xferctl = 0;
			target->t_state = NJSC32_TARST_SDTR;
			njsc32_negotiate_xfer(sc, target);
			goto reply;
#endif
		case NJSC32_TARST_SDTR:
		case NJSC32_TARST_ASYNC:
			njsc32_target_async(sc, target);
			target->t_state = NJSC32_TARST_DONE;
			njsc32_update_xfer_mode(sc, target);
			break;
		default:
			break;
		}
		goto restart;

	case MSG_NOOP:			/* 0x08 */
#ifdef NJSC32_DUALEDGE
		target = cmd->c_target;
		if (target->t_state == NJSC32_TARST_DE) {
			printf("%s: DualEdge transfer\n",
			    device_xname(sc->sc_dev));
			target->t_xferctl = NJSC32_XFR_DUALEDGE_ENABLE;
			/* go to next negotiation */
			target->t_state = NJSC32_TARST_SDTR;
			njsc32_negotiate_xfer(sc, target);
			goto reply;
		}
#endif
		goto restart;

	case MSG_INITIATOR_DET_ERR:	/* 0x05 I->T only */
	case MSG_ABORT:			/* 0x06 I->T only */
	case MSG_PARITY_ERROR:		/* 0x09 I->T only */
	case MSG_LINK_CMD_COMPLETE:	/* 0x0a */
	case MSG_LINK_CMD_COMPLETEF:	/* 0x0b */
	case MSG_BUS_DEV_RESET:		/* 0x0c I->T only */
	case MSG_ABORT_TAG:		/* 0x0d I->T only */
	case MSG_CLEAR_QUEUE:		/* 0x0e I->T only */

#if 0	/* handled above */
	case MSG_SIMPLE_Q_TAG:		/* 0x20 */
#endif
	case MSG_HEAD_OF_Q_TAG:		/* 0x21 I->T only */
	case MSG_ORDERED_Q_TAG:		/* 0x22 I->T only */
	case MSG_IGN_WIDE_RESIDUE:	/* 0x23 */

	default:
#ifdef NJSC32_DEBUG
		PRINTC(cmd, ("msgin: unsupported msg: %#x", msg0));
		if (MSG_IS2BYTE(msg0))
			printf(" %#x", msg);
		printf("\n");
#endif
		break;
	}

reject:
	njsc32_add_msgout(sc, MSG_MESSAGE_REJECT);

reply:
	msgout = njsc32_get_auto_msgout(sc);

restart:
	cctl = NJSC32_CMD_CLEAR_CDB_FIFO_PTR |
	    NJSC32_CMD_AUTO_COMMAND_PHASE |
	    NJSC32_CMD_AUTO_SCSI_RESTART;

	/*
	 * Be careful the second and latter bytes of Message In
	 * shall not be absorbed by AutoSCSI.
	 */
	if (sc->sc_msgincnt == 0)
		cctl |= NJSC32_CMD_AUTO_MSGIN_00_04 | NJSC32_CMD_AUTO_MSGIN_02;

	if (sc->sc_msgoutlen != 0)
		cctl |= NJSC32_CMD_AUTO_ATN;

	njsc32_write_4(sc, NJSC32_REG_SCSI_MSG_OUT, msgout);

	/* (re)start AutoSCSI (may assert ATN) */
	if (reload_params) {
		njsc32_cmd_reload(sc, cmd, cctl);
	} else {
		njsc32_write_2(sc, NJSC32_REG_COMMAND_CONTROL, cctl);
	}

	/* +ATN -> -REQ: need 90ns delay? */

	njsc32_wait_req_negate(sc);	/* wait for REQ negation */

	njsc32_negate_ack(sc);

	return;
}

static void
njsc32_msgout(struct njsc32_softc *sc)
{
	int cctl;
	u_int8_t bus;
	unsigned n;

	if (sc->sc_msgoutlen == 0) {
		/* target entered to Message Out on unexpected timing */
		njsc32_add_msgout(sc, MSG_NOOP);
	}

	cctl = NJSC32_CMD_CLEAR_CDB_FIFO_PTR |
	    NJSC32_CMD_AUTO_COMMAND_PHASE | NJSC32_CMD_AUTO_SCSI_RESTART |
	    NJSC32_CMD_AUTO_MSGIN_00_04 | NJSC32_CMD_AUTO_MSGIN_02;

	/* make sure target is in Message Out phase */
	bus = njsc32_read_1(sc, NJSC32_REG_SCSI_BUS_MONITOR);
	if ((bus & NJSC32_BUSMON_PHASE_MASK) != NJSC32_PHASE_MESSAGE_OUT) {
		/*
		 * Message Out is aborted by target.
		 */
		printf("%s: njsc32_msgout: phase change %#x\n",
		    device_xname(sc->sc_dev), bus);

		/* XXX what to do? */

		/* restart AutoSCSI (negate ATN) */
		njsc32_write_2(sc, NJSC32_REG_COMMAND_CONTROL, cctl);

		sc->sc_msgoutidx = 0;
		return;
	}

	n = sc->sc_msgoutidx;
	if (n == sc->sc_msgoutlen - 1) {
		/*
		 * negate ATN before sending ACK
		 */
		njsc32_write_2(sc, NJSC32_REG_COMMAND_CONTROL, 0);

		sc->sc_msgoutidx = 0;	/* target may retry Message Out */
	} else {
		cctl |= NJSC32_CMD_AUTO_ATN;
		sc->sc_msgoutidx++;
	}

	/* Send Message Out */
	njsc32_write_1(sc, NJSC32_REG_SCSI_OUT_LATCH, sc->sc_msgout[n]);

	/* DBn -> +ACK: need 55ns delay? */

	njsc32_assert_ack(sc);
	njsc32_wait_req_negate(sc);	/* wait for REQ negation */

	/* restart AutoSCSI */
	njsc32_write_2(sc, NJSC32_REG_COMMAND_CONTROL, cctl);

	njsc32_negate_ack(sc);

	/*
	 * do not reset sc->sc_msgoutlen so the target
	 * can retry Message Out phase
	 */
}

static void
njsc32_cmdtimeout(void *arg)
{
	struct njsc32_cmd *cmd = arg;
	struct njsc32_softc *sc;
	int s;

	PRINTC(cmd, ("command timeout\n"));

	sc = cmd->c_sc;

	s = splbio();

	if (sc->sc_stat == NJSC32_STAT_ARBIT)
		njsc32_arbitration_failed(sc);
	else {
		sc->sc_curcmd = NULL;
		sc->sc_stat = NJSC32_STAT_IDLE;
		njsc32_end_cmd(sc, cmd, XS_TIMEOUT);
	}

	/* XXX? */
	njsc32_init(sc, 1);	/* bus reset */

	splx(s);
}

static void
njsc32_reseltimeout(void *arg)
{
	struct njsc32_cmd *cmd = arg;
	struct njsc32_softc *sc;
	int s;

	PRINTC(cmd, ("reselection timeout\n"));

	sc = cmd->c_sc;

	s = splbio();

	/* remove from disconnected list */
	if (cmd->c_flags & NJSC32_CMD_TAGGED) {
		/* I_T_L_Q */
		KASSERT(cmd->c_lu->lu_cmd == NULL);
		TAILQ_REMOVE(&cmd->c_lu->lu_q, cmd, c_q);
	} else {
		/* I_T_L */
		KASSERT(cmd->c_lu->lu_cmd == cmd);
		cmd->c_lu->lu_cmd = NULL;
	}

	njsc32_end_cmd(sc, cmd, XS_TIMEOUT);

	/* XXX? */
	njsc32_init(sc, 1);	/* bus reset */

	splx(s);
}

static inline void
njsc32_end_auto(struct njsc32_softc *sc, struct njsc32_cmd *cmd, int auto_phase)
{
	struct scsipi_xfer *xs;

	if (auto_phase & NJSC32_XPHASE_MSGIN_02) {
		/* Message In: 0x02 Save Data Pointer */

		/*
		 * Adjust saved data pointer
		 * if the command is not completed yet.
		 */
		if ((auto_phase & NJSC32_XPHASE_MSGIN_00) == 0 &&
		    (auto_phase &
		     (NJSC32_XPHASE_DATA_IN | NJSC32_XPHASE_DATA_OUT)) != 0) {
			njsc32_save_ptr(cmd);
		}
		TPRINTF(("BM %u, SGT %u, SACK %u, SAVED_ACK %u\n",
		    njsc32_read_4(sc, NJSC32_REG_BM_CNT),
		    njsc32_read_4(sc, NJSC32_REG_SGT_ADR),
		    njsc32_read_4(sc, NJSC32_REG_SACK_CNT),
		    njsc32_read_4(sc, NJSC32_REG_SAVED_ACK_CNT)));
	}

	xs = cmd->c_xs;

	if (auto_phase & NJSC32_XPHASE_MSGIN_00) {
		/* Command Complete */
		TPRINTC(cmd, ("njsc32_intr: Command Complete\n"));
		switch (xs->status) {
		case SCSI_CHECK: case SCSI_QUEUE_FULL: case SCSI_BUSY:
			/*
			 * scsipi layer will automatically handle the error
			 */
			njsc32_end_cmd(sc, cmd, XS_BUSY);
			break;
		default:
			xs->resid -= cmd->c_dp_max;
			njsc32_end_cmd(sc, cmd, XS_NOERROR);
			break;
		}
	} else if (auto_phase & NJSC32_XPHASE_MSGIN_04) {
		/* Disconnect */
		TPRINTC(cmd, ("njsc32_intr: Disconnect\n"));

		/* for ill-designed devices */
		if ((xs->xs_periph->periph_quirks & PQUIRK_AUTOSAVE) != 0)
			njsc32_save_ptr(cmd);

		/*
		 * move current cmd to disconnected list
		 */
		if (cmd->c_flags & NJSC32_CMD_TAGGED) {
			/* I_T_L_Q */
			if (cmd->c_flags & NJSC32_CMD_TAGGED_HEAD)
				TAILQ_INSERT_HEAD(&cmd->c_lu->lu_q, cmd, c_q);
			else
				TAILQ_INSERT_TAIL(&cmd->c_lu->lu_q, cmd, c_q);
		} else {
			/* I_T_L */
			cmd->c_lu->lu_cmd = cmd;
		}

		/*
		 * schedule timeout -- avoid being
		 * disconnected forever
		 */
		if ((xs->xs_control & XS_CTL_POLL) == 0) {
			callout_stop(&xs->xs_callout);
			callout_reset(&xs->xs_callout, mstohz(xs->timeout),
			    njsc32_reseltimeout, cmd);
		}

	} else {
		/*
		 * target has come to Bus Free phase
		 * probably to notify an error
		 */
		PRINTC(cmd, ("njsc32_intr: unexpected bus free\n"));
		/* try Request Sense */
		xs->status = SCSI_CHECK;
		njsc32_end_cmd(sc, cmd, XS_BUSY);
	}
}

int
njsc32_intr(void *arg)
{
	struct njsc32_softc *sc = arg;
	u_int16_t intr;
	u_int8_t arbstat, bus_phase;
	int auto_phase;
	int idbit;
	struct njsc32_cmd *cmd;

	intr = njsc32_read_2(sc, NJSC32_REG_IRQ);
	if ((intr & NJSC32_IRQ_INTR_PENDING) == 0)
		return 0;	/* not mine */

	TPRINTF(("%s: njsc32_intr: %#x\n", device_xname(sc->sc_dev), intr));

#if 0	/* I don't think this is required */
	/* mask interrupts */
	njsc32_write_2(sc, NJSC32_REG_IRQ, NJSC32_IRQ_MASK_ALL);
#endif

	/* we got an interrupt, so stop the timer */
	njsc32_write_2(sc, NJSC32_REG_TIMER, NJSC32_TIMER_STOP);

	if (intr & NJSC32_IRQ_SCSIRESET) {
		printf("%s: detected bus reset\n", device_xname(sc->sc_dev));
		/* make sure all devices on the bus are certainly reset  */
		njsc32_reset_bus(sc);
		goto out;
	}

	if (sc->sc_stat == NJSC32_STAT_ARBIT) {
		cmd = sc->sc_curcmd;
		KASSERT(cmd);
		arbstat = njsc32_read_1(sc, NJSC32_REG_ARBITRATION_STAT);
		if (arbstat & (NJSC32_ARBSTAT_WIN | NJSC32_ARBSTAT_FAIL)) {
			/*
			 * arbitration done
			 */
			/* clear arbitration status */
			njsc32_write_1(sc, NJSC32_REG_SET_ARBITRATION,
			    NJSC32_SETARB_CLEAR);

			if (arbstat & NJSC32_ARBSTAT_WIN) {
				TPRINTC(cmd,
				    ("njsc32_intr: arbitration won\n"));

				TAILQ_REMOVE(&sc->sc_reqcmd, cmd, c_q);

				sc->sc_stat = NJSC32_STAT_CONNECT;
			} else {
				TPRINTC(cmd,
				    ("njsc32_intr: arbitration failed\n"));

				njsc32_arbitration_failed(sc);

				/* XXX delay */
				/* XXX retry counter */
			}
		}
	}

	if (intr & NJSC32_IRQ_TIMER) {
		TPRINTF(("%s: njsc32_intr: timer interrupt\n",
		    device_xname(sc->sc_dev)));
	}

	if (intr & NJSC32_IRQ_RESELECT) {
		/* Reselection from a target */
		njsc32_arbitration_failed(sc);	/* just in case */
		if ((cmd = sc->sc_curcmd) != NULL) {
			/* ? */
			printf("%s: unexpected reselection\n",
			    device_xname(sc->sc_dev));
			sc->sc_curcmd = NULL;
			sc->sc_stat = NJSC32_STAT_IDLE;
			njsc32_end_cmd(sc, cmd, XS_DRIVER_STUFFUP);
		}

		idbit = njsc32_read_1(sc, NJSC32_REG_RESELECT_ID);
		if ((idbit & (1 << NJSC32_INITIATOR_ID)) == 0 ||
		    (sc->sc_reselid =
		     ffs(idbit & ~(1 << NJSC32_INITIATOR_ID)) - 1) < 0) {
			printf("%s: invalid reselection (id: %#x)\n",
			    device_xname(sc->sc_dev), idbit);
			sc->sc_stat = NJSC32_STAT_IDLE;	/* XXX ? */
		} else {
			sc->sc_stat = NJSC32_STAT_RESEL;
			TPRINTF(("%s: njsc32_intr: reselection from %d\n",
			    device_xname(sc->sc_dev), sc->sc_reselid));
		}
	}

	if (intr & NJSC32_IRQ_PHASE_CHANGE) {
#if 1	/* XXX probably not needed */
		if (sc->sc_stat == NJSC32_STAT_ARBIT)
			PRINTC(sc->sc_curcmd,
			    ("njsc32_intr: cancel arbitration phase\n"));
		njsc32_arbitration_failed(sc);
#endif
		/* current bus phase */
		bus_phase = njsc32_read_1(sc, NJSC32_REG_SCSI_BUS_MONITOR) &
		    NJSC32_BUSMON_PHASE_MASK;

		switch (bus_phase) {
		case NJSC32_PHASE_MESSAGE_IN:
			njsc32_msgin(sc);
			break;

		/*
		 * target may suddenly become Status / Bus Free phase
		 * to notify an error condition
		 */
		case NJSC32_PHASE_STATUS:
			printf("%s: unexpected bus phase: Status\n",
			    device_xname(sc->sc_dev));
			if ((cmd = sc->sc_curcmd) != NULL) {
				cmd->c_xs->status =
				    njsc32_read_1(sc, NJSC32_REG_SCSI_CSB_IN);
				TPRINTC(cmd, ("njsc32_intr: Status %d\n",
				    cmd->c_xs->status));
			}
			break;
		case NJSC32_PHASE_BUSFREE:
			printf("%s: unexpected bus phase: Bus Free\n",
			    device_xname(sc->sc_dev));
			if ((cmd = sc->sc_curcmd) != NULL) {
				sc->sc_curcmd = NULL;
				sc->sc_stat = NJSC32_STAT_IDLE;
				if (cmd->c_xs->status != SCSI_QUEUE_FULL &&
				    cmd->c_xs->status != SCSI_BUSY)
					cmd->c_xs->status = SCSI_CHECK;/* XXX */
				njsc32_end_cmd(sc, cmd, XS_BUSY);
			}
			goto out;
		default:
#ifdef NJSC32_DEBUG
			printf("%s: unexpected bus phase: ",
			    device_xname(sc->sc_dev));
			switch (bus_phase) {
			case NJSC32_PHASE_COMMAND:
				printf("Command\n");
				break;
			case NJSC32_PHASE_MESSAGE_OUT:
				printf("Message Out\n");
				break;
			case NJSC32_PHASE_DATA_IN:
				printf("Data In\n");
				break;
			case NJSC32_PHASE_DATA_OUT:
				printf("Data Out\n");
				break;
			case NJSC32_PHASE_RESELECT:
				printf("Reselect\n");
				break;
			default:
				printf("%#x\n", bus_phase);
				break;
			}
#else
			printf("%s: unexpected bus phase: %#x",
			    device_xname(sc->sc_dev), bus_phase);
#endif
			break;
		}
	}

	if (intr & NJSC32_IRQ_AUTOSCSI) {
		/*
		 * AutoSCSI interrupt
		 */
		auto_phase = njsc32_read_2(sc, NJSC32_REG_EXECUTE_PHASE);
		TPRINTF(("%s: njsc32_intr: AutoSCSI: %#x\n",
		    device_xname(sc->sc_dev), auto_phase));
		njsc32_write_2(sc, NJSC32_REG_EXECUTE_PHASE, 0);

		if (auto_phase & NJSC32_XPHASE_SEL_TIMEOUT) {
			cmd = sc->sc_curcmd;
			if (cmd == NULL) {
				printf("%s: sel no cmd\n",
				    device_xname(sc->sc_dev));
				goto out;
			}
			DPRINTC(cmd, ("njsc32_intr: selection timeout\n"));

			sc->sc_curcmd = NULL;
			sc->sc_stat = NJSC32_STAT_IDLE;
			njsc32_end_cmd(sc, cmd, XS_SELTIMEOUT);

			goto out;
		}

#ifdef NJSC32_TRACE
		if (auto_phase & NJSC32_XPHASE_COMMAND) {
			/* Command phase has been automatically processed */
			TPRINTF(("%s: njsc32_intr: Command\n",
			    device_xname(sc->sc_dev)));
		}
#endif
#ifdef NJSC32_DEBUG
		if (auto_phase & NJSC32_XPHASE_ILLEGAL) {
			printf("%s: njsc32_intr: Illegal phase\n",
			    device_xname(sc->sc_dev));
		}
#endif

		if (auto_phase & NJSC32_XPHASE_PAUSED_MSG_IN) {
			TPRINTF(("%s: njsc32_intr: Process Message In\n",
			    device_xname(sc->sc_dev)));
			njsc32_msgin(sc);
		}

		if (auto_phase & NJSC32_XPHASE_PAUSED_MSG_OUT) {
			TPRINTF(("%s: njsc32_intr: Process Message Out\n",
			    device_xname(sc->sc_dev)));
			njsc32_msgout(sc);
		}

		cmd = sc->sc_curcmd;
		if (cmd == NULL) {
			TPRINTF(("%s: njsc32_intr: no cmd\n",
			    device_xname(sc->sc_dev)));
			goto out;
		}

		if (auto_phase &
		    (NJSC32_XPHASE_DATA_IN | NJSC32_XPHASE_DATA_OUT)) {
			u_int32_t sackcnt, cntoffset;

#ifdef NJSC32_TRACE
			if (auto_phase & NJSC32_XPHASE_DATA_IN)
				PRINTC(cmd, ("njsc32_intr: data in done\n"));
			if (auto_phase & NJSC32_XPHASE_DATA_OUT)
				PRINTC(cmd, ("njsc32_intr: data out done\n"));
			printf("BM %u, SGT %u, SACK %u, SAVED_ACK %u\n",
			    njsc32_read_4(sc, NJSC32_REG_BM_CNT),
			    njsc32_read_4(sc, NJSC32_REG_SGT_ADR),
			    njsc32_read_4(sc, NJSC32_REG_SACK_CNT),
			    njsc32_read_4(sc, NJSC32_REG_SAVED_ACK_CNT));
#endif

			/*
			 * detected parity error on data transfer?
			 */
			if (njsc32_read_1(sc, NJSC32_REG_PARITY_STATUS) &
			    (NJSC32_PARITYSTATUS_ERROR_LSB|
			     NJSC32_PARITYSTATUS_ERROR_MSB)) {

				PRINTC(cmd, ("datain: parity error\n"));

				/* clear parity error */
				njsc32_write_1(sc, NJSC32_REG_PARITY_CONTROL,
				    NJSC32_PARITYCTL_CHECK_ENABLE |
				    NJSC32_PARITYCTL_CLEAR_ERROR);

				if (auto_phase & NJSC32_XPHASE_BUS_FREE) {
					/*
					 * XXX command has already finished
					 * -- what can we do?
					 *
					 * It is not clear current command
					 * caused the error -- reset everything.
					 */
					njsc32_init(sc, 1);	/* XXX */
				} else {
					/* XXX does this case occur? */
#if 1
					printf("%s: datain: parity error\n",
					    device_xname(sc->sc_dev));
#endif
					/*
					 * Make attention condition and try
					 * to send Initiator Detected Error
					 * message.
					 */
					njsc32_init_msgout(sc);
					njsc32_add_msgout(sc,
					    MSG_INITIATOR_DET_ERR);
					njsc32_write_4(sc,
					    NJSC32_REG_SCSI_MSG_OUT,
					    njsc32_get_auto_msgout(sc));
					/* restart autoscsi with ATN */
					njsc32_write_2(sc,
					    NJSC32_REG_COMMAND_CONTROL,
					    NJSC32_CMD_CLEAR_CDB_FIFO_PTR |
					    NJSC32_CMD_AUTO_COMMAND_PHASE |
					    NJSC32_CMD_AUTO_SCSI_RESTART |
					    NJSC32_CMD_AUTO_MSGIN_00_04 |
					    NJSC32_CMD_AUTO_MSGIN_02 |
					    NJSC32_CMD_AUTO_ATN);
				}
				goto out;
			}

			/*
			 * data has been transferred, and current pointer
			 * is changed
			 */
			sackcnt = njsc32_read_4(sc, NJSC32_REG_SACK_CNT);

			/*
			 * The controller returns extra ACK count
			 * if the DMA buffer is not 4byte aligned.
			 */
			cntoffset = le32toh(cmd->c_sgt[0].sg_addr) & 3;
#ifdef NJSC32_DEBUG
			if (cntoffset != 0) {
				printf("sackcnt %u, cntoffset %u\n",
				    sackcnt, cntoffset);
			}
#endif
			/* advance SCSI pointer */
			njsc32_set_cur_ptr(cmd,
			    cmd->c_dp_cur + sackcnt - cntoffset);
		}

		if (auto_phase & NJSC32_XPHASE_MSGOUT) {
			/* Message Out phase has been automatically processed */
			TPRINTC(cmd, ("njsc32_intr: Message Out\n"));
			if ((auto_phase & NJSC32_XPHASE_PAUSED_MSG_IN) == 0 &&
			    sc->sc_msgoutlen <= NJSC32_MSGOUT_MAX_AUTO) {
				njsc32_init_msgout(sc);
			}
		}

		if (auto_phase & NJSC32_XPHASE_STATUS) {
			/* Status phase has been automatically processed */
			cmd->c_xs->status =
			    njsc32_read_1(sc, NJSC32_REG_SCSI_CSB_IN);
			TPRINTC(cmd, ("njsc32_intr: Status %#x\n",
			    cmd->c_xs->status));
		}

		if (auto_phase & NJSC32_XPHASE_BUS_FREE) {
			/* AutoSCSI is finished */

			TPRINTC(cmd, ("njsc32_intr: Bus Free\n"));

			sc->sc_stat = NJSC32_STAT_IDLE;
			sc->sc_curcmd = NULL;

			njsc32_end_auto(sc, cmd, auto_phase);
		}
		goto out;
	}

	if (intr & NJSC32_IRQ_FIFO_THRESHOLD) {
		/* XXX We use DMA, and this shouldn't happen */
		printf("%s: njsc32_intr: FIFO\n", device_xname(sc->sc_dev));
		njsc32_init(sc, 1);
		goto out;
	}
	if (intr & NJSC32_IRQ_PCI) {
		/* XXX? */
		printf("%s: njsc32_intr: PCI\n", device_xname(sc->sc_dev));
	}
	if (intr & NJSC32_IRQ_BMCNTERR) {
		/* XXX? */
		printf("%s: njsc32_intr: BM\n", device_xname(sc->sc_dev));
	}

out:
	/* go next command if controller is idle */
	if (sc->sc_stat == NJSC32_STAT_IDLE)
		njsc32_start(sc);

#if 0
	/* enable interrupts */
	njsc32_write_2(sc, NJSC32_REG_IRQ, 0);
#endif

	return 1;	/* processed */
}
