/*	$NetBSD: ixpide.c,v 1.24 2012/07/31 15:50:36 bouyer Exp $	*/

/*
 *  Copyright (c) 2004 The NetBSD Foundation.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 *  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ixpide.c,v 1.24 2012/07/31 15:50:36 bouyer Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_ixp_reg.h>

static bool	ixpide_resume(device_t, const pmf_qual_t *);
static bool	ixpide_suspend(device_t, const pmf_qual_t *);
static int	ixpide_match(device_t, cfdata_t, void *);
static void	ixpide_attach(device_t, device_t, void *);

static void	ixp_chip_map(struct pciide_softc *, const struct pci_attach_args *);
static void	ixp_setup_channel(struct ata_channel *);

CFATTACH_DECL_NEW(ixpide, sizeof(struct pciide_softc),
    ixpide_match, ixpide_attach, pciide_detach, NULL);

static const char ixpdesc[] = "ATI Technologies IXP IDE Controller";

static const struct pciide_product_desc pciide_ixpide_products[] = {
	{ PCI_PRODUCT_ATI_IXP_IDE_200, 0, ixpdesc, ixp_chip_map },
	{ PCI_PRODUCT_ATI_IXP_IDE_300, 0, ixpdesc, ixp_chip_map },
	{ PCI_PRODUCT_ATI_IXP_IDE_400, 0, ixpdesc, ixp_chip_map },
	{ PCI_PRODUCT_ATI_IXP_IDE_600, 0, ixpdesc, ixp_chip_map },
	{ PCI_PRODUCT_ATI_SB400_SATA_1, 0, ixpdesc, ixp_chip_map },
	{ PCI_PRODUCT_ATI_SB400_SATA_2, 0, ixpdesc, ixp_chip_map },
	{ PCI_PRODUCT_ATI_SB600_SATA_1, 0, ixpdesc, ixp_chip_map },
	{ PCI_PRODUCT_ATI_SB600_SATA_2, 0, ixpdesc, ixp_chip_map },
	{ PCI_PRODUCT_ATI_SB700_SATA_IDE, 0, ixpdesc, ixp_chip_map },
	{ PCI_PRODUCT_ATI_SB700_IDE, 0, ixpdesc, ixp_chip_map },
	{ 0, 			       0, NULL,	   NULL }
};

static int
ixpide_match(device_t parent, cfdata_t cfdata, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ATI &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_MASS_STORAGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_IDE &&
	    pciide_lookup_product(pa->pa_id, pciide_ixpide_products))
		return (2);
	return (0);
}

static void
ixpide_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_ixpide_products));

	if (!pmf_device_register(self, ixpide_suspend, ixpide_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static void
ixp_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	int channel;
	pcireg_t interface;

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
	sc->sc_wdcdev.sc_atac.atac_set_modes = ixp_setup_channel;
	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.wdc_maxdrives = 2;

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

/* Values from linux driver */
static const uint8_t ixp_pio_timings[] = {
	0x5d, 0x47, 0x34, 0x22, 0x20
};

static const uint8_t ixp_mdma_timings[] = {
	0x77, 0x21, 0x20
};

static bool
ixpide_resume(device_t dv, const pmf_qual_t *qual)
{
	struct pciide_softc *sc = device_private(dv);

	pci_conf_write(sc->sc_pc, sc->sc_tag, IXP_MDMA_TIMING,
	    sc->sc_pm_reg[0]);
	pci_conf_write(sc->sc_pc, sc->sc_tag, IXP_PIO_TIMING,
	    sc->sc_pm_reg[1]);

	return true;
}

static bool
ixpide_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct pciide_softc *sc = device_private(dv);

	sc->sc_pm_reg[0] = pci_conf_read(sc->sc_pc, sc->sc_tag,
	    IXP_MDMA_TIMING);
	sc->sc_pm_reg[1] = pci_conf_read(sc->sc_pc, sc->sc_tag,
	    IXP_PIO_TIMING);

	return true;
}

static void
ixp_setup_channel(struct ata_channel *chp)
{
	int drive, s;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	pcireg_t udma, mdma_timing, pio, pio_timing;
	struct ata_drive_datas *drvp;
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);

	pio_timing = pci_conf_read(sc->sc_pc, sc->sc_tag, IXP_PIO_TIMING);
	pio = pci_conf_read(sc->sc_pc, sc->sc_tag, IXP_PIO_CTL);
	mdma_timing = pci_conf_read(sc->sc_pc, sc->sc_tag, IXP_MDMA_TIMING);
	udma = pci_conf_read(sc->sc_pc, sc->sc_tag, IXP_UDMA_CTL);

	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;
		if (drvp->drive_flags & ATA_DRIVE_UDMA) {
			s = splbio();
			drvp->drive_flags &= ~ATA_DRIVE_DMA;
			splx(s);
			IXP_UDMA_ENABLE(udma, chp->ch_channel, drive);
			IXP_SET_MODE(udma, chp->ch_channel, drive, drvp->UDMA_mode);
		} else if (drvp->drive_flags & ATA_DRIVE_DMA) {
			IXP_UDMA_DISABLE(udma, chp->ch_channel, drive);
			IXP_SET_TIMING(mdma_timing, chp->ch_channel, drive,
			    ixp_mdma_timings[drvp->DMA_mode]);
		} else
			IXP_UDMA_DISABLE(udma, chp->ch_channel, drive);

		/*
		 * Set PIO mode and timings
		 * Linux driver avoids PIO mode 1, let's do it too.
		 */
		if (drvp->PIO_mode == 1)
			drvp->PIO_mode = 0;

		IXP_SET_MODE(pio, chp->ch_channel, drive, drvp->PIO_mode);
		IXP_SET_TIMING(pio_timing, chp->ch_channel, drive,
		    ixp_pio_timings[drvp->PIO_mode]);
	}
	pci_conf_write(sc->sc_pc, sc->sc_tag, IXP_UDMA_CTL, udma);
	pci_conf_write(sc->sc_pc, sc->sc_tag, IXP_MDMA_TIMING, mdma_timing);
	pci_conf_write(sc->sc_pc, sc->sc_tag, IXP_PIO_CTL, pio);
	pci_conf_write(sc->sc_pc, sc->sc_tag, IXP_PIO_TIMING, pio_timing);
	ATADEBUG_PRINT(("ixp_setup_channel: udma = %08x, mdma_timing = %08x, pio_mode = %08x,"
	    " pio_timing = %08x\n", udma, mdma_timing, pio, pio_timing), DEBUG_PROBE);
}
