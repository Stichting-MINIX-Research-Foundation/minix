/*	$NetBSD: ninjaata32.c,v 1.18 2012/07/31 15:50:34 bouyer Exp $	*/

/*
 * Copyright (c) 2006 ITOH Yasufumi.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ninjaata32.c,v 1.18 2012/07/31 15:50:34 bouyer Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ata/atavar.h>
#include <dev/ic/wdcreg.h>
#include <dev/ic/wdcvar.h>

#include <dev/ic/ninjaata32reg.h>
#include <dev/ic/ninjaata32var.h>

#ifdef NJATA32_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

static void	njata32_init(struct njata32_softc *, int nosleep);
static void	njata32_irqack(struct ata_channel *);
static void	njata32_clearirq(struct ata_channel *, int);
static void	njata32_setup_channel(struct ata_channel *);
static int	njata32_dma_init(void *, int channel, int drive,
		    void *databuf, size_t datalen, int flags);
static void	njata32_piobm_start(void *, int channel, int drive, int skip,
		    int xferlen, int flags);
static int	njata32_dma_finish(void *, int channel, int drive, int force);
static void	njata32_piobm_done(void *, int channel, int drive);

#if 0	/* ATA DMA is currently unused */
static const uint8_t njata32_timing_dma[NJATA32_MODE_MAX_DMA + 1] = {
	NJATA32_TIMING_DMA0, NJATA32_TIMING_DMA1, NJATA32_TIMING_DMA2
};
#endif
static const uint8_t njata32_timing_pio[NJATA32_MODE_MAX_PIO + 1] = {
	NJATA32_TIMING_PIO0, NJATA32_TIMING_PIO1, NJATA32_TIMING_PIO2,
	NJATA32_TIMING_PIO3, NJATA32_TIMING_PIO4
};

static void
njata32_init(struct njata32_softc *sc, int nosleep)
{

	/* disable interrupts */
	bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
	    NJATA32_REG_IRQ_SELECT, 0);

	/* bus reset */
	bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc), NJATA32_REG_AS,
	    NJATA32_AS_WAIT0 | NJATA32_AS_BUS_RESET);
	if (nosleep)
		delay(50000);
	else
		tsleep(sc, PRIBIO, "njaini", mstohz(50));
	bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc), NJATA32_REG_AS,
	    NJATA32_AS_WAIT0);

	/* initial transfer speed */
	bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
	    NJATA32_REG_TIMING, NJATA32_TIMING_PIO0 + sc->sc_atawait);

	/* setup busmaster mode */
	bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc), NJATA32_REG_IOBM,
	    NJATA32_IOBM_DEFAULT);

	/* enable interrupts */
	bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
	    NJATA32_REG_IRQ_SELECT, NJATA32_IRQ_XFER | NJATA32_IRQ_DEV);
}

void
njata32_attach(struct njata32_softc *sc)
{
	bus_addr_t dmaaddr;
	int i, devno, error;
	struct wdc_regs *wdr;

	/*
	 * allocate DMA resource
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof(struct njata32_dma_page), PAGE_SIZE, 0,
	    &sc->sc_sgt_seg, 1, &sc->sc_sgt_nsegs, BUS_DMA_NOWAIT)) != 0) {
		aprint_error("%s: unable to allocate sgt page, error = %d\n",
		    NJATA32NAME(sc), error);
		return;
	}
	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->sc_sgt_seg,
	    sc->sc_sgt_nsegs, sizeof(struct njata32_dma_page),
	    (void **)&sc->sc_sgtpg,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		aprint_error("%s: unable to map sgt page, error = %d\n",
		    NJATA32NAME(sc), error);
		goto fail1;
	}
	if ((error = bus_dmamap_create(sc->sc_dmat,
	    sizeof(struct njata32_dma_page), 1,
	    sizeof(struct njata32_dma_page), 0, BUS_DMA_NOWAIT,
	    &sc->sc_dmamap_sgt)) != 0) {
		aprint_error("%s: unable to create sgt DMA map, error = %d\n",
		    NJATA32NAME(sc), error);
		goto fail2;
	}
	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap_sgt,
	    sc->sc_sgtpg, sizeof(struct njata32_dma_page),
	    NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error("%s: unable to load sgt DMA map, error = %d\n",
		    NJATA32NAME(sc), error);
		goto fail3;
	}

	dmaaddr = sc->sc_dmamap_sgt->dm_segs[0].ds_addr;

	for (devno = 0; devno < NJATA32_NUM_DEV; devno++) {
		sc->sc_dev[devno].d_sgt = sc->sc_sgtpg->dp_sg[devno];
		sc->sc_dev[devno].d_sgt_dma = dmaaddr +
		    offsetof(struct njata32_dma_page, dp_sg[devno]);

		error = bus_dmamap_create(sc->sc_dmat,
		    NJATA32_MAX_XFER,		/* max total map size */
		    NJATA32_NUM_SG,		/* max number of segments */
		    NJATA32_SGT_MAXSEGLEN,	/* max size of a segment */
		    0,				/* boundary */
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &sc->sc_dev[devno].d_dmamap_xfer);
		if (error) {
			aprint_error("%s: failed to create DMA map "
			    "(error = %d)\n", NJATA32NAME(sc), error);
			goto fail4;
		}
	}

	/* device properties */
	sc->sc_wdcdev.sc_atac.atac_cap =
	    ATAC_CAP_DATA16 | ATAC_CAP_DATA32 | ATAC_CAP_PIOBM;
	sc->sc_wdcdev.irqack = njata32_irqack;
	sc->sc_wdcdev.sc_atac.atac_channels = sc->sc_wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = NJATA32_NCHAN;	/* 1 */
	sc->sc_wdcdev.sc_atac.atac_pio_cap = NJATA32_MODE_MAX_PIO;
#if 0	/* ATA DMA is currently unused */
	sc->sc_wdcdev.sc_atac.atac_dma_cap = NJATA32_MODE_MAX_DMA;
#endif
	sc->sc_wdcdev.sc_atac.atac_set_modes = njata32_setup_channel;

	/* DMA control functions */
	sc->sc_wdcdev.dma_arg = sc;
	sc->sc_wdcdev.dma_init = njata32_dma_init;
	sc->sc_wdcdev.piobm_start = njata32_piobm_start;
	sc->sc_wdcdev.dma_finish = njata32_dma_finish;
	sc->sc_wdcdev.piobm_done = njata32_piobm_done;

	sc->sc_wdcdev.cap |= WDC_CAPABILITY_NO_EXTRA_RESETS;

	sc->sc_wdcdev.regs = wdr = &sc->sc_wdc_regs;

	/* only one channel */
	sc->sc_wdc_chanarray[0] = &sc->sc_ch[0].ch_ata_channel;
	sc->sc_ch[0].ch_ata_channel.ch_channel = 0;
	sc->sc_ch[0].ch_ata_channel.ch_atac = &sc->sc_wdcdev.sc_atac;
	sc->sc_ch[0].ch_ata_channel.ch_queue = &sc->sc_wdc_chqueue;
	sc->sc_wdcdev.wdc_maxdrives = 2; /* max number of drives per channel */

	/* map ATA registers */
	for (i = 0; i < WDC_NREG; i++) {
		if (bus_space_subregion(NJATA32_REGT(sc), NJATA32_REGH(sc),
		    NJATA32_OFFSET_WDCREGS + i,
		    i == wd_data ? 4 : 1, &wdr->cmd_iohs[i]) != 0) {
			aprint_error("%s: couldn't subregion cmd regs\n",
			    NJATA32NAME(sc));
			goto fail4;
		}
	}
	wdc_init_shadow_regs(&sc->sc_ch[0].ch_ata_channel);
	wdr->data32iot = NJATA32_REGT(sc);
	wdr->data32ioh = wdr->cmd_iohs[wd_data];

	/* map ATA ctl reg */
	wdr->ctl_iot = NJATA32_REGT(sc);
	if (bus_space_subregion(NJATA32_REGT(sc), NJATA32_REGH(sc),
	    NJATA32_REG_WD_ALTSTATUS, 1, &wdr->ctl_ioh) != 0) {
		aprint_error("%s: couldn't subregion ctl regs\n",
		    NJATA32NAME(sc));
		goto fail4;
	}

	sc->sc_flags |= NJATA32_CMDPG_MAPPED;

	/* use flags value as busmaster wait */
	if ((sc->sc_atawait =
	    (uint8_t)device_cfdata(sc->sc_wdcdev.sc_atac.atac_dev)->cf_flags))
		aprint_normal("%s: ATA wait = %#x\n",
		    NJATA32NAME(sc), sc->sc_atawait);

	njata32_init(sc, cold);

	wdcattach(&sc->sc_ch[0].ch_ata_channel);

	return;

	/*
	 * cleanup
	 */
fail4:	while (--devno >= 0) {
		bus_dmamap_destroy(sc->sc_dmat,
		    sc->sc_dev[devno].d_dmamap_xfer);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_dmamap_sgt);
fail3:	bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmamap_sgt);
fail2:	bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_sgtpg,
	    sizeof(struct njata32_dma_page));
fail1:	bus_dmamem_free(sc->sc_dmat, &sc->sc_sgt_seg, sc->sc_sgt_nsegs);
}

int
njata32_detach(struct njata32_softc *sc, int flags)
{
	int rv, devno;

	if (sc->sc_flags & NJATA32_CMDPG_MAPPED) {
		if ((rv = wdcdetach(sc->sc_wdcdev.sc_atac.atac_dev, flags)))
			return rv;

		/* free DMA resource */
		for (devno = 0; devno < NJATA32_NUM_DEV; devno++) {
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_dev[devno].d_dmamap_xfer);
		}
		bus_dmamap_unload(sc->sc_dmat, sc->sc_dmamap_sgt);
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmamap_sgt);
		bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_sgtpg,
		    sizeof(struct njata32_dma_page));
		bus_dmamem_free(sc->sc_dmat, &sc->sc_sgt_seg, sc->sc_sgt_nsegs);
	}

	return 0;
}

static void
njata32_irqack(struct ata_channel *chp)
{
	struct njata32_softc *sc = (void *)chp->ch_atac;

	/* disable busmaster */
	bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
	    NJATA32_REG_BM, NJATA32_BM_WAIT0);
}

static void
njata32_clearirq(struct ata_channel *chp, int irq)
{
	struct njata32_softc *sc = (void *)chp->ch_atac;

	aprint_error("%s: unhandled intr: irq %#x, bm %#x, ",
	    NJATA32NAME(sc), irq,
	    bus_space_read_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
		NJATA32_REG_BM));

	/* disable busmaster */
	njata32_irqack(chp);

	/* clear device interrupt */
	aprint_normal("err %#x, seccnt %#x, cyl %#x, sdh %#x, ",
	    bus_space_read_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
		NJATA32_REG_WD_ERROR),
	    bus_space_read_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
		NJATA32_REG_WD_SECCNT),
	    bus_space_read_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
		NJATA32_REG_WD_CYL_LO) |
	    (bus_space_read_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
		NJATA32_REG_WD_CYL_HI) << 8),
	    bus_space_read_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
		NJATA32_REG_WD_SDH));
	aprint_normal("status %#x\n",
	    bus_space_read_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
	    NJATA32_REG_WD_STATUS));
}

static void
njata32_setup_channel(struct ata_channel *chp)
{
	struct njata32_softc *sc = (void *)chp->ch_atac;
	struct ata_drive_datas *drvp;
	int drive;
	uint8_t mode;

	KASSERT(chp->ch_ndrives != 0);

	sc->sc_timing_pio = 0;
#if 0	/* ATA DMA is currently unused */
	sc->sc_timing_dma = 0;
#endif

	for (drive = 0; drive < chp->ch_ndrives; drive++) {
		drvp = &chp->ch_drive[drive];
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;	/* no drive */

#if 0	/* ATA DMA is currently unused */
		if ((drvp->drive_flags & ATA_DRIVE_DMA) != 0) {
			/*
			 * Multiword DMA
			 */
			if ((mode = drvp->DMA_mode) > NJATA32_MODE_MAX_DMA)
				mode = NJATA32_MODE_MAX_DMA;
			if (sc->sc_timing_dma < njata32_timing_dma[mode])
				sc->sc_timing_dma = njata32_timing_dma[mode];
		}
#endif
		/*
		 * PIO
		 */
		if ((mode = drvp->PIO_mode) > NJATA32_MODE_MAX_PIO)
			mode = NJATA32_MODE_MAX_PIO;
		if (sc->sc_timing_pio < njata32_timing_pio[mode])
			sc->sc_timing_pio = njata32_timing_pio[mode];
	}

	sc->sc_timing_pio += sc->sc_atawait;

	/* set timing for PIO */
	bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
	    NJATA32_REG_TIMING, sc->sc_timing_pio);
}

/*
 * map DMA buffer
 */
int
njata32_dma_init(void *v, int channel, int drive, void *databuf,
		 size_t datalen, int flags)
{
	struct njata32_softc *sc = v;
	int error;
	struct njata32_device *dev = &sc->sc_dev[drive];

	KASSERT(channel == 0);
	KASSERT((dev->d_flags & NJATA32_DEV_DMA_MAPPED) == 0);
	KASSERT((dev->d_flags & NJATA32_DEV_DMA_STARTED) == 0);

	KASSERT(flags & (WDC_DMA_PIOBM_ATA | WDC_DMA_PIOBM_ATAPI));

	/* use PIO for short transfer */
	if (datalen < 64 /* needs tune */) {
		DPRINTF(("%s: njata32_dma_init: short transfer (%u)\n",
		    NJATA32NAME(sc), (unsigned)datalen));
		bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
		    NJATA32_REG_TIMING, sc->sc_timing_pio);
		return EINVAL;
	}

	/* use PIO for unaligned transfer (word alignment seems OK) */
	if (((uintptr_t)databuf & 1) || (datalen & 1)) {
		DPRINTF(("%s: njata32_dma_init: unaligned: buf %p, len %u\n",
		    NJATA32NAME(sc), databuf, (unsigned)datalen));
		return EINVAL;
	}

	DPRINTF(("%s: njata32_dma_init: %s: databuf %p, datalen %u\n",
	    NJATA32NAME(sc), (flags & WDC_DMA_READ) ? "read" : "write",
	    databuf, (unsigned)datalen));

	error = bus_dmamap_load(sc->sc_dmat, dev->d_dmamap_xfer,
	    databuf, datalen, NULL, BUS_DMA_NOWAIT | BUS_DMA_STREAMING |
	    ((flags & WDC_DMA_READ) ? BUS_DMA_READ : BUS_DMA_WRITE));
	if (error) {
		printf("%s: load xfer failed, error %d\n",
		    NJATA32NAME(sc), error);
		return error;
	}

	bus_dmamap_sync(sc->sc_dmat, dev->d_dmamap_xfer, 0,
	    dev->d_dmamap_xfer->dm_mapsize,
	    (flags & WDC_DMA_READ) ?
		BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

	dev->d_flags =
	    ((flags & WDC_DMA_READ) ? NJATA32_DEV_DMA_READ : 0) |
	    ((flags & WDC_DMA_PIOBM_ATAPI) ? NJATA32_DEV_DMA_ATAPI : 0) |
	    NJATA32_DEV_DMA_MAPPED;

	return 0;
}

/*
 * start DMA
 *
 * top:  databuf + skip
 * size: xferlen
 */
void
njata32_piobm_start(void *v, int channel, int drive,
		    int skip, int xferlen, int flags)
{
	struct njata32_softc *sc = v;
	struct njata32_device *dev = &sc->sc_dev[drive];
	int i, nsegs, seglen;
	uint8_t bmreg;

	DPRINTF(("%s: njata32_piobm_start: ch%d, dv%d, skip %d, xferlen %d\n",
	    NJATA32NAME(sc), channel, drive, skip, xferlen));

	KASSERT(channel == 0);
	KASSERT(dev->d_flags & NJATA32_DEV_DMA_MAPPED);
	KASSERT((dev->d_flags & NJATA32_DEV_DMA_STARTED) == 0);

	/*
	 * create scatter/gather table
	 * XXX this code may be slow
	 */
	for (i = nsegs = 0;
	    i < dev->d_dmamap_xfer->dm_nsegs && xferlen > 0; i++) {
		if (dev->d_dmamap_xfer->dm_segs[i].ds_len <= skip) {
			skip -= dev->d_dmamap_xfer->dm_segs[i].ds_len;
			continue;
		}

		seglen = dev->d_dmamap_xfer->dm_segs[i].ds_len - skip;
		if (seglen > xferlen)
			seglen = xferlen;

		dev->d_sgt[nsegs].sg_addr =
		    htole32(dev->d_dmamap_xfer->dm_segs[i].ds_addr + skip);
		dev->d_sgt[nsegs].sg_len = htole32(seglen);

		xferlen -= seglen;
		nsegs++;
		skip = 0;
	}
	sc->sc_piobm_nsegs = nsegs;
	/* end mark */
	dev->d_sgt[nsegs - 1].sg_len |= htole32(NJATA32_SGT_ENDMARK);

#ifdef DIAGNOSTIC
	if (xferlen)
		panic("%s: njata32_piobm_start: xferlen residue %d\n",
		    NJATA32NAME(sc), xferlen);
#endif

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_sgt,
	    (char *)dev->d_sgt - (char *)sc->sc_sgtpg,
	    sizeof(struct njata32_sgtable) * nsegs,
	    BUS_DMASYNC_PREWRITE);

	/* set timing for PIO */
	bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
	    NJATA32_REG_TIMING, sc->sc_timing_pio);

	bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc), NJATA32_REG_IOBM,
	    NJATA32_IOBM_DEFAULT);
	bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc), NJATA32_REG_AS,
	    NJATA32_AS_WAIT0);

	/*
	 * interrupt configuration
	 */
	if ((dev->d_flags & (NJATA32_DEV_DMA_READ | NJATA32_DEV_DMA_ATAPI)) ==
	    NJATA32_DEV_DMA_READ) {
		/*
		 * ATA piobm read is executed while device interrupt is active,
		 * so disable device interrupt here
		 */
		bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
		    NJATA32_REG_IRQ_SELECT, NJATA32_IRQ_XFER);
	}

	/* enable scatter/gather busmaster transfer */
	bmreg = NJATA32_BM_EN | NJATA32_BM_SG | NJATA32_BM_WAIT0 |
	    ((dev->d_flags & NJATA32_DEV_DMA_READ) ? NJATA32_BM_RD : 0);
	bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc), NJATA32_REG_BM,
	    bmreg);

	/* load scatter/gather table */
	bus_space_write_4(NJATA32_REGT(sc), NJATA32_REGH(sc),
	    NJATA32_REG_DMAADDR, dev->d_sgt_dma);
	bus_space_write_4(NJATA32_REGT(sc), NJATA32_REGH(sc),
	    NJATA32_REG_DMALENGTH, sizeof(struct njata32_sgtable) * nsegs);

	/* start transfer */
	bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc), NJATA32_REG_BM,
	    (bus_space_read_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
		NJATA32_REG_BM)
	     & ~(NJATA32_BM_RD|NJATA32_BM_SG|NJATA32_BM_WAIT_MASK)) |
	    bmreg | NJATA32_BM_GO);

	sc->sc_devflags = dev->d_flags;
	if (flags & WDC_PIOBM_XFER_IRQ)
		sc->sc_devflags |= NJATA32_DEV_XFER_INTR;
#ifdef DIAGNOSTIC
	dev->d_flags |= NJATA32_DEV_DMA_STARTED;
#endif
}

/*
 * end of DMA
 */
int
njata32_dma_finish(void *v, int channel, int drive,
		   int force)
{
	struct njata32_softc *sc = v;
	struct njata32_device *dev = &sc->sc_dev[drive];
	int bm;
	int error = 0;

	DPRINTF(("%s: njata32_dma_finish: bm = %#x\n", NJATA32NAME(sc),
	    bus_space_read_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
		NJATA32_REG_BM)));

	KASSERT(channel == 0);
	KASSERT(dev->d_flags & NJATA32_DEV_DMA_MAPPED);
	KASSERT(dev->d_flags & NJATA32_DEV_DMA_STARTED);

	bm = bus_space_read_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
	    NJATA32_REG_BM);

#ifdef NJATA32_DEBUG
	printf("%s: irq %#x, bm %#x, 18 %#x, 1c %#x\n", NJATA32NAME(sc),
	    bus_space_read_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
		NJATA32_REG_IRQ_STAT),
	    bm,
	    bus_space_read_4(NJATA32_REGT(sc), NJATA32_REGH(sc), 0x18),
	    bus_space_read_1(NJATA32_REGT(sc), NJATA32_REGH(sc), 0x1c));
#endif

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_sgt,
	    (char *)dev->d_sgt - (char *)sc->sc_sgtpg,
	    sizeof(struct njata32_sgtable) * sc->sc_piobm_nsegs,
	    BUS_DMASYNC_POSTWRITE);

	/* check if DMA is active */
	if (bm & NJATA32_BM_GO) {
		error = WDC_DMAST_NOIRQ;

		switch (force) {
		case WDC_DMAEND_END:
			return error;

		case WDC_DMAEND_ABRT:
			printf("%s: aborting DMA\n", NJATA32NAME(sc));
			break;
		}
	}

	/*
	 * ???
	 * For unknown reason, PIOBM transfer sometimes fails in the middle,
	 * in which case the bit #7 of BM register becomes 0.
	 * Increasing the wait value seems to improve the situation.
	 *
	 * XXX
	 * PIO transfer may also fail, but it seems it can't be detected.
	 */
	if ((bm & NJATA32_BM_DONE) == 0) {
		error |= WDC_DMAST_ERR;
		printf("%s: busmaster error", NJATA32NAME(sc));
		if (sc->sc_atawait < 0x11) {
			if ((sc->sc_atawait & 0xf) == 0)
				sc->sc_atawait++;
			else
				sc->sc_atawait += 0x10;
			printf(", new ATA wait = %#x", sc->sc_atawait);
			njata32_setup_channel(&sc->sc_ch[0].ch_ata_channel);
		}
		printf("\n");
	}

	/* stop command */
	bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc), NJATA32_REG_AS,
	    NJATA32_AS_WAIT0);
	bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc), NJATA32_REG_BM,
	    NJATA32_BM_WAIT0);

	/* set timing for PIO */
	bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
	    NJATA32_REG_TIMING, sc->sc_timing_pio);

	/*
	 * reenable device interrupt in case it was disabled for
	 * this transfer
	 */
	bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
	    NJATA32_REG_IRQ_SELECT, NJATA32_IRQ_XFER | NJATA32_IRQ_DEV);

#if 1	/* should be? */
	if ((sc->sc_devflags & NJATA32_DEV_GOT_XFER_INTR) == 0)
		error |= WDC_DMAST_ERR;
#endif
	sc->sc_devflags = 0;

#ifdef DIAGNOSTIC
	dev->d_flags &= ~NJATA32_DEV_DMA_STARTED;
#endif

	return error;
}

/*
 * unmap DMA buffer
 */
void
njata32_piobm_done(void *v, int channel, int drive)
{
	struct njata32_softc *sc = v;
	struct njata32_device *dev = &sc->sc_dev[drive];

	DPRINTF(("%s: njata32_piobm_done: ch%d dv%d\n",
	    NJATA32NAME(sc), channel, drive));

	KASSERT(channel == 0);
	KASSERT(dev->d_flags & NJATA32_DEV_DMA_MAPPED);
	KASSERT((dev->d_flags & NJATA32_DEV_DMA_STARTED) == 0);

	/* unload dma map */
	bus_dmamap_sync(sc->sc_dmat, dev->d_dmamap_xfer,
	    0, dev->d_dmamap_xfer->dm_mapsize,
	    (dev->d_flags & NJATA32_DEV_DMA_READ) ?
		BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

	bus_dmamap_unload(sc->sc_dmat, dev->d_dmamap_xfer);
	dev->d_flags &= ~NJATA32_DEV_DMA_MAPPED;
}

int
njata32_intr(void *arg)
{
	struct njata32_softc *sc = arg;
	struct ata_channel *chp;
	int irq;

	irq = bus_space_read_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
	    NJATA32_REG_IRQ_STAT);
	if ((irq & (NJATA32_IRQ_XFER | NJATA32_IRQ_DEV)) == 0)
		return 0;	/* not mine */

	DPRINTF(("%s: njata32_intr: irq = %#x, altstatus = %#x\n",
	    NJATA32NAME(sc), irq,
	    bus_space_read_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
		NJATA32_REG_WD_ALTSTATUS)));

	chp = &sc->sc_ch[0].ch_ata_channel;

	if (irq & NJATA32_IRQ_XFER)
		sc->sc_devflags |= NJATA32_DEV_GOT_XFER_INTR;

	if ((irq & (NJATA32_IRQ_XFER | NJATA32_IRQ_DEV)) == NJATA32_IRQ_XFER &&
	    (sc->sc_devflags & NJATA32_DEV_XFER_INTR) == 0) {
		/*
		 * transfer done, wait for device interrupt
		 */
		bus_space_write_1(NJATA32_REGT(sc), NJATA32_REGH(sc),
		    NJATA32_REG_BM, NJATA32_BM_WAIT0);
		return 1;
	}

	/*
	 * If both transfer done interrupt and device interrupt are
	 * active for ATAPI transfer, call wdcintr() twice.
	 */
	if ((sc->sc_devflags & NJATA32_DEV_DMA_ATAPI) &&
	    (irq & (NJATA32_IRQ_XFER | NJATA32_IRQ_DEV)) ==
		(NJATA32_IRQ_XFER | NJATA32_IRQ_DEV) &&
	    (sc->sc_devflags & NJATA32_DEV_XFER_INTR)) {
		if (wdcintr(chp) == 0) {
			njata32_clearirq(&sc->sc_ch[0].ch_ata_channel, irq);
		}
	}

	if (wdcintr(chp) == 0) {
		njata32_clearirq(&sc->sc_ch[0].ch_ata_channel, irq);
	}

	return 1;
}
