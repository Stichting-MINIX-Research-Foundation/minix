/*	$NetBSD: rccide.c,v 1.27 2013/10/07 19:51:55 jakllsch Exp $	*/

/*
 * Copyright (c) 2003 By Noon Software, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rccide.c,v 1.27 2013/10/07 19:51:55 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>

static void serverworks_chip_map(struct pciide_softc *,
				 const struct pci_attach_args *);
static void serverworks_setup_channel(struct ata_channel *);
static int  serverworks_pci_intr(void *);
static int  serverworkscsb6_pci_intr(void *);

static int  rccide_match(device_t, cfdata_t, void *);
static void rccide_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(rccide, sizeof(struct pciide_softc),
    rccide_match, rccide_attach, pciide_detach, NULL);

static const struct pciide_product_desc pciide_serverworks_products[] =  {
	{ PCI_PRODUCT_SERVERWORKS_OSB4_IDE,
	  0,
	  "ServerWorks OSB4 IDE Controller",
	  serverworks_chip_map,
	},
	{ PCI_PRODUCT_SERVERWORKS_CSB5_IDE,
	  0,
	  "ServerWorks CSB5 IDE Controller",
	  serverworks_chip_map,
	},
	{ PCI_PRODUCT_SERVERWORKS_CSB6_IDE,
	  0,
	  "ServerWorks CSB6 RAID/IDE Controller",
	  serverworks_chip_map,
	},
	{ PCI_PRODUCT_SERVERWORKS_CSB6_RAID,
	  0,
	  "ServerWorks CSB6 RAID/IDE Controller",
	  serverworks_chip_map,
	},
	{ PCI_PRODUCT_SERVERWORKS_HT1000_IDE,
	  0,
	  "ServerWorks HT-1000 IDE Controller",
	  serverworks_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL,
	}
};

static int
rccide_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SERVERWORKS &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_MASS_STORAGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_IDE) {
		if (pciide_lookup_product(pa->pa_id,
		    pciide_serverworks_products))
			return (2);
	}
	return (0);
}

static void
rccide_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_serverworks_products));
}

static void
serverworks_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	pcitag_t pcib_tag;
	int channel;

	if (pciide_chipen(sc, pa) == 0)
		return;

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
	case PCI_PRODUCT_SERVERWORKS_OSB4_IDE:
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 2;
		break;
	case PCI_PRODUCT_SERVERWORKS_CSB5_IDE:
		if (PCI_REVISION(pa->pa_class) < 0x92)
			sc->sc_wdcdev.sc_atac.atac_udma_cap = 4;
		else
			sc->sc_wdcdev.sc_atac.atac_udma_cap = 5;
		break;
	case PCI_PRODUCT_SERVERWORKS_CSB6_IDE:
	case PCI_PRODUCT_SERVERWORKS_CSB6_RAID:
	case PCI_PRODUCT_SERVERWORKS_HT1000_IDE:
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 5;
		break;
	}

	sc->sc_wdcdev.sc_atac.atac_set_modes = serverworks_setup_channel;
	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = 2;
	sc->sc_wdcdev.wdc_maxdrives = 2;

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		switch (sc->sc_pp->ide_product) {
		case PCI_PRODUCT_SERVERWORKS_CSB6_IDE:
		case PCI_PRODUCT_SERVERWORKS_CSB6_RAID:
			pciide_mapchan(pa, cp, interface,
			    serverworkscsb6_pci_intr);
			break;
		default:
			pciide_mapchan(pa, cp, interface,
			    serverworks_pci_intr);
		}
	}

	pcib_tag = pci_make_tag(pa->pa_pc, pa->pa_bus, pa->pa_device, 0);
	pci_conf_write(pa->pa_pc, pcib_tag, 0x64,
	    (pci_conf_read(pa->pa_pc, pcib_tag, 0x64) & ~0x2000) | 0x4000);
}

static void
serverworks_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	struct atac_softc *atac = chp->ch_atac;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	int channel = chp->ch_channel;
	int drive, unit, s;
	u_int32_t pio_time, dma_time, pio_mode, udma_mode;
	u_int32_t idedma_ctl;
	static const u_int8_t pio_modes[5] = {0x5d, 0x47, 0x34, 0x22, 0x20};
	static const u_int8_t dma_modes[3] = {0x77, 0x21, 0x20};

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	pio_time = pci_conf_read(sc->sc_pc, sc->sc_tag, 0x40);
	dma_time = pci_conf_read(sc->sc_pc, sc->sc_tag, 0x44);
	pio_mode = pci_conf_read(sc->sc_pc, sc->sc_tag, 0x48);
	udma_mode = pci_conf_read(sc->sc_pc, sc->sc_tag, 0x54);

	pio_time &= ~(0xffff << (16 * channel));
	dma_time &= ~(0xffff << (16 * channel));
	pio_mode &= ~(0xff << (8 * channel + 16));
	udma_mode &= ~(0xff << (8 * channel + 16));
	udma_mode &= ~(3 << (2 * channel));

	idedma_ctl = 0;

	/* Per drive settings */
	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;
		unit = drive + 2 * channel;
		/* add timing values, setup DMA if needed */
		pio_time |= pio_modes[drvp->PIO_mode] << (8 * (unit^1));
		pio_mode |= drvp->PIO_mode << (4 * unit + 16);
		if ((atac->atac_cap & ATAC_CAP_UDMA) &&
		    (drvp->drive_flags & ATA_DRIVE_UDMA)) {
			/* use Ultra/DMA, check for 80-pin cable */
			if (drvp->UDMA_mode > 2 &&
			    (PCI_PRODUCT(pci_conf_read(sc->sc_pc, sc->sc_tag,
			    			       PCI_SUBSYS_ID_REG))
			     & (1 << (14 + channel))) == 0)
				drvp->UDMA_mode = 2;
			dma_time |= dma_modes[drvp->DMA_mode] << (8 * (unit^1));
			udma_mode |= drvp->UDMA_mode << (4 * unit + 16);
			udma_mode |= 1 << unit;
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else if ((atac->atac_cap & ATAC_CAP_DMA) &&
		    (drvp->drive_flags & ATA_DRIVE_DMA)) {
			/* use Multiword DMA */
			s = splbio();
			drvp->drive_flags &= ~ATA_DRIVE_UDMA;
			splx(s);
			dma_time |= dma_modes[drvp->DMA_mode] << (8 * (unit^1));
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else {
			/* PIO only */
			s = splbio();
			drvp->drive_flags &= ~(ATA_DRIVE_UDMA | ATA_DRIVE_DMA);
			splx(s);
		}
	}

	pci_conf_write(sc->sc_pc, sc->sc_tag, 0x40, pio_time);
	pci_conf_write(sc->sc_pc, sc->sc_tag, 0x44, dma_time);
	if (sc->sc_pp->ide_product != PCI_PRODUCT_SERVERWORKS_OSB4_IDE)
		pci_conf_write(sc->sc_pc, sc->sc_tag, 0x48, pio_mode);
	pci_conf_write(sc->sc_pc, sc->sc_tag, 0x54, udma_mode);

	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
		    idedma_ctl);
	}
}

static int
serverworks_pci_intr(void *arg)
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
		if ((dmastat & (IDEDMA_CTL_ACT | IDEDMA_CTL_INTR)) !=
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

static int
serverworkscsb6_pci_intr(void *arg)
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct ata_channel *wdc_cp;
	int rv = 0;
	int i, crv;

	for (i = 0; i < sc->sc_wdcdev.sc_atac.atac_nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->ata_channel;
		/*
		 * The CSB6 doesn't assert IDEDMA_CTL_INTR for non-DMA commands.
		 * Until we find a way to know if the controller posted an
		 * interrupt, always call wdcintr(), which will try to guess
		 * if the interrupt was for us or not (and checks
		 * IDEDMA_CTL_INTR for DMA commands only).
		 */
		crv = wdcintr(wdc_cp);
		if (crv != 0)
			rv = 1;
	}
	return rv;
}
