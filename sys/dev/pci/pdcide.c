/*	$NetBSD: pdcide.c,v 1.35 2013/10/07 19:51:55 jakllsch Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: pdcide.c,v 1.35 2013/10/07 19:51:55 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_pdc202xx_reg.h>

static void pdc202xx_chip_map(struct pciide_softc *,
    const struct pci_attach_args *);
static void pdc202xx_setup_channel(struct ata_channel *);
static void pdc20268_setup_channel(struct ata_channel *);
static int  pdc202xx_pci_intr(void *);
static int  pdc20265_pci_intr(void *);
static void pdc20262_dma_start(void *, int, int);
static int  pdc20262_dma_finish(void *, int, int, int);

static int  pdcide_match(device_t, cfdata_t, void *);
static void pdcide_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(pdcide, sizeof(struct pciide_softc),
    pdcide_match, pdcide_attach, pciide_detach, NULL);

static const struct pciide_product_desc pciide_promise_products[] =  {
	{ PCI_PRODUCT_PROMISE_PDC20246,
	  0,
	  "Promise Ultra33/ATA Bus Master IDE Accelerator",
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20262,
	  0,
	  "Promise Ultra66/ATA Bus Master IDE Accelerator",
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20267,
	  0,
	  "Promise Ultra100/ATA Bus Master IDE Accelerator",
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20265,
	  0,
	  "Promise Ultra100/ATA Bus Master IDE Accelerator",
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20268,
	  0,
	  "Promise Ultra100TX2/ATA Bus Master IDE Accelerator",
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20270,
	  0,
	  "Promise Ultra100TX2v2/ATA Bus Master IDE Accelerator",
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20269,
	  0,
	  "Promise Ultra133/ATA Bus Master IDE Accelerator",
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20276,
	  0,
	  "Promise Ultra133TX2/ATA Bus Master IDE Accelerator",
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20275,
	  0,
	  "Promise Ultra133/ATA Bus Master IDE Accelerator (MB)",
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20271,
	  0,
	  "Promise Ultra133TX2v2/ATA Bus Master IDE Accelerator",
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_PDC20277,
	  0,
	  "Promise Fasttrak133 Lite Bus Master IDE Accelerator",
	  pdc202xx_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

static int
pdcide_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_PROMISE) {
		if (pciide_lookup_product(pa->pa_id, pciide_promise_products))
			return (2);
	}
	return (0);
}

static void
pdcide_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_promise_products));

}

/* Macros to test product */
#define PDC_IS_262(sc)							\
	((sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20262 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20267 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20265 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20268 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20270 || \
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20269 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20276 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20275 || \
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20271 || \
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20277)
#define PDC_IS_265(sc)							\
	((sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20267 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20265 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20268 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20270 || \
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20269 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20276 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20275 || \
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20271 || \
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20277)
#define PDC_IS_268(sc)							\
	((sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20268 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20270 || \
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20269 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20276 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20275 || \
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20271 || \
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20277)
#define PDC_IS_276(sc)							\
	((sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20269 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20276 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20275 || \
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20271 || \
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20277)

static void
pdc202xx_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	int channel;
	pcireg_t interface, st, mode;

	if (!PDC_IS_268(sc)) {
		st = pci_conf_read(sc->sc_pc, sc->sc_tag, PDC2xx_STATE);
		ATADEBUG_PRINT(("pdc202xx_setup_chip: controller state 0x%x\n",
		    st), DEBUG_PROBE);
		/* turn off  RAID mode */
		if (st & PDC2xx_STATE_IDERAID) {
			ATADEBUG_PRINT(("pdc202xx_setup_chip: turning off RAID mode\n"), DEBUG_PROBE);
			st &= ~PDC2xx_STATE_IDERAID;
			pci_conf_write(sc->sc_pc, sc->sc_tag, PDC2xx_STATE, st);
		}
	} else
		st = PDC2xx_STATE_NATIVE | PDC262_STATE_EN(0) | PDC262_STATE_EN(1);

	if (pciide_chipen(sc, pa) == 0)
		return;

	/*
	 * can't rely on the PCI_CLASS_REG content if the chip was in raid
	 * mode. We have to fake interface
	 */
	interface = PCIIDE_INTERFACE_SETTABLE(0) | PCIIDE_INTERFACE_SETTABLE(1);
	if (st & PDC2xx_STATE_NATIVE)
		interface |= PCIIDE_INTERFACE_PCI(0) | PCIIDE_INTERFACE_PCI(1);

	aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "bus-master DMA support present");
	pciide_mapreg_dma(sc, pa);
	aprint_verbose("\n");
	sc->sc_wdcdev.sc_atac.atac_cap = ATAC_CAP_DATA16 | ATAC_CAP_DATA32;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA | ATAC_CAP_UDMA;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_MASS_STORAGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_RAID)
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_RAID;
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;
	if (PDC_IS_276(sc))
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 6;
	else if (PDC_IS_265(sc))
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 5;
	else if (PDC_IS_262(sc))
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 4;
	else
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 2;
	sc->sc_wdcdev.sc_atac.atac_set_modes = PDC_IS_268(sc) ?
			pdc20268_setup_channel : pdc202xx_setup_channel;
	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.wdc_maxdrives = 2;

	wdc_allocate_regs(&sc->sc_wdcdev);

	if (sc->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20262 ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20267 ||
	    sc->sc_pp->ide_product == PCI_PRODUCT_PROMISE_PDC20265) {
		sc->sc_wdcdev.dma_start = pdc20262_dma_start;
		sc->sc_wdcdev.dma_finish = pdc20262_dma_finish;
	}

	if (!PDC_IS_268(sc)) {
		/* setup failsafe defaults */
		mode = 0;
		mode = PDC2xx_TIM_SET_PA(mode, pdc2xx_pa[0]);
		mode = PDC2xx_TIM_SET_PB(mode, pdc2xx_pb[0]);
		mode = PDC2xx_TIM_SET_MB(mode, pdc2xx_dma_mb[0]);
		mode = PDC2xx_TIM_SET_MC(mode, pdc2xx_dma_mc[0]);
		for (channel = 0;
		     channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
		     channel++) {
			ATADEBUG_PRINT(("pdc202xx_setup_chip: channel %d "
			    "drive 0 initial timings  0x%x, now 0x%x\n",
			    channel, pci_conf_read(sc->sc_pc, sc->sc_tag,
			    PDC2xx_TIM(channel, 0)), mode | PDC2xx_TIM_IORDYp),
			    DEBUG_PROBE);
			pci_conf_write(sc->sc_pc, sc->sc_tag,
			    PDC2xx_TIM(channel, 0), mode | PDC2xx_TIM_IORDYp);
			ATADEBUG_PRINT(("pdc202xx_setup_chip: channel %d "
			    "drive 1 initial timings  0x%x, now 0x%x\n",
			    channel, pci_conf_read(sc->sc_pc, sc->sc_tag,
			    PDC2xx_TIM(channel, 1)), mode), DEBUG_PROBE);
			pci_conf_write(sc->sc_pc, sc->sc_tag,
			    PDC2xx_TIM(channel, 1), mode);
		}

		mode = PDC2xx_SCR_DMA;
		if (PDC_IS_265(sc)) {
			mode = PDC2xx_SCR_SET_GEN(mode, PDC265_SCR_GEN_LAT);
		} else if (PDC_IS_262(sc)) {
			mode = PDC2xx_SCR_SET_GEN(mode, PDC262_SCR_GEN_LAT);
		} else {
			/* the BIOS set it up this way */
			mode = PDC2xx_SCR_SET_GEN(mode, 0x1);
		}
		mode = PDC2xx_SCR_SET_I2C(mode, 0x3); /* ditto */
		mode = PDC2xx_SCR_SET_POLL(mode, 0x1); /* ditto */
		ATADEBUG_PRINT(("pdc202xx_setup_chip: initial SCR  0x%x, "
		    "now 0x%x\n",
		    bus_space_read_4(sc->sc_dma_iot, sc->sc_dma_ioh,
			PDC2xx_SCR),
		    mode), DEBUG_PROBE);
		bus_space_write_4(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC2xx_SCR, mode);

		/* controller initial state register is OK even without BIOS */
		/* Set DMA mode to IDE DMA compatibility */
		mode =
		    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh, PDC2xx_PM);
		ATADEBUG_PRINT(("pdc202xx_setup_chip: primary mode 0x%x", mode),
		    DEBUG_PROBE);
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh, PDC2xx_PM,
		    mode | 0x1);
		mode =
		    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh, PDC2xx_SM);
		ATADEBUG_PRINT((", secondary mode 0x%x\n", mode ), DEBUG_PROBE);
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh, PDC2xx_SM,
		    mode | 0x1);
	}
	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		if ((st & (PDC_IS_262(sc) ?
		    PDC262_STATE_EN(channel):PDC246_STATE_EN(channel))) == 0) {
			aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "%s channel ignored (disabled)\n", cp->name);
			cp->ata_channel.ch_flags |= ATACH_DISABLED;
			continue;
		}
		pciide_mapchan(pa, cp, interface,
		    PDC_IS_265(sc) ? pdc20265_pci_intr : pdc202xx_pci_intr);
		/* clear interrupt, in case there is one pending */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
		    IDEDMA_CTL_INTR);
	}
	return;
}

static void
pdc202xx_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	int drive, s;
	pcireg_t mode, st;
	u_int32_t idedma_ctl, scr, atapi;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	int channel = chp->ch_channel;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	idedma_ctl = 0;
	ATADEBUG_PRINT(("pdc202xx_setup_channel %s: scr 0x%x\n",
	    device_xname(sc->sc_wdcdev.sc_atac.atac_dev),
	    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh, PDC262_U66)),
	    DEBUG_PROBE);

	/* Per channel settings */
	if (PDC_IS_262(sc)) {
		scr = bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC262_U66);
		st = pci_conf_read(sc->sc_pc, sc->sc_tag, PDC2xx_STATE);
		/* Trim UDMA mode */
		if ((st & PDC262_STATE_80P(channel)) != 0 ||
		    (chp->ch_drive[0].drive_flags & ATA_DRIVE_UDMA &&
		    chp->ch_drive[0].UDMA_mode <= 2) ||
		    (chp->ch_drive[1].drive_flags & ATA_DRIVE_UDMA &&
		    chp->ch_drive[1].UDMA_mode <= 2)) {
			if (chp->ch_drive[0].UDMA_mode > 2)
				chp->ch_drive[0].UDMA_mode = 2;
			if (chp->ch_drive[1].UDMA_mode > 2)
				chp->ch_drive[1].UDMA_mode = 2;
		}
		/* Set U66 if needed */
		if ((chp->ch_drive[0].drive_flags & ATA_DRIVE_UDMA &&
		    chp->ch_drive[0].UDMA_mode > 2) ||
		    (chp->ch_drive[1].drive_flags & ATA_DRIVE_UDMA &&
		    chp->ch_drive[1].UDMA_mode > 2))
			scr |= PDC262_U66_EN(channel);
		else
			scr &= ~PDC262_U66_EN(channel);
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC262_U66, scr);
		ATADEBUG_PRINT(("pdc202xx_setup_channel %s:%d: ATAPI 0x%x\n",
		    device_xname(sc->sc_wdcdev.sc_atac.atac_dev), channel,
		    bus_space_read_4(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC262_ATAPI(channel))), DEBUG_PROBE);
		if (chp->ch_drive[0].drive_type == ATA_DRIVET_ATAPI ||
			chp->ch_drive[1].drive_type == ATA_DRIVET_ATAPI) {
			if (((chp->ch_drive[0].drive_flags & ATA_DRIVE_UDMA) &&
			    !(chp->ch_drive[1].drive_flags & ATA_DRIVE_UDMA) &&
			    (chp->ch_drive[1].drive_flags & ATA_DRIVE_DMA)) ||
			    ((chp->ch_drive[1].drive_flags & ATA_DRIVE_UDMA) &&
			    !(chp->ch_drive[0].drive_flags & ATA_DRIVE_UDMA) &&
			    (chp->ch_drive[0].drive_flags & ATA_DRIVE_DMA)))
				atapi = 0;
			else
				atapi = PDC262_ATAPI_UDMA;
			bus_space_write_4(sc->sc_dma_iot, sc->sc_dma_ioh,
			    PDC262_ATAPI(channel), atapi);
		}
	}
	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;
		mode = 0;
		if (drvp->drive_flags & ATA_DRIVE_UDMA) {
			/* use Ultra/DMA */
			s = splbio();
			drvp->drive_flags &= ~ATA_DRIVE_DMA;
			splx(s);
			mode = PDC2xx_TIM_SET_MB(mode,
			    pdc2xx_udma_mb[drvp->UDMA_mode]);
			mode = PDC2xx_TIM_SET_MC(mode,
			    pdc2xx_udma_mc[drvp->UDMA_mode]);
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else if (drvp->drive_flags & ATA_DRIVE_DMA) {
			mode = PDC2xx_TIM_SET_MB(mode,
			    pdc2xx_dma_mb[drvp->DMA_mode]);
			mode = PDC2xx_TIM_SET_MC(mode,
			    pdc2xx_dma_mc[drvp->DMA_mode]);
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else {
			mode = PDC2xx_TIM_SET_MB(mode,
			    pdc2xx_dma_mb[0]);
			mode = PDC2xx_TIM_SET_MC(mode,
			    pdc2xx_dma_mc[0]);
		}
		mode = PDC2xx_TIM_SET_PA(mode, pdc2xx_pa[drvp->PIO_mode]);
		mode = PDC2xx_TIM_SET_PB(mode, pdc2xx_pb[drvp->PIO_mode]);
		if (drvp->drive_type == ATA_DRIVET_ATA)
			mode |= PDC2xx_TIM_PRE;
		mode |= PDC2xx_TIM_SYNC | PDC2xx_TIM_ERRDY;
		if (drvp->PIO_mode >= 3) {
			mode |= PDC2xx_TIM_IORDY;
			if (drive == 0)
				mode |= PDC2xx_TIM_IORDYp;
		}
		ATADEBUG_PRINT(("pdc202xx_setup_channel: %s:%d:%d "
		    "timings 0x%x\n",
		    device_xname(sc->sc_wdcdev.sc_atac.atac_dev),
		    chp->ch_channel, drive, mode), DEBUG_PROBE);
		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    PDC2xx_TIM(chp->ch_channel, drive), mode);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL],
		    0, idedma_ctl);
	}
}

static void
pdc20268_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	int drive, s;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	int u100;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	idedma_ctl = 0;

	/* I don't know what this is for, FreeBSD does it ... */
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CMD + 0x1 + IDEDMA_SCH_OFFSET * chp->ch_channel, 0x0b);

	/*
	 * cable type detect, from FreeBSD
	 */
	u100 = (bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CMD + 0x3 + IDEDMA_SCH_OFFSET * chp->ch_channel) & 0x04) ?
	    0 : 1;

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
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
			if (drvp->UDMA_mode > 2 && u100 == 0)
				drvp->UDMA_mode = 2;
		} else if (drvp->drive_flags & ATA_DRIVE_DMA) {
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		}
	}
	/* nothing to do to setup modes, the controller snoop SET_FEATURE cmd */
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL],
		    0, idedma_ctl);
	}
}

static int
pdc202xx_pci_intr(void *arg)
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct ata_channel *wdc_cp;
	int i, rv, crv;
	u_int32_t scr;

	rv = 0;
	scr = bus_space_read_4(sc->sc_dma_iot, sc->sc_dma_ioh, PDC2xx_SCR);
	for (i = 0; i < sc->sc_wdcdev.sc_atac.atac_nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->ata_channel;
		/* If a compat channel skip. */
		if (cp->compat)
			continue;
		if (scr & PDC2xx_SCR_INT(i)) {
			crv = wdcintr(wdc_cp);
			if (crv == 0)
				aprint_error("%s:%d: bogus intr (reg 0x%x)\n",
				    device_xname(
				      sc->sc_wdcdev.sc_atac.atac_dev), i, scr);
			else
				rv = 1;
		}
	}
	return rv;
}

static int
pdc20265_pci_intr(void *arg)
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct ata_channel *wdc_cp;
	int i, rv, crv;
	u_int32_t dmastat;

	rv = 0;
	for (i = 0; i < sc->sc_wdcdev.sc_atac.atac_nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->ata_channel;
		/* If a compat channel skip. */
		if (cp->compat)
			continue;
#if 0
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh, IDEDMA_CMD + 0x1 + IDEDMA_SCH_OFFSET * i, 0x0b);
		if ((bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh, IDEDMA_CMD + 0x3 + IDEDMA_SCH_OFFSET * i) & 0x20) == 0)
			continue;
#endif
		/*
		 * The Ultra/100 seems to assert PDC2xx_SCR_INT * spuriously,
		 * however it asserts INT in IDEDMA_CTL even for non-DMA ops.
		 * So use it instead (requires 2 reg reads instead of 1,
		 * but we can't do it another way).
		 */
		dmastat = bus_space_read_1(sc->sc_dma_iot,
		    cp->dma_iohs[IDEDMA_CTL], 0);
		if((dmastat & IDEDMA_CTL_INTR) == 0)
			continue;
		crv = wdcintr(wdc_cp);
		if (crv == 0)
			aprint_error("%s:%d: bogus intr\n",
			    device_xname(sc->sc_wdcdev.sc_atac.atac_dev), i);
		else
			rv = 1;
	}
	return rv;
}

static void
pdc20262_dma_start(void *v, int channel, int drive)
{
	struct pciide_softc *sc = v;
	struct pciide_dma_maps *dma_maps =
	    &sc->pciide_channels[channel].dma_maps[drive];
	int atapi;

	if (dma_maps->dma_flags & WDC_DMA_LBA48) {
		atapi = (dma_maps->dma_flags & WDC_DMA_READ) ?
		    PDC262_ATAPI_LBA48_READ : PDC262_ATAPI_LBA48_WRITE;
		atapi |= dma_maps->dmamap_xfer->dm_mapsize >> 1;
		bus_space_write_4(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC262_ATAPI(channel), atapi);
	}

	pciide_dma_start(v, channel, drive);
}

static int
pdc20262_dma_finish(void *v, int channel, int drive, int force)
{
	struct pciide_softc *sc = v;
	struct pciide_dma_maps *dma_maps =
	    &sc->pciide_channels[channel].dma_maps[drive];
	struct ata_channel *chp;
	int atapi, error;

	error = pciide_dma_finish(v, channel, drive, force);

	if (dma_maps->dma_flags & WDC_DMA_LBA48) {
		chp = sc->wdc_chanarray[channel];
		atapi = 0;
		if (chp->ch_drive[0].drive_type == ATA_DRIVET_ATAPI ||
		    chp->ch_drive[1].drive_type == ATA_DRIVET_ATAPI) {
			if ((!(chp->ch_drive[0].drive_flags & ATA_DRIVE_UDMA) ||
			    (chp->ch_drive[1].drive_flags & ATA_DRIVE_UDMA) ||
			    !(chp->ch_drive[1].drive_flags & ATA_DRIVE_DMA)) &&
			    (!(chp->ch_drive[1].drive_flags & ATA_DRIVE_UDMA) ||
			    (chp->ch_drive[0].drive_flags & ATA_DRIVE_UDMA) ||
			    !(chp->ch_drive[0].drive_flags & ATA_DRIVE_DMA)))
				atapi = PDC262_ATAPI_UDMA;
		}
		bus_space_write_4(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC262_ATAPI(channel), atapi);
	}

	return error;
}
