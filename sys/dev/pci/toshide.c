/*	$NetBSD: toshide.c,v 1.11 2013/10/07 19:53:05 jakllsch Exp $	*/

/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: toshide.c,v 1.11 2013/10/07 19:53:05 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_piccolo_reg.h>

static void piccolo_chip_map(struct pciide_softc *,
    const struct pci_attach_args *);
static void piccolo_setup_channel(struct ata_channel *);

static int  piccolo_match(device_t, cfdata_t, void *);
static void piccolo_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(toshide, sizeof(struct pciide_softc),
    piccolo_match, piccolo_attach, pciide_detach, NULL);

static const struct pciide_product_desc pciide_toshiba2_products[] = {
	{
		PCI_PRODUCT_TOSHIBA2_PICCOLO,
		0,
		"Toshiba Piccolo IDE controller",
		piccolo_chip_map,
	},
	{
		PCI_PRODUCT_TOSHIBA2_PICCOLO2,
		0,
		"Toshiba Piccolo 2 IDE controller",
		piccolo_chip_map,
	},
	{
		PCI_PRODUCT_TOSHIBA2_PICCOLO3,
		0,
		"Toshiba Piccolo 3 IDE controller",
		piccolo_chip_map,
	},
	{
		PCI_PRODUCT_TOSHIBA2_PICCOLO5,
		0,
		"Toshiba Piccolo 5 IDE controller",
		piccolo_chip_map,
	},
	{
		0,
		0,
		NULL,
		NULL,
	}
};

static int
piccolo_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_TOSHIBA2) {
		if (pciide_lookup_product(pa->pa_id, pciide_toshiba2_products))
			return 2;
	}
	return 0;
}

static void
piccolo_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);
	const struct pciide_product_desc *pp;

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_TOSHIBA2)
		pp = pciide_lookup_product(pa->pa_id, pciide_toshiba2_products);
	else
		pp = NULL;
	if (pp == NULL)
		panic("toshide_attach");
	pciide_common_attach(sc, pa, pp);
}

static void
piccolo_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	pcireg_t interface;
	int channel;

	if (pciide_chipen(sc, pa) == 0)
		return;

	aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "bus-master DMA support present");

	pciide_mapreg_dma(sc, pa);
	aprint_verbose("\n");

	sc->sc_wdcdev.sc_atac.atac_cap = ATAC_CAP_DATA32 | ATAC_CAP_DATA16;
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 5;

	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA | ATAC_CAP_UDMA;
		sc->sc_wdcdev.irqack = pciide_irqack;
		sc->sc_wdcdev.sc_atac.atac_dma_cap = 3;
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 2;
	}

	sc->sc_wdcdev.sc_atac.atac_set_modes = piccolo_setup_channel;

	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = 1;
	sc->sc_wdcdev.wdc_maxdrives = 2;
	/*
	 * XXX one for now. We'll figure out how to talk to the second channel
	 * later, hopefully! Second interface config is via the
	 * "alternate PCI Configuration Space" whatever that is!
	 */

	interface = PCI_INTERFACE(pa->pa_class);

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;

		pciide_mapchan(pa, cp, interface, pciide_pci_intr);
	}
}

static void
piccolo_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	u_int32_t idedma_ctl;
	int drive, s;
	pcireg_t pxdx;
#ifdef TOSHIDE_DEBUG
	pcireg_t pxdx_prime;
#endif

	idedma_ctl = 0;

	/* Set up DMA if needed. */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {

		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;

		if (drvp->drive_flags & ATA_DRIVE_UDMA) {
			/* use Ultra/DMA */
			s = splbio();
			drvp->drive_flags &= ~ATA_DRIVE_DMA;
			splx(s);

			/*
			 * Use UDMA - we can go up to mode 2 so no need to
			 * check anything since nearly all drives with UDMA
			 * are mode 2 or faster
			 */
			pxdx = pci_conf_read(sc->sc_pc, sc->sc_tag,
			    PICCOLO_DMA_TIMING);
 		        pxdx &= PICCOLO_UDMA_MASK;
 		        pxdx |= piccolo_udma_times[2];
 		        pci_conf_write(sc->sc_pc, sc->sc_tag,
			    PICCOLO_DMA_TIMING, pxdx);
#ifdef TOSHIDE_DEBUG
			/* XXX sanity check */
 			pxdx_prime = pci_conf_read(sc->sc_pc, sc->sc_tag,
			    PICCOLO_DMA_TIMING);
			aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "UDMA want %x, set %x, got %x\n",
			    piccolo_udma_times[2], pxdx, pxdx_prime);
#endif
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);

		}
		else if (drvp->drive_flags & ATA_DRIVE_DMA) {
			/*
			 * Use Multiword DMA
			 */
			if (drvp->PIO_mode > (drvp->DMA_mode + 2))
				drvp->PIO_mode = drvp->DMA_mode + 2;
			if (drvp->DMA_mode + 2 > (drvp->PIO_mode))
				drvp->DMA_mode = (drvp->PIO_mode > 2) ?
				    drvp->PIO_mode - 2 : 0;

			pxdx = pci_conf_read(sc->sc_pc, sc->sc_tag,
			    PICCOLO_DMA_TIMING);
 		        pxdx &= PICCOLO_DMA_MASK;
 			pxdx |= piccolo_mw_dma_times[drvp->DMA_mode];
 		        pci_conf_write(sc->sc_pc, sc->sc_tag,
			    PICCOLO_DMA_TIMING, pxdx);
#ifdef TOSHIDE_DEBUG
			/* XXX sanity check */
 			pxdx_prime = pci_conf_read(sc->sc_pc, sc->sc_tag,
			    PICCOLO_DMA_TIMING);
			aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "DMA %d want %x, set %x, got %x\n",
			    drvp->DMA_mode,
			    piccolo_mw_dma_times[drvp->DMA_mode], pxdx,
			    pxdx_prime);
#endif
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);

		}
		else {
			pxdx = pci_conf_read(sc->sc_pc, sc->sc_tag,
			    PICCOLO_PIO_TIMING);
 		        pxdx &= PICCOLO_PIO_MASK;
			pxdx |= piccolo_pio_times[drvp->PIO_mode];
 		        pci_conf_write(sc->sc_pc, sc->sc_tag,
			    PICCOLO_PIO_TIMING, pxdx);
#ifdef TOSHIDE_DEBUG
			/* XXX sanity check */
 			pxdx_prime = pci_conf_read(sc->sc_pc, sc->sc_tag,
			    PICCOLO_PIO_TIMING);
			aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "PIO %d want %x, set %x, got %x\n", drvp->PIO_mode,
			    piccolo_pio_times[drvp->PIO_mode], pxdx,
			    pxdx_prime);
#endif
		}

	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
		    idedma_ctl);
	}
}
