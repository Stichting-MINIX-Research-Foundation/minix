/*	$NetBSD: artsata.c,v 1.26 2014/03/29 19:28:24 christos Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of Wasabi Systems, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: artsata.c,v 1.26 2014/03/29 19:28:24 christos Exp $");

#include "opt_pciide.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_i31244_reg.h>

#include <dev/ata/satareg.h>
#include <dev/ata/satavar.h>
#include <dev/ata/atareg.h>
#include <dev/ata/atavar.h>

static void artisea_chip_map(struct pciide_softc*,
    const struct pci_attach_args *);

static int  artsata_match(device_t, cfdata_t, void *);
static void artsata_attach(device_t, device_t, void *);

static const struct pciide_product_desc pciide_artsata_products[] =  {
	{ PCI_PRODUCT_INTEL_31244,
	  0,
	  "Intel 31244 Serial ATA Controller",
	  artisea_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

struct artisea_cmd_map
{
	u_int8_t offset;
	u_int8_t size;
};

static const struct artisea_cmd_map artisea_dpa_cmd_map[] =
{
	{ARTISEA_SUPDDR, 4},	/* 0 Data */
	{ARTISEA_SUPDER, 1},	/* 1 Error */
	{ARTISEA_SUPDCSR, 2},	/* 2 Sector Count */
	{ARTISEA_SUPDSNR, 2},	/* 3 Sector Number */
	{ARTISEA_SUPDCLR, 2},	/* 4 Cylinder Low */
	{ARTISEA_SUPDCHR, 2},	/* 5 Cylinder High */
	{ARTISEA_SUPDDHR, 1},	/* 6 Device/Head */
	{ARTISEA_SUPDCR, 1},	/* 7 Command */
	{ARTISEA_SUPDSR, 1},	/* 8 Status */
	{ARTISEA_SUPDFR, 2}	/* 9 Feature */
};

#define ARTISEA_NUM_CHAN 4

CFATTACH_DECL_NEW(artsata, sizeof(struct pciide_softc),
    artsata_match, artsata_attach, pciide_detach, NULL);

static int
artsata_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL) {
		if (pciide_lookup_product(pa->pa_id, pciide_artsata_products))
			return (2);
	}
	return (0);
}

static void
artsata_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_artsata_products));

}

static void
artisea_mapregs(const struct pci_attach_args *pa, struct pciide_channel *cp,
    int (*pci_intr)(void *))
{
	struct pciide_softc *sc = CHAN_TO_PCIIDE(&cp->ata_channel);
	struct ata_channel *wdc_cp = &cp->ata_channel;
	struct wdc_regs *wdr = CHAN_TO_WDC_REGS(wdc_cp);
	const char *intrstr;
	pci_intr_handle_t intrhandle;
	int i;
	char intrbuf[PCI_INTRSTR_LEN];

	cp->compat = 0;

	if (sc->sc_pci_ih == NULL) {
		if (pci_intr_map(pa, &intrhandle) != 0) {
			aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "couldn't map native-PCI interrupt\n");
			goto bad;
		}
		intrstr = pci_intr_string(pa->pa_pc, intrhandle,
		    intrbuf, sizeof(intrbuf));
		sc->sc_pci_ih = pci_intr_establish(pa->pa_pc,
		    intrhandle, IPL_BIO, pci_intr, sc);
		if (sc->sc_pci_ih != NULL) {
			aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "using %s for native-PCI interrupt\n", intrstr);
		} else {
			aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "couldn't establish native-PCI interrupt");
			if (intrstr != NULL)
				aprint_error(" at %s", intrstr);
			aprint_error("\n");
			goto bad;
		}
	}
	cp->ih = sc->sc_pci_ih;
	wdr->cmd_iot = sc->sc_ba5_st;
	if (bus_space_subregion (sc->sc_ba5_st, sc->sc_ba5_sh,
	    ARTISEA_DPA_PORT_BASE(wdc_cp->ch_channel), 0x200,
	    &wdr->cmd_baseioh) != 0) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "couldn't map %s channel cmd regs\n", cp->name);
		goto bad;
	}

	wdr->ctl_iot = sc->sc_ba5_st;
	if (bus_space_subregion(wdr->cmd_iot, wdr->cmd_baseioh,
	    ARTISEA_SUPDDCTLR, 1, &cp->ctl_baseioh) != 0) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "couldn't map %s channel ctl regs\n", cp->name);
		goto bad;
	}
	wdr->ctl_ioh = cp->ctl_baseioh;

	for (i = 0; i < WDC_NREG + 2; i++) {

		if (bus_space_subregion(wdr->cmd_iot, wdr->cmd_baseioh,
		    artisea_dpa_cmd_map[i].offset, artisea_dpa_cmd_map[i].size,
		    &wdr->cmd_iohs[i]) != 0) {
			aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "couldn't subregion %s channel cmd regs\n",
			    cp->name);
			goto bad;
		}
	}
	wdr->data32iot = wdr->cmd_iot;
	wdr->data32ioh = wdr->cmd_iohs[0];

	wdr->sata_iot = wdr->cmd_iot;
	wdr->sata_baseioh = wdr->cmd_baseioh;

	if (bus_space_subregion(wdr->sata_iot, wdr->sata_baseioh,
	    ARTISEA_SUPERSET_DPA_OFF + ARTISEA_SUPDSSSR, 1,
	    &wdr->sata_status) != 0) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "couldn't map channel %d sata_status regs\n",
		    wdc_cp->ch_channel);
		goto bad;
	}
	if (bus_space_subregion(wdr->sata_iot, wdr->sata_baseioh,
	    ARTISEA_SUPERSET_DPA_OFF + ARTISEA_SUPDSSER, 1,
	    &wdr->sata_error) != 0) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "couldn't map channel %d sata_error regs\n",
		    wdc_cp->ch_channel);
		goto bad;
	}
	if (bus_space_subregion(wdr->sata_iot, wdr->sata_baseioh,
	    ARTISEA_SUPERSET_DPA_OFF + ARTISEA_SUPDSSCR, 1,
	    &wdr->sata_control) != 0) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "couldn't map channel %d sata_control regs\n",
		    wdc_cp->ch_channel);
		goto bad;
	}

	wdcattach(wdc_cp);
	return;

bad:
	wdc_cp->ch_flags |= ATACH_DISABLED;
	return;
}

static int
artisea_chansetup(struct pciide_softc *sc, int channel,
    pcireg_t interface)
{
	struct pciide_channel *cp = &sc->pciide_channels[channel];
	sc->wdc_chanarray[channel] = &cp->ata_channel;
	cp->name = PCIIDE_CHANNEL_NAME(channel);
	cp->ata_channel.ch_channel = channel;
	cp->ata_channel.ch_atac = &sc->sc_wdcdev.sc_atac;
	cp->ata_channel.ch_queue =
	    malloc(sizeof(struct ata_queue), M_DEVBUF, M_NOWAIT);
	if (cp->ata_channel.ch_queue == NULL) {
		aprint_error("%s %s channel: "
		    "can't allocate memory for command queue",
		device_xname(sc->sc_wdcdev.sc_atac.atac_dev), cp->name);
		return 0;
	}
	return 1;
}

static void
artisea_mapreg_dma(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *pc;
	int chan;
	u_int32_t dma_ctl;
	u_int32_t cacheline_len;

	aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "bus-master DMA support present");

	sc->sc_dma_ok = 1;

	/*
	 * Errata #4 says that if the cacheline length is not set correctly,
	 * we can get corrupt MWI and Memory-Block-Write transactions.
	 */
	cacheline_len = PCI_CACHELINE(pci_conf_read (pa->pa_pc, pa->pa_tag,
	    PCI_BHLC_REG));
	if (cacheline_len == 0) {
		aprint_verbose(", but unused (cacheline size not set in PCI conf)\n");
		sc->sc_dma_ok = 0;
		return;
	}

	/*
	 * Final step of the work-around is to force the DMA engine to use
	 * the cache-line length information.
	 */
	dma_ctl = pci_conf_read(pa->pa_pc, pa->pa_tag, ARTISEA_PCI_SUDCSCR);
	dma_ctl |= SUDCSCR_DMA_WCAE | SUDCSCR_DMA_RCAE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, ARTISEA_PCI_SUDCSCR, dma_ctl);

	sc->sc_wdcdev.dma_arg = sc;
	sc->sc_wdcdev.dma_init = pciide_dma_init;
	sc->sc_wdcdev.dma_start = pciide_dma_start;
	sc->sc_wdcdev.dma_finish = pciide_dma_finish;
	sc->sc_dma_iot = sc->sc_ba5_st;
	sc->sc_dmat = pa->pa_dmat;

	if (device_cfdata(sc->sc_wdcdev.sc_atac.atac_dev)->cf_flags &
	    PCIIDE_OPTIONS_NODMA) {
		aprint_verbose(
		    ", but unused (forced off by config file)\n");
		sc->sc_dma_ok = 0;
		return;
	}

	/*
	 * Set up the default handles for the DMA registers.
	 * Just reserve 32 bits for each handle, unless space
	 * doesn't permit it.
	 */
	for (chan = 0; chan < ARTISEA_NUM_CHAN; chan++) {
		pc = &sc->pciide_channels[chan];
		if (bus_space_subregion(sc->sc_ba5_st, sc->sc_ba5_sh,
		    ARTISEA_DPA_PORT_BASE(chan) + ARTISEA_SUPDDCMDR, 2,
		    &pc->dma_iohs[IDEDMA_CMD]) != 0 ||
		    bus_space_subregion(sc->sc_ba5_st, sc->sc_ba5_sh,
		    ARTISEA_DPA_PORT_BASE(chan) + ARTISEA_SUPDDSR, 1,
		    &pc->dma_iohs[IDEDMA_CTL]) != 0 ||
		    bus_space_subregion(sc->sc_ba5_st, sc->sc_ba5_sh,
		    ARTISEA_DPA_PORT_BASE(chan) + ARTISEA_SUPDDDTPR, 4,
		    &pc->dma_iohs[IDEDMA_TBL]) != 0) {
			sc->sc_dma_ok = 0;
			aprint_verbose(", but can't subregion registers\n");
			return;
		}
	}

	aprint_verbose("\n");
}

static void
artisea_chip_map_dpa(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	pcireg_t interface;
	int channel;

	interface = PCI_INTERFACE(pa->pa_class);

	aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "interface wired in DPA mode\n");

	if (pci_mapreg_map(pa, ARTISEA_PCI_DPA_BASE, PCI_MAPREG_MEM_TYPE_64BIT,
	    0, &sc->sc_ba5_st, &sc->sc_ba5_sh, NULL, &sc->sc_ba5_ss) != 0)
		return;

	artisea_mapreg_dma(sc, pa);

	sc->sc_wdcdev.cap = WDC_CAPABILITY_WIDEREGS;

	sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DATA16 | ATAC_CAP_DATA32;
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA | ATAC_CAP_UDMA;
		sc->sc_wdcdev.irqack = pciide_irqack;
		sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 6;
	}
	sc->sc_wdcdev.sc_atac.atac_set_modes = sata_setup_channel;

	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = ARTISEA_NUM_CHAN;
	sc->sc_wdcdev.sc_atac.atac_probe = wdc_sataprobe;
	sc->sc_wdcdev.wdc_maxdrives = 1;

	wdc_allocate_regs(&sc->sc_wdcdev);

	/*
	 * Perform a quick check to ensure that the device isn't configured
	 * in Spread-spectrum clocking mode.  This feature is buggy and has
	 * been removed from the latest documentation.
	 *
	 * Note that although this bit is in the Channel regs, it's the same
	 * for all channels, so we check it just once here.
	 */
	if ((bus_space_read_4 (sc->sc_ba5_st, sc->sc_ba5_sh,
	    ARTISEA_DPA_PORT_BASE(0) + ARTISEA_SUPERSET_DPA_OFF +
	    ARTISEA_SUPDPFR) & SUPDPFR_SSCEN) != 0) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "Spread-specturm clocking not supported by device\n");
		return;
	}

	/* Clear the LED0-only bit.  */
	pci_conf_write (pa->pa_pc, pa->pa_tag, ARTISEA_PCI_SUECSR0,
	    pci_conf_read (pa->pa_pc, pa->pa_tag, ARTISEA_PCI_SUECSR0) &
	    ~SUECSR0_LED0_ONLY);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];
		if (artisea_chansetup(sc, channel, interface) == 0)
			continue;
		/* XXX We can probably do interrupts more efficiently.  */
		artisea_mapregs(pa, cp, pciide_pci_intr);
	}
}

static void
artisea_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	pcireg_t interface;
	int channel;

	if (pciide_chipen(sc, pa) == 0)
		return;

	interface = PCI_INTERFACE(pa->pa_class);

	if (interface == 0) {
		artisea_chip_map_dpa (sc, pa);
		return;
	}

	aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "bus-master DMA support present");
#ifdef PCIIDE_I31244_DISABLEDMA
	if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_31244 &&
	    PCI_REVISION(pa->pa_class) == 0) {
		aprint_verbose(" but disabled due to rev. 0");
		sc->sc_dma_ok = 0;
	} else
#endif
		pciide_mapreg_dma(sc, pa);
	aprint_verbose("\n");

	/*
	 * XXX Configure LEDs to show activity.
	 */

	sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DATA16 | ATAC_CAP_DATA32;
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA | ATAC_CAP_UDMA;
		sc->sc_wdcdev.irqack = pciide_irqack;
		sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 6;
	}
	sc->sc_wdcdev.sc_atac.atac_set_modes = sata_setup_channel;

	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		pciide_mapchan(pa, cp, interface, pciide_pci_intr);
	}
}
