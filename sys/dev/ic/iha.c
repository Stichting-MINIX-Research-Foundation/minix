/*	$NetBSD: iha.c,v 1.42 2011/05/24 16:38:25 joerg Exp $ */

/*-
 * Copyright (c) 2001, 2002 Izumi Tsutsui
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

/*-
 * Device driver for the INI-9XXXU/UW or INIC-940/950 PCI SCSI Controller.
 *
 *  Written for 386bsd and FreeBSD by
 *	Winston Hung		<winstonh@initio.com>
 *
 * Copyright (c) 1997-1999 Initio Corp.
 * Copyright (c) 2000, 2001 Ken Westerback
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Ported to NetBSD by Izumi Tsutsui <tsutsui@NetBSD.org> from OpenBSD:
 * $OpenBSD: iha.c,v 1.3 2001/02/20 00:47:33 krw Exp $
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: iha.c,v 1.42 2011/05/24 16:38:25 joerg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsi_spc.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_message.h>

#include <dev/ic/ihareg.h>
#include <dev/ic/ihavar.h>

/*
 * SCSI Rate Table, indexed by FLAG_SCSI_RATE field of
 * tcs flags.
 */
static const uint8_t iha_rate_tbl[] = {
	/* fast 20		  */
	/* nanosecond divide by 4 */
	12,	/* 50ns,  20M	  */
	18,	/* 75ns,  13.3M	  */
	25,	/* 100ns, 10M	  */
	31,	/* 125ns, 8M	  */
	37,	/* 150ns, 6.6M	  */
	43,	/* 175ns, 5.7M	  */
	50,	/* 200ns, 5M	  */
	62	/* 250ns, 4M	  */
};
#define IHA_MAX_PERIOD	62

#ifdef notused
static uint16_t eeprom_default[EEPROM_SIZE] = {
	/* -- Header ------------------------------------ */
	/* signature */
	EEP_SIGNATURE,
	/* size, revision */
	EEP_WORD(EEPROM_SIZE * 2, 0x01),
	/* -- Host Adapter Structure -------------------- */
	/* model */
	0x0095,
	/* model info, number of channel */
	EEP_WORD(0x00, 1),
	/* BIOS config */
	EEP_BIOSCFG_DEFAULT,
	/* host adapter config */
	0,

	/* -- eeprom_adapter[0] ------------------------------- */
	/* ID, adapter config 1 */
	EEP_WORD(7, CFG_DEFAULT),
	/* adapter config 2, number of targets */
	EEP_WORD(0x00, 8),
	/* target flags */
	EEP_WORD(FLAG_DEFAULT, FLAG_DEFAULT),
	EEP_WORD(FLAG_DEFAULT, FLAG_DEFAULT),
	EEP_WORD(FLAG_DEFAULT, FLAG_DEFAULT),
	EEP_WORD(FLAG_DEFAULT, FLAG_DEFAULT),
	EEP_WORD(FLAG_DEFAULT, FLAG_DEFAULT),
	EEP_WORD(FLAG_DEFAULT, FLAG_DEFAULT),
	EEP_WORD(FLAG_DEFAULT, FLAG_DEFAULT),
	EEP_WORD(FLAG_DEFAULT, FLAG_DEFAULT),

	/* -- eeprom_adapter[1] ------------------------------- */
	/* ID, adapter config 1 */
	EEP_WORD(7, CFG_DEFAULT),
	/* adapter config 2, number of targets */
	EEP_WORD(0x00, 8),
	/* target flags */
	EEP_WORD(FLAG_DEFAULT, FLAG_DEFAULT),
	EEP_WORD(FLAG_DEFAULT, FLAG_DEFAULT),
	EEP_WORD(FLAG_DEFAULT, FLAG_DEFAULT),
	EEP_WORD(FLAG_DEFAULT, FLAG_DEFAULT),
	EEP_WORD(FLAG_DEFAULT, FLAG_DEFAULT),
	EEP_WORD(FLAG_DEFAULT, FLAG_DEFAULT),
	EEP_WORD(FLAG_DEFAULT, FLAG_DEFAULT),
	EEP_WORD(FLAG_DEFAULT, FLAG_DEFAULT),
	/* reserved[5] */
	0, 0, 0, 0, 0,
	/* checksum */
	0
};
#endif

static void iha_append_free_scb(struct iha_softc *, struct iha_scb *);
static void iha_append_done_scb(struct iha_softc *, struct iha_scb *, uint8_t);
static inline struct iha_scb *iha_pop_done_scb(struct iha_softc *);

static struct iha_scb *iha_find_pend_scb(struct iha_softc *);
static inline void iha_append_pend_scb(struct iha_softc *, struct iha_scb *);
static inline void iha_push_pend_scb(struct iha_softc *, struct iha_scb *);
static inline void iha_del_pend_scb(struct iha_softc *, struct iha_scb *);
static inline void iha_mark_busy_scb(struct iha_scb *);

static inline void iha_set_ssig(struct iha_softc *, uint8_t, uint8_t);

static int iha_alloc_sglist(struct iha_softc *);

static void iha_scsipi_request(struct scsipi_channel *, scsipi_adapter_req_t,
    void *);
static void iha_update_xfer_mode(struct iha_softc *, int);

static void iha_reset_scsi_bus(struct iha_softc *);
static void iha_reset_chip(struct iha_softc *);
static void iha_reset_dma(struct iha_softc *);
static void iha_reset_tcs(struct tcs *, uint8_t);

static void iha_main(struct iha_softc *);
static void iha_scsi(struct iha_softc *);
static void iha_select(struct iha_softc *, struct iha_scb *, uint8_t);
static int iha_wait(struct iha_softc *, uint8_t);

static void iha_exec_scb(struct iha_softc *, struct iha_scb *);
static void iha_done_scb(struct iha_softc *, struct iha_scb *);
static int iha_push_sense_request(struct iha_softc *, struct iha_scb *);

static void iha_timeout(void *);
static void iha_abort_xs(struct iha_softc *, struct scsipi_xfer *, uint8_t);
static uint8_t iha_data_over_run(struct iha_scb *);

static int iha_next_state(struct iha_softc *);
static int iha_state_1(struct iha_softc *);
static int iha_state_2(struct iha_softc *);
static int iha_state_3(struct iha_softc *);
static int iha_state_4(struct iha_softc *);
static int iha_state_5(struct iha_softc *);
static int iha_state_6(struct iha_softc *);
static int iha_state_8(struct iha_softc *);

static int iha_xfer_data(struct iha_softc *, struct iha_scb *, int);
static int iha_xpad_in(struct iha_softc *);
static int iha_xpad_out(struct iha_softc *);

static int iha_status_msg(struct iha_softc *);
static void iha_busfree(struct iha_softc *);
static int iha_resel(struct iha_softc *);

static int iha_msgin(struct iha_softc *);
static int iha_msgin_extended(struct iha_softc *);
static int iha_msgin_sdtr(struct iha_softc *);
static int iha_msgin_ignore_wid_resid(struct iha_softc *);

static int  iha_msgout(struct iha_softc *, uint8_t);
static void iha_msgout_abort(struct iha_softc *, uint8_t);
static int  iha_msgout_reject(struct iha_softc *);
static int  iha_msgout_extended(struct iha_softc *);
static int  iha_msgout_wdtr(struct iha_softc *);
static int  iha_msgout_sdtr(struct iha_softc *);

static void iha_wide_done(struct iha_softc *);
static void iha_sync_done(struct iha_softc *);

static void iha_bad_seq(struct iha_softc *);

static void iha_read_eeprom(struct iha_softc *, struct iha_eeprom *);
static int iha_se2_rd_all(struct iha_softc *, uint16_t *);
static void iha_se2_instr(struct iha_softc *, int);
static uint16_t iha_se2_rd(struct iha_softc *, int);
#ifdef notused
static void iha_se2_update_all(struct iha_softc *);
static void iha_se2_wr(struct iha_softc *, int, uint16_t);
#endif

/*
 * iha_append_free_scb - append the supplied SCB to the tail of the
 *			 sc_freescb queue after clearing and resetting
 *			 everything possible.
 */
static void
iha_append_free_scb(struct iha_softc *sc, struct iha_scb *scb)
{
	int s;

	s = splbio();

	if (scb == sc->sc_actscb)
		sc->sc_actscb = NULL;

	scb->status = STATUS_QUEUED;
	scb->ha_stat = HOST_OK;
	scb->ta_stat = SCSI_OK;

	scb->nextstat = 0;
	scb->scb_tagmsg = 0;

	scb->xs = NULL;
	scb->tcs = NULL;

	/*
	 * scb_tagid, sg_addr, sglist
	 * SCB_SensePtr are set at initialization
	 * and never change
	 */

	TAILQ_INSERT_TAIL(&sc->sc_freescb, scb, chain);

	splx(s);
}

static void
iha_append_done_scb(struct iha_softc *sc, struct iha_scb *scb, uint8_t hastat)
{
	struct tcs *tcs;
	int s;

	s = splbio();

	if (scb->xs != NULL)
		callout_stop(&scb->xs->xs_callout);

	if (scb == sc->sc_actscb)
		sc->sc_actscb = NULL;

	tcs = scb->tcs;

	if (scb->scb_tagmsg != 0) {
		if (tcs->tagcnt)
			tcs->tagcnt--;
	} else if (tcs->ntagscb == scb)
		tcs->ntagscb = NULL;

	scb->status = STATUS_QUEUED;
	scb->ha_stat = hastat;

	TAILQ_INSERT_TAIL(&sc->sc_donescb, scb, chain);

	splx(s);
}

static inline struct iha_scb *
iha_pop_done_scb(struct iha_softc *sc)
{
	struct iha_scb *scb;
	int s;

	s = splbio();

	scb = TAILQ_FIRST(&sc->sc_donescb);

	if (scb != NULL) {
		scb->status = STATUS_RENT;
		TAILQ_REMOVE(&sc->sc_donescb, scb, chain);
	}

	splx(s);

	return (scb);
}

/*
 * iha_find_pend_scb - scan the pending queue for a SCB that can be
 *		       processed immediately. Return NULL if none found
 *		       and a pointer to the SCB if one is found. If there
 *		       is an active SCB, return NULL!
 */
static struct iha_scb *
iha_find_pend_scb(struct iha_softc *sc)
{
	struct iha_scb *scb;
	struct tcs *tcs;
	int s;

	s = splbio();

	if (sc->sc_actscb != NULL)
		scb = NULL;

	else
		TAILQ_FOREACH(scb, &sc->sc_pendscb, chain) {
			if ((scb->xs->xs_control & XS_CTL_RESET) != 0)
				/* ALWAYS willing to reset a device */
				break;

			tcs = scb->tcs;

			if ((scb->scb_tagmsg) != 0) {
				/*
				 * A Tagged I/O. OK to start If no
				 * non-tagged I/O is active on the same
				 * target
				 */
				if (tcs->ntagscb == NULL)
					break;

			} else	if (scb->cmd[0] == SCSI_REQUEST_SENSE) {
				/*
				 * OK to do a non-tagged request sense
				 * even if a non-tagged I/O has been
				 * started, 'cuz we don't allow any
				 * disconnect during a request sense op
				 */
				break;

			} else	if (tcs->tagcnt == 0) {
				/*
				 * No tagged I/O active on this target,
				 * ok to start a non-tagged one if one
				 * is not already active
				 */
				if (tcs->ntagscb == NULL)
					break;
			}
		}

	splx(s);

	return (scb);
}

static inline void
iha_append_pend_scb(struct iha_softc *sc, struct iha_scb *scb)
{
	/* ASSUMPTION: only called within a splbio()/splx() pair */

	if (scb == sc->sc_actscb)
		sc->sc_actscb = NULL;

	scb->status = STATUS_QUEUED;

	TAILQ_INSERT_TAIL(&sc->sc_pendscb, scb, chain);
}

static inline void
iha_push_pend_scb(struct iha_softc *sc, struct iha_scb *scb)
{
	int s;

	s = splbio();

	if (scb == sc->sc_actscb)
		sc->sc_actscb = NULL;

	scb->status = STATUS_QUEUED;

	TAILQ_INSERT_HEAD(&sc->sc_pendscb, scb, chain);

	splx(s);
}

/*
 * iha_del_pend_scb - remove scb from sc_pendscb
 */
static inline void
iha_del_pend_scb(struct iha_softc *sc, struct iha_scb *scb)
{
	int s;

	s = splbio();

	TAILQ_REMOVE(&sc->sc_pendscb, scb, chain);

	splx(s);
}

static inline void
iha_mark_busy_scb(struct iha_scb *scb)
{
	int  s;

	s = splbio();

	scb->status = STATUS_BUSY;

	if (scb->scb_tagmsg == 0)
		scb->tcs->ntagscb = scb;
	else
		scb->tcs->tagcnt++;

	splx(s);
}

/*
 * iha_set_ssig - read the current scsi signal mask, then write a new
 *		  one which turns off/on the specified signals.
 */
static inline void
iha_set_ssig(struct iha_softc *sc, uint8_t offsigs, uint8_t onsigs)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	uint8_t currsigs;

	currsigs = bus_space_read_1(iot, ioh, TUL_SSIGI);
	bus_space_write_1(iot, ioh, TUL_SSIGO, (currsigs & ~offsigs) | onsigs);
}

/*
 * iha_intr - the interrupt service routine for the iha driver
 */
int
iha_intr(void *arg)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct iha_softc *sc;
	int s;

	sc  = (struct iha_softc *)arg;
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	if ((bus_space_read_1(iot, ioh, TUL_STAT0) & INTPD) == 0)
		return (0);

	s = splbio(); /* XXX - Or are interrupts off when ISR's are called? */

	if (sc->sc_semaph != SEMAPH_IN_MAIN) {
		/* XXX - need these inside a splbio()/splx()? */
		bus_space_write_1(iot, ioh, TUL_IMSK, MASK_ALL);
		sc->sc_semaph = SEMAPH_IN_MAIN;

		iha_main(sc);

		sc->sc_semaph = ~SEMAPH_IN_MAIN;
		bus_space_write_1(iot, ioh, TUL_IMSK, (MASK_ALL & ~MSCMP));
	}

	splx(s);

	return (1);
}

void
iha_attach(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct iha_scb *scb;
	struct iha_eeprom eeprom;
	struct eeprom_adapter *conf;
	int i, error, reg;

	iha_read_eeprom(sc, &eeprom);

	conf = &eeprom.adapter[0];

	/*
	 * fill in the rest of the iha_softc fields
	 */
	sc->sc_id = CFG_ID(conf->config1);
	sc->sc_semaph = ~SEMAPH_IN_MAIN;
	sc->sc_status0 = 0;
	sc->sc_actscb = NULL;

	TAILQ_INIT(&sc->sc_freescb);
	TAILQ_INIT(&sc->sc_pendscb);
	TAILQ_INIT(&sc->sc_donescb);
	error = iha_alloc_sglist(sc);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "cannot allocate sglist\n");
		return;
	}

	sc->sc_scb = malloc(sizeof(struct iha_scb) * IHA_MAX_SCB,
	    M_DEVBUF, M_NOWAIT|M_ZERO);
	if (sc->sc_scb == NULL) {
		aprint_error_dev(sc->sc_dev, "cannot allocate SCB\n");
		return;
	}

	for (i = 0, scb = sc->sc_scb; i < IHA_MAX_SCB; i++, scb++) {
		scb->scb_tagid = i;
		scb->sgoffset = IHA_SG_SIZE * i;
		scb->sglist = sc->sc_sglist + IHA_MAX_SG_ENTRIES * i;
		scb->sg_addr =
		    sc->sc_dmamap->dm_segs[0].ds_addr + scb->sgoffset;

		error = bus_dmamap_create(sc->sc_dmat,
		    MAXPHYS, IHA_MAX_SG_ENTRIES, MAXPHYS, 0,
		    BUS_DMA_NOWAIT, &scb->dmap);

		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "couldn't create SCB DMA map, error = %d\n",
			    error);
			return;
		}
		TAILQ_INSERT_TAIL(&sc->sc_freescb, scb, chain);
	}

	/* Mask all the interrupts */
	bus_space_write_1(iot, ioh, TUL_IMSK, MASK_ALL);

	/* Stop any I/O and reset the scsi module */
	iha_reset_dma(sc);
	bus_space_write_1(iot, ioh, TUL_SCTRL0, RSMOD);

	/* Program HBA's SCSI ID */
	bus_space_write_1(iot, ioh, TUL_SID, sc->sc_id << 4);

	/*
	 * Configure the channel as requested by the NVRAM settings read
	 * by iha_read_eeprom() above.
	 */

	sc->sc_sconf1 = SCONFIG0DEFAULT;
	if ((conf->config1 & CFG_EN_PAR) != 0)
		sc->sc_sconf1 |= SPCHK;
	bus_space_write_1(iot, ioh, TUL_SCONFIG0, sc->sc_sconf1);

	/* set selection time out 250 ms */
	bus_space_write_1(iot, ioh, TUL_STIMO, STIMO_250MS);

	/* Enable desired SCSI termination configuration read from eeprom */
	reg = 0;
	if (conf->config1 & CFG_ACT_TERM1)
		reg |= ENTMW;
	if (conf->config1 & CFG_ACT_TERM2)
		reg |= ENTM;
	bus_space_write_1(iot, ioh, TUL_DCTRL0, reg);

	reg = bus_space_read_1(iot, ioh, TUL_GCTRL1) & ~ATDEN;
	if (conf->config1 & CFG_AUTO_TERM)
		reg |= ATDEN;
	bus_space_write_1(iot, ioh, TUL_GCTRL1, reg);

	for (i = 0; i < IHA_MAX_TARGETS / 2; i++) {
		sc->sc_tcs[i * 2    ].flags = EEP_LBYTE(conf->tflags[i]);
		sc->sc_tcs[i * 2 + 1].flags = EEP_HBYTE(conf->tflags[i]);
		iha_reset_tcs(&sc->sc_tcs[i * 2    ], sc->sc_sconf1);
		iha_reset_tcs(&sc->sc_tcs[i * 2 + 1], sc->sc_sconf1);
	}

	iha_reset_chip(sc);
	bus_space_write_1(iot, ioh, TUL_SIEN, ALL_INTERRUPTS);

	/*
	 * fill in the adapter.
	 */
	sc->sc_adapter.adapt_dev = sc->sc_dev;
	sc->sc_adapter.adapt_nchannels = 1;
	sc->sc_adapter.adapt_openings = IHA_MAX_SCB;
	sc->sc_adapter.adapt_max_periph = IHA_MAX_SCB;
	sc->sc_adapter.adapt_ioctl = NULL;
	sc->sc_adapter.adapt_minphys = minphys;
	sc->sc_adapter.adapt_request = iha_scsipi_request;

	/*
	 * fill in the channel.
	 */
	sc->sc_channel.chan_adapter = &sc->sc_adapter;
	sc->sc_channel.chan_bustype = &scsi_bustype;
	sc->sc_channel.chan_channel = 0;
	sc->sc_channel.chan_ntargets = CFG_TARGET(conf->config2);
	sc->sc_channel.chan_nluns = 8;
	sc->sc_channel.chan_id = sc->sc_id;

	/*
	 * Now try to attach all the sub devices.
	 */
	config_found(sc->sc_dev, &sc->sc_channel, scsiprint);
}

/*
 * iha_alloc_sglist - allocate and map sglist for SCB's
 */
static int
iha_alloc_sglist(struct iha_softc *sc)
{
	bus_dma_segment_t seg;
	int error, rseg;

	/*
	 * Allocate DMA-safe memory for the SCB's sglist
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat,
	    IHA_SG_SIZE * IHA_MAX_SCB,
	    PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		printf(": unable to allocate sglist, error = %d\n", error);
		return (error);
	}
	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	    IHA_SG_SIZE * IHA_MAX_SCB, (void **)&sc->sc_sglist,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		printf(": unable to map sglist, error = %d\n", error);
		return (error);
	}

	/*
	 * Create and load the DMA map used for the SCBs
	 */
	if ((error = bus_dmamap_create(sc->sc_dmat,
	    IHA_SG_SIZE * IHA_MAX_SCB, 1, IHA_SG_SIZE * IHA_MAX_SCB,
	    0, BUS_DMA_NOWAIT, &sc->sc_dmamap)) != 0) {
		printf(": unable to create control DMA map, error = %d\n",
		    error);
		return (error);
	}
	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap,
	    sc->sc_sglist, IHA_SG_SIZE * IHA_MAX_SCB,
	    NULL, BUS_DMA_NOWAIT)) != 0) {
		printf(": unable to load control DMA map, error = %d\n", error);
		return (error);
	}

	memset(sc->sc_sglist, 0, IHA_SG_SIZE * IHA_MAX_SCB);

	return (0);
}

void
iha_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
    void *arg)
{
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	struct iha_scb *scb;
	struct iha_softc *sc;
	int error, s;

	sc = device_private(chan->chan_adapter->adapt_dev);

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;
		periph = xs->xs_periph;

		/* XXX This size isn't actually a hardware restriction. */
		if (xs->cmdlen > sizeof(scb->cmd) ||
		    periph->periph_target >= IHA_MAX_TARGETS) {
			xs->error = XS_DRIVER_STUFFUP;
			scsipi_done(xs);
			return;
		}

		s = splbio();
		scb = TAILQ_FIRST(&sc->sc_freescb);
		if (scb != NULL) {
			scb->status = STATUS_RENT;
			TAILQ_REMOVE(&sc->sc_freescb, scb, chain);
		}
		else {
			printf("unable to allocate scb\n");
#ifdef DIAGNOSTIC
			scsipi_printaddr(periph);
			panic("iha_scsipi_request");
#else
			splx(s);
			return;
#endif
		}
		splx(s);

		scb->target = periph->periph_target;
		scb->lun = periph->periph_lun;
		scb->tcs = &sc->sc_tcs[scb->target];
		scb->scb_id = MSG_IDENTIFY(periph->periph_lun,
		    (xs->xs_control & XS_CTL_REQSENSE) == 0);

		scb->xs = xs;
		scb->cmdlen = xs->cmdlen;
		memcpy(&scb->cmd, xs->cmd, xs->cmdlen);
		scb->buflen = xs->datalen;
		scb->flags = 0;
		if (xs->xs_control & XS_CTL_DATA_OUT)
			scb->flags |= FLAG_DATAOUT;
		if (xs->xs_control & XS_CTL_DATA_IN)
			scb->flags |= FLAG_DATAIN;

		if (scb->flags & (FLAG_DATAIN | FLAG_DATAOUT)) {
			error = bus_dmamap_load(sc->sc_dmat, scb->dmap,
			    xs->data, scb->buflen, NULL,
			    ((xs->xs_control & XS_CTL_NOSLEEP) ?
			     BUS_DMA_NOWAIT : BUS_DMA_WAITOK) |
			    BUS_DMA_STREAMING |
			    ((scb->flags & FLAG_DATAIN) ?
			     BUS_DMA_READ : BUS_DMA_WRITE));

			if (error) {
				printf("%s: error %d loading DMA map\n",
				    device_xname(sc->sc_dev), error);
				iha_append_free_scb(sc, scb);
				xs->error = XS_DRIVER_STUFFUP;
				scsipi_done(xs);
				return;
			}
			bus_dmamap_sync(sc->sc_dmat, scb->dmap,
			    0, scb->dmap->dm_mapsize,
			    (scb->flags & FLAG_DATAIN) ?
			    BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);
		}

		iha_exec_scb(sc, scb);
		return;

	case ADAPTER_REQ_GROW_RESOURCES:
		return; /* XXX */

	case ADAPTER_REQ_SET_XFER_MODE:
		{
			struct tcs *tcs;
			struct scsipi_xfer_mode *xm = arg;

			tcs = &sc->sc_tcs[xm->xm_target];

			if ((xm->xm_mode & PERIPH_CAP_WIDE16) != 0 &&
			    (tcs->flags & FLAG_NO_WIDE) == 0)
				tcs->flags &= ~(FLAG_WIDE_DONE|FLAG_SYNC_DONE);

			if ((xm->xm_mode & PERIPH_CAP_SYNC) != 0 &&
			    (tcs->flags & FLAG_NO_SYNC) == 0)
				tcs->flags &= ~FLAG_SYNC_DONE;

			/*
			 * If we're not going to negotiate, send the
			 * notification now, since it won't happen later.
			 */
			if ((tcs->flags & (FLAG_WIDE_DONE|FLAG_SYNC_DONE)) ==
			    (FLAG_WIDE_DONE|FLAG_SYNC_DONE))
				iha_update_xfer_mode(sc, xm->xm_target);

			return;
		}
	}
}

void
iha_update_xfer_mode(struct iha_softc *sc, int target)
{
	struct tcs *tcs = &sc->sc_tcs[target];
	struct scsipi_xfer_mode xm;

	xm.xm_target = target;
	xm.xm_mode = 0;
	xm.xm_period = 0;
	xm.xm_offset = 0;

	if (tcs->syncm & PERIOD_WIDE_SCSI)
		xm.xm_mode |= PERIPH_CAP_WIDE16;

	if (tcs->period) {
		xm.xm_mode |= PERIPH_CAP_SYNC;
		xm.xm_period = tcs->period;
		xm.xm_offset = tcs->offset;
	}

	scsipi_async_event(&sc->sc_channel, ASYNC_EVENT_XFER_MODE, &xm);
}

static void
iha_reset_scsi_bus(struct iha_softc *sc)
{
	struct iha_scb *scb;
	struct tcs *tcs;
	int i, s;

	s = splbio();

	iha_reset_dma(sc);

	for (i = 0, scb = sc->sc_scb; i < IHA_MAX_SCB; i++, scb++)
		switch (scb->status) {
		case STATUS_BUSY:
			iha_append_done_scb(sc, scb, HOST_SCSI_RST);
			break;

		case STATUS_SELECT:
			iha_push_pend_scb(sc, scb);
			break;

		default:
			break;
		}

	for (i = 0, tcs = sc->sc_tcs; i < IHA_MAX_TARGETS; i++, tcs++)
		iha_reset_tcs(tcs, sc->sc_sconf1);

	splx(s);
}

void
iha_reset_chip(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/* reset tulip chip */

	bus_space_write_1(iot, ioh, TUL_SCTRL0, RSCSI);

	do {
		sc->sc_sistat = bus_space_read_1(iot, ioh, TUL_SISTAT);
	} while ((sc->sc_sistat & SRSTD) == 0);

	iha_set_ssig(sc, 0, 0);

	/* Clear any active interrupt*/
	(void)bus_space_read_1(iot, ioh, TUL_SISTAT);
}

/*
 * iha_reset_dma - abort any active DMA xfer, reset tulip FIFO.
 */
static void
iha_reset_dma(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	if ((bus_space_read_1(iot, ioh, TUL_ISTUS1) & XPEND) != 0) {
		/* if DMA xfer is pending, abort DMA xfer */
		bus_space_write_1(iot, ioh, TUL_DCMD, ABTXFR);
		/* wait Abort DMA xfer done */
		while ((bus_space_read_1(iot, ioh, TUL_ISTUS0) & DABT) == 0)
			;
	}

	bus_space_write_1(iot, ioh, TUL_SCTRL0, RSFIFO);
}

/*
 * iha_reset_tcs - reset the target control structure pointed
 *		   to by tcs to default values. tcs flags
 *		   only has the negotiation done bits reset as
 *		   the other bits are fixed at initialization.
 */
static void
iha_reset_tcs(struct tcs *tcs, uint8_t config0)
{

	tcs->flags &= ~(FLAG_SYNC_DONE | FLAG_WIDE_DONE);
	tcs->period = 0;
	tcs->offset = 0;
	tcs->tagcnt = 0;
	tcs->ntagscb  = NULL;
	tcs->syncm = 0;
	tcs->sconfig0 = config0;
}

/*
 * iha_main - process the active SCB, taking one off pending and making it
 *	      active if necessary, and any done SCB's created as
 *	      a result until there are no interrupts pending and no pending
 *	      SCB's that can be started.
 */
static void
iha_main(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh =sc->sc_ioh;
	struct iha_scb *scb;

	for (;;) {
		iha_scsi(sc);

		while ((scb = iha_pop_done_scb(sc)) != NULL)
			iha_done_scb(sc, scb);

		/*
		 * If there are no interrupts pending, or we can't start
		 * a pending sc, break out of the for(;;). Otherwise
		 * continue the good work with another call to
		 * iha_scsi().
		 */
		if (((bus_space_read_1(iot, ioh, TUL_STAT0) & INTPD) == 0)
		    && (iha_find_pend_scb(sc) == NULL))
			break;
	}
}

/*
 * iha_scsi - service any outstanding interrupts. If there are none, try to
 *            start another SCB currently in the pending queue.
 */
static void
iha_scsi(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct iha_scb *scb;
	struct tcs *tcs;
	uint8_t stat;

	/* service pending interrupts asap */

	stat = bus_space_read_1(iot, ioh, TUL_STAT0);
	if ((stat & INTPD) != 0) {
		sc->sc_status0 = stat;
		sc->sc_status1 = bus_space_read_1(iot, ioh, TUL_STAT1);
		sc->sc_sistat = bus_space_read_1(iot, ioh, TUL_SISTAT);

		sc->sc_phase = sc->sc_status0 & PH_MASK;

		if ((sc->sc_sistat & SRSTD) != 0) {
			iha_reset_scsi_bus(sc);
			return;
		}

		if ((sc->sc_sistat & RSELED) != 0) {
			iha_resel(sc);
			return;
		}

		if ((sc->sc_sistat & (STIMEO | DISCD)) != 0) {
			iha_busfree(sc);
			return;
		}

		if ((sc->sc_sistat & (SCMDN | SBSRV)) != 0) {
			iha_next_state(sc);
			return;
		}

		if ((sc->sc_sistat & SELED) != 0)
			iha_set_ssig(sc, 0, 0);
	}

	/*
	 * There were no interrupts pending which required action elsewhere, so
	 * see if it is possible to start the selection phase on a pending SCB
	 */
	if ((scb = iha_find_pend_scb(sc)) == NULL)
		return;

	tcs = scb->tcs;

	/* program HBA's SCSI ID & target SCSI ID */
	bus_space_write_1(iot, ioh, TUL_SID, (sc->sc_id << 4) | scb->target);

	if ((scb->xs->xs_control & XS_CTL_RESET) == 0) {
		bus_space_write_1(iot, ioh, TUL_SYNCM, tcs->syncm);

		if ((tcs->flags & FLAG_NO_NEG_SYNC) == 0 ||
		    (tcs->flags & FLAG_NO_NEG_WIDE) == 0)
			iha_select(sc, scb, SELATNSTOP);

		else if (scb->scb_tagmsg != 0)
			iha_select(sc, scb, SEL_ATN3);

		else
			iha_select(sc, scb, SEL_ATN);

	} else {
		iha_select(sc, scb, SELATNSTOP);
		scb->nextstat = 8;
	}

	if ((scb->xs->xs_control & XS_CTL_POLL) != 0) {
		int timeout;
		for (timeout = scb->xs->timeout; timeout > 0; timeout--) {
			if (iha_wait(sc, NO_OP) == -1)
				break;
			if (iha_next_state(sc) == -1)
				break;
			delay(1000); /* Only happens in boot, so it's ok */
		}

		/*
		 * Since done queue processing not done until AFTER this
		 * function returns, scb is on the done queue, not
		 * the free queue at this point and still has valid data
		 *
		 * Conversely, xs->error has not been set yet
		 */
		if (timeout == 0)
			iha_timeout(scb);
	}
}

static void
iha_select(struct iha_softc *sc, struct iha_scb *scb, uint8_t select_type)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	switch (select_type) {
	case SEL_ATN:
		bus_space_write_1(iot, ioh, TUL_SFIFO, scb->scb_id);
		bus_space_write_multi_1(iot, ioh, TUL_SFIFO,
		    scb->cmd, scb->cmdlen);

		scb->nextstat = 2;
		break;

	case SELATNSTOP:
		scb->nextstat = 1;
		break;

	case SEL_ATN3:
		bus_space_write_1(iot, ioh, TUL_SFIFO, scb->scb_id);
		bus_space_write_1(iot, ioh, TUL_SFIFO, scb->scb_tagmsg);
		bus_space_write_1(iot, ioh, TUL_SFIFO, scb->scb_tagid);

		bus_space_write_multi_1(iot, ioh, TUL_SFIFO, scb->cmd,
		    scb->cmdlen);

		scb->nextstat = 2;
		break;

	default:
		printf("[debug] iha_select() - unknown select type = 0x%02x\n",
		    select_type);
		return;
	}

	iha_del_pend_scb(sc, scb);
	scb->status = STATUS_SELECT;

	sc->sc_actscb = scb;

	bus_space_write_1(iot, ioh, TUL_SCMD, select_type);
}

/*
 * iha_wait - wait for an interrupt to service or a SCSI bus phase change
 *            after writing the supplied command to the tulip chip. If
 *            the command is NO_OP, skip the command writing.
 */
static int
iha_wait(struct iha_softc *sc, uint8_t cmd)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	if (cmd != NO_OP)
		bus_space_write_1(iot, ioh, TUL_SCMD, cmd);

	/*
	 * Have to do this here, in addition to in iha_isr, because
	 * interrupts might be turned off when we get here.
	 */
	do {
		sc->sc_status0 = bus_space_read_1(iot, ioh, TUL_STAT0);
	} while ((sc->sc_status0 & INTPD) == 0);

	sc->sc_status1 = bus_space_read_1(iot, ioh, TUL_STAT1);
	sc->sc_sistat = bus_space_read_1(iot, ioh, TUL_SISTAT);

	sc->sc_phase = sc->sc_status0 & PH_MASK;

	if ((sc->sc_sistat & SRSTD) != 0) {
		/* SCSI bus reset interrupt */
		iha_reset_scsi_bus(sc);
		return (-1);
	}

	if ((sc->sc_sistat & RSELED) != 0)
		/* Reselection interrupt */
		return (iha_resel(sc));

	if ((sc->sc_sistat & STIMEO) != 0) {
		/* selected/reselected timeout interrupt */
		iha_busfree(sc);
		return (-1);
	}

	if ((sc->sc_sistat & DISCD) != 0) {
		/* BUS disconnection interrupt */
		if ((sc->sc_flags & FLAG_EXPECT_DONE_DISC) != 0) {
			bus_space_write_1(iot, ioh, TUL_SCTRL0, RSFIFO);
			bus_space_write_1(iot, ioh, TUL_SCONFIG0,
			    SCONFIG0DEFAULT);
			bus_space_write_1(iot, ioh, TUL_SCTRL1, EHRSL);
			iha_append_done_scb(sc, sc->sc_actscb, HOST_OK);
			sc->sc_flags &= ~FLAG_EXPECT_DONE_DISC;

		} else if ((sc->sc_flags & FLAG_EXPECT_DISC) != 0) {
			bus_space_write_1(iot, ioh, TUL_SCTRL0, RSFIFO);
			bus_space_write_1(iot, ioh, TUL_SCONFIG0,
			    SCONFIG0DEFAULT);
			bus_space_write_1(iot, ioh, TUL_SCTRL1, EHRSL);
			sc->sc_actscb = NULL;
			sc->sc_flags &= ~FLAG_EXPECT_DISC;

		} else
			iha_busfree(sc);

		return (-1);
	}

	return (sc->sc_phase);
}

static void
iha_exec_scb(struct iha_softc *sc, struct iha_scb *scb)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_dmamap_t dm;
	struct scsipi_xfer *xs = scb->xs;
	int nseg, s;

	dm = scb->dmap;
	nseg = dm->dm_nsegs;

	if (nseg > 1) {
		struct iha_sg_element *sg = scb->sglist;
		int i;

		for (i = 0; i < nseg; i++) {
			sg[i].sg_len = htole32(dm->dm_segs[i].ds_len);
			sg[i].sg_addr = htole32(dm->dm_segs[i].ds_addr);
		}
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    scb->sgoffset, IHA_SG_SIZE,
		    BUS_DMASYNC_PREWRITE);

		scb->flags |= FLAG_SG;
		scb->sg_size = scb->sg_max = nseg;
		scb->sg_index = 0;

		scb->bufaddr = scb->sg_addr;
	} else
		scb->bufaddr = dm->dm_segs[0].ds_addr;

	if ((xs->xs_control & XS_CTL_POLL) == 0) {
		int timeout = mstohz(xs->timeout);
		if (timeout == 0)
			timeout = 1;
		callout_reset(&xs->xs_callout, timeout, iha_timeout, scb);
	}

	s = splbio();

	if (((scb->xs->xs_control & XS_RESET) != 0) ||
	    (scb->cmd[0] == SCSI_REQUEST_SENSE))
		iha_push_pend_scb(sc, scb);   /* Insert SCB at head of Pend */
	else
		iha_append_pend_scb(sc, scb); /* Append SCB to tail of Pend */

	/*
	 * Run through iha_main() to ensure something is active, if
	 * only this new SCB.
	 */
	if (sc->sc_semaph != SEMAPH_IN_MAIN) {
		iot = sc->sc_iot;
		ioh = sc->sc_ioh;

		bus_space_write_1(iot, ioh, TUL_IMSK, MASK_ALL);
		sc->sc_semaph = SEMAPH_IN_MAIN;

		splx(s);
		iha_main(sc);
		s = splbio();

		sc->sc_semaph = ~SEMAPH_IN_MAIN;
		bus_space_write_1(iot, ioh, TUL_IMSK, (MASK_ALL & ~MSCMP));
	}

	splx(s);
}

/*
 * iha_done_scb - We have a scb which has been processed by the
 *                adaptor, now we look to see how the operation went.
 */
static void
iha_done_scb(struct iha_softc *sc, struct iha_scb *scb)
{
	struct scsipi_xfer *xs = scb->xs;

	if (xs != NULL) {
		/* Cancel the timeout. */
		callout_stop(&xs->xs_callout);

		if (scb->flags & (FLAG_DATAIN | FLAG_DATAOUT)) {
			bus_dmamap_sync(sc->sc_dmat, scb->dmap,
			    0, scb->dmap->dm_mapsize,
			    (scb->flags & FLAG_DATAIN) ?
			    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, scb->dmap);
		}

		xs->status = scb->ta_stat;

		switch (scb->ha_stat) {
		case HOST_OK:
			switch (scb->ta_stat) {
			case SCSI_OK:
			case SCSI_CONDITION_MET:
			case SCSI_INTERM:
			case SCSI_INTERM_COND_MET:
				xs->resid = scb->buflen;
				xs->error = XS_NOERROR;
				if ((scb->flags & FLAG_RSENS) != 0)
					xs->error = XS_SENSE;
				break;

			case SCSI_RESV_CONFLICT:
			case SCSI_BUSY:
			case SCSI_QUEUE_FULL:
				xs->error = XS_BUSY;
				break;

			case SCSI_TERMINATED:
			case SCSI_ACA_ACTIVE:
			case SCSI_CHECK:
				scb->tcs->flags &=
				    ~(FLAG_SYNC_DONE | FLAG_WIDE_DONE);

				if ((scb->flags & FLAG_RSENS) != 0 ||
				    iha_push_sense_request(sc, scb) != 0) {
					scb->flags &= ~FLAG_RSENS;
					printf("%s: request sense failed\n",
					    device_xname(sc->sc_dev));
					xs->error = XS_DRIVER_STUFFUP;
					break;
				}

				xs->error = XS_SENSE;
				return;

			default:
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
			break;

		case HOST_SEL_TOUT:
			xs->error = XS_SELTIMEOUT;
			break;

		case HOST_SCSI_RST:
		case HOST_DEV_RST:
			xs->error = XS_RESET;
			break;

		case HOST_SPERR:
			printf("%s: SCSI Parity error detected\n",
			    device_xname(sc->sc_dev));
			xs->error = XS_DRIVER_STUFFUP;
			break;

		case HOST_TIMED_OUT:
			xs->error = XS_TIMEOUT;
			break;

		case HOST_DO_DU:
		case HOST_BAD_PHAS:
		default:
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}

		scsipi_done(xs);
	}

	iha_append_free_scb(sc, scb);
}

/*
 * iha_push_sense_request - obtain auto sense data by pushing the
 *			    SCB needing it back onto the pending
 *			    queue with a REQUEST_SENSE CDB.
 */
static int
iha_push_sense_request(struct iha_softc *sc, struct iha_scb *scb)
{
	struct scsipi_xfer *xs = scb->xs;
	struct scsipi_periph *periph = xs->xs_periph;
	struct scsi_request_sense *ss = (struct scsi_request_sense *)scb->cmd;
	int lun = periph->periph_lun;
	int err;

	memset(ss, 0, sizeof(*ss));
	ss->opcode = SCSI_REQUEST_SENSE;
	ss->byte2 = lun << SCSI_CMD_LUN_SHIFT;
	ss->length = sizeof(struct scsi_sense_data);

	scb->flags = FLAG_RSENS | FLAG_DATAIN;

	scb->scb_id &= ~MSG_IDENTIFY_DISCFLAG;

	scb->scb_tagmsg = 0;
	scb->ta_stat = SCSI_OK;

	scb->cmdlen = sizeof(struct scsi_request_sense);
	scb->buflen = ss->length;

	err = bus_dmamap_load(sc->sc_dmat, scb->dmap,
	    &xs->sense.scsi_sense, scb->buflen, NULL,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (err != 0) {
		printf("iha_push_sense_request: cannot bus_dmamap_load()\n");
		xs->error = XS_DRIVER_STUFFUP;
		return 1;
	}
	bus_dmamap_sync(sc->sc_dmat, scb->dmap,
	    0, scb->buflen, BUS_DMASYNC_PREREAD);

	/* XXX What about queued command? */
	iha_exec_scb(sc, scb);

	return 0;
}

static void
iha_timeout(void *arg)
{
	struct iha_scb *scb = (struct iha_scb *)arg;
	struct scsipi_xfer *xs = scb->xs;
	struct scsipi_periph *periph;
	struct iha_softc *sc;

	if (xs == NULL) {
		printf("[debug] iha_timeout called with xs == NULL\n");
		return;
	}

	periph = xs->xs_periph;

	sc = device_private(periph->periph_channel->chan_adapter->adapt_dev);

	scsipi_printaddr(periph);
	printf("SCSI OpCode 0x%02x timed out\n", xs->cmd->opcode);
	iha_abort_xs(sc, xs, HOST_TIMED_OUT);
}

/*
 * iha_abort_xs - find the SCB associated with the supplied xs and
 *                stop all processing on it, moving it to the done
 *                queue with the supplied host status value.
 */
static void
iha_abort_xs(struct iha_softc *sc, struct scsipi_xfer *xs, uint8_t hastat)
{
	struct iha_scb *scb;
	int i, s;

	s = splbio();

	/* Check the pending queue for the SCB pointing to xs */

	TAILQ_FOREACH(scb, &sc->sc_pendscb, chain)
		if (scb->xs == xs) {
			iha_del_pend_scb(sc, scb);
			iha_append_done_scb(sc, scb, hastat);
			splx(s);
			return;
		}

	/*
	 * If that didn't work, check all BUSY/SELECTING SCB's for one
	 * pointing to xs
	 */

	for (i = 0, scb = sc->sc_scb; i < IHA_MAX_SCB; i++, scb++)
		switch (scb->status) {
		case STATUS_BUSY:
		case STATUS_SELECT:
			if (scb->xs == xs) {
				iha_append_done_scb(sc, scb, hastat);
				splx(s);
				return;
			}
			break;
		default:
			break;
		}

	splx(s);
}

/*
 * iha_data_over_run - return HOST_OK for all SCSI opcodes where BufLen
 *		       is an 'Allocation Length'. All other SCSI opcodes
 *		       get HOST_DO_DU as they SHOULD have xferred all the
 *		       data requested.
 *
 *		       The list of opcodes using 'Allocation Length' was
 *		       found by scanning all the SCSI-3 T10 drafts. See
 *		       www.t10.org for the curious with a .pdf reader.
 */
static uint8_t
iha_data_over_run(struct iha_scb *scb)
{
	switch (scb->cmd[0]) {
	case 0x03: /* Request Sense                   SPC-2 */
	case 0x12: /* Inquiry                         SPC-2 */
	case 0x1a: /* Mode Sense (6 byte version)     SPC-2 */
	case 0x1c: /* Receive Diagnostic Results      SPC-2 */
	case 0x23: /* Read Format Capacities          MMC-2 */
	case 0x29: /* Read Generation                 SBC   */
	case 0x34: /* Read Position                   SSC-2 */
	case 0x37: /* Read Defect Data                SBC   */
	case 0x3c: /* Read Buffer                     SPC-2 */
	case 0x42: /* Read Sub Channel                MMC-2 */
	case 0x43: /* Read TOC/PMA/ATIP               MMC   */

	/* XXX - 2 with same opcode of 0x44? */
	case 0x44: /* Read Header/Read Density Suprt  MMC/SSC*/

	case 0x46: /* Get Configuration               MMC-2 */
	case 0x4a: /* Get Event/Status Notification   MMC-2 */
	case 0x4d: /* Log Sense                       SPC-2 */
	case 0x51: /* Read Disc Information           MMC   */
	case 0x52: /* Read Track Information          MMC   */
	case 0x59: /* Read Master CUE                 MMC   */
	case 0x5a: /* Mode Sense (10 byte version)    SPC-2 */
	case 0x5c: /* Read Buffer Capacity            MMC   */
	case 0x5e: /* Persistent Reserve In           SPC-2 */
	case 0x84: /* Receive Copy Results            SPC-2 */
	case 0xa0: /* Report LUNs                     SPC-2 */
	case 0xa3: /* Various Report requests         SBC-2/SCC-2*/
	case 0xa4: /* Report Key                      MMC-2 */
	case 0xad: /* Read DVD Structure              MMC-2 */
	case 0xb4: /* Read Element Status (Attached)  SMC   */
	case 0xb5: /* Request Volume Element Address  SMC   */
	case 0xb7: /* Read Defect Data (12 byte ver.) SBC   */
	case 0xb8: /* Read Element Status (Independ.) SMC   */
	case 0xba: /* Report Redundancy               SCC-2 */
	case 0xbd: /* Mechanism Status                MMC   */
	case 0xbe: /* Report Basic Redundancy         SCC-2 */

		return (HOST_OK);

	default:
		return (HOST_DO_DU);
	}
}

/*
 * iha_next_state - process the current SCB as requested in its
 *                  nextstat member.
 */
static int
iha_next_state(struct iha_softc *sc)
{

	if (sc->sc_actscb == NULL)
		return (-1);

	switch (sc->sc_actscb->nextstat) {
	case 1:
		if (iha_state_1(sc) == 3)
			goto state_3;
		break;

	case 2:
		switch (iha_state_2(sc)) {
		case 3:
			goto state_3;
		case 4:
			goto state_4;
		default:
			break;
		}
		break;

	case 3:
	state_3:
		if (iha_state_3(sc) == 4)
			goto state_4;
		break;

	case 4:
	state_4:
		switch (iha_state_4(sc)) {
		case 0:
			return (0);
		case 6:
			goto state_6;
		default:
			break;
		}
		break;

	case 5:
		switch (iha_state_5(sc)) {
		case 4:
			goto state_4;
		case 6:
			goto state_6;
		default:
			break;
		}
		break;

	case 6:
	state_6:
		iha_state_6(sc);
		break;

	case 8:
		iha_state_8(sc);
		break;

	default:
#ifdef IHA_DEBUG_STATE
		printf("[debug] -unknown state: %i-\n",
		    sc->sc_actscb->nextstat);
#endif
		iha_bad_seq(sc);
		break;
	}

	return (-1);
}

/*
 * iha_state_1 - selection is complete after a SELATNSTOP. If the target
 *               has put the bus into MSG_OUT phase start wide/sync
 *               negotiation. Otherwise clear the FIFO and go to state 3,
 *	    	 which will send the SCSI CDB to the target.
 */
static int
iha_state_1(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct iha_scb *scb = sc->sc_actscb;
	struct tcs *tcs;
	int flags;

	iha_mark_busy_scb(scb);

	tcs = scb->tcs;

	bus_space_write_1(iot, ioh, TUL_SCONFIG0, tcs->sconfig0);

	/*
	 * If we are in PHASE_MSG_OUT, send
	 *     a) IDENT message (with tags if appropriate)
	 *     b) WDTR if the target is configured to negotiate wide xfers
	 *     ** OR **
	 *     c) SDTR if the target is configured to negotiate sync xfers
	 *	  but not wide ones
	 *
	 * If we are NOT, then the target is not asking for anything but
	 * the data/command, so go straight to state 3.
	 */
	if (sc->sc_phase == PHASE_MSG_OUT) {
		bus_space_write_1(iot, ioh, TUL_SCTRL1, (ESBUSIN | EHRSL));
		bus_space_write_1(iot, ioh, TUL_SFIFO, scb->scb_id);

		if (scb->scb_tagmsg != 0) {
			bus_space_write_1(iot, ioh, TUL_SFIFO,
			    scb->scb_tagmsg);
			bus_space_write_1(iot, ioh, TUL_SFIFO,
			    scb->scb_tagid);
		}

		flags = tcs->flags;
		if ((flags & FLAG_NO_NEG_WIDE) == 0) {
			if (iha_msgout_wdtr(sc) == -1)
				return (-1);
		} else if ((flags & FLAG_NO_NEG_SYNC) == 0) {
			if (iha_msgout_sdtr(sc) == -1)
				return (-1);
		}

	} else {
		bus_space_write_1(iot, ioh, TUL_SCTRL0, RSFIFO);
		iha_set_ssig(sc, REQ | BSY | SEL | ATN, 0);
	}

	return (3);
}

/*
 * iha_state_2 - selection is complete after a SEL_ATN or SEL_ATN3. If the SCSI
 *		 CDB has already been send, go to state 4 to start the data
 *		 xfer. Otherwise reset the FIFO and go to state 3, sending
 *		 the SCSI CDB.
 */
static int
iha_state_2(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct iha_scb *scb = sc->sc_actscb;

	iha_mark_busy_scb(scb);

	bus_space_write_1(iot, ioh, TUL_SCONFIG0, scb->tcs->sconfig0);

	if ((sc->sc_status1 & CPDNE) != 0)
		return (4);

	bus_space_write_1(iot, ioh, TUL_SCTRL0, RSFIFO);

	iha_set_ssig(sc, REQ | BSY | SEL | ATN, 0);

	return (3);
}

/*
 * iha_state_3 - send the SCSI CDB to the target, processing any status
 *		 or other messages received until that is done or
 *		 abandoned.
 */
static int
iha_state_3(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct iha_scb *scb = sc->sc_actscb;
	int flags;

	for (;;) {
		switch (sc->sc_phase) {
		case PHASE_CMD_OUT:
			bus_space_write_multi_1(iot, ioh, TUL_SFIFO,
			    scb->cmd, scb->cmdlen);
			if (iha_wait(sc, XF_FIFO_OUT) == -1)
				return (-1);
			else if (sc->sc_phase == PHASE_CMD_OUT) {
				iha_bad_seq(sc);
				return (-1);
			} else
				return (4);

		case PHASE_MSG_IN:
			scb->nextstat = 3;
			if (iha_msgin(sc) == -1)
				return (-1);
			break;

		case PHASE_STATUS_IN:
			if (iha_status_msg(sc) == -1)
				return (-1);
			break;

		case PHASE_MSG_OUT:
			flags = scb->tcs->flags;
			if ((flags & FLAG_NO_NEG_SYNC) != 0) {
				if (iha_msgout(sc, MSG_NOOP) == -1)
					return (-1);
			} else if (iha_msgout_sdtr(sc) == -1)
				return (-1);
			break;

		default:
			printf("[debug] -s3- bad phase = %d\n", sc->sc_phase);
			iha_bad_seq(sc);
			return (-1);
		}
	}
}

/*
 * iha_state_4 - start a data xfer. Handle any bus state
 *               transitions until PHASE_DATA_IN/_OUT
 *               or the attempt is abandoned. If there is
 *               no data to xfer, go to state 6 and finish
 *               processing the current SCB.
 */
static int
iha_state_4(struct iha_softc *sc)
{
	struct iha_scb *scb = sc->sc_actscb;

	if ((scb->flags & (FLAG_DATAIN | FLAG_DATAOUT)) ==
	    (FLAG_DATAIN | FLAG_DATAOUT))
		return (6); /* Both dir flags set => NO xfer was requested */

	for (;;) {
		if (scb->buflen == 0)
			return (6);

		switch (sc->sc_phase) {
		case PHASE_STATUS_IN:
			if ((scb->flags & (FLAG_DATAIN | FLAG_DATAOUT)) != 0)
				scb->ha_stat = iha_data_over_run(scb);
			if ((iha_status_msg(sc)) == -1)
				return (-1);
			break;

		case PHASE_MSG_IN:
			scb->nextstat = 4;
			if (iha_msgin(sc) == -1)
				return (-1);
			break;

		case PHASE_MSG_OUT:
			if ((sc->sc_status0 & SPERR) != 0) {
				scb->buflen = 0;
				scb->ha_stat = HOST_SPERR;
				if (iha_msgout(sc, MSG_INITIATOR_DET_ERR) == -1)
					return (-1);
				else
					return (6);
			} else {
				if (iha_msgout(sc, MSG_NOOP) == -1)
					return (-1);
			}
			break;

		case PHASE_DATA_IN:
			return (iha_xfer_data(sc, scb, FLAG_DATAIN));

		case PHASE_DATA_OUT:
			return (iha_xfer_data(sc, scb, FLAG_DATAOUT));

		default:
			iha_bad_seq(sc);
			return (-1);
		}
	}
}

/*
 * iha_state_5 - handle the partial or final completion of the current
 *		 data xfer. If DMA is still active stop it. If there is
 *		 more data to xfer, go to state 4 and start the xfer.
 *		 If not go to state 6 and finish the SCB.
 */
static int
iha_state_5(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct iha_scb *scb = sc->sc_actscb;
	struct iha_sg_element *sg;
	uint32_t cnt;
	uint8_t period, stat;
	long xcnt;  /* cannot use unsigned!! see code: if (xcnt < 0) */
	int i;

	cnt = bus_space_read_4(iot, ioh, TUL_STCNT0) & TCNT;

	/*
	 * Stop any pending DMA activity and check for parity error.
	 */

	if ((bus_space_read_1(iot, ioh, TUL_DCMD) & XDIR) != 0) {
		/* Input Operation */
		if ((sc->sc_status0 & SPERR) != 0)
			scb->ha_stat = HOST_SPERR;

		if ((bus_space_read_1(iot, ioh, TUL_ISTUS1) & XPEND) != 0) {
			bus_space_write_1(iot, ioh, TUL_DCTRL0,
			    bus_space_read_1(iot, ioh, TUL_DCTRL0) | SXSTP);
			while (bus_space_read_1(iot, ioh, TUL_ISTUS1) & XPEND)
				;
		}

	} else {
		/* Output Operation */
		if ((sc->sc_status1 & SXCMP) == 0) {
			period = scb->tcs->syncm;
			if ((period & PERIOD_WIDE_SCSI) != 0)
				cnt += (bus_space_read_1(iot, ioh,
				    TUL_SFIFOCNT) & FIFOC) * 2;
			else
				cnt += bus_space_read_1(iot, ioh,
				    TUL_SFIFOCNT) & FIFOC;
		}

		if ((bus_space_read_1(iot, ioh, TUL_ISTUS1) & XPEND) != 0) {
			bus_space_write_1(iot, ioh, TUL_DCMD, ABTXFR);
			do
				stat = bus_space_read_1(iot, ioh, TUL_ISTUS0);
			while ((stat & DABT) == 0);
		}

		if ((cnt == 1) && (sc->sc_phase == PHASE_DATA_OUT)) {
			if (iha_wait(sc, XF_FIFO_OUT) == -1)
				return (-1);
			cnt = 0;

		} else if ((sc->sc_status1 & SXCMP) == 0)
			bus_space_write_1(iot, ioh, TUL_SCTRL0, RSFIFO);
	}

	if (cnt == 0) {
		scb->buflen = 0;
		return (6);
	}

	/* Update active data pointer and restart the I/O at the new point */

	xcnt = scb->buflen - cnt;	/* xcnt == bytes xferred */
	scb->buflen = cnt;	  	/* cnt  == bytes left    */

	if ((scb->flags & FLAG_SG) != 0) {
		sg = &scb->sglist[scb->sg_index];
		for (i = scb->sg_index; i < scb->sg_max; sg++, i++) {
			xcnt -= le32toh(sg->sg_len);
			if (xcnt < 0) {
				xcnt += le32toh(sg->sg_len);

				sg->sg_addr =
				    htole32(le32toh(sg->sg_addr) + xcnt);
				sg->sg_len =
				    htole32(le32toh(sg->sg_len) - xcnt);
				bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
				    scb->sgoffset, IHA_SG_SIZE,
				    BUS_DMASYNC_PREWRITE);

				scb->bufaddr += (i - scb->sg_index) *
				    sizeof(struct iha_sg_element);
				scb->sg_size = scb->sg_max - i;
				scb->sg_index = i;

				return (4);
			}
		}
		return (6);

	} else
		scb->bufaddr += xcnt;

	return (4);
}

/*
 * iha_state_6 - finish off the active scb (may require several
 *		 iterations if PHASE_MSG_IN) and return -1 to indicate
 *		 the bus is free.
 */
static int
iha_state_6(struct iha_softc *sc)
{

	for (;;) {
		switch (sc->sc_phase) {
		case PHASE_STATUS_IN:
			if (iha_status_msg(sc) == -1)
				return (-1);
			break;

		case PHASE_MSG_IN:
			sc->sc_actscb->nextstat = 6;
			if ((iha_msgin(sc)) == -1)
				return (-1);
			break;

		case PHASE_MSG_OUT:
			if ((iha_msgout(sc, MSG_NOOP)) == -1)
				return (-1);
			break;

		case PHASE_DATA_IN:
			if (iha_xpad_in(sc) == -1)
				return (-1);
			break;

		case PHASE_DATA_OUT:
			if (iha_xpad_out(sc) == -1)
				return (-1);
			break;

		default:
			iha_bad_seq(sc);
			return (-1);
		}
	}
}

/*
 * iha_state_8 - reset the active device and all busy SCBs using it
 */
static int
iha_state_8(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct iha_scb *scb;
	int i;
	uint8_t tar;

	if (sc->sc_phase == PHASE_MSG_OUT) {
		bus_space_write_1(iot, ioh, TUL_SFIFO, MSG_BUS_DEV_RESET);

		scb = sc->sc_actscb;

		/* This SCB finished correctly -- resetting the device */
		iha_append_done_scb(sc, scb, HOST_OK);

		iha_reset_tcs(scb->tcs, sc->sc_sconf1);

		tar = scb->target;
		for (i = 0, scb = sc->sc_scb; i < IHA_MAX_SCB; i++, scb++)
			if (scb->target == tar)
				switch (scb->status) {
				case STATUS_BUSY:
					iha_append_done_scb(sc,
					    scb, HOST_DEV_RST);
					break;

				case STATUS_SELECT:
					iha_push_pend_scb(sc, scb);
					break;

				default:
					break;
				}

		sc->sc_flags |= FLAG_EXPECT_DISC;

		if (iha_wait(sc, XF_FIFO_OUT) == -1)
			return (-1);
	}

	iha_bad_seq(sc);
	return (-1);
}

/*
 * iha_xfer_data - initiate the DMA xfer of the data
 */
static int
iha_xfer_data(struct iha_softc *sc, struct iha_scb *scb, int direction)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	uint32_t xferlen;
	uint8_t xfercmd;

	if ((scb->flags & (FLAG_DATAIN | FLAG_DATAOUT)) != direction)
		return (6); /* wrong direction, abandon I/O */

	bus_space_write_4(iot, ioh, TUL_STCNT0, scb->buflen);

	xfercmd = STRXFR;
	if (direction == FLAG_DATAIN)
		xfercmd |= XDIR;

	if (scb->flags & FLAG_SG) {
		xferlen = scb->sg_size * sizeof(struct iha_sg_element);
		xfercmd |= SGXFR;
	} else
		xferlen = scb->buflen;

	bus_space_write_4(iot, ioh, TUL_DXC,  xferlen);
	bus_space_write_4(iot, ioh, TUL_DXPA, scb->bufaddr);
	bus_space_write_1(iot, ioh, TUL_DCMD, xfercmd);

	bus_space_write_1(iot, ioh, TUL_SCMD,
	    (direction == FLAG_DATAIN) ? XF_DMA_IN : XF_DMA_OUT);

	scb->nextstat = 5;

	return (0);
}

static int
iha_xpad_in(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct iha_scb *scb = sc->sc_actscb;

	if ((scb->flags & (FLAG_DATAIN | FLAG_DATAOUT)) != 0)
		scb->ha_stat = HOST_DO_DU;

	for (;;) {
		if ((scb->tcs->syncm & PERIOD_WIDE_SCSI) != 0)
			bus_space_write_4(iot, ioh, TUL_STCNT0, 2);
		else
			bus_space_write_4(iot, ioh, TUL_STCNT0, 1);

		switch (iha_wait(sc, XF_FIFO_IN)) {
		case -1:
			return (-1);

		case PHASE_DATA_IN:
			(void)bus_space_read_1(iot, ioh, TUL_SFIFO);
			break;

		default:
			bus_space_write_1(iot, ioh, TUL_SCTRL0, RSFIFO);
			return (6);
		}
	}
}

static int
iha_xpad_out(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct iha_scb *scb = sc->sc_actscb;

	if ((scb->flags & (FLAG_DATAIN | FLAG_DATAOUT)) != 0)
		scb->ha_stat = HOST_DO_DU;

	for (;;) {
		if ((scb->tcs->syncm & PERIOD_WIDE_SCSI) != 0)
			bus_space_write_4(iot, ioh, TUL_STCNT0, 2);
		else
			bus_space_write_4(iot, ioh, TUL_STCNT0, 1);

		bus_space_write_1(iot, ioh, TUL_SFIFO, 0);

		switch (iha_wait(sc, XF_FIFO_OUT)) {
		case -1:
			return (-1);

		case PHASE_DATA_OUT:
			break;

		default:
			/* Disable wide CPU to allow read 16 bits */
			bus_space_write_1(iot, ioh, TUL_SCTRL1, EHRSL);
			bus_space_write_1(iot, ioh, TUL_SCTRL0, RSFIFO);
			return (6);
		}
	}
}

static int
iha_status_msg(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct iha_scb *scb;
	uint8_t msg;
	int phase;

	if ((phase = iha_wait(sc, CMD_COMP)) == -1)
		return (-1);

	scb = sc->sc_actscb;

	scb->ta_stat = bus_space_read_1(iot, ioh, TUL_SFIFO);

	if (phase == PHASE_MSG_OUT) {
		if ((sc->sc_status0 & SPERR) == 0)
			bus_space_write_1(iot, ioh, TUL_SFIFO, MSG_NOOP);
		else
			bus_space_write_1(iot, ioh, TUL_SFIFO,
			    MSG_PARITY_ERROR);

		return (iha_wait(sc, XF_FIFO_OUT));

	} else if (phase == PHASE_MSG_IN) {
		msg = bus_space_read_1(iot, ioh, TUL_SFIFO);

		if ((sc->sc_status0 & SPERR) != 0)
			switch (iha_wait(sc, MSG_ACCEPT)) {
			case -1:
				return (-1);
			case PHASE_MSG_OUT:
				bus_space_write_1(iot, ioh, TUL_SFIFO,
				    MSG_PARITY_ERROR);
				return (iha_wait(sc, XF_FIFO_OUT));
			default:
				iha_bad_seq(sc);
				return (-1);
			}

		if (msg == MSG_CMDCOMPLETE) {
			if ((scb->ta_stat &
			    (SCSI_INTERM | SCSI_BUSY)) == SCSI_INTERM) {
				iha_bad_seq(sc);
				return (-1);
			}
			sc->sc_flags |= FLAG_EXPECT_DONE_DISC;
			bus_space_write_1(iot, ioh, TUL_SCTRL0, RSFIFO);
			return (iha_wait(sc, MSG_ACCEPT));
		}

		if ((msg == MSG_LINK_CMD_COMPLETE)
		    || (msg == MSG_LINK_CMD_COMPLETEF)) {
			if ((scb->ta_stat &
			    (SCSI_INTERM | SCSI_BUSY)) == SCSI_INTERM)
				return (iha_wait(sc, MSG_ACCEPT));
		}
	}

	iha_bad_seq(sc);
	return (-1);
}

/*
 * iha_busfree - SCSI bus free detected as a result of a TIMEOUT or
 *		 DISCONNECT interrupt. Reset the tulip FIFO and
 *		 SCONFIG0 and enable hardware reselect. Move any active
 *		 SCB to sc_donescb list. Return an appropriate host status
 *		 if an I/O was active.
 */
static void
iha_busfree(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct iha_scb *scb;

	bus_space_write_1(iot, ioh, TUL_SCTRL0, RSFIFO);
	bus_space_write_1(iot, ioh, TUL_SCONFIG0, SCONFIG0DEFAULT);
	bus_space_write_1(iot, ioh, TUL_SCTRL1, EHRSL);

	scb = sc->sc_actscb;

	if (scb != NULL) {
		if (scb->status == STATUS_SELECT)
			/* selection timeout   */
			iha_append_done_scb(sc, scb, HOST_SEL_TOUT);
		else
			/* Unexpected bus free */
			iha_append_done_scb(sc, scb, HOST_BAD_PHAS);
	}
}

/*
 * iha_resel - handle a detected SCSI bus reselection request.
 */
static int
iha_resel(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct iha_scb *scb;
	struct tcs *tcs;
	uint8_t tag, target, lun, msg, abortmsg;

	if (sc->sc_actscb != NULL) {
		if (sc->sc_actscb->status == STATUS_SELECT)
			iha_push_pend_scb(sc, sc->sc_actscb);
		sc->sc_actscb = NULL;
	}

	target = bus_space_read_1(iot, ioh, TUL_SBID);
	lun = bus_space_read_1(iot, ioh, TUL_SALVC) & IHA_MSG_IDENTIFY_LUNMASK;

	tcs = &sc->sc_tcs[target];

	bus_space_write_1(iot, ioh, TUL_SCONFIG0, tcs->sconfig0);
	bus_space_write_1(iot, ioh, TUL_SYNCM, tcs->syncm);

	abortmsg = MSG_ABORT; /* until a valid tag has been obtained */

	if (tcs->ntagscb != NULL)
		/* There is a non-tagged I/O active on the target */
		scb = tcs->ntagscb;

	else {
		/*
		 * Since there is no active non-tagged operation
		 * read the tag type, the tag itself, and find
		 * the appropriate scb by indexing sc_scb with
		 * the tag.
		 */

		switch (iha_wait(sc, MSG_ACCEPT)) {
		case -1:
			return (-1);
		case PHASE_MSG_IN:
			bus_space_write_4(iot, ioh, TUL_STCNT0, 1);
			if ((iha_wait(sc, XF_FIFO_IN)) == -1)
				return (-1);
			break;
		default:
			goto abort;
		}

		msg = bus_space_read_1(iot, ioh, TUL_SFIFO); /* Read Tag Msg */

		if ((msg < MSG_SIMPLE_Q_TAG) || (msg > MSG_ORDERED_Q_TAG))
			goto abort;

		switch (iha_wait(sc, MSG_ACCEPT)) {
		case -1:
			return (-1);
		case PHASE_MSG_IN:
			bus_space_write_4(iot, ioh, TUL_STCNT0, 1);
			if ((iha_wait(sc, XF_FIFO_IN)) == -1)
				return (-1);
			break;
		default:
			goto abort;
		}

		tag  = bus_space_read_1(iot, ioh, TUL_SFIFO); /* Read Tag ID */
		scb = &sc->sc_scb[tag];

		abortmsg = MSG_ABORT_TAG; /* Now that we have valdid tag! */
	}

	if ((scb->target != target)
	    || (scb->lun != lun)
	    || (scb->status != STATUS_BUSY)) {
 abort:
		iha_msgout_abort(sc, abortmsg);
		return (-1);
	}

	sc->sc_actscb = scb;

	if (iha_wait(sc, MSG_ACCEPT) == -1)
		return (-1);

	return (iha_next_state(sc));
}

static int
iha_msgin(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int flags;
	int phase;
	uint8_t msg;

	for (;;) {
		if ((bus_space_read_1(iot, ioh, TUL_SFIFOCNT) & FIFOC) > 0)
			bus_space_write_1(iot, ioh, TUL_SCTRL0, RSFIFO);

		bus_space_write_4(iot, ioh, TUL_STCNT0, 1);

		phase = iha_wait(sc, XF_FIFO_IN);
		msg = bus_space_read_1(iot, ioh, TUL_SFIFO);

		switch (msg) {
		case MSG_DISCONNECT:
			sc->sc_flags |= FLAG_EXPECT_DISC;
			if (iha_wait(sc, MSG_ACCEPT) != -1)
				iha_bad_seq(sc);
			phase = -1;
			break;
		case MSG_SAVEDATAPOINTER:
		case MSG_RESTOREPOINTERS:
		case MSG_NOOP:
			phase = iha_wait(sc, MSG_ACCEPT);
			break;
		case MSG_MESSAGE_REJECT:
			/* XXX - need to clear FIFO like other 'Clear ATN'?*/
			iha_set_ssig(sc, REQ | BSY | SEL | ATN, 0);
			flags = sc->sc_actscb->tcs->flags;
			if ((flags & FLAG_NO_NEG_SYNC) == 0)
				iha_set_ssig(sc, REQ | BSY | SEL, ATN);
			phase = iha_wait(sc, MSG_ACCEPT);
			break;
		case MSG_EXTENDED:
			phase = iha_msgin_extended(sc);
			break;
		case MSG_IGN_WIDE_RESIDUE:
			phase = iha_msgin_ignore_wid_resid(sc);
			break;
		case MSG_CMDCOMPLETE:
			sc->sc_flags |= FLAG_EXPECT_DONE_DISC;
			bus_space_write_1(iot, ioh, TUL_SCTRL0, RSFIFO);
			phase = iha_wait(sc, MSG_ACCEPT);
			if (phase != -1) {
				iha_bad_seq(sc);
				return (-1);
			}
			break;
		default:
			printf("[debug] iha_msgin: bad msg type: %d\n", msg);
			phase = iha_msgout_reject(sc);
			break;
		}

		if (phase != PHASE_MSG_IN)
			return (phase);
	}
	/* NOTREACHED */
}

static int
iha_msgin_extended(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int flags, i, phase, msglen, msgcode;

	/*
	 * XXX - can we just stop reading and reject, or do we have to
	 *	 read all input, discarding the excess, and then reject
	 */
	for (i = 0; i < IHA_MAX_EXTENDED_MSG; i++) {
		phase = iha_wait(sc, MSG_ACCEPT);

		if (phase != PHASE_MSG_IN)
			return (phase);

		bus_space_write_4(iot, ioh, TUL_STCNT0, 1);

		if (iha_wait(sc, XF_FIFO_IN) == -1)
			return (-1);

		sc->sc_msg[i] = bus_space_read_1(iot, ioh, TUL_SFIFO);

		if (sc->sc_msg[0] == i)
			break;
	}

	msglen	= sc->sc_msg[0];
	msgcode = sc->sc_msg[1];

	if ((msglen == MSG_EXT_SDTR_LEN) && (msgcode == MSG_EXT_SDTR)) {
		if (iha_msgin_sdtr(sc) == 0) {
			iha_sync_done(sc);
			return (iha_wait(sc, MSG_ACCEPT));
		}

		iha_set_ssig(sc, REQ | BSY | SEL, ATN);

		phase = iha_wait(sc, MSG_ACCEPT);
		if (phase != PHASE_MSG_OUT)
			return (phase);

		/* Clear FIFO for important message - final SYNC offer */
		bus_space_write_1(iot, ioh, TUL_SCTRL0, RSFIFO);

		iha_sync_done(sc); /* This is our final offer */

	} else if ((msglen == MSG_EXT_WDTR_LEN) && (msgcode == MSG_EXT_WDTR)) {

		flags = sc->sc_actscb->tcs->flags;

		if ((flags & FLAG_NO_WIDE) != 0)
			/* Offer 8bit xfers only */
			sc->sc_msg[2] = MSG_EXT_WDTR_BUS_8_BIT;

		else if (sc->sc_msg[2] > MSG_EXT_WDTR_BUS_32_BIT)
			/* BAD MSG */
			return (iha_msgout_reject(sc));

		else if (sc->sc_msg[2] == MSG_EXT_WDTR_BUS_32_BIT)
			/* Offer 16bit instead */
			sc->sc_msg[2] = MSG_EXT_WDTR_BUS_16_BIT;

		else {
			iha_wide_done(sc);
			if ((flags & FLAG_NO_NEG_SYNC) == 0)
				iha_set_ssig(sc, REQ | BSY | SEL, ATN);
			return (iha_wait(sc, MSG_ACCEPT));
		}

		iha_set_ssig(sc, REQ | BSY | SEL, ATN);

		phase = iha_wait(sc, MSG_ACCEPT);
		if (phase != PHASE_MSG_OUT)
			return (phase);
	} else
		return (iha_msgout_reject(sc));

	return (iha_msgout_extended(sc));
}

/*
 * iha_msgin_sdtr - check SDTR msg in sc_msg. If the offer is
 *		    acceptable leave sc_msg as is and return 0.
 *		    If the negotiation must continue, modify sc_msg
 *		    as needed and return 1. Else return 0.
 */
static int
iha_msgin_sdtr(struct iha_softc *sc)
{
	int flags;
	int newoffer;
	uint8_t default_period;

	flags = sc->sc_actscb->tcs->flags;

	default_period = iha_rate_tbl[flags & FLAG_SCSI_RATE];

	if (sc->sc_msg[3] == 0)
		/* target offered async only. Accept it. */
		return (0);

	newoffer = 0;

	if ((flags & FLAG_NO_SYNC) != 0) {
		sc->sc_msg[3] = 0;
		newoffer = 1;
	}

	if (sc->sc_msg[3] > IHA_MAX_OFFSET) {
		sc->sc_msg[3] = IHA_MAX_OFFSET;
		newoffer = 1;
	}

	if (sc->sc_msg[2] < default_period) {
		sc->sc_msg[2] = default_period;
		newoffer = 1;
	}

	if (sc->sc_msg[2] > IHA_MAX_PERIOD) {
		/* Use async */
		sc->sc_msg[3] = 0;
		newoffer = 1;
	}

	return (newoffer);
}

static int
iha_msgin_ignore_wid_resid(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int phase;

	phase = iha_wait(sc, MSG_ACCEPT);

	if (phase == PHASE_MSG_IN) {
		phase = iha_wait(sc, XF_FIFO_IN);

		if (phase != -1) {
			bus_space_write_1(iot, ioh, TUL_SFIFO, 0);
			(void)bus_space_read_1(iot, ioh, TUL_SFIFO);
			(void)bus_space_read_1(iot, ioh, TUL_SFIFO);

			phase = iha_wait(sc, MSG_ACCEPT);
		}
	}

	return (phase);
}

static int
iha_msgout(struct iha_softc *sc, uint8_t msg)
{

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, TUL_SFIFO, msg);

	return (iha_wait(sc, XF_FIFO_OUT));
}

static void
iha_msgout_abort(struct iha_softc *sc, uint8_t aborttype)
{

	iha_set_ssig(sc, REQ | BSY | SEL, ATN);

	switch (iha_wait(sc, MSG_ACCEPT)) {
	case -1:
		break;

	case PHASE_MSG_OUT:
		sc->sc_flags |= FLAG_EXPECT_DISC;
		if (iha_msgout(sc, aborttype) != -1)
			iha_bad_seq(sc);
		break;

	default:
		iha_bad_seq(sc);
		break;
	}
}

static int
iha_msgout_reject(struct iha_softc *sc)
{

	iha_set_ssig(sc, REQ | BSY | SEL, ATN);

	if (iha_wait(sc, MSG_ACCEPT) == PHASE_MSG_OUT)
		return (iha_msgout(sc, MSG_MESSAGE_REJECT));

	return (-1);
}

static int
iha_msgout_extended(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int phase;

	bus_space_write_1(iot, ioh, TUL_SFIFO, MSG_EXTENDED);

	bus_space_write_multi_1(iot, ioh, TUL_SFIFO,
	    sc->sc_msg, sc->sc_msg[0] + 1);

	phase = iha_wait(sc, XF_FIFO_OUT);

	bus_space_write_1(iot, ioh, TUL_SCTRL0, RSFIFO);
	iha_set_ssig(sc, REQ | BSY | SEL | ATN, 0);

	return (phase);
}

static int
iha_msgout_wdtr(struct iha_softc *sc)
{

	sc->sc_actscb->tcs->flags |= FLAG_WIDE_DONE;

	sc->sc_msg[0] = MSG_EXT_WDTR_LEN;
	sc->sc_msg[1] = MSG_EXT_WDTR;
	sc->sc_msg[2] = MSG_EXT_WDTR_BUS_16_BIT;

	return (iha_msgout_extended(sc));
}

static int
iha_msgout_sdtr(struct iha_softc *sc)
{
	struct tcs *tcs = sc->sc_actscb->tcs;

	tcs->flags |= FLAG_SYNC_DONE;

	sc->sc_msg[0] = MSG_EXT_SDTR_LEN;
	sc->sc_msg[1] = MSG_EXT_SDTR;
	sc->sc_msg[2] = iha_rate_tbl[tcs->flags & FLAG_SCSI_RATE];
	sc->sc_msg[3] = IHA_MAX_OFFSET; /* REQ/ACK */

	return (iha_msgout_extended(sc));
}

static void
iha_wide_done(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct tcs *tcs = sc->sc_actscb->tcs;

	tcs->syncm = 0;
	tcs->period = 0;
	tcs->offset = 0;

	if (sc->sc_msg[2] != 0)
		tcs->syncm |= PERIOD_WIDE_SCSI;

	tcs->sconfig0 &= ~ALTPD;
	tcs->flags &= ~FLAG_SYNC_DONE;
	tcs->flags |=  FLAG_WIDE_DONE;

	iha_update_xfer_mode(sc, sc->sc_actscb->target);

	bus_space_write_1(iot, ioh, TUL_SCONFIG0, tcs->sconfig0);
	bus_space_write_1(iot, ioh, TUL_SYNCM, tcs->syncm);
}

static void
iha_sync_done(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct tcs *tcs = sc->sc_actscb->tcs;
	int i;

	tcs->period = sc->sc_msg[2];
	tcs->offset = sc->sc_msg[3];
	if (tcs->offset != 0) {
		tcs->syncm |= tcs->offset;

		/* pick the highest possible rate */
		for (i = 0; i < sizeof(iha_rate_tbl); i++)
			if (iha_rate_tbl[i] >= tcs->period)
				break;

		tcs->syncm |= (i << 4);
		tcs->sconfig0 |= ALTPD;
	}

	tcs->flags |= FLAG_SYNC_DONE;

	iha_update_xfer_mode(sc, sc->sc_actscb->target);

	bus_space_write_1(iot, ioh, TUL_SCONFIG0, tcs->sconfig0);
	bus_space_write_1(iot, ioh, TUL_SYNCM, tcs->syncm);
}

/*
 * iha_bad_seq - a SCSI bus phase was encountered out of the
 *               correct/expected sequence. Reset the SCSI bus.
 */
static void
iha_bad_seq(struct iha_softc *sc)
{
	struct iha_scb *scb = sc->sc_actscb;

	if (scb != NULL)
		iha_append_done_scb(sc, scb, HOST_BAD_PHAS);

	iha_reset_scsi_bus(sc);
	iha_reset_chip(sc);
}

/*
 * iha_read_eeprom - read Serial EEPROM value & set to defaults
 *		     if required. XXX - Writing does NOT work!
 */
static void
iha_read_eeprom(struct iha_softc *sc, struct iha_eeprom *eeprom)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	uint16_t *tbuf = (uint16_t *)eeprom;
	uint8_t gctrl;

	/* Enable EEProm programming */
	gctrl = bus_space_read_1(iot, ioh, TUL_GCTRL0) | EEPRG;
	bus_space_write_1(iot, ioh, TUL_GCTRL0, gctrl);

	/* Read EEProm */
	if (iha_se2_rd_all(sc, tbuf) == 0)
		panic("%s: cannot read EEPROM", device_xname(sc->sc_dev));

	/* Disable EEProm programming */
	gctrl = bus_space_read_1(iot, ioh, TUL_GCTRL0) & ~EEPRG;
	bus_space_write_1(iot, ioh, TUL_GCTRL0, gctrl);
}

#ifdef notused
/*
 * iha_se2_update_all - Update SCSI H/A configuration parameters from
 *			serial EEPROM Setup default pattern. Only
 *			change those values different from the values
 *			in iha_eeprom.
 */
static void
iha_se2_update_all(struct iha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	uint16_t *np;
	uint32_t chksum;
	int i;

	/* Enable erase/write state of EEPROM */
	iha_se2_instr(sc, ENABLE_ERASE);
	bus_space_write_1(iot, ioh, TUL_NVRAM, 0);
	EEP_WAIT();

	np = (uint16_t *)&eeprom_default;

	for (i = 0, chksum = 0; i < EEPROM_SIZE - 1; i++) {
		iha_se2_wr(sc, i, *np);
		chksum += *np++;
	}

	chksum &= 0x0000ffff;
	iha_se2_wr(sc, 31, chksum);

	/* Disable erase/write state of EEPROM */
	iha_se2_instr(sc, 0);
	bus_space_write_1(iot, ioh, TUL_NVRAM, 0);
	EEP_WAIT();
}

/*
 * iha_se2_wr - write the given 16 bit value into the Serial EEPROM
 *		at the specified offset
 */
static void
iha_se2_wr(struct iha_softc *sc, int addr, uint16_t writeword)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i, bit;

	/* send 'WRITE' Instruction == address | WRITE bit */
	iha_se2_instr(sc, addr | WRITE);

	for (i = 16; i > 0; i--) {
		if (writeword & (1 << (i - 1)))
			bus_space_write_1(iot, ioh, TUL_NVRAM, NVRCS | NVRDO);
		else
			bus_space_write_1(iot, ioh, TUL_NVRAM, NVRCS);
		EEP_WAIT();
		bus_space_write_1(iot, ioh, TUL_NVRAM, NVRCS | NVRCK);
		EEP_WAIT();
	}

	bus_space_write_1(iot, ioh, TUL_NVRAM, NVRCS);
	EEP_WAIT();
	bus_space_write_1(iot, ioh, TUL_NVRAM, 0);
	EEP_WAIT();
	bus_space_write_1(iot, ioh, TUL_NVRAM, NVRCS);
	EEP_WAIT();

	for (;;) {
		bus_space_write_1(iot, ioh, TUL_NVRAM, NVRCS | NVRCK);
		EEP_WAIT();
		bus_space_write_1(iot, ioh, TUL_NVRAM, NVRCS);
		EEP_WAIT();
		bit = bus_space_read_1(iot, ioh, TUL_NVRAM) & NVRDI;
		EEP_WAIT();
		if (bit != 0)
			break; /* write complete */
	}

	bus_space_write_1(iot, ioh, TUL_NVRAM, 0);
}
#endif

/*
 * iha_se2_rd - read & return the 16 bit value at the specified
 *		offset in the Serial E2PROM
 *
 */
static uint16_t
iha_se2_rd(struct iha_softc *sc, int addr)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i, bit;
	uint16_t readword;

	/* Send 'READ' instruction == address | READ bit */
	iha_se2_instr(sc, addr | READ);

	readword = 0;
	for (i = 16; i > 0; i--) {
		bus_space_write_1(iot, ioh, TUL_NVRAM, NVRCS | NVRCK);
		EEP_WAIT();
		bus_space_write_1(iot, ioh, TUL_NVRAM, NVRCS);
		EEP_WAIT();
		/* sample data after the following edge of clock     */
		bit = bus_space_read_1(iot, ioh, TUL_NVRAM) & NVRDI ? 1 : 0;
		EEP_WAIT();

		readword |= bit << (i - 1);
	}

	bus_space_write_1(iot, ioh, TUL_NVRAM, 0);

	return (readword);
}

/*
 * iha_se2_rd_all - Read SCSI H/A config parameters from serial EEPROM
 */
static int
iha_se2_rd_all(struct iha_softc *sc, uint16_t *tbuf)
{
	struct iha_eeprom *eeprom = (struct iha_eeprom *)tbuf;
	uint32_t chksum;
	int i;

	for (i = 0, chksum = 0; i < EEPROM_SIZE - 1; i++) {
		*tbuf = iha_se2_rd(sc, i);
		chksum += *tbuf++;
	}
	*tbuf = iha_se2_rd(sc, 31); /* read checksum from EEPROM */

	chksum &= 0x0000ffff; /* lower 16 bits */

	return (eeprom->signature == EEP_SIGNATURE) &&
	    (eeprom->checksum == chksum);
}

/*
 * iha_se2_instr - write an octet to serial E2PROM one bit at a time
 */
static void
iha_se2_instr(struct iha_softc *sc, int instr)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int b, i;

	b = NVRCS | NVRDO; /* Write the start bit (== 1) */

	bus_space_write_1(iot, ioh, TUL_NVRAM, b);
	EEP_WAIT();
	bus_space_write_1(iot, ioh, TUL_NVRAM, b | NVRCK);
	EEP_WAIT();

	for (i = 8; i > 0; i--) {
		if (instr & (1 << (i - 1)))
			b = NVRCS | NVRDO; /* Write a 1 bit */
		else
			b = NVRCS;	   /* Write a 0 bit */

		bus_space_write_1(iot, ioh, TUL_NVRAM, b);
		EEP_WAIT();
		bus_space_write_1(iot, ioh, TUL_NVRAM, b | NVRCK);
		EEP_WAIT();
	}

	bus_space_write_1(iot, ioh, TUL_NVRAM, NVRCS);
}
