/*	$NetBSD: slide.c,v 1.29 2013/10/07 19:51:55 jakllsch Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
__KERNEL_RCSID(0, "$NetBSD: slide.c,v 1.29 2013/10/07 19:51:55 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_sl82c105_reg.h>

static void sl82c105_chip_map(struct pciide_softc*,
    const struct pci_attach_args*);
static void sl82c105_setup_channel(struct ata_channel*);

static int  slide_match(device_t, cfdata_t, void *);
static void slide_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(slide, sizeof(struct pciide_softc),
    slide_match, slide_attach, pciide_detach, NULL);

static const struct pciide_product_desc pciide_symphony_products[] = {
	{ PCI_PRODUCT_SYMPHONY_82C105,
	  0,
	  "Symphony Labs 82C105 IDE controller",
	  sl82c105_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL,
	}
};

static const struct pciide_product_desc pciide_winbond_products[] =  {
	{ PCI_PRODUCT_WINBOND_W83C553F_1,
	  0,
	  "Winbond W83C553F IDE controller",
	  sl82c105_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL,
	}
};

static int
slide_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SYMPHONY) {
		if (pciide_lookup_product(pa->pa_id, pciide_symphony_products))
			return (2);
	}
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_WINBOND) {
		if (pciide_lookup_product(pa->pa_id, pciide_winbond_products))
			return (2);
	}
	return (0);
}

static void
slide_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);
	const struct pciide_product_desc *pp = NULL;

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SYMPHONY)
		pp = pciide_lookup_product(pa->pa_id, pciide_symphony_products);
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_WINBOND)
		pp = pciide_lookup_product(pa->pa_id, pciide_winbond_products);
	if (pp == NULL)
		panic("slide_attach");
	pciide_common_attach(sc, pa, pp);
}

static int
sl82c105_bugchk(const struct pci_attach_args *pa)
{

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_WINBOND ||
	    PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_WINBOND_W83C553F_0)
		return (0);

	if (PCI_REVISION(pa->pa_class) <= 0x05)
		return (1);

	return (0);
}

static void
sl82c105_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	pcireg_t interface, idecr;
	int channel;

	if (pciide_chipen(sc, pa) == 0)
		return;

	aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "bus-master DMA support present");

	/*
	 * Check to see if we're part of the Winbond 83c553 Southbridge.
	 * If so, we need to disable DMA on rev. <= 5 of the southbridge.
	 */
	if (pci_find_device(NULL, sl82c105_bugchk)) {
		aprint_verbose(" but disabled due to 83c553 rev. <= 0x05");
		sc->sc_dma_ok = 0;
	} else
		pciide_mapreg_dma(sc, pa);
	aprint_verbose("\n");

	sc->sc_wdcdev.sc_atac.atac_cap = ATAC_CAP_DATA32 | ATAC_CAP_DATA16;
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA;
		sc->sc_wdcdev.irqack = pciide_irqack;
		sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;
	}
	sc->sc_wdcdev.sc_atac.atac_set_modes = sl82c105_setup_channel;

	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.wdc_maxdrives = 2;

	idecr = pci_conf_read(sc->sc_pc, sc->sc_tag, SYMPH_IDECSR);

	interface = PCI_INTERFACE(pa->pa_class);

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		if ((channel == 0 && (idecr & IDECR_P0EN) == 0) ||
		    (channel == 1 && (idecr & IDECR_P1EN) == 0)) {
			aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "%s channel ignored (disabled)\n", cp->name);
			cp->ata_channel.ch_flags |= ATACH_DISABLED;
			continue;
		}
		pciide_mapchan(pa, cp, interface, pciide_pci_intr);
	}
}

static void
sl82c105_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	int pxdx_reg, drive, s;
	pcireg_t pxdx;

	/* Set up DMA if needed. */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		pxdx_reg = ((chp->ch_channel == 0) ? SYMPH_P0D0CR
						   : SYMPH_P1D0CR) +
			    (drive * 4);

		pxdx = pci_conf_read(sc->sc_pc, sc->sc_tag, pxdx_reg);

		pxdx &= ~(PxDx_CMD_ON_MASK|PxDx_CMD_OFF_MASK);
		pxdx &= ~(PxDx_PWEN|PxDx_RDYEN|PxDx_RAEN);

		drvp = &chp->ch_drive[drive];
		/* If no drive, skip. */
		if (drvp->drive_type == ATA_DRIVET_NONE) {
			pci_conf_write(sc->sc_pc, sc->sc_tag, pxdx_reg, pxdx);
			continue;
		}

		if (drvp->drive_flags & ATA_DRIVE_DMA) {
			/*
			 * Timings will be used for both PIO and DMA,
			 * so adjust DMA mode if needed.
			 */
			if (drvp->PIO_mode >= 3) {
				if ((drvp->DMA_mode + 2) > drvp->PIO_mode)
					drvp->DMA_mode = drvp->PIO_mode - 2;
				if (drvp->DMA_mode < 1) {
					/*
					 * Can't mix both PIO and DMA.
					 * Disable DMA.
					 */
					s = splbio();
					drvp->drive_flags &= ~ATA_DRIVE_DMA;
					splx(s);
				}
			} else {
				/*
				 * Can't mix both PIO and DMA.  Disable
				 * DMA.
				 */
				s = splbio();
				drvp->drive_flags &= ~ATA_DRIVE_DMA;
				splx(s);
			}
		}

		if (drvp->drive_flags & ATA_DRIVE_DMA) {
			/* Use multi-word DMA. */
			pxdx |= symph_mw_dma_times[drvp->DMA_mode].cmd_on <<
			    PxDx_CMD_ON_SHIFT;
			pxdx |= symph_mw_dma_times[drvp->DMA_mode].cmd_off;
		} else {
			pxdx |= symph_pio_times[drvp->PIO_mode].cmd_on <<
			    PxDx_CMD_ON_SHIFT;
			pxdx |= symph_pio_times[drvp->PIO_mode].cmd_off;
		}

		/* XXX PxDx_PWEN? PxDx_RDYEN? PxDx_RAEN? */

		/* ...and set the mode for this drive. */
		pci_conf_write(sc->sc_pc, sc->sc_tag, pxdx_reg, pxdx);
	}
}
