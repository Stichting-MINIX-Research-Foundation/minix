/*	$NetBSD: rdcide.c,v 1.8 2014/07/08 18:01:26 msaitoh Exp $	*/

/*
 * Copyright (c) 2011 Manuel Bouyer.
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
__KERNEL_RCSID(0, "$NetBSD: rdcide.c,v 1.8 2014/07/08 18:01:26 msaitoh Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/rdcide_reg.h>

static void rdcide_chip_map(struct pciide_softc *,
    const struct pci_attach_args *);
static void rdcide_setup_channel(struct ata_channel *);

static bool rdcide_resume(device_t, const pmf_qual_t *);
static bool rdcide_suspend(device_t, const pmf_qual_t *);
static int  rdcide_match(device_t, cfdata_t, void *);
static void rdcide_attach(device_t, device_t, void *);

static const struct pciide_product_desc pciide_intel_products[] =  {
	{ PCI_PRODUCT_RDC_R1011_IDE,
	  0,
	  "RDC R1011 IDE controller",
	  rdcide_chip_map,
	},
	{ PCI_PRODUCT_RDC_R1012_IDE,
	  0,
	  "RDC R1012 IDE controller",
	  rdcide_chip_map,
	},
};

CFATTACH_DECL_NEW(rdcide, sizeof(struct pciide_softc),
    rdcide_match, rdcide_attach, NULL, NULL);

static int
rdcide_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_RDC) {
		if (pciide_lookup_product(pa->pa_id, pciide_intel_products))
			return (2);
	}
	return (0);
}

static void
rdcide_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_intel_products));

	if (!pmf_device_register(self, rdcide_suspend, rdcide_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static bool
rdcide_resume(device_t dv, const pmf_qual_t *qual)
{
	struct pciide_softc *sc = device_private(dv);

	pci_conf_write(sc->sc_pc, sc->sc_tag, RDCIDE_PATR,
	    sc->sc_pm_reg[0]);
	pci_conf_write(sc->sc_pc, sc->sc_tag, RDCIDE_PSD1ATR,
	    sc->sc_pm_reg[1]);
	pci_conf_write(sc->sc_pc, sc->sc_tag, RDCIDE_UDCCR,
	    sc->sc_pm_reg[2]);
	pci_conf_write(sc->sc_pc, sc->sc_tag, RDCIDE_IIOCR,
	    sc->sc_pm_reg[3]);

	return true;
}

static bool
rdcide_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct pciide_softc *sc = device_private(dv);

	sc->sc_pm_reg[0] = pci_conf_read(sc->sc_pc, sc->sc_tag,
	    RDCIDE_PATR);
	sc->sc_pm_reg[1] = pci_conf_read(sc->sc_pc, sc->sc_tag,
	    RDCIDE_PSD1ATR);
	sc->sc_pm_reg[2] = pci_conf_read(sc->sc_pc, sc->sc_tag,
	    RDCIDE_UDCCR);
	sc->sc_pm_reg[3] = pci_conf_read(sc->sc_pc, sc->sc_tag,
	    RDCIDE_IIOCR);

	return true;
}

static void
rdcide_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	int channel;
	u_int32_t patr;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);

	if (pciide_chipen(sc, pa) == 0)
		return;

	aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "bus-master DMA support present");
	pciide_mapreg_dma(sc, pa);
	aprint_verbose("\n");
	sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DATA16 | ATAC_CAP_DATA32;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA | ATAC_CAP_UDMA;
		sc->sc_wdcdev.irqack = pciide_irqack;
		sc->sc_wdcdev.dma_init = pciide_dma_init;
	}
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;
	sc->sc_wdcdev.sc_atac.atac_udma_cap = 5;
	sc->sc_wdcdev.sc_atac.atac_set_modes = rdcide_setup_channel;
	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.wdc_maxdrives = 2;

	ATADEBUG_PRINT(("rdcide_setup_chip: old PATR=0x%x",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, RDCIDE_PATR)),
	    DEBUG_PROBE);
	ATADEBUG_PRINT((", PSD1ATR=0x%x",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, RDCIDE_PSD1ATR)),
	    DEBUG_PROBE);
	ATADEBUG_PRINT((", UDCCR 0x%x",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, RDCIDE_UDCCR)),
	    DEBUG_PROBE);
	ATADEBUG_PRINT((", IIOCR 0x%x",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, RDCIDE_IIOCR)),
	    DEBUG_PROBE);
	ATADEBUG_PRINT(("\n"), DEBUG_PROBE);

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		patr = pci_conf_read(sc->sc_pc, sc->sc_tag, RDCIDE_PATR);
		if ((patr & RDCIDE_PATR_EN(channel)) == 0) {
			aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "%s channel ignored (disabled)\n", cp->name);
			cp->ata_channel.ch_flags |= ATACH_DISABLED;
			continue;
		}
		pciide_mapchan(pa, cp, interface, pciide_pci_intr);
	}
	ATADEBUG_PRINT(("rdcide_setup_chip: PATR=0x%x",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, RDCIDE_PATR)),
	    DEBUG_PROBE);
	ATADEBUG_PRINT((", PSD1ATR=0x%x",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, RDCIDE_PSD1ATR)),
	    DEBUG_PROBE);
	ATADEBUG_PRINT((", UDCCR 0x%x",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, RDCIDE_UDCCR)),
	    DEBUG_PROBE);
	ATADEBUG_PRINT((", IIOCR 0x%x",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, RDCIDE_IIOCR)),
	    DEBUG_PROBE);
	ATADEBUG_PRINT(("\n"), DEBUG_PROBE);

}

static void
rdcide_setup_channel(struct ata_channel *chp)
{
	u_int8_t drive;
	u_int32_t patr, psd1atr, udccr, iiocr;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	struct ata_drive_datas *drvp = cp->ata_channel.ch_drive;

	patr = pci_conf_read(sc->sc_pc, sc->sc_tag, RDCIDE_PATR);
	psd1atr = pci_conf_read(sc->sc_pc, sc->sc_tag, RDCIDE_PSD1ATR);
	udccr = pci_conf_read(sc->sc_pc, sc->sc_tag, RDCIDE_UDCCR);
	iiocr = pci_conf_read(sc->sc_pc, sc->sc_tag, RDCIDE_IIOCR);

	/* setup DMA */
	pciide_channel_dma_setup(cp);

	/* clear modes */
	patr = patr & (RDCIDE_PATR_EN(0) | RDCIDE_PATR_EN(1));
	psd1atr &= ~RDCIDE_PSD1ATR_SETUP_MASK(chp->ch_channel);
	psd1atr &= ~RDCIDE_PSD1ATR_HOLD_MASK(chp->ch_channel);
	for (drive = 0; drive < 2; drive++) {
		udccr &= ~RDCIDE_UDCCR_EN(chp->ch_channel, drive);
		udccr &= ~RDCIDE_UDCCR_TIM_MASK(chp->ch_channel, drive);
		iiocr &= ~RDCIDE_IIOCR_CLK_MASK(chp->ch_channel, drive);
	}
	/* now setup modes */
	for (drive = 0; drive < 2; drive++) {
		if (drvp[drive].drive_type == ATA_DRIVET_NONE)
			continue;
		if (drvp[drive].drive_type == ATA_DRIVET_ATAPI)
			patr |= RDCIDE_PATR_ATA(chp->ch_channel, drive);
		if (drive == 0) {
			patr |= RDCIDE_PATR_SETUP(
			    rdcide_setup[drvp[drive].PIO_mode],
			    chp->ch_channel);
			patr |= RDCIDE_PATR_HOLD(
			    rdcide_hold[drvp[drive].PIO_mode],
			    chp->ch_channel);
		} else {
			patr |= RDCIDE_PATR_DEV1_TEN(chp->ch_channel);
			psd1atr |= RDCIDE_PSD1ATR_SETUP(
			    rdcide_setup[drvp[drive].PIO_mode],
			    chp->ch_channel);
			psd1atr |= RDCIDE_PSD1ATR_HOLD(
			    rdcide_hold[drvp[drive].PIO_mode],
			    chp->ch_channel);
		}
		if (drvp[drive].PIO_mode > 0) {
			patr |= RDCIDE_PATR_FTIM(chp->ch_channel, drive);
			patr |= RDCIDE_PATR_IORDY(chp->ch_channel, drive);
		}
		if (drvp[drive].drive_flags & ATA_DRIVE_DMA) {
			patr |= RDCIDE_PATR_DMAEN(chp->ch_channel, drive);
		}
		if ((drvp[drive].drive_flags & ATA_DRIVE_UDMA) == 0)
			continue;

		if ((iiocr & RDCIDE_IIOCR_CABLE(chp->ch_channel, drive)) == 0
		    && drvp[drive].UDMA_mode > 2)
			drvp[drive].UDMA_mode = 2;
		udccr |= RDCIDE_UDCCR_EN(chp->ch_channel, drive);
		udccr |= RDCIDE_UDCCR_TIM(
		    rdcide_udmatim[drvp[drive].UDMA_mode],
		    chp->ch_channel, drive);
		iiocr |= RDCIDE_IIOCR_CLK(
		    rdcide_udmaclk[drvp[drive].UDMA_mode],
		    chp->ch_channel, drive);
	}

	pci_conf_write(sc->sc_pc, sc->sc_tag, RDCIDE_PATR, patr);
	pci_conf_write(sc->sc_pc, sc->sc_tag, RDCIDE_PSD1ATR, psd1atr);
	pci_conf_write(sc->sc_pc, sc->sc_tag, RDCIDE_UDCCR, udccr);
	pci_conf_write(sc->sc_pc, sc->sc_tag, RDCIDE_IIOCR, iiocr);
}
