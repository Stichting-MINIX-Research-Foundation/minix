/*	$NetBSD: oosiop.c,v 1.15 2014/12/15 11:02:33 skrll Exp $	*/

/*
 * Copyright (c) 2001 Shuichiro URATA.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * NCR53C700 SCSI I/O processor (OOSIOP) driver
 *
 * TODO:
 *   - More better error handling.
 *   - Implement tagged queuing.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: oosiop.c,v 1.15 2014/12/15 11:02:33 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_message.h>

#include <sys/cpu.h>
#include <sys/bus.h>

#include <dev/ic/oosiopreg.h>
#include <dev/ic/oosiopvar.h>
#include <dev/microcode/siop/oosiop.out>

static int	oosiop_alloc_cb(struct oosiop_softc *, int);

static inline void oosiop_relocate_io(struct oosiop_softc *, bus_addr_t);
static inline void oosiop_relocate_tc(struct oosiop_softc *, bus_addr_t);
static inline void oosiop_fixup_select(struct oosiop_softc *, bus_addr_t,
		         int);
static inline void oosiop_fixup_jump(struct oosiop_softc *, bus_addr_t,
		         bus_addr_t);
static inline void oosiop_fixup_move(struct oosiop_softc *, bus_addr_t,
		         bus_size_t, bus_addr_t);

static void	oosiop_load_script(struct oosiop_softc *);
static void	oosiop_setup_sgdma(struct oosiop_softc *, struct oosiop_cb *);
static void	oosiop_setup_dma(struct oosiop_softc *);
static void	oosiop_flush_fifo(struct oosiop_softc *);
static void	oosiop_clear_fifo(struct oosiop_softc *);
static void	oosiop_phasemismatch(struct oosiop_softc *);
static void	oosiop_setup_syncxfer(struct oosiop_softc *);
static void	oosiop_set_syncparam(struct oosiop_softc *, int, int, int);
static void	oosiop_minphys(struct buf *);
static void	oosiop_scsipi_request(struct scsipi_channel *,
		    scsipi_adapter_req_t, void *);
static void	oosiop_done(struct oosiop_softc *, struct oosiop_cb *);
static void	oosiop_timeout(void *);
static void	oosiop_reset(struct oosiop_softc *);
static void	oosiop_reset_bus(struct oosiop_softc *);
static void	oosiop_scriptintr(struct oosiop_softc *);
static void	oosiop_msgin(struct oosiop_softc *, struct oosiop_cb *);

/* Trap interrupt code for unexpected data I/O */
#define	DATAIN_TRAP	0xdead0001
#define	DATAOUT_TRAP	0xdead0002

/* Possible TP and SCF conbination */
static const struct {
	uint8_t		tp;
	uint8_t		scf;
} synctbl[] = {
	{0, 1},		/* SCLK /  4.0 */
	{1, 1},		/* SCLK /  5.0 */
	{2, 1},		/* SCLK /  6.0 */
	{3, 1},		/* SCLK /  7.0 */
	{1, 2},		/* SCLK /  7.5 */
	{4, 1},		/* SCLK /  8.0 */
	{5, 1},		/* SCLK /  9.0 */
	{6, 1},		/* SCLK / 10.0 */
	{3, 2},		/* SCLK / 10.5 */
	{7, 1},		/* SCLK / 11.0 */
	{4, 2},		/* SCLK / 12.0 */
	{5, 2},		/* SCLK / 13.5 */
	{3, 3},		/* SCLK / 14.0 */
	{6, 2},		/* SCLK / 15.0 */
	{4, 3},		/* SCLK / 16.0 */
	{7, 2},		/* SCLK / 16.5 */
	{5, 3},		/* SCLK / 18.0 */
	{6, 3},		/* SCLK / 20.0 */
	{7, 3}		/* SCLK / 22.0 */
};
#define	NSYNCTBL	(sizeof(synctbl) / sizeof(synctbl[0]))

#define	oosiop_period(sc, tp, scf)					\
	    (((1000000000 / (sc)->sc_freq) * (tp) * (scf)) / 40)

void
oosiop_attach(struct oosiop_softc *sc)
{
	bus_size_t scrsize;
	bus_dma_segment_t seg;
	struct oosiop_cb *cb;
	int err, i, nseg;

	/*
	 * Allocate DMA-safe memory for the script and map it.
	 */
	scrsize = sizeof(oosiop_script);
	err = bus_dmamem_alloc(sc->sc_dmat, scrsize, PAGE_SIZE, 0, &seg, 1,
	    &nseg, BUS_DMA_NOWAIT);
	if (err) {
		aprint_error(": failed to allocate script memory, err=%d\n",
		    err);
		return;
	}
	err = bus_dmamem_map(sc->sc_dmat, &seg, nseg, scrsize,
	    (void **)&sc->sc_scr, BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (err) {
		aprint_error(": failed to map script memory, err=%d\n", err);
		return;
	}
	err = bus_dmamap_create(sc->sc_dmat, scrsize, 1, scrsize, 0,
	    BUS_DMA_NOWAIT, &sc->sc_scrdma);
	if (err) {
		aprint_error(": failed to create script map, err=%d\n", err);
		return;
	}
	err = bus_dmamap_load(sc->sc_dmat, sc->sc_scrdma, sc->sc_scr, scrsize,
	    NULL, BUS_DMA_NOWAIT | BUS_DMA_WRITE);
	if (err) {
		aprint_error(": failed to load script map, err=%d\n", err);
		return;
	}
	sc->sc_scrbase = sc->sc_scrdma->dm_segs[0].ds_addr;

	/* Initialize command block array */
	TAILQ_INIT(&sc->sc_free_cb);
	TAILQ_INIT(&sc->sc_cbq);
	if (oosiop_alloc_cb(sc, OOSIOP_NCB) != 0)
		return;

	/* Use first cb to reselection msgin buffer */
	cb = TAILQ_FIRST(&sc->sc_free_cb);
	sc->sc_reselbuf = cb->xferdma->dm_segs[0].ds_addr +
	    offsetof(struct oosiop_xfer, msgin[0]);

	for (i = 0; i < OOSIOP_NTGT; i++) {
		sc->sc_tgt[i].nexus = NULL;
		sc->sc_tgt[i].flags = 0;
	}

	/* Setup asynchronous clock divisor parameters */
	if (sc->sc_freq <= 25000000) {
		sc->sc_ccf = 10;
		sc->sc_dcntl = OOSIOP_DCNTL_CF_1;
	} else if (sc->sc_freq <= 37500000) {
		sc->sc_ccf = 15;
		sc->sc_dcntl = OOSIOP_DCNTL_CF_1_5;
	} else if (sc->sc_freq <= 50000000) {
		sc->sc_ccf = 20;
		sc->sc_dcntl = OOSIOP_DCNTL_CF_2;
	} else {
		sc->sc_ccf = 30;
		sc->sc_dcntl = OOSIOP_DCNTL_CF_3;
	}

	if (sc->sc_chip == OOSIOP_700)
		sc->sc_minperiod = oosiop_period(sc, 4, sc->sc_ccf);
	else
		sc->sc_minperiod = oosiop_period(sc, 4, 10);

	if (sc->sc_minperiod < 25)
		sc->sc_minperiod = 25;	/* limit to 10MB/s */

	aprint_normal(": NCR53C700%s rev %d, %dMHz, SCSI ID %d\n",
	    sc->sc_chip == OOSIOP_700_66 ? "-66" : "",
	    oosiop_read_1(sc, OOSIOP_CTEST7) >> 4,
	    sc->sc_freq / 1000000, sc->sc_id);
	/*
	 * Reset all
	 */
	oosiop_reset(sc);
	oosiop_reset_bus(sc);

	/*
	 * Start SCRIPTS processor
	 */
	oosiop_load_script(sc);
	sc->sc_active = 0;
	oosiop_write_4(sc, OOSIOP_DSP, sc->sc_scrbase + Ent_wait_reselect);

	/*
	 * Fill in the scsipi_adapter.
	 */
	sc->sc_adapter.adapt_dev = sc->sc_dev;
	sc->sc_adapter.adapt_nchannels = 1;
	sc->sc_adapter.adapt_openings = OOSIOP_NCB;
	sc->sc_adapter.adapt_max_periph = 1;
	sc->sc_adapter.adapt_ioctl = NULL;
	sc->sc_adapter.adapt_minphys = oosiop_minphys;
	sc->sc_adapter.adapt_request = oosiop_scsipi_request;

	/*
	 * Fill in the scsipi_channel.
	 */
	sc->sc_channel.chan_adapter = &sc->sc_adapter;
	sc->sc_channel.chan_bustype = &scsi_bustype;
	sc->sc_channel.chan_channel = 0;
	sc->sc_channel.chan_ntargets = OOSIOP_NTGT;
	sc->sc_channel.chan_nluns = 8;
	sc->sc_channel.chan_id = sc->sc_id;

	/*
	 * Now try to attach all the sub devices.
	 */
	config_found(sc->sc_dev, &sc->sc_channel, scsiprint);
}

static int
oosiop_alloc_cb(struct oosiop_softc *sc, int ncb)
{
	struct oosiop_cb *cb;
	void *xfer_kva;
	struct oosiop_xfer *xfer;
	bus_size_t xfersize;
	bus_dma_segment_t seg;
	int i, s, err, nseg;

	/*
	 * Allocate oosiop_cb.
	 */
	cb = malloc(sizeof(struct oosiop_cb) * ncb, M_DEVBUF, M_NOWAIT|M_ZERO);
	if (cb == NULL) {
		printf(": failed to allocate cb memory\n");
		err = ENOMEM;
		goto fail0;
	}

	/*
	 * Allocate DMA-safe memory for the oosiop_xfer and map it.
	 */
	xfersize = sizeof(struct oosiop_xfer) * ncb;
	err = bus_dmamem_alloc(sc->sc_dmat, xfersize, PAGE_SIZE, 0, &seg, 1,
	    &nseg, BUS_DMA_NOWAIT);
	if (err) {
		printf(": failed to allocate xfer block memory, err=%d\n", err);
		goto fail1;
	}
	KASSERT(nseg == 1);
	err = bus_dmamem_map(sc->sc_dmat, &seg, nseg, xfersize, &xfer_kva,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (err) {
		printf(": failed to map xfer block memory, err=%d\n", err);
		goto fail2;
	}
	xfer = xfer_kva;

	/* Initialize each command block */
	for (i = 0; i < ncb; i++) {
		err = bus_dmamap_create(sc->sc_dmat, PAGE_SIZE, 1, PAGE_SIZE,
		    0, BUS_DMA_NOWAIT, &cb[i].cmddma);
		if (err) {
			printf(": failed to create cmddma map, err=%d\n", err);
			goto loop_fail0;
		}
		err = bus_dmamap_create(sc->sc_dmat, OOSIOP_MAX_XFER,
		    OOSIOP_NSG, OOSIOP_DBC_MAX, 0, BUS_DMA_NOWAIT,
		    &cb[i].datadma);
		if (err) {
			printf(": failed to create datadma map, err=%d\n", err);
			goto loop_fail1;
		}

		err = bus_dmamap_create(sc->sc_dmat,
		    sizeof(struct oosiop_xfer), 1, sizeof(struct oosiop_xfer),
		    0, BUS_DMA_NOWAIT, &cb[i].xferdma);
		if (err) {
			printf(": failed to create xfer block map, err=%d\n",
			    err);
			goto loop_fail2;
		}
		err = bus_dmamap_load(sc->sc_dmat, cb[i].xferdma, &xfer[i],
		    sizeof(struct oosiop_xfer), NULL, BUS_DMA_NOWAIT);
		if (err) {
			printf(": failed to load xfer block, err=%d\n", err);
			goto loop_fail3;
		}

		cb[i].xfer = &xfer[i];
		continue;

loop_fail4: __unused
		bus_dmamap_unload(sc->sc_dmat, cb[i].xferdma);
loop_fail3:	bus_dmamap_destroy(sc->sc_dmat, cb[i].xferdma);
loop_fail2:	bus_dmamap_destroy(sc->sc_dmat, cb[i].datadma);
loop_fail1:	bus_dmamap_destroy(sc->sc_dmat, cb[i].cmddma);
loop_fail0:	goto fail3;
	}

	for (i = 0; i < ncb; i++) {
		s = splbio();
		TAILQ_INSERT_TAIL(&sc->sc_free_cb, &cb[i], chain);
		splx(s);
	}

	/* Success!  */
	return 0;

fail3:	while (i--) {
		bus_dmamap_unload(sc->sc_dmat, cb[i].xferdma);
		bus_dmamap_destroy(sc->sc_dmat, cb[i].xferdma);
		bus_dmamap_destroy(sc->sc_dmat, cb[i].datadma);
		bus_dmamap_destroy(sc->sc_dmat, cb[i].cmddma);
	}
	bus_dmamem_unmap(sc->sc_dmat, xfer_kva, xfersize);
fail2:	bus_dmamem_free(sc->sc_dmat, &seg, 1);
fail1:	free(cb, M_DEVBUF);
fail0:	KASSERT(err);
	return err;
}

static inline void
oosiop_relocate_io(struct oosiop_softc *sc, bus_addr_t addr)
{
	uint32_t dcmd;
	int32_t dsps;

	dcmd = le32toh(sc->sc_scr[addr / 4 + 0]);
	dsps = le32toh(sc->sc_scr[addr / 4 + 1]);

	/* convert relative to absolute */
	if (dcmd & 0x04000000) {
		dcmd &= ~0x04000000;
#if 0
		/*
		 * sign extension isn't needed here because
		 * ncr53cxxx.c generates 32 bit dsps.
		 */
		dsps <<= 8;
		dsps >>= 8;
#endif
		sc->sc_scr[addr / 4 + 0] = htole32(dcmd);
		dsps += addr + 8;
	}

	sc->sc_scr[addr / 4 + 1] = htole32(dsps + sc->sc_scrbase);
}

static inline void
oosiop_relocate_tc(struct oosiop_softc *sc, bus_addr_t addr)
{
	uint32_t dcmd;
	int32_t dsps;

	dcmd = le32toh(sc->sc_scr[addr / 4 + 0]);
	dsps = le32toh(sc->sc_scr[addr / 4 + 1]);

	/* convert relative to absolute */
	if (dcmd & 0x00800000) {
		dcmd &= ~0x00800000;
		sc->sc_scr[addr / 4] = htole32(dcmd);
#if 0
		/*
		 * sign extension isn't needed here because
		 * ncr53cxxx.c generates 32 bit dsps.
		 */
		dsps <<= 8;
		dsps >>= 8;
#endif
		dsps += addr + 8;
	}

	sc->sc_scr[addr / 4 + 1] = htole32(dsps + sc->sc_scrbase);
}

static inline void
oosiop_fixup_select(struct oosiop_softc *sc, bus_addr_t addr, int id)
{
	uint32_t dcmd;

	dcmd = le32toh(sc->sc_scr[addr / 4]);
	dcmd &= 0xff00ffff;
	dcmd |= 0x00010000 << id;
	sc->sc_scr[addr / 4] = htole32(dcmd);
}

static inline void
oosiop_fixup_jump(struct oosiop_softc *sc, bus_addr_t addr, bus_addr_t dst)
{

	sc->sc_scr[addr / 4 + 1] = htole32(dst);
}

static inline void
oosiop_fixup_move(struct oosiop_softc *sc, bus_addr_t addr, bus_size_t dbc,
    bus_addr_t dsps)
{
	uint32_t dcmd;

	dcmd = le32toh(sc->sc_scr[addr / 4]);
	dcmd &= 0xff000000;
	dcmd |= dbc & 0x00ffffff;
	sc->sc_scr[addr / 4 + 0] = htole32(dcmd);
	sc->sc_scr[addr / 4 + 1] = htole32(dsps);
}

static void
oosiop_load_script(struct oosiop_softc *sc)
{
	int i;

	/* load script */
	for (i = 0; i < sizeof(oosiop_script) / sizeof(oosiop_script[0]); i++)
		sc->sc_scr[i] = htole32(oosiop_script[i]);

	/* relocate script */
	for (i = 0; i < (sizeof(oosiop_script) / 8); i++) {
		switch (oosiop_script[i * 2] >> 27) {
		case 0x08:	/* select */
		case 0x0a:	/* wait reselect */
			oosiop_relocate_io(sc, i * 8);
			break;
		case 0x10:	/* jump */
		case 0x11:	/* call */
			oosiop_relocate_tc(sc, i * 8);
			break;
		}
	}

	oosiop_fixup_move(sc, Ent_p_resel_msgin_move, 1, sc->sc_reselbuf);
	OOSIOP_SCRIPT_SYNC(sc, BUS_DMASYNC_PREWRITE);
}

static void
oosiop_setup_sgdma(struct oosiop_softc *sc, struct oosiop_cb *cb)
{
	int i, n, off, control;
	struct oosiop_xfer *xfer;

	OOSIOP_XFERSCR_SYNC(sc, cb,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	off = cb->curdp;
	xfer = cb->xfer;
	control = cb->xs->xs_control;

	if (control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT)) {
		/* Find start segment */
		for (i = 0; i < cb->datadma->dm_nsegs; i++) {
			if (off < cb->datadma->dm_segs[i].ds_len)
				break;
			off -= cb->datadma->dm_segs[i].ds_len;
		}

		/* build MOVE block */
		if (control & XS_CTL_DATA_IN) {
			n = 0;
			while (i < cb->datadma->dm_nsegs) {
				xfer->datain_scr[n * 2 + 0] =
				    htole32(0x09000000 |
				    (cb->datadma->dm_segs[i].ds_len - off));
				xfer->datain_scr[n * 2 + 1] =
				    htole32(cb->datadma->dm_segs[i].ds_addr +
				    off);
				n++;
				i++;
				off = 0;
			}
			xfer->datain_scr[n * 2 + 0] = htole32(0x80080000);
			xfer->datain_scr[n * 2 + 1] =
			    htole32(sc->sc_scrbase + Ent_phasedispatch);
		}
		if (control & XS_CTL_DATA_OUT) {
			n = 0;
			while (i < cb->datadma->dm_nsegs) {
				xfer->dataout_scr[n * 2 + 0] =
				    htole32(0x08000000 |
				    (cb->datadma->dm_segs[i].ds_len - off));
				xfer->dataout_scr[n * 2 + 1] =
				    htole32(cb->datadma->dm_segs[i].ds_addr +
				    off);
				n++;
				i++;
				off = 0;
			}
			xfer->dataout_scr[n * 2 + 0] = htole32(0x80080000);
			xfer->dataout_scr[n * 2 + 1] =
			    htole32(sc->sc_scrbase + Ent_phasedispatch);
		}
	}
	if ((control & XS_CTL_DATA_IN) == 0) {
		xfer->datain_scr[0] = htole32(0x98080000);
		xfer->datain_scr[1] = htole32(DATAIN_TRAP);
	}
	if ((control & XS_CTL_DATA_OUT) == 0) {
		xfer->dataout_scr[0] = htole32(0x98080000);
		xfer->dataout_scr[1] = htole32(DATAOUT_TRAP);
	}
	OOSIOP_XFERSCR_SYNC(sc, cb,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
}

/*
 * Setup DMA pointer into script.
 */
static void
oosiop_setup_dma(struct oosiop_softc *sc)
{
	struct oosiop_cb *cb;
	bus_addr_t xferbase;

	cb = sc->sc_curcb;
	xferbase = cb->xferdma->dm_segs[0].ds_addr;

	OOSIOP_SCRIPT_SYNC(sc, BUS_DMASYNC_POSTWRITE);

	oosiop_fixup_select(sc, Ent_p_select, cb->id);
	oosiop_fixup_jump(sc, Ent_p_datain_jump, xferbase +
	    offsetof(struct oosiop_xfer, datain_scr[0]));
	oosiop_fixup_jump(sc, Ent_p_dataout_jump, xferbase +
	    offsetof(struct oosiop_xfer, dataout_scr[0]));
	oosiop_fixup_move(sc, Ent_p_msgin_move, 1, xferbase +
	    offsetof(struct oosiop_xfer, msgin[0]));
	oosiop_fixup_move(sc, Ent_p_extmsglen_move, 1, xferbase +
	    offsetof(struct oosiop_xfer, msgin[1]));
	oosiop_fixup_move(sc, Ent_p_msgout_move, cb->msgoutlen, xferbase +
	    offsetof(struct oosiop_xfer, msgout[0]));
	oosiop_fixup_move(sc, Ent_p_status_move, 1, xferbase +
	    offsetof(struct oosiop_xfer, status));
	oosiop_fixup_move(sc, Ent_p_cmdout_move, cb->xs->cmdlen,
	    cb->cmddma->dm_segs[0].ds_addr);

	OOSIOP_SCRIPT_SYNC(sc, BUS_DMASYNC_PREWRITE);
}

static void
oosiop_flush_fifo(struct oosiop_softc *sc)
{

	oosiop_write_1(sc, OOSIOP_DFIFO, oosiop_read_1(sc, OOSIOP_DFIFO) |
	    OOSIOP_DFIFO_FLF);
	while ((oosiop_read_1(sc, OOSIOP_CTEST1) & OOSIOP_CTEST1_FMT) !=
	    OOSIOP_CTEST1_FMT)
		;
	oosiop_write_1(sc, OOSIOP_DFIFO, oosiop_read_1(sc, OOSIOP_DFIFO) &
	    ~OOSIOP_DFIFO_FLF);
}

static void
oosiop_clear_fifo(struct oosiop_softc *sc)
{

	oosiop_write_1(sc, OOSIOP_DFIFO, oosiop_read_1(sc, OOSIOP_DFIFO) |
	    OOSIOP_DFIFO_CLF);
	while ((oosiop_read_1(sc, OOSIOP_CTEST1) & OOSIOP_CTEST1_FMT) !=
	    OOSIOP_CTEST1_FMT)
		;
	oosiop_write_1(sc, OOSIOP_DFIFO, oosiop_read_1(sc, OOSIOP_DFIFO) &
	    ~OOSIOP_DFIFO_CLF);
}

static void
oosiop_phasemismatch(struct oosiop_softc *sc)
{
	struct oosiop_cb *cb;
	uint32_t dsp, dbc, n, i, len;
	uint8_t dfifo, sstat1;

	cb = sc->sc_curcb;
	if (cb == NULL)
		return;

	dsp = oosiop_read_4(sc, OOSIOP_DSP);
	dbc = oosiop_read_4(sc, OOSIOP_DBC) & OOSIOP_DBC_MAX;
	len = 0;

	n = dsp - cb->xferdma->dm_segs[0].ds_addr - 8;
	if (n >= offsetof(struct oosiop_xfer, datain_scr[0]) &&
	    n < offsetof(struct oosiop_xfer, datain_scr[OOSIOP_NSG * 2])) {
		n -= offsetof(struct oosiop_xfer, datain_scr[0]);
		n >>= 3;
		OOSIOP_DINSCR_SYNC(sc, cb,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		for (i = 0; i <= n; i++)
			len += le32toh(cb->xfer->datain_scr[i * 2]) &
			    0x00ffffff;
		OOSIOP_DINSCR_SYNC(sc, cb,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		/* All data in the chip are already flushed */
	} else if (n >= offsetof(struct oosiop_xfer, dataout_scr[0]) &&
	    n < offsetof(struct oosiop_xfer, dataout_scr[OOSIOP_NSG * 2])) {
		n -= offsetof(struct oosiop_xfer, dataout_scr[0]);
		n >>= 3;
		OOSIOP_DOUTSCR_SYNC(sc, cb,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		for (i = 0; i <= n; i++)
			len += le32toh(cb->xfer->dataout_scr[i * 2]) &
			    0x00ffffff;
		OOSIOP_DOUTSCR_SYNC(sc, cb,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		dfifo = oosiop_read_1(sc, OOSIOP_DFIFO);
		dbc += ((dfifo & OOSIOP_DFIFO_BO) - (dbc & OOSIOP_DFIFO_BO)) &
		    OOSIOP_DFIFO_BO;

		sstat1 = oosiop_read_1(sc, OOSIOP_SSTAT1);
		if (sstat1 & OOSIOP_SSTAT1_OLF)
			dbc++;
		if ((sc->sc_tgt[cb->id].sxfer != 0) &&
		    (sstat1 & OOSIOP_SSTAT1_ORF) != 0)
			dbc++;

		oosiop_clear_fifo(sc);
	} else {
		printf("%s: phase mismatch addr=%08x\n",
		    device_xname(sc->sc_dev),
		    oosiop_read_4(sc, OOSIOP_DSP) - 8);
		oosiop_clear_fifo(sc);
		return;
	}

	len -= dbc;
	if (len) {
		cb->curdp += len;
		oosiop_setup_sgdma(sc, cb);
	}
}

static void
oosiop_setup_syncxfer(struct oosiop_softc *sc)
{
	int id;

	id = sc->sc_curcb->id;
	if (sc->sc_chip != OOSIOP_700)
		oosiop_write_1(sc, OOSIOP_SBCL, sc->sc_tgt[id].scf);

	oosiop_write_1(sc, OOSIOP_SXFER, sc->sc_tgt[id].sxfer);
}

static void
oosiop_set_syncparam(struct oosiop_softc *sc, int id, int period, int offset)
{
	int i, p;
	struct scsipi_xfer_mode xm;

	xm.xm_target = id;
	xm.xm_mode = 0;
	xm.xm_period = 0;
	xm.xm_offset = 0;

	if (offset == 0) {
		/* Asynchronous */
		sc->sc_tgt[id].scf = 0;
		sc->sc_tgt[id].sxfer = 0;
	} else {
		/* Synchronous */
		if (sc->sc_chip == OOSIOP_700) {
			for (i = 4; i < 12; i++) {
				p = oosiop_period(sc, i, sc->sc_ccf);
				if (p >= period)
					break;
			}
			if (i == 12) {
				printf("%s: target %d period too large\n",
				    device_xname(sc->sc_dev), id);
				i = 11;	/* XXX */
			}
			sc->sc_tgt[id].scf = 0;
			sc->sc_tgt[id].sxfer = ((i - 4) << 4) | offset;
		} else {
			for (i = 0; i < NSYNCTBL; i++) {
				p = oosiop_period(sc, synctbl[i].tp + 4,
				    (synctbl[i].scf + 1) * 5);
				if (p >= period)
					break;
			}
			if (i == NSYNCTBL) {
				printf("%s: target %d period too large\n",
				    device_xname(sc->sc_dev), id);
				i = NSYNCTBL - 1;	/* XXX */
			}
			sc->sc_tgt[id].scf = synctbl[i].scf;
			sc->sc_tgt[id].sxfer = (synctbl[i].tp << 4) | offset;
		}

		xm.xm_mode |= PERIPH_CAP_SYNC;
		xm.xm_period = period;
		xm.xm_offset = offset;
	}

	scsipi_async_event(&sc->sc_channel, ASYNC_EVENT_XFER_MODE, &xm);
}

static void
oosiop_minphys(struct buf *bp)
{

	if (bp->b_bcount > OOSIOP_MAX_XFER)
		bp->b_bcount = OOSIOP_MAX_XFER;
	minphys(bp);
}

static void
oosiop_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
    void *arg)
{
	struct scsipi_xfer *xs;
	struct oosiop_softc *sc;
	struct oosiop_cb *cb;
	struct oosiop_xfer *xfer;
	struct scsipi_xfer_mode *xm;
	int s, err;

	sc = device_private(chan->chan_adapter->adapt_dev);

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;

		s = splbio();
		cb = TAILQ_FIRST(&sc->sc_free_cb);
		TAILQ_REMOVE(&sc->sc_free_cb, cb, chain);
		splx(s);

		cb->xs = xs;
		cb->flags = 0;
		cb->id = xs->xs_periph->periph_target;
		cb->lun = xs->xs_periph->periph_lun;
		cb->curdp = 0;
		cb->savedp = 0;
		xfer = cb->xfer;

		/* Setup SCSI command buffer DMA */
		err = bus_dmamap_load(sc->sc_dmat, cb->cmddma, xs->cmd,
		    xs->cmdlen, NULL, ((xs->xs_control & XS_CTL_NOSLEEP) ?
		    BUS_DMA_NOWAIT : BUS_DMA_WAITOK) | BUS_DMA_WRITE);
		if (err) {
			printf("%s: unable to load cmd DMA map: %d",
			    device_xname(sc->sc_dev), err);
			xs->error = XS_RESOURCE_SHORTAGE;
			TAILQ_INSERT_TAIL(&sc->sc_free_cb, cb, chain);
			scsipi_done(xs);
			return;
		}
		bus_dmamap_sync(sc->sc_dmat, cb->cmddma, 0, xs->cmdlen,
		    BUS_DMASYNC_PREWRITE);

		/* Setup data buffer DMA */
		if (xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT)) {
			err = bus_dmamap_load(sc->sc_dmat, cb->datadma,
			    xs->data, xs->datalen, NULL,
			    ((xs->xs_control & XS_CTL_NOSLEEP) ?
			    BUS_DMA_NOWAIT : BUS_DMA_WAITOK) |
			    BUS_DMA_STREAMING |
			    ((xs->xs_control & XS_CTL_DATA_IN) ? BUS_DMA_READ :
			    BUS_DMA_WRITE));
			if (err) {
				printf("%s: unable to load data DMA map: %d",
				    device_xname(sc->sc_dev), err);
				xs->error = XS_RESOURCE_SHORTAGE;
				bus_dmamap_unload(sc->sc_dmat, cb->cmddma);
				TAILQ_INSERT_TAIL(&sc->sc_free_cb, cb, chain);
				scsipi_done(xs);
				return;
			}
			bus_dmamap_sync(sc->sc_dmat, cb->datadma,
			    0, xs->datalen,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		}

		oosiop_setup_sgdma(sc, cb);

		/* Setup msgout buffer */
		OOSIOP_XFERMSG_SYNC(sc, cb,
		   BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		xfer->msgout[0] = MSG_IDENTIFY(cb->lun,
		    (xs->xs_control & XS_CTL_REQSENSE) == 0);
		cb->msgoutlen = 1;

		if (sc->sc_tgt[cb->id].flags & TGTF_SYNCNEG) {
			/* Send SDTR */
			xfer->msgout[1] = MSG_EXTENDED;
			xfer->msgout[2] = MSG_EXT_SDTR_LEN;
			xfer->msgout[3] = MSG_EXT_SDTR;
			xfer->msgout[4] = sc->sc_minperiod;
			xfer->msgout[5] = OOSIOP_MAX_OFFSET;
			cb->msgoutlen = 6;
			sc->sc_tgt[cb->id].flags &= ~TGTF_SYNCNEG;
			sc->sc_tgt[cb->id].flags |= TGTF_WAITSDTR;
		}

		xfer->status = SCSI_OOSIOP_NOSTATUS;

		OOSIOP_XFERMSG_SYNC(sc, cb,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		s = splbio();

		TAILQ_INSERT_TAIL(&sc->sc_cbq, cb, chain);

		if (!sc->sc_active) {
			/* Abort script to start selection */
			oosiop_write_1(sc, OOSIOP_ISTAT, OOSIOP_ISTAT_ABRT);
		}
		if (xs->xs_control & XS_CTL_POLL) {
			/* Poll for command completion */
			while ((xs->xs_status & XS_STS_DONE) == 0) {
				delay(1000);
				oosiop_intr(sc);
			}
		}

		splx(s);

		return;

	case ADAPTER_REQ_GROW_RESOURCES:
		return;

	case ADAPTER_REQ_SET_XFER_MODE:
		xm = arg;
		if (xm->xm_mode & PERIPH_CAP_SYNC)
			sc->sc_tgt[xm->xm_target].flags |= TGTF_SYNCNEG;
		else
			oosiop_set_syncparam(sc, xm->xm_target, 0, 0);

		return;
	}
}

static void
oosiop_done(struct oosiop_softc *sc, struct oosiop_cb *cb)
{
	struct scsipi_xfer *xs;

	xs = cb->xs;
	if (cb == sc->sc_curcb)
		sc->sc_curcb = NULL;
	if (cb == sc->sc_lastcb)
		sc->sc_lastcb = NULL;
	sc->sc_tgt[cb->id].nexus = NULL;

	callout_stop(&xs->xs_callout);

	bus_dmamap_sync(sc->sc_dmat, cb->cmddma, 0, xs->cmdlen,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, cb->cmddma);

	if (xs->datalen > 0) {
		bus_dmamap_sync(sc->sc_dmat, cb->datadma, 0, xs->datalen,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, cb->datadma);
	}

	xs->status = cb->xfer->status;
	xs->resid = 0;	/* XXX */

	if (cb->flags & CBF_SELTOUT)
		xs->error = XS_SELTIMEOUT;
	else if (cb->flags & CBF_TIMEOUT)
		xs->error = XS_TIMEOUT;
	else switch (xs->status) {
	case SCSI_OK:
		xs->error = XS_NOERROR;
		break;

	case SCSI_BUSY:
	case SCSI_CHECK:
		xs->error = XS_BUSY;
		break;
	case SCSI_OOSIOP_NOSTATUS:
		/* the status byte was not updated, cmd was aborted. */
		xs->error = XS_SELTIMEOUT;
		break;

	default:
		xs->error = XS_RESET;
		break;
	}

	scsipi_done(xs);

	/* Put it on the free list. */
	TAILQ_INSERT_TAIL(&sc->sc_free_cb, cb, chain);
}

static void
oosiop_timeout(void *arg)
{
	struct oosiop_cb *cb;
	struct scsipi_periph *periph;
	struct oosiop_softc *sc;
	int s;

	cb = arg;
	periph = cb->xs->xs_periph;
	sc = device_private(periph->periph_channel->chan_adapter->adapt_dev);
	scsipi_printaddr(periph);
	printf("timed out\n");

	s = splbio();

	cb->flags |= CBF_TIMEOUT;
	oosiop_done(sc, cb);

	splx(s);
}

static void
oosiop_reset(struct oosiop_softc *sc)
{
	int i, s;

	s = splbio();

	/* Stop SCRIPTS processor */
	oosiop_write_1(sc, OOSIOP_ISTAT, OOSIOP_ISTAT_ABRT);
	delay(100);
	oosiop_write_1(sc, OOSIOP_ISTAT, 0);

	/* Reset the chip */
	oosiop_write_1(sc, OOSIOP_DCNTL, sc->sc_dcntl | OOSIOP_DCNTL_RST);
	delay(100);
	oosiop_write_1(sc, OOSIOP_DCNTL, sc->sc_dcntl);
	delay(10000);

	/* Set up various chip parameters */
	oosiop_write_1(sc, OOSIOP_SCNTL0, OOSIOP_ARB_FULL | OOSIOP_SCNTL0_EPG);
	oosiop_write_1(sc, OOSIOP_SCNTL1, OOSIOP_SCNTL1_ESR);
	oosiop_write_1(sc, OOSIOP_DCNTL, sc->sc_dcntl);
	oosiop_write_1(sc, OOSIOP_DMODE, OOSIOP_DMODE_BL_8);
	oosiop_write_1(sc, OOSIOP_SCID, OOSIOP_SCID_VALUE(sc->sc_id));
	oosiop_write_1(sc, OOSIOP_DWT, 0xff);	/* Enable DMA timeout */
	oosiop_write_1(sc, OOSIOP_CTEST7, 0);
	oosiop_write_1(sc, OOSIOP_SXFER, 0);

	/* Clear all interrupts */
	(void)oosiop_read_1(sc, OOSIOP_SSTAT0);
	(void)oosiop_read_1(sc, OOSIOP_SSTAT1);
	(void)oosiop_read_1(sc, OOSIOP_DSTAT);

	/* Enable interrupts */
	oosiop_write_1(sc, OOSIOP_SIEN,
	    OOSIOP_SIEN_M_A | OOSIOP_SIEN_STO | OOSIOP_SIEN_SGE |
	    OOSIOP_SIEN_UDC | OOSIOP_SIEN_RST | OOSIOP_SIEN_PAR);
	oosiop_write_1(sc, OOSIOP_DIEN,
	    OOSIOP_DIEN_ABRT | OOSIOP_DIEN_SSI | OOSIOP_DIEN_SIR |
	    OOSIOP_DIEN_WTD | OOSIOP_DIEN_IID);

	/* Set target state to asynchronous */
	for (i = 0; i < OOSIOP_NTGT; i++) {
		sc->sc_tgt[i].flags = 0;
		sc->sc_tgt[i].scf = 0;
		sc->sc_tgt[i].sxfer = 0;
	}

	splx(s);
}

static void
oosiop_reset_bus(struct oosiop_softc *sc)
{
	int s, i;

	s = splbio();

	/* Assert SCSI RST */
	oosiop_write_1(sc, OOSIOP_SCNTL1, OOSIOP_SCNTL1_RST);
	delay(25);	/* Reset hold time (25us) */
	oosiop_write_1(sc, OOSIOP_SCNTL1, 0);

	/* Remove all nexuses */
	for (i = 0; i < OOSIOP_NTGT; i++) {
		if (sc->sc_tgt[i].nexus) {
			sc->sc_tgt[i].nexus->xfer->status =
			    SCSI_OOSIOP_NOSTATUS; /* XXX */
			oosiop_done(sc, sc->sc_tgt[i].nexus);
		}
	}

	sc->sc_curcb = NULL;

	delay(250000);	/* Reset to selection (250ms) */

	splx(s);
}

/*
 * interrupt handler
 */
int
oosiop_intr(struct oosiop_softc *sc)
{
	struct oosiop_cb *cb;
	uint32_t dcmd;
	int timeout;
	uint8_t istat, dstat, sstat0;

	istat = oosiop_read_1(sc, OOSIOP_ISTAT);

	if ((istat & (OOSIOP_ISTAT_SIP | OOSIOP_ISTAT_DIP)) == 0)
		return (0);

	sc->sc_nextdsp = Ent_wait_reselect;

	/* DMA interrupts */
	if (istat & OOSIOP_ISTAT_DIP) {
		oosiop_write_1(sc, OOSIOP_ISTAT, 0);

		dstat = oosiop_read_1(sc, OOSIOP_DSTAT);

		if (dstat & OOSIOP_DSTAT_ABRT) {
			sc->sc_nextdsp = oosiop_read_4(sc, OOSIOP_DSP) -
			    sc->sc_scrbase - 8;

			if (sc->sc_nextdsp == Ent_p_resel_msgin_move &&
			    (oosiop_read_1(sc, OOSIOP_SBCL) & OOSIOP_ACK)) {
				if ((dstat & OOSIOP_DSTAT_DFE) == 0)
					oosiop_flush_fifo(sc);
				sc->sc_nextdsp += 8;
			}
		}

		if (dstat & OOSIOP_DSTAT_SSI) {
			sc->sc_nextdsp = oosiop_read_4(sc, OOSIOP_DSP) -
			    sc->sc_scrbase;
			printf("%s: single step %08x\n",
			    device_xname(sc->sc_dev), sc->sc_nextdsp);
		}

		if (dstat & OOSIOP_DSTAT_SIR) {
			if ((dstat & OOSIOP_DSTAT_DFE) == 0)
				oosiop_flush_fifo(sc);
			oosiop_scriptintr(sc);
		}

		if (dstat & OOSIOP_DSTAT_WTD) {
			printf("%s: DMA time out\n", device_xname(sc->sc_dev));
			oosiop_reset(sc);
		}

		if (dstat & OOSIOP_DSTAT_IID) {
			dcmd = oosiop_read_4(sc, OOSIOP_DBC);
			if ((dcmd & 0xf8000000) == 0x48000000) {
				printf("%s: REQ asserted on WAIT DISCONNECT\n",
				    device_xname(sc->sc_dev));
				sc->sc_nextdsp = Ent_phasedispatch; /* XXX */
			} else {
				printf("%s: invalid SCRIPTS instruction "
				    "addr=%08x dcmd=%08x dsps=%08x\n",
				    device_xname(sc->sc_dev),
				    oosiop_read_4(sc, OOSIOP_DSP) - 8, dcmd,
				    oosiop_read_4(sc, OOSIOP_DSPS));
				oosiop_reset(sc);
				OOSIOP_SCRIPT_SYNC(sc, BUS_DMASYNC_POSTWRITE);
				oosiop_load_script(sc);
			}
		}

		if ((dstat & OOSIOP_DSTAT_DFE) == 0)
			oosiop_clear_fifo(sc);
	}

	/* SCSI interrupts */
	if (istat & OOSIOP_ISTAT_SIP) {
		if (istat & OOSIOP_ISTAT_DIP)
			delay(1);
		sstat0 = oosiop_read_1(sc, OOSIOP_SSTAT0);

		if (sstat0 & OOSIOP_SSTAT0_M_A) {
			/* SCSI phase mismatch during MOVE operation */
			oosiop_phasemismatch(sc);
			sc->sc_nextdsp = Ent_phasedispatch;
		}

		if (sstat0 & OOSIOP_SSTAT0_STO) {
			if (sc->sc_curcb) {
				sc->sc_curcb->flags |= CBF_SELTOUT;
				oosiop_done(sc, sc->sc_curcb);
			}
		}

		if (sstat0 & OOSIOP_SSTAT0_SGE) {
			printf("%s: SCSI gross error\n",
			    device_xname(sc->sc_dev));
			oosiop_reset(sc);
		}

		if (sstat0 & OOSIOP_SSTAT0_UDC) {
			/* XXX */
			if (sc->sc_curcb) {
				printf("%s: unexpected disconnect\n",
				    device_xname(sc->sc_dev));
				oosiop_done(sc, sc->sc_curcb);
			}
		}

		if (sstat0 & OOSIOP_SSTAT0_RST)
			oosiop_reset(sc);

		if (sstat0 & OOSIOP_SSTAT0_PAR)
			printf("%s: parity error\n", device_xname(sc->sc_dev));
	}

	/* Start next command if available */
	if (sc->sc_nextdsp == Ent_wait_reselect && TAILQ_FIRST(&sc->sc_cbq)) {
		cb = sc->sc_curcb = TAILQ_FIRST(&sc->sc_cbq);
		TAILQ_REMOVE(&sc->sc_cbq, cb, chain);
		sc->sc_tgt[cb->id].nexus = cb;

		oosiop_setup_dma(sc);
		oosiop_setup_syncxfer(sc);
		sc->sc_lastcb = cb;
		sc->sc_nextdsp = Ent_start_select;

		/* Schedule timeout */
		if ((cb->xs->xs_control & XS_CTL_POLL) == 0) {
			timeout = mstohz(cb->xs->timeout) + 1;
			callout_reset(&cb->xs->xs_callout, timeout,
			    oosiop_timeout, cb);
		}
	}

	sc->sc_active = (sc->sc_nextdsp != Ent_wait_reselect);

	/* Restart script */
	oosiop_write_4(sc, OOSIOP_DSP, sc->sc_nextdsp + sc->sc_scrbase);

	return (1);
}

static void
oosiop_scriptintr(struct oosiop_softc *sc)
{
	struct oosiop_cb *cb;
	uint32_t icode;
	uint32_t dsp;
	int i;
	uint8_t sfbr, resid, resmsg;

	cb = sc->sc_curcb;
	icode = oosiop_read_4(sc, OOSIOP_DSPS);

	switch (icode) {
	case A_int_done:
		if (cb)
			oosiop_done(sc, cb);
		break;

	case A_int_msgin:
		if (cb)
			oosiop_msgin(sc, cb);
		break;

	case A_int_extmsg:
		/* extended message in DMA setup request */
		sfbr = oosiop_read_1(sc, OOSIOP_SFBR);
		OOSIOP_SCRIPT_SYNC(sc, BUS_DMASYNC_POSTWRITE);
		oosiop_fixup_move(sc, Ent_p_extmsgin_move, sfbr,
		    cb->xferdma->dm_segs[0].ds_addr +
		    offsetof(struct oosiop_xfer, msgin[2]));
		OOSIOP_SCRIPT_SYNC(sc, BUS_DMASYNC_PREWRITE);
		sc->sc_nextdsp = Ent_rcv_extmsg;
		break;

	case A_int_resel:
		/* reselected */
		resid = oosiop_read_1(sc, OOSIOP_SFBR);
		for (i = 0; i < OOSIOP_NTGT; i++)
			if (resid & (1 << i))
				break;
		if (i == OOSIOP_NTGT) {
			printf("%s: missing reselection target id\n",
			    device_xname(sc->sc_dev));
			break;
		}
		sc->sc_resid = i;
		sc->sc_nextdsp = Ent_wait_resel_identify;

		if (cb) {
			/* Current command was lost arbitration */
			sc->sc_tgt[cb->id].nexus = NULL;
			TAILQ_INSERT_HEAD(&sc->sc_cbq, cb, chain);
			sc->sc_curcb = NULL;
		}

		break;

	case A_int_res_id:
		cb = sc->sc_tgt[sc->sc_resid].nexus;
		resmsg = oosiop_read_1(sc, OOSIOP_SFBR);
		if (MSG_ISIDENTIFY(resmsg) && cb &&
		    (resmsg & MSG_IDENTIFY_LUNMASK) == cb->lun) {
			sc->sc_curcb = cb;
			if (cb != sc->sc_lastcb) {
				oosiop_setup_dma(sc);
				oosiop_setup_syncxfer(sc);
				sc->sc_lastcb = cb;
			}
			if (cb->curdp != cb->savedp) {
				cb->curdp = cb->savedp;
				oosiop_setup_sgdma(sc, cb);
			}
			sc->sc_nextdsp = Ent_ack_msgin;
		} else {
			/* Reselection from invalid target */
			oosiop_reset_bus(sc);
		}
		break;

	case A_int_resfail:
		/* reselect failed */
		break;

	case A_int_disc:
		/* disconnected */
		sc->sc_curcb = NULL;
		break;

	case A_int_err:
		/* generic error */
		dsp = oosiop_read_4(sc, OOSIOP_DSP);
		printf("%s: script error at 0x%08x\n",
		    device_xname(sc->sc_dev), dsp - 8);
		sc->sc_curcb = NULL;
		break;

	case DATAIN_TRAP:
		printf("%s: unexpected datain\n", device_xname(sc->sc_dev));
		/* XXX: need to reset? */
		break;

	case DATAOUT_TRAP:
		printf("%s: unexpected dataout\n", device_xname(sc->sc_dev));
		/* XXX: need to reset? */
		break;

	default:
		printf("%s: unknown intr code %08x\n",
		    device_xname(sc->sc_dev), icode);
		break;
	}
}

static void
oosiop_msgin(struct oosiop_softc *sc, struct oosiop_cb *cb)
{
	struct oosiop_xfer *xfer;
	int msgout;

	xfer = cb->xfer;
	sc->sc_nextdsp = Ent_ack_msgin;
	msgout = 0;

	OOSIOP_XFERMSG_SYNC(sc, cb,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	switch (xfer->msgin[0]) {
	case MSG_EXTENDED:
		switch (xfer->msgin[2]) {
		case MSG_EXT_SDTR:
			if (sc->sc_tgt[cb->id].flags & TGTF_WAITSDTR) {
				/* Host initiated SDTR */
				sc->sc_tgt[cb->id].flags &= ~TGTF_WAITSDTR;
			} else {
				/* Target initiated SDTR */
				if (xfer->msgin[3] < sc->sc_minperiod)
					xfer->msgin[3] = sc->sc_minperiod;
				if (xfer->msgin[4] > OOSIOP_MAX_OFFSET)
					xfer->msgin[4] = OOSIOP_MAX_OFFSET;
				xfer->msgout[0] = MSG_EXTENDED;
				xfer->msgout[1] = MSG_EXT_SDTR_LEN;
				xfer->msgout[2] = MSG_EXT_SDTR;
				xfer->msgout[3] = xfer->msgin[3];
				xfer->msgout[4] = xfer->msgin[4];
				cb->msgoutlen = 5;
				msgout = 1;
			}
			oosiop_set_syncparam(sc, cb->id, (int)xfer->msgin[3],
			    (int)xfer->msgin[4]);
			oosiop_setup_syncxfer(sc);
			break;

		default:
			/* Reject message */
			xfer->msgout[0] = MSG_MESSAGE_REJECT;
			cb->msgoutlen = 1;
			msgout = 1;
			break;
		}
		break;

	case MSG_SAVEDATAPOINTER:
		cb->savedp = cb->curdp;
		break;

	case MSG_RESTOREPOINTERS:
		if (cb->curdp != cb->savedp) {
			cb->curdp = cb->savedp;
			oosiop_setup_sgdma(sc, cb);
		}
		break;

	case MSG_MESSAGE_REJECT:
		if (sc->sc_tgt[cb->id].flags & TGTF_WAITSDTR) {
			/* SDTR rejected */
			sc->sc_tgt[cb->id].flags &= ~TGTF_WAITSDTR;
			oosiop_set_syncparam(sc, cb->id, 0, 0);
			oosiop_setup_syncxfer(sc);
		}
		break;

	default:
		/* Reject message */
		xfer->msgout[0] = MSG_MESSAGE_REJECT;
		cb->msgoutlen = 1;
		msgout = 1;
	}

	OOSIOP_XFERMSG_SYNC(sc, cb,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	if (msgout) {
		OOSIOP_SCRIPT_SYNC(sc, BUS_DMASYNC_POSTWRITE);
		oosiop_fixup_move(sc, Ent_p_msgout_move, cb->msgoutlen,
		    cb->xferdma->dm_segs[0].ds_addr +
		    offsetof(struct oosiop_xfer, msgout[0]));
		OOSIOP_SCRIPT_SYNC(sc, BUS_DMASYNC_PREWRITE);
		sc->sc_nextdsp = Ent_sendmsg;
	}
}
