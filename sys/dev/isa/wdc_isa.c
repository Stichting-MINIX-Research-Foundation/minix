/*	$NetBSD: wdc_isa.c,v 1.59 2012/07/31 15:50:35 bouyer Exp $ */

/*-
 * Copyright (c) 1998, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Onno van der Linden.
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
__KERNEL_RCSID(0, "$NetBSD: wdc_isa.c,v 1.59 2012/07/31 15:50:35 bouyer Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/wdcreg.h>
#include <dev/ata/atavar.h>
#include <dev/ic/wdcvar.h>

#define	WDC_ISA_REG_NPORTS	8
#define	WDC_ISA_AUXREG_OFFSET	0x206
#define	WDC_ISA_AUXREG_NPORTS	1 /* XXX "fdc" owns ports 0x3f7/0x377 */

/* options passed via the 'flags' config keyword */
#define WDC_OPTIONS_32			0x01 /* try to use 32bit data I/O */
#define WDC_OPTIONS_ATA_NOSTREAM	0x04
#define WDC_OPTIONS_ATAPI_NOSTREAM	0x08

struct wdc_isa_softc {
	struct	wdc_softc sc_wdcdev;
	struct	ata_channel *wdc_chanlist[1];
	struct	ata_channel ata_channel;
	struct	ata_queue wdc_chqueue;
	struct	wdc_regs wdc_regs;
	isa_chipset_tag_t sc_ic;
	void	*sc_ih;
	int	sc_drq;
};

static int	wdc_isa_probe(device_t , cfdata_t, void *);
static void	wdc_isa_attach(device_t, device_t, void *);
static int	wdc_isa_detach(device_t, int);

CFATTACH_DECL3_NEW(wdc_isa, sizeof(struct wdc_isa_softc),
    wdc_isa_probe, wdc_isa_attach, wdc_isa_detach, NULL, NULL,
    wdc_childdetached, DVF_DETACH_SHUTDOWN);

#if 0
static void	wdc_isa_dma_setup(struct wdc_isa_softc *);
static int	wdc_isa_dma_init(void*, int, int, void *, size_t, int);
static void 	wdc_isa_dma_start(void*, int, int);
static int	wdc_isa_dma_finish(void*, int, int, int);
#endif

static int
wdc_isa_probe(device_t parent, cfdata_t match, void *aux)
{
	struct ata_channel ch;
	struct isa_attach_args *ia = aux;
	struct wdc_softc wdc;
	struct wdc_regs wdr;
	int result = 0, i;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);
	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ)
		return (0);
	if (ia->ia_ndrq > 0 && ia->ia_drq[0].ir_drq == ISA_UNKNOWN_DRQ)
		ia->ia_ndrq = 0;

	memset(&wdc, 0, sizeof(wdc));
	memset(&ch, 0, sizeof(ch));
	ch.ch_atac = &wdc.sc_atac;
	wdc.regs = &wdr;

	wdr.cmd_iot = ia->ia_iot;

	if (bus_space_map(wdr.cmd_iot, ia->ia_io[0].ir_addr,
	    WDC_ISA_REG_NPORTS, 0, &wdr.cmd_baseioh))
		goto out;

	for (i = 0; i < WDC_ISA_REG_NPORTS; i++) {
		if (bus_space_subregion(wdr.cmd_iot, wdr.cmd_baseioh, i,
		    i == 0 ? 4 : 1, &wdr.cmd_iohs[i]) != 0)
			goto outunmap;
	}
	wdc_init_shadow_regs(&ch);

	wdr.ctl_iot = ia->ia_iot;
	if (bus_space_map(wdr.ctl_iot, ia->ia_io[0].ir_addr +
	    WDC_ISA_AUXREG_OFFSET, WDC_ISA_AUXREG_NPORTS, 0, &wdr.ctl_ioh))
		goto outunmap;

	result = wdcprobe(&ch);
	if (result) {
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = WDC_ISA_REG_NPORTS;

		ia->ia_nirq = 1;

		ia->ia_niomem = 0;
	}

	bus_space_unmap(wdr.ctl_iot, wdr.ctl_ioh, WDC_ISA_AUXREG_NPORTS);
outunmap:
	bus_space_unmap(wdr.cmd_iot, wdr.cmd_baseioh, WDC_ISA_REG_NPORTS);
out:
	return (result);
}

static int
wdc_isa_detach(device_t self, int flags)
{
	struct wdc_isa_softc *sc = device_private(self);
	struct wdc_regs *wdr = &sc->wdc_regs;
	int rc;

	if ((rc = wdcdetach(self, flags)) != 0)
		return rc;

	isa_intr_disestablish(sc->sc_ic, sc->sc_ih);

	bus_space_unmap(wdr->ctl_iot, wdr->ctl_ioh, WDC_ISA_AUXREG_NPORTS);
	bus_space_unmap(wdr->cmd_iot, wdr->cmd_baseioh, WDC_ISA_REG_NPORTS);

	return 0;
}

static void
wdc_isa_attach(device_t parent, device_t self, void *aux)
{
	struct wdc_isa_softc *sc = device_private(self);
	struct wdc_regs *wdr;
	struct isa_attach_args *ia = aux;
	int wdc_cf_flags = device_cfdata(self)->cf_flags;
	int i;

	sc->sc_wdcdev.sc_atac.atac_dev = self;
	sc->sc_wdcdev.regs = wdr = &sc->wdc_regs;
	wdr->cmd_iot = ia->ia_iot;
	wdr->ctl_iot = ia->ia_iot;
	sc->sc_ic = ia->ia_ic;
	if (bus_space_map(wdr->cmd_iot, ia->ia_io[0].ir_addr,
	    WDC_ISA_REG_NPORTS, 0, &wdr->cmd_baseioh) ||
	    bus_space_map(wdr->ctl_iot,
	      ia->ia_io[0].ir_addr + WDC_ISA_AUXREG_OFFSET,
	      WDC_ISA_AUXREG_NPORTS, 0, &wdr->ctl_ioh)) {
		aprint_error(": couldn't map registers\n");
		return;
	}

	for (i = 0; i < WDC_ISA_REG_NPORTS; i++) {
		if (bus_space_subregion(wdr->cmd_iot,
		      wdr->cmd_baseioh, i, i == 0 ? 4 : 1,
		      &wdr->cmd_iohs[i]) != 0) {
			aprint_error(": couldn't subregion registers\n");
			return;
		}
	}

	wdr->data32iot = wdr->cmd_iot;
	wdr->data32ioh = wdr->cmd_iohs[0];

#if 0
	if (ia->ia_ndrq > 0 && ia->ia_drq[0].ir_drq != ISA_UNKNOWN_DRQ) {
		sc->sc_drq = ia->ia_drq[0].ir_drq;

		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA;
		sc->sc_wdcdev.dma_arg = sc;
		sc->sc_wdcdev.dma_init = wdc_isa_dma_init;
		sc->sc_wdcdev.dma_start = wdc_isa_dma_start;
		sc->sc_wdcdev.dma_finish = wdc_isa_dma_finish;
		wdc_isa_dma_setup(sc);
	}
#endif
	sc->sc_wdcdev.cap |= WDC_CAPABILITY_PREATA;
	sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DATA16;
	if (wdc_cf_flags & WDC_OPTIONS_32)
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DATA32;
	if (wdc_cf_flags & WDC_OPTIONS_ATA_NOSTREAM)
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_ATA_NOSTREAM;
	if (wdc_cf_flags & WDC_OPTIONS_ATAPI_NOSTREAM)
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_ATAPI_NOSTREAM;

	sc->sc_wdcdev.sc_atac.atac_pio_cap = 0;
	sc->wdc_chanlist[0] = &sc->ata_channel;
	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanlist;
	sc->sc_wdcdev.sc_atac.atac_nchannels = 1;
	sc->sc_wdcdev.wdc_maxdrives = 2;
	sc->ata_channel.ch_channel = 0;
	sc->ata_channel.ch_atac = &sc->sc_wdcdev.sc_atac;
	sc->ata_channel.ch_queue = &sc->wdc_chqueue;
	wdc_init_shadow_regs(&sc->ata_channel);

	aprint_normal("\n");

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_BIO, wdcintr, &sc->ata_channel);

	wdcattach(&sc->ata_channel);
}

#if 0
static void
wdc_isa_dma_setup(struct wdc_isa_softc *sc)
{
	bus_size_t maxsize;

	if ((maxsize = isa_dmamaxsize(sc->sc_ic, sc->sc_drq)) < MAXPHYS) {
		aprint_error_dev(sc_wdcdev.sc_atac.atac_dev,
		    "max DMA size %lu is less than required %d\n",
		    (u_long)maxsize, MAXPHYS);
		sc->sc_wdcdev.sc_atac.atac_cap &= ~ATAC_CAP_DMA;
		return;
	}

	if (isa_drq_alloc(sc->sc_ic, sc->sc_drq) != 0) {
		aprint_error_dev(sc_wdcdev.sc_atac.atac_dev,
		    "can't reserve drq %d\n", sc->sc_drq);
		sc->sc_wdcdev.sc_atac.atac_cap &= ~ATAC_CAP_DMA;
		return;
	}

	if (isa_dmamap_create(sc->sc_ic, sc->sc_drq,
	    MAXPHYS, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW)) {
		aprint_error_dev(sc_wdcdev.sc_atac.atac_dev,
		    "can't create map for drq %d\n", sc->sc_drq);
		sc->sc_wdcdev.sc_atac.atac_cap &= ~ATAC_CAP_DMA;
	}
}

static int
wdc_isa_dma_init(void *v, int channel, int drive, void *databuf,
    size_t datalen, int read)
{
	struct wdc_isa_softc *sc = v;

	isa_dmastart(sc->sc_ic, sc->sc_drq, databuf, datalen, NULL,
	    (read ? DMAMODE_READ : DMAMODE_WRITE) | DMAMODE_DEMAND,
	    BUS_DMA_NOWAIT);
	return 0;
}

static void
wdc_isa_dma_start(void *v, int channel, int drive)
{
	/* nothing to do */
}

static int
wdc_isa_dma_finish(void *v, int channel, int drive, int read)
{
	struct wdc_isa_softc *sc = v;

	isa_dmadone(sc->sc_ic, sc->sc_drq);
	return 0;
}
#endif
