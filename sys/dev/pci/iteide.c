/*	$NetBSD: iteide.c,v 1.19 2013/10/07 19:51:55 jakllsch Exp $	*/

/*
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Grant Beattie.
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
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: iteide.c,v 1.19 2013/10/07 19:51:55 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_ite_reg.h>

static void ite_chip_map(struct pciide_softc*, const struct pci_attach_args*);
static void ite_setup_channel(struct ata_channel*);

static int  iteide_match(device_t, cfdata_t, void *);
static void iteide_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(iteide, sizeof(struct pciide_softc),
    iteide_match, iteide_attach, pciide_detach, NULL);

static const struct pciide_product_desc pciide_ite_products[] =  {
	{ PCI_PRODUCT_ITE_IT8211,
	  0,
	  "Integrated Technology Express IDE controller",
	  ite_chip_map,
	},
	{ PCI_PRODUCT_ITE_IT8212,
	  0,
	  "Integrated Technology Express IDE controller",
	  ite_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

static int
iteide_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ITE &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_MASS_STORAGE) {
		if (pciide_lookup_product(pa->pa_id, pciide_ite_products))
			return (2);
	}
	return (0);
}

static void
iteide_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_ite_products));
}

static void
ite_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	int channel;
	pcireg_t interface;
	pcireg_t cfg, modectl;

	/* fake interface since IT8212 claims to be a RAID device */
	interface = PCIIDE_INTERFACE_BUS_MASTER_DMA |
	    PCIIDE_INTERFACE_PCI(0) | PCIIDE_INTERFACE_PCI(1);

	cfg = pci_conf_read(sc->sc_pc, sc->sc_tag, IT_CFG);
	modectl = pci_conf_read(sc->sc_pc, sc->sc_tag, IT_MODE);
	ATADEBUG_PRINT(("%s: cfg=0x%x, modectl=0x%x\n",
	    device_xname(sc->sc_wdcdev.sc_atac.atac_dev), cfg & IT_CFG_MASK,
	    modectl & IT_MODE_MASK), DEBUG_PROBE);

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
	sc->sc_wdcdev.sc_atac.atac_udma_cap = 6;

	sc->sc_wdcdev.sc_atac.atac_set_modes = ite_setup_channel;
	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.wdc_maxdrives = 2;

	wdc_allocate_regs(&sc->sc_wdcdev);

	/* Disable RAID */
	modectl &= ~IT_MODE_RAID1;
	/* Disable CPU firmware mode */
	modectl &= ~IT_MODE_CPU;

	pci_conf_write(sc->sc_pc, sc->sc_tag, IT_MODE, modectl);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels; channel++) {
		cp = &sc->pciide_channels[channel];

		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;

		pciide_mapchan(pa, cp, interface, pciide_pci_intr);
	}
	/* Re-read configuration registers after channels setup */
	cfg = pci_conf_read(sc->sc_pc, sc->sc_tag, IT_CFG);
	modectl = pci_conf_read(sc->sc_pc, sc->sc_tag, IT_MODE);
	ATADEBUG_PRINT(("%s: cfg=0x%x, modectl=0x%x\n",
	    device_xname(sc->sc_wdcdev.sc_atac.atac_dev), cfg & IT_CFG_MASK,
	    modectl & IT_MODE_MASK), DEBUG_PROBE);
}

static void
ite_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	int drive, mode = 0;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	int channel = chp->ch_channel;
	pcireg_t cfg, modectl;
	pcireg_t tim;

	cfg = pci_conf_read(sc->sc_pc, sc->sc_tag, IT_CFG);
	modectl = pci_conf_read(sc->sc_pc, sc->sc_tag, IT_MODE);
	tim = pci_conf_read(sc->sc_pc, sc->sc_tag, IT_TIM(channel));
	ATADEBUG_PRINT(("%s:%d: tim=0x%x\n",
	    device_xname(sc->sc_wdcdev.sc_atac.atac_dev),
	    channel, tim), DEBUG_PROBE);

	/* Setup DMA if needed */
	pciide_channel_dma_setup(cp);

	/* Clear all bits for this channel */
	idedma_ctl = 0;

	/* Per channel settings */
	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];

		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;

		if ((chp->ch_atac->atac_cap & ATAC_CAP_UDMA) != 0 &&
		    (drvp->drive_flags & ATA_DRIVE_UDMA) != 0) {
			/* Setup UltraDMA mode */
			drvp->drive_flags &= ~ATA_DRIVE_DMA;
			modectl &= ~IT_MODE_DMA(channel, drive);

#if 0
			/* Check cable, only works in CPU firmware mode */
			if (drvp->UDMA_mode > 2 &&
			    (cfg & IT_CFG_CABLE(channel, drive)) == 0) {
				ATADEBUG_PRINT(("(%s:%d:%d): "
				    "80-wire cable not detected\n",
				    device_xname(sc->sc_wdcdev.sc_atac.atac_dev),
				    channel, drive), DEBUG_PROBE);
				drvp->UDMA_mode = 2;
			}
#endif

			if (drvp->UDMA_mode >= 5)
				tim |= IT_TIM_UDMA5(drive);
			else
				tim &= ~IT_TIM_UDMA5(drive);

			mode = drvp->PIO_mode;
		} else if ((chp->ch_atac->atac_cap & ATAC_CAP_DMA) != 0 &&
		    (drvp->drive_flags & ATA_DRIVE_DMA) != 0) {
			/* Setup multiword DMA mode */
			drvp->drive_flags &= ~ATA_DRIVE_UDMA;
			modectl |= IT_MODE_DMA(channel, drive);

			/* mode = min(pio, dma + 2) */
			if (drvp->PIO_mode <= (drvp->DMA_mode + 2))
				mode = drvp->PIO_mode;
			else
				mode = drvp->DMA_mode + 2;
		} else {
			goto pio;
		}
		idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);

pio:
		/* Setup PIO mode */
		if (mode <= 2) {
			drvp->DMA_mode = 0;
			drvp->PIO_mode = 0;
			mode = 0;
		} else {
			drvp->PIO_mode = mode;
			drvp->DMA_mode = mode - 2;
		}

		/* Enable IORDY if PIO mode >= 3 */
		if (drvp->PIO_mode >= 3)
			cfg |= IT_CFG_IORDY(channel);
	}

	ATADEBUG_PRINT(("%s: tim=0x%x\n",
	    device_xname(sc->sc_wdcdev.sc_atac.atac_dev), tim), DEBUG_PROBE);

	pci_conf_write(sc->sc_pc, sc->sc_tag, IT_CFG, cfg);
	pci_conf_write(sc->sc_pc, sc->sc_tag, IT_MODE, modectl);
	pci_conf_write(sc->sc_pc, sc->sc_tag, IT_TIM(channel), tim);

	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot,
		    cp->dma_iohs[IDEDMA_CTL], 0, idedma_ctl);
	}
}
