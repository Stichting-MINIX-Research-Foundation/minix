/*	$NetBSD: hptide.c,v 1.34 2013/10/07 19:51:55 jakllsch Exp $	*/

/*
 * Copyright (c) 1999, 2000, 2001 Manuel Bouyer.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hptide.c,v 1.34 2013/10/07 19:51:55 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_hpt_reg.h>

static void hpt_chip_map(struct pciide_softc*, const struct pci_attach_args*);
static void hpt_setup_channel(struct ata_channel*);
static int  hpt_pci_intr(void *);

static int  hptide_match(device_t, cfdata_t, void *);
static void hptide_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(hptide, sizeof(struct pciide_softc),
    hptide_match, hptide_attach, pciide_detach, NULL);

static const struct pciide_product_desc pciide_triones_products[] =  {
	{ PCI_PRODUCT_TRIONES_HPT302,
	  0,
	  NULL,
	  hpt_chip_map
	},
	{ PCI_PRODUCT_TRIONES_HPT366,
	  0,
	  NULL,
	  hpt_chip_map,
	},
	{ PCI_PRODUCT_TRIONES_HPT371,
	  0,
	  NULL,
	  hpt_chip_map,
	},
	{ PCI_PRODUCT_TRIONES_HPT372A,
	  0,
	  NULL,
	  hpt_chip_map
	},
	{ PCI_PRODUCT_TRIONES_HPT374,
	  0,
	  NULL,
	  hpt_chip_map
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

static int
hptide_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_TRIONES) {
		if (pciide_lookup_product(pa->pa_id, pciide_triones_products))
			return (2);
	}
	return (0);
}

static void
hptide_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_triones_products));

}

static void
hpt_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	int i, compatchan, revision;
	pcireg_t interface;

	if (pciide_chipen(sc, pa) == 0)
		return;

	revision = PCI_REVISION(pa->pa_class);
	aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "Triones/Highpoint ");
	switch (sc->sc_pp->ide_product) {
	case PCI_PRODUCT_TRIONES_HPT302:
		aprint_normal("HPT302 IDE Controller\n");
		break;
	case PCI_PRODUCT_TRIONES_HPT371:
		aprint_normal("HPT371 IDE Controller\n");
		break;
	case PCI_PRODUCT_TRIONES_HPT374:
		aprint_normal("HPT374 IDE Controller\n");
		break;
	case PCI_PRODUCT_TRIONES_HPT372A:
		aprint_normal("HPT372A IDE Controller\n");
		break;
	case PCI_PRODUCT_TRIONES_HPT366:
		if (revision == HPT372_REV)
			aprint_normal("HPT372 IDE Controller\n");
		else if (revision == HPT370_REV)
			aprint_normal("HPT370 IDE Controller\n");
		else if (revision == HPT370A_REV)
			aprint_normal("HPT370A IDE Controller\n");
		else if (revision == HPT368_REV)
			aprint_normal("HPT368 IDE Controller\n");
		else if (revision == HPT366_REV)
			aprint_normal("HPT366 IDE Controller\n");
		else
			aprint_normal("unknown HPT IDE controller rev %d\n",
			    revision);
		break;
	default:
		aprint_normal("unknown HPT IDE controller 0x%x\n",
		    sc->sc_pp->ide_product);
	}

	/*
	 * when the chip is in native mode it identifies itself as a
	 * 'misc mass storage'. Fake interface in this case.
	 */
	if (PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_IDE) {
		interface = PCI_INTERFACE(pa->pa_class);
	} else {
		interface = PCIIDE_INTERFACE_BUS_MASTER_DMA |
		    PCIIDE_INTERFACE_PCI(0);
		if ((sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT366 &&
		    (revision == HPT368_REV || revision == HPT370_REV || revision == HPT370A_REV ||
		     revision == HPT372_REV)) ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT302 ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT371 ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT372A ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT374)
			interface |= PCIIDE_INTERFACE_PCI(1);
	}

	aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "bus-master DMA support present");
	pciide_mapreg_dma(sc, pa);
	aprint_verbose("\n");
	sc->sc_wdcdev.sc_atac.atac_cap = ATAC_CAP_DATA16 | ATAC_CAP_DATA32;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA | ATAC_CAP_UDMA;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;

	sc->sc_wdcdev.sc_atac.atac_set_modes = hpt_setup_channel;
	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	if (sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT366 &&
	    (revision == HPT366_REV || revision == HPT368_REV)) {
		sc->sc_wdcdev.sc_atac.atac_nchannels = 1;
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 4;
	} else {
		sc->sc_wdcdev.sc_atac.atac_nchannels = 2;
		if (sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT374 ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT372A ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT371 ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT302 ||
		    (sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT366 &&
		    revision == HPT372_REV))
			sc->sc_wdcdev.sc_atac.atac_udma_cap = 6;
		else
			sc->sc_wdcdev.sc_atac.atac_udma_cap = 5;
	}
	sc->sc_wdcdev.wdc_maxdrives = 2;

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (i = 0; i < sc->sc_wdcdev.sc_atac.atac_nchannels; i++) {
		cp = &sc->pciide_channels[i];
		if (sc->sc_wdcdev.sc_atac.atac_nchannels > 1) {
			compatchan = i;
			if((pciide_pci_read(sc->sc_pc, sc->sc_tag,
			   HPT370_CTRL1(i)) & HPT370_CTRL1_EN) == 0) {
				aprint_normal(
				    "%s: %s channel ignored (disabled)\n",
				    device_xname(
				      sc->sc_wdcdev.sc_atac.atac_dev),
				    cp->name);
				cp->ata_channel.ch_flags |= ATACH_DISABLED;
				continue;
			}
		} else {
			/*
			 * The 366 has 2 PCI IDE functions, one for primary and
			 * one for secondary. So we need to call
			 * pciide_mapregs_compat() with the real channel.
			 */
			if (pa->pa_function == 0)
				compatchan = 0;
			else if (pa->pa_function == 1)
				compatchan = 1;
			else {
				aprint_error_dev(
				    sc->sc_wdcdev.sc_atac.atac_dev,
				    "unexpected PCI function %d\n",
				    pa->pa_function);
				return;
			}
		}
		if (pciide_chansetup(sc, i, interface) == 0)
			continue;
		if (interface & PCIIDE_INTERFACE_PCI(i)) {
			pciide_mapregs_native(pa, cp, hpt_pci_intr);
		} else {
			pciide_mapregs_compat(pa, cp, compatchan);
			if ((cp->ata_channel.ch_flags & ATACH_DISABLED) == 0)
				pciide_map_compat_intr(pa, cp,
				    sc->sc_cy_compatchan);
		}
		wdcattach(&cp->ata_channel);
	}
	if ((sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT366 &&
	    (revision == HPT368_REV || revision == HPT370_REV || revision == HPT370A_REV ||
	     revision == HPT372_REV)) ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT302 ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT371 ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT372A ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT374) {
		/*
		 * HPT370_REV and highter has a bit to disable interrupts,
		 * make sure to clear it
		 */
		pciide_pci_write(sc->sc_pc, sc->sc_tag, HPT_CSEL,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, HPT_CSEL) &
		    ~HPT_CSEL_IRQDIS);
	}
	/* set clocks, etc (mandatory on 372/4, optional otherwise) */
	if ((sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT366 &&
	     revision == HPT372_REV ) ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT302 ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT371 ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT372A ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_TRIONES_HPT374)
		pciide_pci_write(sc->sc_pc, sc->sc_tag, HPT_SC2,
		    (pciide_pci_read(sc->sc_pc, sc->sc_tag, HPT_SC2) &
		     HPT_SC2_MAEN) | HPT_SC2_OSC_EN);
	return;
}

static void
hpt_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	int drive, s;
	int cable;
	u_int32_t before, after;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	int revision =
	     PCI_REVISION(pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_CLASS_REG));
	const u_int32_t *tim_pio, *tim_dma, *tim_udma;

	cable = pciide_pci_read(sc->sc_pc, sc->sc_tag, HPT_CSEL);

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	idedma_ctl = 0;

	/* select the timing arrays for the chip */
	switch (sc->sc_pp->ide_product) {
	case PCI_PRODUCT_TRIONES_HPT374:
		tim_udma = hpt374_udma;
		tim_dma = hpt374_dma;
		tim_pio = hpt374_pio;
		break;
	case PCI_PRODUCT_TRIONES_HPT302:
	case PCI_PRODUCT_TRIONES_HPT371:
	case PCI_PRODUCT_TRIONES_HPT372A:
		tim_udma = hpt372_udma;
		tim_dma = hpt372_dma;
		tim_pio = hpt372_pio;
		break;
	case PCI_PRODUCT_TRIONES_HPT366:
	default:
		switch (revision) {
		case HPT372_REV:
			tim_udma = hpt372_udma;
			tim_dma = hpt372_dma;
			tim_pio = hpt372_pio;
			break;
		case HPT370_REV:
		case HPT370A_REV:
			tim_udma = hpt370_udma;
			tim_dma = hpt370_dma;
			tim_pio = hpt370_pio;
			break;
		case HPT368_REV:
		case HPT366_REV:
		default:
			tim_udma = hpt366_udma;
			tim_dma = hpt366_dma;
			tim_pio = hpt366_pio;
			break;
		}
	}

	/* Per drive settings */
	for (drive = 0; drive < chp->ch_ndrives; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;
		before = pci_conf_read(sc->sc_pc, sc->sc_tag,
					HPT_IDETIM(chp->ch_channel, drive));

		/* add timing values, setup DMA if needed */
		if (drvp->drive_flags & ATA_DRIVE_UDMA) {
			/* use Ultra/DMA */
			s = splbio();
			drvp->drive_flags &= ~ATA_DRIVE_DMA;
			splx(s);
			if ((cable & HPT_CSEL_CBLID(chp->ch_channel)) != 0 &&
			    drvp->UDMA_mode > 2)
				drvp->UDMA_mode = 2;
			after = tim_udma[drvp->UDMA_mode];
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else if (drvp->drive_flags & ATA_DRIVE_DMA) {
			/*
			 * use Multiword DMA.
			 * Timings will be used for both PIO and DMA, so adjust
			 * DMA mode if needed
			 */
			if (drvp->PIO_mode >= 3 &&
			    (drvp->DMA_mode + 2) > drvp->PIO_mode) {
				drvp->DMA_mode = drvp->PIO_mode - 2;
			}
			after = tim_dma[drvp->DMA_mode];
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else {
			/* PIO only */
			after = tim_pio[drvp->PIO_mode];
		}
		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    HPT_IDETIM(chp->ch_channel, drive), after);
		ATADEBUG_PRINT(("%s: bus speed register set to 0x%08x "
		    "(BIOS 0x%08x)\n", device_xname(drvp->drv_softc),
		    after, before), DEBUG_PROBE);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
		    idedma_ctl);
	}
}

static int
hpt_pci_intr(void *arg)
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct ata_channel *wdc_cp;
	int rv = 0;
	int dmastat, i, crv;

	for (i = 0; i < sc->sc_wdcdev.sc_atac.atac_nchannels; i++) {
		cp = &sc->pciide_channels[i];
		dmastat = bus_space_read_1(sc->sc_dma_iot,
		    cp->dma_iohs[IDEDMA_CTL], 0);
		if((dmastat & ( IDEDMA_CTL_ACT | IDEDMA_CTL_INTR)) !=
		    IDEDMA_CTL_INTR)
			continue;
		wdc_cp = &cp->ata_channel;
		crv = wdcintr(wdc_cp);
		if (crv == 0) {
			aprint_error("%s:%d: bogus intr\n",
			    device_xname(sc->sc_wdcdev.sc_atac.atac_dev), i);
			bus_space_write_1(sc->sc_dma_iot,
			    cp->dma_iohs[IDEDMA_CTL], 0, dmastat);
		} else
			rv = 1;
	}
	return rv;
}
