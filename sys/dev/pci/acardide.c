/*	$NetBSD: acardide.c,v 1.31 2013/10/07 19:51:55 jakllsch Exp $	*/

/*-
 * Copyright (c) 2001 Izumi Tsutsui.  All rights reserved.
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
__KERNEL_RCSID(0, "$NetBSD: acardide.c,v 1.31 2013/10/07 19:51:55 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_acard_reg.h>

static void acard_chip_map(struct pciide_softc*, const struct pci_attach_args*);
static void acard_setup_channel(struct ata_channel*);
#if 0 /* XXX !! */
static int  acard_pci_intr(void *);
#endif

static int  acardide_match(device_t, cfdata_t, void *);
static void acardide_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(acardide, sizeof(struct pciide_softc),
    acardide_match, acardide_attach, pciide_detach, NULL);

static const struct pciide_product_desc pciide_acard_products[] =  {
	{ PCI_PRODUCT_ACARD_ATP850U,
	  0,
	  "Acard ATP850U Ultra33 IDE Controller",
	  acard_chip_map,
	},
	{ PCI_PRODUCT_ACARD_ATP860,
	  0,
	  "Acard ATP860 Ultra66 IDE Controller",
	  acard_chip_map,
	},
	{ PCI_PRODUCT_ACARD_ATP860A,
	  0,
	  "Acard ATP860-A Ultra66 IDE Controller",
	  acard_chip_map,
	},
	{ PCI_PRODUCT_ACARD_ATP865,
	  0,
	  "Acard ATP865 Ultra133 IDE Controller",
	  acard_chip_map,
	},
	{ PCI_PRODUCT_ACARD_ATP865A,
	  0,
	  "Acard ATP865-A Ultra133 IDE Controller",
	  acard_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

static int
acardide_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ACARD) {
		if (pciide_lookup_product(pa->pa_id, pciide_acard_products))
			return (2);
	}
	return (0);
}

static void
acardide_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_acard_products));

}

#define	ACARD_IS_850(sc)						\
	((sc)->sc_pp->ide_product == PCI_PRODUCT_ACARD_ATP850U)

static void
acard_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	int i;
	pcireg_t interface;

	if (pciide_chipen(sc, pa) == 0)
		return;

	/*
	 * when the chip is in native mode it identifies itself as a
	 * 'misc mass storage'. Fake interface in this case.
	 */
	if (PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_IDE) {
		interface = PCI_INTERFACE(pa->pa_class);
	} else {
		interface = PCIIDE_INTERFACE_BUS_MASTER_DMA |
		    PCIIDE_INTERFACE_PCI(0) | PCIIDE_INTERFACE_PCI(1);
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
	switch (sc->sc_pp->ide_product) {
	case PCI_PRODUCT_ACARD_ATP860:
	case PCI_PRODUCT_ACARD_ATP860A:
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 4;
		break;
	case PCI_PRODUCT_ACARD_ATP865:
	case PCI_PRODUCT_ACARD_ATP865A:
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 6;
		break;
	default:
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 2;
		break;
	}

	sc->sc_wdcdev.sc_atac.atac_set_modes = acard_setup_channel;
	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = 2;
	sc->sc_wdcdev.wdc_maxdrives = 2;

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (i = 0; i < sc->sc_wdcdev.sc_atac.atac_nchannels; i++) {
		cp = &sc->pciide_channels[i];
		if (pciide_chansetup(sc, i, interface) == 0)
			continue;
		pciide_mapchan(pa, cp, interface, pciide_pci_intr);
	}
	if (!ACARD_IS_850(sc)) {
		u_int32_t reg;
		reg = pci_conf_read(sc->sc_pc, sc->sc_tag, ATP8x0_CTRL);
		reg &= ~ATP860_CTRL_INT;
		pci_conf_write(sc->sc_pc, sc->sc_tag, ATP8x0_CTRL, reg);
	}
}

static void
acard_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	struct atac_softc *atac = chp->ch_atac;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	int channel = chp->ch_channel;
	int drive, s;
	u_int32_t idetime, udma_mode;
	u_int32_t idedma_ctl;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	if (ACARD_IS_850(sc)) {
		idetime = 0;
		udma_mode = pci_conf_read(sc->sc_pc, sc->sc_tag, ATP850_UDMA);
		udma_mode &= ~ATP850_UDMA_MASK(channel);
	} else {
		idetime = pci_conf_read(sc->sc_pc, sc->sc_tag, ATP860_IDETIME);
		idetime &= ~ATP860_SETTIME_MASK(channel);
		udma_mode = pci_conf_read(sc->sc_pc, sc->sc_tag, ATP860_UDMA);
		udma_mode &= ~ATP860_UDMA_MASK(channel);

		/* check 80 pins cable */
		if ((chp->ch_drive[0].drive_flags & ATA_DRIVE_UDMA) ||
		    (chp->ch_drive[1].drive_flags & ATA_DRIVE_UDMA)) {
			if (pci_conf_read(sc->sc_pc, sc->sc_tag, ATP8x0_CTRL)
			    & ATP860_CTRL_80P(chp->ch_channel)) {
				if (chp->ch_drive[0].UDMA_mode > 2)
					chp->ch_drive[0].UDMA_mode = 2;
				if (chp->ch_drive[1].UDMA_mode > 2)
					chp->ch_drive[1].UDMA_mode = 2;
			}
		}
	}

	idedma_ctl = 0;

	/* Per drive settings */
	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;
		/* add timing values, setup DMA if needed */
		if ((atac->atac_cap & ATAC_CAP_UDMA) &&
		    (drvp->drive_flags & ATA_DRIVE_UDMA)) {
			/* use Ultra/DMA */
			if (ACARD_IS_850(sc)) {
				idetime |= ATP850_SETTIME(drive,
				    acard_act_udma[drvp->UDMA_mode],
				    acard_rec_udma[drvp->UDMA_mode]);
				udma_mode |= ATP850_UDMA_MODE(channel, drive,
				    acard_udma_conf[drvp->UDMA_mode]);
			} else {
				idetime |= ATP860_SETTIME(channel, drive,
				    acard_act_udma[drvp->UDMA_mode],
				    acard_rec_udma[drvp->UDMA_mode]);
				udma_mode |= ATP860_UDMA_MODE(channel, drive,
				    acard_udma_conf[drvp->UDMA_mode]);
			}
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else if ((atac->atac_cap & ATAC_CAP_DMA) &&
		    (drvp->drive_flags & ATA_DRIVE_DMA)) {
			/* use Multiword DMA */
			s = splbio();
			drvp->drive_flags &= ~ATA_DRIVE_UDMA;
			splx(s);
			if (ACARD_IS_850(sc)) {
				idetime |= ATP850_SETTIME(drive,
				    acard_act_dma[drvp->DMA_mode],
				    acard_rec_dma[drvp->DMA_mode]);
			} else {
				idetime |= ATP860_SETTIME(channel, drive,
				    acard_act_dma[drvp->DMA_mode],
				    acard_rec_dma[drvp->DMA_mode]);
			}
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else {
			/* PIO only */
			s = splbio();
			drvp->drive_flags &= ~(ATA_DRIVE_UDMA | ATA_DRIVE_DMA);
			splx(s);
			if (ACARD_IS_850(sc)) {
				idetime |= ATP850_SETTIME(drive,
				    acard_act_pio[drvp->PIO_mode],
				    acard_rec_pio[drvp->PIO_mode]);
			} else {
				idetime |= ATP860_SETTIME(channel, drive,
				    acard_act_pio[drvp->PIO_mode],
				    acard_rec_pio[drvp->PIO_mode]);
			}
		pci_conf_write(sc->sc_pc, sc->sc_tag, ATP8x0_CTRL,
		    pci_conf_read(sc->sc_pc, sc->sc_tag, ATP8x0_CTRL)
		    | ATP8x0_CTRL_EN(channel));
		}
	}

	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
		    idedma_ctl);
	}

	if (ACARD_IS_850(sc)) {
		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    ATP850_IDETIME(channel), idetime);
		pci_conf_write(sc->sc_pc, sc->sc_tag, ATP850_UDMA, udma_mode);
	} else {
		pci_conf_write(sc->sc_pc, sc->sc_tag, ATP860_IDETIME, idetime);
		pci_conf_write(sc->sc_pc, sc->sc_tag, ATP860_UDMA, udma_mode);
	}
}

#if 0 /* XXX !! */
static int
acard_pci_intr(void *arg)
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
		if ((dmastat & IDEDMA_CTL_INTR) == 0)
			continue;
		wdc_cp = &cp->ata_channel;
		if ((wdc_cp->ch_flags & ATACH_IRQ_WAIT) == 0) {
			(void)wdcintr(wdc_cp);
			bus_space_write_1(sc->sc_dma_iot,
			    cp->dma_iohs[IDEDMA_CTL], 0, dmastat);
			continue;
		}
		crv = wdcintr(wdc_cp);
		if (crv == 0) {
			printf("%s:%d: bogus intr\n",
			    device_xname(sc->sc_wdcdev.sc_atac.atac_dev), i);
			bus_space_write_1(sc->sc_dma_iot,
			    cp->dma_iohs[IDEDMA_CTL], 0, dmastat);
		} else if (crv == 1)
			rv = 1;
		else if (rv == 0)
			rv = crv;
	}
	return rv;
}
#endif
