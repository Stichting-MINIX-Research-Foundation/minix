/* $NetBSD: hdaudio.c,v 1.3 2015/07/26 17:54:33 jmcneill Exp $ */

/*
 * Copyright (c) 2009 Precedence Technologies Ltd <support@precedence.co.uk>
 * Copyright (c) 2009 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Precedence Technologies Ltd
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hdaudio.c,v 1.3 2015/07/26 17:54:33 jmcneill Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kmem.h>
#include <sys/module.h>

#include "hdaudiovar.h"
#include "hdaudioreg.h"
#include "hdaudioio.h"
#include "hdaudio_verbose.h"

/* #define	HDAUDIO_DEBUG */

#define	HDAUDIO_RESET_TIMEOUT	5000
#define HDAUDIO_CORB_TIMEOUT	1000
#define	HDAUDIO_RIRB_TIMEOUT	5000

#define	HDAUDIO_CODEC_DELAY	1000	/* spec calls for 250 */

dev_type_open(hdaudioopen);
dev_type_close(hdaudioclose);
dev_type_ioctl(hdaudioioctl);

const struct cdevsw hdaudio_cdevsw = {
	.d_open = hdaudioopen,
	.d_close = hdaudioclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = hdaudioioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

extern struct cfdriver hdaudio_cd;

#define	HDAUDIOUNIT(x)	minor((x))

static void
hdaudio_stream_init(struct hdaudio_softc *sc, int nis, int nos, int nbidir)
{
	int i, cnt = 0;

	for (i = 0; i < nis && cnt < HDAUDIO_MAX_STREAMS; i++) {
		sc->sc_stream[cnt].st_host = sc;
		sc->sc_stream[cnt].st_enable = true;
		sc->sc_stream[cnt].st_shift = cnt;
		sc->sc_stream[cnt++].st_type = HDAUDIO_STREAM_ISS;
	}
	for (i = 0; i < nos && cnt < HDAUDIO_MAX_STREAMS; i++) {
		sc->sc_stream[cnt].st_host = sc;
		sc->sc_stream[cnt].st_enable = true;
		sc->sc_stream[cnt].st_shift = cnt;
		sc->sc_stream[cnt++].st_type = HDAUDIO_STREAM_OSS;
	}
	for (i = 0; i < nbidir && cnt < HDAUDIO_MAX_STREAMS; i++) {
		sc->sc_stream[cnt].st_host = sc;
		sc->sc_stream[cnt].st_enable = true;
		sc->sc_stream[cnt].st_shift = cnt;
		sc->sc_stream[cnt++].st_type = HDAUDIO_STREAM_BSS;
	}

	for (i = 0; i < cnt; i++)
		hdaudio_stream_stop(&sc->sc_stream[i]);

	sc->sc_stream_mask = 0;
}

static void
hdaudio_codec_init(struct hdaudio_softc *sc)
{
	int i;

	for (i = 0; i < HDAUDIO_MAX_CODECS; i++) {
		sc->sc_codec[i].co_addr = i;
		sc->sc_codec[i].co_host = sc;
	}
}

static void
hdaudio_init(struct hdaudio_softc *sc)
{
	uint16_t gcap;
	int nos, nis, nbidir;
#if defined(HDAUDIO_DEBUG)
	uint8_t vmin, vmaj;
	int nsdo, addr64;
#endif

#if defined(HDAUDIO_DEBUG)
	vmaj = hda_read1(sc, HDAUDIO_MMIO_VMAJ);
	vmin = hda_read1(sc, HDAUDIO_MMIO_VMIN);

	hda_print(sc, "High Definition Audio version %d.%d\n", vmaj, vmin);
#endif

	gcap = hda_read2(sc, HDAUDIO_MMIO_GCAP);
	nis = HDAUDIO_GCAP_ISS(gcap);
	nos = HDAUDIO_GCAP_OSS(gcap);
	nbidir = HDAUDIO_GCAP_BSS(gcap);

	/* Initialize codecs and streams */
	hdaudio_codec_init(sc);
	hdaudio_stream_init(sc, nis, nos, nbidir);

#if defined(HDAUDIO_DEBUG)
	nsdo = HDAUDIO_GCAP_NSDO(gcap);
	addr64 = HDAUDIO_GCAP_64OK(gcap);

	hda_print(sc, "OSS %d ISS %d BSS %d SDO %d%s\n",
	    nos, nis, nbidir, nsdo, addr64 ? " 64-bit" : "");
#endif
}

static int
hdaudio_codec_probe(struct hdaudio_softc *sc)
{
	uint16_t statests;
	int codecid;

	statests = hda_read2(sc, HDAUDIO_MMIO_STATESTS);
	for (codecid = 0; codecid < HDAUDIO_MAX_CODECS; codecid++)
		if (statests & (1 << codecid))
			sc->sc_codec[codecid].co_valid = true;
	hda_write2(sc, HDAUDIO_MMIO_STATESTS, statests);

	return statests;
}

int
hdaudio_dma_alloc(struct hdaudio_softc *sc, struct hdaudio_dma *dma,
    int flags)
{
	int err;

	KASSERT(dma->dma_size > 0);

	err = bus_dmamem_alloc(sc->sc_dmat, dma->dma_size, 128, 0,
	    dma->dma_segs, sizeof(dma->dma_segs) / sizeof(dma->dma_segs[0]),
	    &dma->dma_nsegs, BUS_DMA_WAITOK);
	if (err)
		return err;
	err = bus_dmamem_map(sc->sc_dmat, dma->dma_segs, dma->dma_nsegs,
	    dma->dma_size, &dma->dma_addr, BUS_DMA_WAITOK | flags);
	if (err)
		goto free;
	err = bus_dmamap_create(sc->sc_dmat, dma->dma_size, dma->dma_nsegs,
	    dma->dma_size, 0, BUS_DMA_WAITOK, &dma->dma_map);
	if (err)
		goto unmap;
	err = bus_dmamap_load(sc->sc_dmat, dma->dma_map, dma->dma_addr,
	    dma->dma_size, NULL, BUS_DMA_WAITOK | flags);
	if (err)
		goto destroy;

	dma->dma_valid = true;
	return 0;

destroy:
	bus_dmamap_destroy(sc->sc_dmat, dma->dma_map);	
unmap:
	bus_dmamem_unmap(sc->sc_dmat, dma->dma_addr, dma->dma_size);
free:
	bus_dmamem_free(sc->sc_dmat, dma->dma_segs, dma->dma_nsegs);

	dma->dma_valid = false;
	return err;
}

void
hdaudio_dma_free(struct hdaudio_softc *sc, struct hdaudio_dma *dma)
{
	if (dma->dma_valid == false)
		return;
	bus_dmamap_unload(sc->sc_dmat, dma->dma_map);
	bus_dmamap_destroy(sc->sc_dmat, dma->dma_map);	
	bus_dmamem_unmap(sc->sc_dmat, dma->dma_addr, dma->dma_size);
	bus_dmamem_free(sc->sc_dmat, dma->dma_segs, dma->dma_nsegs);
	dma->dma_valid = false;
}

static void
hdaudio_corb_enqueue(struct hdaudio_softc *sc, int addr, int nid,
    uint32_t control, uint32_t param)
{
	uint32_t *corb = DMA_KERNADDR(&sc->sc_corb);
	uint32_t verb;
	uint16_t corbrp;
	int wp;

	/* Build command */
	verb = (addr << 28) | (nid << 20) | (control << 8) | param;

	/* Fetch and update write pointer */
	corbrp = hda_read2(sc, HDAUDIO_MMIO_CORBWP);
	wp = (corbrp & 0xff) + 1;
	if (wp >= (sc->sc_corb.dma_size / sizeof(*corb)))
		wp = 0;

	/* Enqueue command */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_corb.dma_map, 0,
	    sc->sc_corb.dma_size, BUS_DMASYNC_POSTWRITE);
	corb[wp] = verb;
	bus_dmamap_sync(sc->sc_dmat, sc->sc_corb.dma_map, 0,
	    sc->sc_corb.dma_size, BUS_DMASYNC_PREWRITE);

	/* Commit updated write pointer */
	hda_write2(sc, HDAUDIO_MMIO_CORBWP, wp);
}

static void
hdaudio_rirb_unsol(struct hdaudio_softc *sc, struct rirb_entry *entry)
{
	struct hdaudio_codec *co;
	struct hdaudio_function_group *fg;
	uint8_t codecid = RIRB_CODEC_ID(entry);
	unsigned int i;

	if (codecid >= HDAUDIO_MAX_CODECS) {
		hda_error(sc, "unsol: codec id 0x%02x out of range\n", codecid);
		return;
	}
	co = &sc->sc_codec[codecid];
	if (sc->sc_codec[codecid].co_valid == false) {
		hda_error(sc, "unsol: codec id 0x%02x not valid\n", codecid);
		return;
	}

	for (i = 0; i < co->co_nfg; i++) {
		fg = &co->co_fg[i];
		if (fg->fg_device && fg->fg_unsol)
			fg->fg_unsol(fg->fg_device, entry->resp);
	}
}

static uint32_t
hdaudio_rirb_dequeue(struct hdaudio_softc *sc, bool unsol)
{
	uint16_t rirbwp;
	uint64_t *rirb = DMA_KERNADDR(&sc->sc_rirb);
	struct rirb_entry entry;
	int retry;

	for (;;) {
		retry = HDAUDIO_RIRB_TIMEOUT;

		rirbwp = hda_read2(sc, HDAUDIO_MMIO_RIRBWP);
		while (--retry > 0 && (rirbwp & 0xff) == sc->sc_rirbrp) {
			if (unsol) {
				/* don't wait for more unsol events */
				hda_trace(sc, "unsol: rirb empty\n");
				return 0xffffffff;
			}
			hda_delay(10);
			rirbwp = hda_read2(sc, HDAUDIO_MMIO_RIRBWP);
		}
		if (retry == 0) {
			hda_error(sc, "RIRB timeout\n");
			return 0xffffffff;
		}

		sc->sc_rirbrp++;
		if (sc->sc_rirbrp >= (sc->sc_rirb.dma_size / sizeof(*rirb)))
			sc->sc_rirbrp = 0;

		bus_dmamap_sync(sc->sc_dmat, sc->sc_rirb.dma_map, 0,
		    sc->sc_rirb.dma_size, BUS_DMASYNC_POSTREAD);
		entry = *(struct rirb_entry *)&rirb[sc->sc_rirbrp];
		bus_dmamap_sync(sc->sc_dmat, sc->sc_rirb.dma_map, 0,
		    sc->sc_rirb.dma_size, BUS_DMASYNC_PREREAD);

		hda_trace(sc, "%s: response %08X %08X\n",
		    unsol ? "unsol" : "cmd  ",
		    entry.resp, entry.resp_ex);

		if (RIRB_UNSOL(&entry)) {
			hdaudio_rirb_unsol(sc, &entry);
			continue;
		}

		return entry.resp;
	}
}

uint32_t
hdaudio_command(struct hdaudio_codec *co, int nid, uint32_t control,
    uint32_t param)
{
	uint32_t result;
	struct hdaudio_softc *sc = co->co_host;
	mutex_enter(&sc->sc_corb_mtx);
	result = hdaudio_command_unlocked(co, nid, control, param);
	mutex_exit(&sc->sc_corb_mtx);
	return result;
}

uint32_t
hdaudio_command_unlocked(struct hdaudio_codec *co, int nid, uint32_t control,
    uint32_t param)
{
	struct hdaudio_softc *sc = co->co_host;
	uint32_t result;

	hda_trace(sc, "cmd  : request %08X %08X (%02X)\n",
	    control, param, nid);
	hdaudio_corb_enqueue(sc, co->co_addr, nid, control, param);
	result = hdaudio_rirb_dequeue(sc, false);

	return result;
}

static int
hdaudio_corb_setsize(struct hdaudio_softc *sc)
{
	uint8_t corbsize;
	bus_size_t bufsize = 0;

	/*
	 * The size of the CORB is programmable to 2, 16, or 256 entries
	 * by using the CORBSIZE register. Choose a size based on the
	 * controller capabilities, preferring a larger size when possible.
	 */
	corbsize = hda_read1(sc, HDAUDIO_MMIO_CORBSIZE);
	corbsize &= ~0x3;
	if ((corbsize >> 4) & 0x4) {
		corbsize |= 0x2;
		bufsize = 1024;
	} else if ((corbsize >> 4) & 0x2) {
		corbsize |= 0x1;
		bufsize = 64;
	} else if ((corbsize >> 4) & 0x1) {
		corbsize |= 0x0;
		bufsize = 8;
	} else {
		hda_error(sc, "couldn't configure CORB size\n");
		return ENXIO;
	}

#if defined(HDAUDIO_DEBUG)
	hda_print(sc, "using %d byte CORB (cap %X)\n",
	    (int)bufsize, corbsize >> 4);
#endif

	sc->sc_corb.dma_size = bufsize;
	sc->sc_corb.dma_sizereg = corbsize;

	return 0;
}

static int
hdaudio_corb_config(struct hdaudio_softc *sc)
{
	uint32_t corbubase, corblbase;
	uint16_t corbrp;
	int retry = HDAUDIO_CORB_TIMEOUT;

	/* Program command buffer base address and size */
	corblbase = (uint32_t)DMA_DMAADDR(&sc->sc_corb);
	corbubase = (uint32_t)(((uint64_t)DMA_DMAADDR(&sc->sc_corb)) >> 32);
	hda_write4(sc, HDAUDIO_MMIO_CORBLBASE, corblbase);
	hda_write4(sc, HDAUDIO_MMIO_CORBUBASE, corbubase);
	hda_write1(sc, HDAUDIO_MMIO_CORBSIZE, sc->sc_corb.dma_sizereg);

	/* Clear the read and write pointers */
	hda_write2(sc, HDAUDIO_MMIO_CORBRP, HDAUDIO_CORBRP_RP_RESET);
	hda_write2(sc, HDAUDIO_MMIO_CORBRP, 0);
	do {
		hda_delay(10);
		corbrp = hda_read2(sc, HDAUDIO_MMIO_CORBRP);
	} while (--retry > 0 && (corbrp & HDAUDIO_CORBRP_RP_RESET) != 0);
	if (retry == 0) {
		hda_error(sc, "timeout resetting CORB\n");
		return ETIME;
	}
	hda_write2(sc, HDAUDIO_MMIO_CORBWP, 0);

	return 0;
}

static int
hdaudio_corb_stop(struct hdaudio_softc *sc)
{
	uint8_t corbctl;
	int retry = HDAUDIO_CORB_TIMEOUT;

	/* Stop the CORB if necessary */
	corbctl = hda_read1(sc, HDAUDIO_MMIO_CORBCTL);
	if (corbctl & HDAUDIO_CORBCTL_RUN) {
		corbctl &= ~HDAUDIO_CORBCTL_RUN;
		hda_write4(sc, HDAUDIO_MMIO_CORBCTL, corbctl);
		do {
			hda_delay(10);
			corbctl = hda_read4(sc, HDAUDIO_MMIO_CORBCTL);
		} while (--retry > 0 && (corbctl & HDAUDIO_CORBCTL_RUN) != 0);
		if (retry == 0) {
			hda_error(sc, "timeout stopping CORB\n");
			return ETIME;
		}
	}

	return 0;
}

static int
hdaudio_corb_start(struct hdaudio_softc *sc)
{
	uint8_t corbctl;
	int retry = HDAUDIO_CORB_TIMEOUT;

	/* Start the CORB if necessary */
	corbctl = hda_read1(sc, HDAUDIO_MMIO_CORBCTL);
	if ((corbctl & HDAUDIO_CORBCTL_RUN) == 0) {
		corbctl |= HDAUDIO_CORBCTL_RUN;
		hda_write4(sc, HDAUDIO_MMIO_CORBCTL, corbctl);
		do {
			hda_delay(10);
			corbctl = hda_read4(sc, HDAUDIO_MMIO_CORBCTL);
		} while (--retry > 0 && (corbctl & HDAUDIO_CORBCTL_RUN) == 0);
		if (retry == 0) {
			hda_error(sc, "timeout starting CORB\n");
			return ETIME;
		}
	}

	return 0;
}

static int
hdaudio_rirb_stop(struct hdaudio_softc *sc)
{
	uint8_t rirbctl;
	int retry = HDAUDIO_RIRB_TIMEOUT;

	/* Stop the RIRB if necessary */
	rirbctl = hda_read1(sc, HDAUDIO_MMIO_RIRBCTL);
	if (rirbctl & (HDAUDIO_RIRBCTL_RUN|HDAUDIO_RIRBCTL_ROI_EN)) {
		rirbctl &= ~HDAUDIO_RIRBCTL_RUN;
		rirbctl &= ~HDAUDIO_RIRBCTL_ROI_EN;
		hda_write1(sc, HDAUDIO_MMIO_RIRBCTL, rirbctl);
		do {
			hda_delay(10);
			rirbctl = hda_read1(sc, HDAUDIO_MMIO_RIRBCTL);
		} while (--retry > 0 && (rirbctl & HDAUDIO_RIRBCTL_RUN) != 0);
		if (retry == 0) {
			hda_error(sc, "timeout stopping RIRB\n");
			return ETIME;
		}
	}

	return 0;
}

static int
hdaudio_rirb_start(struct hdaudio_softc *sc)
{
	uint8_t rirbctl;
	int retry = HDAUDIO_RIRB_TIMEOUT;

	/* Start the RIRB if necessary */
	rirbctl = hda_read1(sc, HDAUDIO_MMIO_RIRBCTL);
	if ((rirbctl & (HDAUDIO_RIRBCTL_RUN|HDAUDIO_RIRBCTL_INT_EN)) == 0) {
		rirbctl |= HDAUDIO_RIRBCTL_RUN;
		rirbctl |= HDAUDIO_RIRBCTL_INT_EN;
		hda_write1(sc, HDAUDIO_MMIO_RIRBCTL, rirbctl);
		do {
			hda_delay(10);
			rirbctl = hda_read1(sc, HDAUDIO_MMIO_RIRBCTL);
		} while (--retry > 0 && (rirbctl & HDAUDIO_RIRBCTL_RUN) == 0);
		if (retry == 0) {
			hda_error(sc, "timeout starting RIRB\n");
			return ETIME;
		}
	}

	return 0;
}

static int
hdaudio_rirb_setsize(struct hdaudio_softc *sc)
{
	uint8_t rirbsize;
	bus_size_t bufsize = 0;

	/*
	 * The size of the RIRB is programmable to 2, 16, or 256 entries
	 * by using the RIRBSIZE register. Choose a size based on the
	 * controller capabilities, preferring a larger size when possible.
	 */
	rirbsize = hda_read1(sc, HDAUDIO_MMIO_RIRBSIZE);
	rirbsize &= ~0x3;
	if ((rirbsize >> 4) & 0x4) {
		rirbsize |= 0x2;
		bufsize = 2048;
	} else if ((rirbsize >> 4) & 0x2) {
		rirbsize |= 0x1;
		bufsize = 128;
	} else if ((rirbsize >> 4) & 0x1) {
		rirbsize |= 0x0;
		bufsize = 16;
	} else {
		hda_error(sc, "couldn't configure RIRB size\n");
		return ENXIO;
	}

#if defined(HDAUDIO_DEBUG)
	hda_print(sc, "using %d byte RIRB (cap %X)\n",
	    (int)bufsize, rirbsize >> 4);
#endif

	sc->sc_rirb.dma_size = bufsize;
	sc->sc_rirb.dma_sizereg = rirbsize;

	return 0;
}

static int
hdaudio_rirb_config(struct hdaudio_softc *sc)
{
	uint32_t rirbubase, rirblbase;
	uint32_t rirbwp;
	int retry = HDAUDIO_RIRB_TIMEOUT;

	/* Program command buffer base address and size */
	rirblbase = (uint32_t)DMA_DMAADDR(&sc->sc_rirb);
	rirbubase = (uint32_t)(((uint64_t)DMA_DMAADDR(&sc->sc_rirb)) >> 32);
	hda_write4(sc, HDAUDIO_MMIO_RIRBLBASE, rirblbase);
	hda_write4(sc, HDAUDIO_MMIO_RIRBUBASE, rirbubase);
	hda_write1(sc, HDAUDIO_MMIO_RIRBSIZE, sc->sc_rirb.dma_sizereg);

	/* Clear the write pointer */
	hda_write2(sc, HDAUDIO_MMIO_RIRBWP, HDAUDIO_RIRBWP_WP_RESET);
	hda_write2(sc, HDAUDIO_MMIO_RIRBWP, 0);
	do {
		hda_delay(10);
		rirbwp = hda_read2(sc, HDAUDIO_MMIO_RIRBWP);
	} while (--retry > 0 && (rirbwp & HDAUDIO_RIRBWP_WP_RESET) != 0);
	if (retry == 0) {
		hda_error(sc, "timeout resetting RIRB\n");
		return ETIME;
	}
	sc->sc_rirbrp = 0;

	return 0;
}

static int
hdaudio_reset(struct hdaudio_softc *sc)
{
	int retry = HDAUDIO_RESET_TIMEOUT;
	uint32_t gctl;
	int err;

	if ((err = hdaudio_rirb_stop(sc)) != 0) {
		hda_error(sc, "couldn't reset because RIRB is busy\n");
		return err;
	}
	if ((err = hdaudio_corb_stop(sc)) != 0) {
		hda_error(sc, "couldn't reset because CORB is busy\n");
		return err;
	}

	/* Disable wake events */
	hda_write2(sc, HDAUDIO_MMIO_WAKEEN, 0);

	/* Disable interrupts */
	hda_write4(sc, HDAUDIO_MMIO_INTCTL, 0);

	/* Clear state change status register */
	hda_write2(sc, HDAUDIO_MMIO_STATESTS,
	    hda_read2(sc, HDAUDIO_MMIO_STATESTS));
	hda_write1(sc, HDAUDIO_MMIO_RIRBSTS,
	    hda_read1(sc, HDAUDIO_MMIO_RIRBSTS));

	/* If the controller isn't in reset state, initiate the transition */
	gctl = hda_read4(sc, HDAUDIO_MMIO_GCTL);
	if (gctl & HDAUDIO_GCTL_CRST) {
		gctl &= ~HDAUDIO_GCTL_CRST;
		hda_write4(sc, HDAUDIO_MMIO_GCTL, gctl);
		do {
			hda_delay(10);
			gctl = hda_read4(sc, HDAUDIO_MMIO_GCTL);
		} while (--retry > 0 && (gctl & HDAUDIO_GCTL_CRST) != 0);
		if (retry == 0) {
			hda_error(sc, "timeout entering reset state\n");
			return ETIME;
		}
	}

	/* Now the controller is in reset state, so bring it out */
	retry = HDAUDIO_RESET_TIMEOUT;
	hda_write4(sc, HDAUDIO_MMIO_GCTL, gctl | HDAUDIO_GCTL_CRST);
	do {
		hda_delay(10);
		gctl = hda_read4(sc, HDAUDIO_MMIO_GCTL);
	} while (--retry > 0 && (gctl & HDAUDIO_GCTL_CRST) == 0);
	if (retry == 0) {
		hda_error(sc, "timeout leaving reset state\n");
		return ETIME;
	}

	/* Accept unsolicited responses */
	hda_write4(sc, HDAUDIO_MMIO_GCTL, gctl | HDAUDIO_GCTL_UNSOL_EN);

	return 0;
}

static void
hdaudio_intr_enable(struct hdaudio_softc *sc)
{
	hda_write4(sc, HDAUDIO_MMIO_INTSTS,
	    hda_read4(sc, HDAUDIO_MMIO_INTSTS));
	hda_write4(sc, HDAUDIO_MMIO_INTCTL,
	    HDAUDIO_INTCTL_GIE | HDAUDIO_INTCTL_CIE);
}

static void
hdaudio_intr_disable(struct hdaudio_softc *sc)
{
	hda_write4(sc, HDAUDIO_MMIO_INTCTL, 0);
}

static int
hdaudio_config_print(void *opaque, const char *pnp)
{
	prop_dictionary_t dict = opaque;
	uint8_t fgtype, nid;
	uint16_t vendor, product;
	const char *type = "unknown";

	prop_dictionary_get_uint8(dict, "function-group-type", &fgtype);
	prop_dictionary_get_uint8(dict, "node-id", &nid);
	prop_dictionary_get_uint16(dict, "vendor-id", &vendor);
	prop_dictionary_get_uint16(dict, "product-id", &product);
	if (pnp) {
		if (fgtype == HDAUDIO_GROUP_TYPE_AFG)
			type = "hdafg";
		else if (fgtype == HDAUDIO_GROUP_TYPE_VSM_FG)
			type = "hdvsmfg";

		aprint_normal("%s at %s", type, pnp);
	}
	aprint_debug(" vendor 0x%04X product 0x%04X nid 0x%02X",
	    vendor, product, nid);

	return UNCONF;
}

static void
hdaudio_attach_fg(struct hdaudio_function_group *fg, prop_array_t config)
{
	struct hdaudio_codec *co = fg->fg_codec;
	struct hdaudio_softc *sc = co->co_host;
	prop_dictionary_t args = prop_dictionary_create();
	uint64_t fgptr = (vaddr_t)fg;
	int locs[1];

	prop_dictionary_set_uint8(args, "function-group-type", fg->fg_type);
	prop_dictionary_set_uint64(args, "function-group", fgptr);
	prop_dictionary_set_uint8(args, "node-id", fg->fg_nid);
	prop_dictionary_set_uint16(args, "vendor-id", fg->fg_vendor);
	prop_dictionary_set_uint16(args, "product-id", fg->fg_product);
	if (config)
		prop_dictionary_set(args, "pin-config", config);

	locs[0] = fg->fg_nid;

	fg->fg_device = config_found_sm_loc(sc->sc_dev, "hdaudiobus",
	    locs, args, hdaudio_config_print, config_stdsubmatch);

	prop_object_release(args);
}

static void
hdaudio_codec_attach(struct hdaudio_codec *co)
{
	struct hdaudio_function_group *fg;
	uint32_t vid, snc, fgrp;
	int starting_node, num_nodes, nid;

	if (co->co_valid == false)
		return;

	vid = hdaudio_command(co, 0, CORB_GET_PARAMETER, COP_VENDOR_ID);
	snc = hdaudio_command(co, 0, CORB_GET_PARAMETER,
	    COP_SUBORDINATE_NODE_COUNT);

	/* make sure the vendor and product IDs are valid */
	if (vid == 0xffffffff || vid == 0x00000000)
		return;

#ifdef HDAUDIO_DEBUG
	struct hdaudio_softc *sc = co->co_host;
	uint32_t rid = hdaudio_command(co, 0, CORB_GET_PARAMETER,
	    COP_REVISION_ID);
	hda_print(sc, "Codec%02X: %04X:%04X HDA %d.%d rev %d stepping %d\n",
	    co->co_addr, vid >> 16, vid & 0xffff,
	    (rid >> 20) & 0xf, (rid >> 16) & 0xf,
	    (rid >> 8) & 0xff, rid & 0xff);
#endif
	starting_node = (snc >> 16) & 0xff;
	num_nodes = snc & 0xff;

	co->co_nfg = num_nodes;
	co->co_fg = kmem_zalloc(co->co_nfg * sizeof(*co->co_fg), KM_SLEEP);

	for (nid = starting_node; nid < starting_node + num_nodes; nid++) {
		fg = &co->co_fg[nid - starting_node];
		fg->fg_codec = co;
		fg->fg_nid = nid;
		fg->fg_vendor = vid >> 16;
		fg->fg_product = vid & 0xffff;

		fgrp = hdaudio_command(co, nid, CORB_GET_PARAMETER,
		    COP_FUNCTION_GROUP_TYPE);
		switch (fgrp & 0xff) {
		case 0x01:	/* Audio Function Group */
			fg->fg_type = HDAUDIO_GROUP_TYPE_AFG;
			break;
		case 0x02:	/* Vendor Specific Modem Function Group */
			fg->fg_type = HDAUDIO_GROUP_TYPE_VSM_FG;
			break;
		default:
			/* Function group type not supported */
			fg->fg_type = HDAUDIO_GROUP_TYPE_UNKNOWN;
			break;
		}
		hdaudio_attach_fg(fg, NULL);
	}
}

int
hdaudio_stream_tag(struct hdaudio_stream *st)
{
	int ret = 0;

	switch (st->st_type) {
	case HDAUDIO_STREAM_ISS:
		ret = 1;
		break;
	case HDAUDIO_STREAM_OSS:
		ret = 2;
		break;
	case HDAUDIO_STREAM_BSS:
		ret = 3;
		break;
	}

	return ret;
}

int
hdaudio_attach(device_t dev, struct hdaudio_softc *sc)
{
	int err, i;

	KASSERT(sc->sc_memvalid == true);

	sc->sc_dev = dev;
	mutex_init(&sc->sc_corb_mtx, MUTEX_DEFAULT, IPL_AUDIO);
	mutex_init(&sc->sc_stream_mtx, MUTEX_DEFAULT, IPL_AUDIO);

	hdaudio_init(sc);

	/*
	 * Put the controller into a known state by entering and leaving
	 * CRST as necessary.
	 */
	if ((err = hdaudio_reset(sc)) != 0)
		goto fail;

	/*
	 * From the spec:
	 *
	 * Must wait 250us after reading CRST as a 1 before assuming that
	 * codecs have all made status change requests and have been
	 * registered by the controller.
	 *
	 * In reality, we need to wait longer than this.
	 */
	hda_delay(HDAUDIO_CODEC_DELAY);
	if (hdaudio_codec_probe(sc) == 0) {
		hda_error(sc, "no codecs found\n");
		err = ENODEV;
		goto fail;
	}

	/*
	 * Ensure that the device is in a known state
	 */
	hda_write2(sc, HDAUDIO_MMIO_STATESTS, HDAUDIO_STATESTS_SDIWAKE);
	hda_write1(sc, HDAUDIO_MMIO_RIRBSTS,
	    HDAUDIO_RIRBSTS_RIRBOIS | HDAUDIO_RIRBSTS_RINTFL);
	hda_write4(sc, HDAUDIO_MMIO_INTSTS,
	    hda_read4(sc, HDAUDIO_MMIO_INTSTS));
	hda_write4(sc, HDAUDIO_MMIO_DPLBASE, 0);
	hda_write4(sc, HDAUDIO_MMIO_DPUBASE, 0);

	/*
	 * Initialize the CORB. First negotiate a command buffer size,
	 * then allocate and configure it.
	 */
	if ((err = hdaudio_corb_setsize(sc)) != 0)
		goto fail;
	if ((err = hdaudio_dma_alloc(sc, &sc->sc_corb, BUS_DMA_WRITE)) != 0)
		goto fail;
	if ((err = hdaudio_corb_config(sc)) != 0)
		goto fail;

	/*
	 * Initialize the RIRB.
	 */
	if ((err = hdaudio_rirb_setsize(sc)) != 0)
		goto fail;
	if ((err = hdaudio_dma_alloc(sc, &sc->sc_rirb, BUS_DMA_READ)) != 0)
		goto fail;
	if ((err = hdaudio_rirb_config(sc)) != 0)
		goto fail;

	/*
	 * Start the CORB and RIRB
	 */
	if ((err = hdaudio_corb_start(sc)) != 0)
		goto fail;
	if ((err = hdaudio_rirb_start(sc)) != 0)
		goto fail;

	/*
	 * Identify and attach discovered codecs
	 */
	for (i = 0; i < HDAUDIO_MAX_CODECS; i++)
		hdaudio_codec_attach(&sc->sc_codec[i]);

	/*
	 * Enable interrupts
	 */
	hdaudio_intr_enable(sc);

fail:
	if (err)
		hda_error(sc, "device driver failed to attach\n");
	return err;
}

int
hdaudio_detach(struct hdaudio_softc *sc, int flags)
{
	int error;

	/* Disable interrupts */
	hdaudio_intr_disable(sc);

	error = config_detach_children(sc->sc_dev, flags);
	if (error != 0) {
		hdaudio_intr_enable(sc);
		return error;
	}

	mutex_destroy(&sc->sc_corb_mtx);
	mutex_destroy(&sc->sc_stream_mtx);

	hdaudio_dma_free(sc, &sc->sc_corb);
	hdaudio_dma_free(sc, &sc->sc_rirb);

	return 0;
}

bool
hdaudio_resume(struct hdaudio_softc *sc)
{
	if (hdaudio_reset(sc) != 0)
		return false;

	hda_delay(HDAUDIO_CODEC_DELAY);

	/*
	 * Ensure that the device is in a known state
	 */
	hda_write2(sc, HDAUDIO_MMIO_STATESTS, HDAUDIO_STATESTS_SDIWAKE);
	hda_write1(sc, HDAUDIO_MMIO_RIRBSTS,
	    HDAUDIO_RIRBSTS_RIRBOIS | HDAUDIO_RIRBSTS_RINTFL);
	hda_write4(sc, HDAUDIO_MMIO_INTSTS,
	    hda_read4(sc, HDAUDIO_MMIO_INTSTS));
	hda_write4(sc, HDAUDIO_MMIO_DPLBASE, 0);
	hda_write4(sc, HDAUDIO_MMIO_DPUBASE, 0);

	if (hdaudio_corb_config(sc) != 0)
		return false;
	if (hdaudio_rirb_config(sc) != 0)
		return false;
	if (hdaudio_corb_start(sc) != 0)
		return false;
	if (hdaudio_rirb_start(sc) != 0)
		return false;

	hdaudio_intr_enable(sc);

	return true;
}

int
hdaudio_rescan(struct hdaudio_softc *sc, const char *ifattr, const int *locs)
{
	struct hdaudio_codec *co;
	struct hdaudio_function_group *fg;
	unsigned int codec;

	if (!ifattr_match(ifattr, "hdaudiobus"))
		return 0;

	for (codec = 0; codec < HDAUDIO_MAX_CODECS; codec++) {
		co = &sc->sc_codec[codec];
		fg = co->co_fg;
		if (!co->co_valid || fg == NULL)
			continue;
		if (fg->fg_device)
			continue;
		hdaudio_attach_fg(fg, NULL);
	}

	return 0;
}

void
hdaudio_childdet(struct hdaudio_softc *sc, device_t child)
{
	struct hdaudio_codec *co;
	struct hdaudio_function_group *fg;
	unsigned int codec;

	for (codec = 0; codec < HDAUDIO_MAX_CODECS; codec++) {
		co = &sc->sc_codec[codec];
		fg = co->co_fg;
		if (!co->co_valid || fg == NULL)
			continue;
		if (fg->fg_device == child)
			fg->fg_device = NULL;
	}
}

int
hdaudio_intr(struct hdaudio_softc *sc)
{
	struct hdaudio_stream *st;
	uint32_t intsts, stream_mask;
	int streamid = 0;
	uint8_t rirbsts;

	intsts = hda_read4(sc, HDAUDIO_MMIO_INTSTS);
	if (!(intsts & HDAUDIO_INTSTS_GIS))
		return 0;

	if (intsts & HDAUDIO_INTSTS_CIS) {
		rirbsts = hda_read1(sc, HDAUDIO_MMIO_RIRBSTS);
		if (rirbsts & HDAUDIO_RIRBSTS_RINTFL) {
			mutex_enter(&sc->sc_corb_mtx);
			hdaudio_rirb_dequeue(sc, true);
			mutex_exit(&sc->sc_corb_mtx);
		}
		if (rirbsts & (HDAUDIO_RIRBSTS_RIRBOIS|HDAUDIO_RIRBSTS_RINTFL))
			hda_write1(sc, HDAUDIO_MMIO_RIRBSTS, rirbsts);
		hda_write4(sc, HDAUDIO_MMIO_INTSTS, HDAUDIO_INTSTS_CIS);
	}
	if (intsts & HDAUDIO_INTSTS_SIS_MASK) {
		mutex_enter(&sc->sc_stream_mtx);
		stream_mask = intsts & sc->sc_stream_mask;
		while (streamid < HDAUDIO_MAX_STREAMS && stream_mask != 0) {
			st = &sc->sc_stream[streamid++];
			if ((stream_mask & 1) != 0 && st->st_intr) {
				st->st_intr(st);
			}
			stream_mask >>= 1;
		}
		mutex_exit(&sc->sc_stream_mtx);
		hda_write4(sc, HDAUDIO_MMIO_INTSTS, HDAUDIO_INTSTS_SIS_MASK);
	}

	return 1;
}

struct hdaudio_stream *
hdaudio_stream_establish(struct hdaudio_softc *sc,
    enum hdaudio_stream_type type, int (*intr)(struct hdaudio_stream *),
    void *cookie)
{
	struct hdaudio_stream *st;
	struct hdaudio_dma dma;
	int i, err;

	dma.dma_size = sizeof(struct hdaudio_bdl_entry) * HDAUDIO_BDL_MAX;
	dma.dma_sizereg = 0;
	err = hdaudio_dma_alloc(sc, &dma, BUS_DMA_COHERENT | BUS_DMA_NOCACHE);
	if (err)
		return NULL;

	mutex_enter(&sc->sc_stream_mtx);
	for (i = 0; i < HDAUDIO_MAX_STREAMS; i++) {
		st = &sc->sc_stream[i];
		if (st->st_enable == false)
			break;
		if (st->st_type != type)
			continue;
		if (sc->sc_stream_mask & (1 << i))
			continue;

		/* Allocate stream */
		st->st_bdl = dma;
		st->st_intr = intr;
		st->st_cookie = cookie;
		sc->sc_stream_mask |= (1 << i);
		mutex_exit(&sc->sc_stream_mtx);
		return st;
	}
	mutex_exit(&sc->sc_stream_mtx);

	/* No streams of requested type available */
	hdaudio_dma_free(sc, &dma);
	return NULL;
}

void
hdaudio_stream_disestablish(struct hdaudio_stream *st)
{
	struct hdaudio_softc *sc = st->st_host;
	struct hdaudio_dma dma;

	KASSERT(sc->sc_stream_mask & (1 << st->st_shift));

	mutex_enter(&sc->sc_stream_mtx);
	sc->sc_stream_mask &= ~(1 << st->st_shift);
	st->st_intr = NULL;
	st->st_cookie = NULL;
	dma = st->st_bdl;
	st->st_bdl.dma_valid = false;
	mutex_exit(&sc->sc_stream_mtx);

	/* Can't bus_dmamem_unmap while holding a mutex.  */
	hdaudio_dma_free(sc, &dma);
}

/*
 * Convert most of audio_params_t to stream fmt descriptor; noticably missing
 * is the # channels bits, as this is encoded differently in codec and
 * stream descriptors.
 *
 * TODO: validate that the stream and selected codecs can handle the fmt
 */
uint16_t
hdaudio_stream_param(struct hdaudio_stream *st, const audio_params_t *param)
{
	uint16_t fmt = 0;

	switch (param->encoding) {
	case AUDIO_ENCODING_AC3:
		fmt |= HDAUDIO_FMT_TYPE_NONPCM;
		break;
	default:
		fmt |= HDAUDIO_FMT_TYPE_PCM;
		break;
	}

	switch (param->sample_rate) {
	case 8000:
		fmt |= HDAUDIO_FMT_BASE_48 | HDAUDIO_FMT_MULT(1) |
		    HDAUDIO_FMT_DIV(6);
		break;
	case 11025:
		fmt |= HDAUDIO_FMT_BASE_44 | HDAUDIO_FMT_MULT(1) |
		    HDAUDIO_FMT_DIV(4);
		break;
	case 16000:
		fmt |= HDAUDIO_FMT_BASE_48 | HDAUDIO_FMT_MULT(1) |
		    HDAUDIO_FMT_DIV(3);
		break;
	case 22050:
		fmt |= HDAUDIO_FMT_BASE_44 | HDAUDIO_FMT_MULT(1) |
		    HDAUDIO_FMT_DIV(2);
		break;
	case 32000:
		fmt |= HDAUDIO_FMT_BASE_48 | HDAUDIO_FMT_MULT(2) |
		    HDAUDIO_FMT_DIV(3);
		break;
	case 44100:
		fmt |= HDAUDIO_FMT_BASE_44 | HDAUDIO_FMT_MULT(1);
		break;
	case 48000:
		fmt |= HDAUDIO_FMT_BASE_48 | HDAUDIO_FMT_MULT(1);
		break;
	case 88200:
		fmt |= HDAUDIO_FMT_BASE_44 | HDAUDIO_FMT_MULT(2);
		break;
	case 96000:
		fmt |= HDAUDIO_FMT_BASE_48 | HDAUDIO_FMT_MULT(2);
		break;
	case 176400:
		fmt |= HDAUDIO_FMT_BASE_44 | HDAUDIO_FMT_MULT(4);
		break;
	case 192000:
		fmt |= HDAUDIO_FMT_BASE_48 | HDAUDIO_FMT_MULT(4);
		break;
	default:
		return 0;
	}

	if (param->precision == 16 && param->validbits == 8)
		fmt |= HDAUDIO_FMT_BITS_8_16;
	else if (param->precision == 16 && param->validbits == 16)
		fmt |= HDAUDIO_FMT_BITS_16_16;
	else if (param->precision == 32 && param->validbits == 20)
		fmt |= HDAUDIO_FMT_BITS_20_32;
	else if (param->precision == 32 && param->validbits == 24)
		fmt |= HDAUDIO_FMT_BITS_24_32;
	else if (param->precision == 32 && param->validbits == 32)
		fmt |= HDAUDIO_FMT_BITS_32_32;
	else
		return 0;

	return fmt;
}

void
hdaudio_stream_reset(struct hdaudio_stream *st)
{
	struct hdaudio_softc *sc = st->st_host;
	int snum = st->st_shift;
	int retry;
	uint8_t ctl0;

	ctl0 = hda_read1(sc, HDAUDIO_SD_CTL0(snum));
	ctl0 |= HDAUDIO_CTL_SRST;
	hda_write1(sc, HDAUDIO_SD_CTL0(snum), ctl0);

	retry = HDAUDIO_RESET_TIMEOUT;
	do {
		ctl0 = hda_read1(sc, HDAUDIO_SD_CTL0(snum));
		if (ctl0 & HDAUDIO_CTL_SRST)
			break;
		hda_delay(10);
	} while (--retry > 0);
	if (retry == 0) {
		hda_error(sc, "timeout entering stream reset state\n");
		return;
	}

	ctl0 &= ~HDAUDIO_CTL_SRST;
	hda_write1(sc, HDAUDIO_SD_CTL0(snum), ctl0);

	retry = HDAUDIO_RESET_TIMEOUT;
	do {
		ctl0 = hda_read1(sc, HDAUDIO_SD_CTL0(snum));
		if (!(ctl0 & HDAUDIO_CTL_SRST))
			break;
		hda_delay(10);
	} while (--retry > 0);
	if (retry == 0) {
		hda_error(sc, "timeout leaving stream reset state\n");
		return;
	}
}

void
hdaudio_stream_start(struct hdaudio_stream *st, int blksize,
    bus_size_t dmasize, const audio_params_t *params)
{
	struct hdaudio_softc *sc = st->st_host;
	struct hdaudio_bdl_entry *bdl;
	uint64_t dmaaddr;
	uint32_t intctl;
	uint16_t fmt;
	uint8_t ctl0, ctl2;
	int cnt, snum = st->st_shift;

	KASSERT(sc->sc_stream_mask & (1 << st->st_shift));
	KASSERT(st->st_data.dma_valid == true);
	KASSERT(st->st_bdl.dma_valid == true);

	hdaudio_stream_stop(st);
	if ((sc->sc_flags & HDAUDIO_FLAG_NO_STREAM_RESET) == 0)
		hdaudio_stream_reset(st);

	/*
	 * Configure buffer descriptor list
	 */
	dmaaddr = DMA_DMAADDR(&st->st_data);
	bdl = DMA_KERNADDR(&st->st_bdl);
	for (cnt = 0; cnt < HDAUDIO_BDL_MAX; cnt++) {
		bdl[cnt].address_lo = (uint32_t)dmaaddr;
		bdl[cnt].address_hi = dmaaddr >> 32;
		bdl[cnt].length = blksize;
		bdl[cnt].flags = HDAUDIO_BDL_ENTRY_IOC;
		dmaaddr += blksize;
		if (dmaaddr >= DMA_DMAADDR(&st->st_data) + dmasize) {
			cnt++;
			break;
		}
	}

	/*
	 * Program buffer descriptor list
	 */
	dmaaddr = DMA_DMAADDR(&st->st_bdl);
	hda_write4(sc, HDAUDIO_SD_BDPL(snum), (uint32_t)dmaaddr);
	hda_write4(sc, HDAUDIO_SD_BDPU(snum), (uint32_t)(dmaaddr >> 32));
	hda_write2(sc, HDAUDIO_SD_LVI(snum), (cnt - 1) & 0xff);

	/*
	 * Program cyclic buffer length
	 */
	hda_write4(sc, HDAUDIO_SD_CBL(snum), dmasize);

	/*
	 * Program stream number (tag). Although controller hardware is
	 * capable of transmitting any stream number (0-15), by convention
	 * stream 0 is reserved as unused by software, so that converters
	 * whose stream numbers have been reset to 0 do not unintentionally
	 * decode data not intended for them.
	 */
	ctl2 = hda_read1(sc, HDAUDIO_SD_CTL2(snum));
	ctl2 &= ~0xf0;
	ctl2 |= hdaudio_stream_tag(st) << 4;
	hda_write1(sc, HDAUDIO_SD_CTL2(snum), ctl2);

	/*
	 * Program stream format
	 */
	fmt = hdaudio_stream_param(st, params) |
	    HDAUDIO_FMT_CHAN(params->channels);
	hda_write2(sc, HDAUDIO_SD_FMT(snum), fmt);

	/*
	 * Switch on interrupts for this stream
	 */
	intctl = hda_read4(sc, HDAUDIO_MMIO_INTCTL);
	intctl |= (1 << st->st_shift);
	hda_write4(sc, HDAUDIO_MMIO_INTCTL, intctl);

	/*
	 * Start running the stream
	 */
	ctl0 = hda_read1(sc, HDAUDIO_SD_CTL0(snum));
	ctl0 |= HDAUDIO_CTL_DEIE | HDAUDIO_CTL_FEIE | HDAUDIO_CTL_IOCE |
	    HDAUDIO_CTL_RUN;
	hda_write1(sc, HDAUDIO_SD_CTL0(snum), ctl0);
}

void
hdaudio_stream_stop(struct hdaudio_stream *st)
{
	struct hdaudio_softc *sc = st->st_host;
	uint32_t intctl;
	uint8_t ctl0;
	int snum = st->st_shift;

	/*
	 * Stop running the stream
	 */
	ctl0 = hda_read1(sc, HDAUDIO_SD_CTL0(snum));
	ctl0 &= ~(HDAUDIO_CTL_DEIE | HDAUDIO_CTL_FEIE | HDAUDIO_CTL_IOCE |
	    HDAUDIO_CTL_RUN);
	hda_write1(sc, HDAUDIO_SD_CTL0(snum), ctl0);

	/*
	 * Switch off interrupts for this stream
	 */
	intctl = hda_read4(sc, HDAUDIO_MMIO_INTCTL);
	intctl &= ~(1 << st->st_shift);
	hda_write4(sc, HDAUDIO_MMIO_INTCTL, intctl);
}

/*
 * /dev/hdaudioN interface
 */

static const char *
hdaudioioctl_fgrp_to_cstr(enum function_group_type type)
{
	switch (type) {
	case HDAUDIO_GROUP_TYPE_AFG:
		return "afg";
	case HDAUDIO_GROUP_TYPE_VSM_FG:
		return "vsmfg";
	default:
		return "unknown";
	}
}

static struct hdaudio_function_group *
hdaudioioctl_fgrp_lookup(struct hdaudio_softc *sc, int codecid, int nid)
{
	struct hdaudio_codec *co;
	struct hdaudio_function_group *fg = NULL;
	int i;

	if (codecid < 0 || codecid >= HDAUDIO_MAX_CODECS)
		return NULL;
	co = &sc->sc_codec[codecid];
	if (co->co_valid == false)
		return NULL;

	for (i = 0; i < co->co_nfg; i++)
		if (co->co_fg[i].fg_nid == nid) {
			fg = &co->co_fg[i];
			break;
		}

	return fg;
}

static int
hdaudioioctl_fgrp_info(struct hdaudio_softc *sc, prop_dictionary_t request,
    prop_dictionary_t response)
{
	struct hdaudio_codec *co;
	struct hdaudio_function_group *fg;
	prop_array_t array;
	prop_dictionary_t dict;
	int codecid, fgid;

	array = prop_array_create();
	if (array == NULL)
		return ENOMEM;

	for (codecid = 0; codecid < HDAUDIO_MAX_CODECS; codecid++) {
		co = &sc->sc_codec[codecid];
		if (co->co_valid == false)
			continue;
		for (fgid = 0; fgid < co->co_nfg; fgid++) {
			fg = &co->co_fg[fgid];
			dict = prop_dictionary_create();
			if (dict == NULL)
				return ENOMEM;
			prop_dictionary_set_cstring_nocopy(dict,
			    "type", hdaudioioctl_fgrp_to_cstr(fg->fg_type));
			prop_dictionary_set_int16(dict, "nid", fg->fg_nid);
			prop_dictionary_set_int16(dict, "codecid", codecid);
			prop_dictionary_set_uint16(dict, "vendor-id",
			    fg->fg_vendor);
			prop_dictionary_set_uint16(dict, "product-id",
			    fg->fg_product);
			prop_dictionary_set_uint32(dict, "subsystem-id",
			    sc->sc_subsystem);
			if (fg->fg_device)
				prop_dictionary_set_cstring(dict, "device",
				    device_xname(fg->fg_device));
			else
				prop_dictionary_set_cstring_nocopy(dict,
				    "device", "<none>");
			prop_array_add(array, dict);
		}
	}

	prop_dictionary_set(response, "function-group-info", array);
	return 0;
}

static int
hdaudioioctl_fgrp_getconfig(struct hdaudio_softc *sc,
    prop_dictionary_t request, prop_dictionary_t response)
{
	struct hdaudio_function_group *fg;
	prop_dictionary_t dict;
	prop_array_t array;
	uint32_t nodecnt, wcap, config;
	int16_t codecid, nid, i;
	int startnode, endnode;

	if (!prop_dictionary_get_int16(request, "codecid", &codecid) ||
	    !prop_dictionary_get_int16(request, "nid", &nid))
		return EINVAL;

	fg = hdaudioioctl_fgrp_lookup(sc, codecid, nid);
	if (fg == NULL)
		return ENODEV;

	array = prop_array_create();
	if (array == NULL)
		return ENOMEM;

	nodecnt = hdaudio_command(fg->fg_codec, fg->fg_nid,
	    CORB_GET_PARAMETER, COP_SUBORDINATE_NODE_COUNT);
	startnode = COP_NODECNT_STARTNODE(nodecnt);
	endnode = startnode + COP_NODECNT_NUMNODES(nodecnt);

	for (i = startnode; i < endnode; i++) {
		wcap = hdaudio_command(fg->fg_codec, i,
		    CORB_GET_PARAMETER, COP_AUDIO_WIDGET_CAPABILITIES);
		if (COP_AWCAP_TYPE(wcap) != COP_AWCAP_TYPE_PIN_COMPLEX)
			continue;
		config = hdaudio_command(fg->fg_codec, i,
		    CORB_GET_CONFIGURATION_DEFAULT, 0);
		dict = prop_dictionary_create();
		if (dict == NULL)
			return ENOMEM;
		prop_dictionary_set_int16(dict, "nid", i);
		prop_dictionary_set_uint32(dict, "config", config);
		prop_array_add(array, dict);
	}

	prop_dictionary_set(response, "pin-config", array);

	return 0;
}

static int
hdaudioioctl_fgrp_setconfig(struct hdaudio_softc *sc,
    prop_dictionary_t request, prop_dictionary_t response)
{
	struct hdaudio_function_group *fg;
	prop_array_t config;
	int16_t codecid, nid;
	int err;

	if (!prop_dictionary_get_int16(request, "codecid", &codecid) ||
	    !prop_dictionary_get_int16(request, "nid", &nid))
		return EINVAL;

	fg = hdaudioioctl_fgrp_lookup(sc, codecid, nid);
	if (fg == NULL)
		return ENODEV;

	if (fg->fg_device) {
		err = config_detach(fg->fg_device, 0);
		if (err)
			return err;
		fg->fg_device = NULL;
	}

	/* "pin-config" may be NULL, this means "use BIOS configuration" */
	config = prop_dictionary_get(request, "pin-config");
	if (config && prop_object_type(config) != PROP_TYPE_ARRAY) {
		prop_object_release(config);
		return EINVAL;
	}
	hdaudio_attach_fg(fg, config);
	if (config)
		prop_object_release(config);

	return 0;
}

static int
hdaudio_dispatch_fgrp_ioctl(struct hdaudio_softc *sc, u_long cmd,
    prop_dictionary_t request, prop_dictionary_t response)
{
	struct hdaudio_function_group *fg;
	int (*infocb)(void *, prop_dictionary_t, prop_dictionary_t);
	prop_dictionary_t fgrp_dict;
	uint64_t info_fn;
	int16_t codecid, nid;
	void *fgrp_sc; 
	bool rv;
	int err;

	if (!prop_dictionary_get_int16(request, "codecid", &codecid) ||
	    !prop_dictionary_get_int16(request, "nid", &nid))
		return EINVAL;

	fg = hdaudioioctl_fgrp_lookup(sc, codecid, nid);
	if (fg == NULL)
		return ENODEV;
	if (fg->fg_device == NULL)
		return ENXIO;
	fgrp_sc = device_private(fg->fg_device);
	fgrp_dict = device_properties(fg->fg_device);

	switch (fg->fg_type) {
	case HDAUDIO_GROUP_TYPE_AFG:
		switch (cmd) {
		case HDAUDIO_FGRP_CODEC_INFO:
			rv = prop_dictionary_get_uint64(fgrp_dict,
			    "codecinfo-callback", &info_fn);
			if (!rv)
				return ENXIO;
			infocb = (void *)(uintptr_t)info_fn;
			err = infocb(fgrp_sc, request, response);
			break;
		case HDAUDIO_FGRP_WIDGET_INFO:
			rv = prop_dictionary_get_uint64(fgrp_dict,
			    "widgetinfo-callback", &info_fn);
			if (!rv)
				return ENXIO;
			infocb = (void *)(uintptr_t)info_fn;
			err = infocb(fgrp_sc, request, response);
			break;
		default:
			err = EINVAL;
			break;
		}
		break;

	default:
		err = EINVAL;
		break;
	}
	return err;
}

int
hdaudioopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	device_t self;

	self = device_lookup(&hdaudio_cd, HDAUDIOUNIT(dev));
	if (self == NULL)
		return ENXIO;

	return 0;
}

int
hdaudioclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	return 0;
}

int
hdaudioioctl(dev_t dev, u_long cmd, void *addr, int flag, struct lwp *l)
{
	struct hdaudio_softc *sc;
	struct plistref *pref = addr;
	prop_dictionary_t request, response;
	int err;

	sc = device_lookup_private(&hdaudio_cd, HDAUDIOUNIT(dev));
	if (sc == NULL)
		return ENXIO;

	response = prop_dictionary_create();
	if (response == NULL)
		return ENOMEM;

	err = prop_dictionary_copyin_ioctl(pref, cmd, &request);
	if (err) {
		prop_object_release(response);
		return err;
	}

	switch (cmd) {
	case HDAUDIO_FGRP_INFO:
		err = hdaudioioctl_fgrp_info(sc, request, response);
		break;
	case HDAUDIO_FGRP_GETCONFIG:
		err = hdaudioioctl_fgrp_getconfig(sc, request, response);
		break;
	case HDAUDIO_FGRP_SETCONFIG:
		err = hdaudioioctl_fgrp_setconfig(sc, request, response);
		break;
	case HDAUDIO_FGRP_CODEC_INFO:
	case HDAUDIO_FGRP_WIDGET_INFO:
		err = hdaudio_dispatch_fgrp_ioctl(sc, cmd, request, response);
		break;
	default:
		err = EINVAL;
		break;
	}

	if (!err)
		err = prop_dictionary_copyout_ioctl(pref, cmd, response);

	if (response)
		prop_object_release(response);
	prop_object_release(request);
	return err;
}

MODULE(MODULE_CLASS_DRIVER, hdaudio, NULL);

static int
hdaudio_modcmd(modcmd_t cmd, void *opaque)
{
	int error = 0;
#ifdef _MODULE
	int bmaj = -1, cmaj = -1;
#endif

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = devsw_attach("hdaudio", NULL, &bmaj,
		    &hdaudio_cdevsw, &cmaj);
#endif
		return error;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		devsw_detach(NULL, &hdaudio_cdevsw);
#endif
		return 0;
	default:
		return ENOTTY;
	}
}

DEV_VERBOSE_DEFINE(hdaudio);
