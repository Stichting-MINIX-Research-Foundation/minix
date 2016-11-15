/*	$NetBSD: siside.c,v 1.36 2014/05/31 16:25:29 christos Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: siside.c,v 1.36 2014/05/31 16:25:29 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_sis_reg.h>

static void sis_chip_map(struct pciide_softc *, const struct pci_attach_args *);
static void sis_sata_chip_map(struct pciide_softc *,
    const struct pci_attach_args *);
static void sis_setup_channel(struct ata_channel *);
static void sis96x_setup_channel(struct ata_channel *);

static int  sis_hostbr_match(const struct pci_attach_args *);
static int  sis_south_match(const struct pci_attach_args *);

static int  siside_match(device_t, cfdata_t, void *);
static void siside_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(siside, sizeof(struct pciide_softc),
    siside_match, siside_attach, pciide_detach, NULL);

static const struct pciide_product_desc pciide_sis_products[] =  {
	{ PCI_PRODUCT_SIS_5597_IDE,
	  0,
	  NULL,
	  sis_chip_map,
	},
	{ PCI_PRODUCT_SIS_180_SATA,
	  0,
	  NULL,
	  sis_sata_chip_map,
	},
	{ PCI_PRODUCT_SIS_181_SATA,
	  0,
	  NULL,
	  sis_sata_chip_map,
	},
	{ PCI_PRODUCT_SIS_182_SATA,
	  0,
	  NULL,
	  sis_sata_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

static int
siside_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SIS) {
		if (pciide_lookup_product(pa->pa_id, pciide_sis_products))
			return (2);
	}
	return (0);
}

static void
siside_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	pcireg_t csr;

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_sis_products));

	csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	if (csr & PCI_COMMAND_INTERRUPT_DISABLE) {
		csr &= ~PCI_COMMAND_INTERRUPT_DISABLE;
		pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr);
	}
}

static struct sis_hostbr_type {
	u_int16_t id;
	u_int8_t rev;
	u_int8_t udma_mode;
	const char *name;
	u_int8_t type;
#define SIS_TYPE_NOUDMA	0
#define SIS_TYPE_66	1
#define SIS_TYPE_100OLD	2
#define SIS_TYPE_100NEW 3
#define SIS_TYPE_133OLD 4
#define SIS_TYPE_133NEW 5
#define SIS_TYPE_SOUTH	6
} sis_hostbr_type[] = {
	/* Most infos here are from sos@freebsd.org */
	{PCI_PRODUCT_SIS_530HB, 0x00, 4, "530", SIS_TYPE_66},
#if 0
	/*
	 * controllers associated to a rev 0x2 530 Host to PCI Bridge
	 * have problems with UDMA (info provided by Christos)
	 */
	{PCI_PRODUCT_SIS_530HB, 0x02, 0, "530 (buggy)", SIS_TYPE_NOUDMA},
#endif
	{PCI_PRODUCT_SIS_540HB, 0x00, 4, "540", SIS_TYPE_66},
	{PCI_PRODUCT_SIS_550HB, 0x00, 4, "550", SIS_TYPE_66},
	{PCI_PRODUCT_SIS_620,   0x00, 4, "620", SIS_TYPE_66},
	{PCI_PRODUCT_SIS_630,   0x00, 4, "630", SIS_TYPE_66},
	{PCI_PRODUCT_SIS_630,   0x30, 5, "630S", SIS_TYPE_100NEW},
	{PCI_PRODUCT_SIS_633,   0x00, 5, "633", SIS_TYPE_100NEW},
	{PCI_PRODUCT_SIS_635,   0x00, 5, "635", SIS_TYPE_100NEW},
	{PCI_PRODUCT_SIS_640,   0x00, 4, "640", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_645,   0x00, 6, "645", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_646,   0x00, 6, "645DX", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_648,   0x00, 6, "648", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_650,   0x00, 6, "650", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_651,   0x00, 6, "651", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_652,   0x00, 6, "652", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_655,   0x00, 6, "655", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_658,   0x00, 6, "658", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_661,	0x00, 6, "661", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_730,   0x00, 5, "730", SIS_TYPE_100OLD},
	{PCI_PRODUCT_SIS_733,   0x00, 5, "733", SIS_TYPE_100NEW},
	{PCI_PRODUCT_SIS_735,   0x00, 5, "735", SIS_TYPE_100NEW},
	{PCI_PRODUCT_SIS_740,   0x00, 5, "740", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_741,   0x00, 5, "741", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_745,   0x00, 5, "745", SIS_TYPE_100NEW},
	{PCI_PRODUCT_SIS_746,   0x00, 6, "746", SIS_TYPE_100NEW},
	{PCI_PRODUCT_SIS_748,   0x00, 6, "748", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_750,   0x00, 6, "750", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_751,   0x00, 6, "751", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_752,   0x00, 6, "752", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_755,   0x00, 6, "755", SIS_TYPE_SOUTH},
	{PCI_PRODUCT_SIS_760,	0x00, 6, "760", SIS_TYPE_133NEW},
	/*
	 * From sos@freebsd.org: the 0x961 ID will never be found in real world
	 * {PCI_PRODUCT_SIS_961,   0x00, 6, "961", SIS_TYPE_133NEW},
	 */
	{PCI_PRODUCT_SIS_962,   0x00, 6, "962", SIS_TYPE_133NEW},
	{PCI_PRODUCT_SIS_963,   0x00, 6, "963", SIS_TYPE_133NEW},
	{PCI_PRODUCT_SIS_964,   0x00, 6, "964", SIS_TYPE_133NEW},
	{PCI_PRODUCT_SIS_965,   0x00, 6, "965", SIS_TYPE_133NEW},
};

static struct sis_hostbr_type *sis_hostbr_type_match;

static int
sis_hostbr_match(const struct pci_attach_args *pa)
{
	int i;
	pcireg_t id, masqid, reg;

	id = pa->pa_id;

	if (PCI_VENDOR(id) != PCI_VENDOR_SIS)
		return 0;
	if (PCI_PRODUCT(id) == PCI_PRODUCT_SIS_85C503) {
		reg = pci_conf_read(pa->pa_pc, pa->pa_tag, SIS96x_DETECT);
		pci_conf_write(pa->pa_pc, pa->pa_tag, SIS96x_DETECT,
		    reg | SIS96x_DETECT_MASQ);
		masqid = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_ID_REG);
		if (((PCI_PRODUCT(masqid) & 0xfff0) != 0x0960)
		    && (PCI_PRODUCT(masqid) != 0x0018)) {
			pci_conf_write(pa->pa_pc, pa->pa_tag, SIS96x_DETECT,
			    reg);
		} else {
			id = masqid;
		}
	}

	sis_hostbr_type_match = NULL;
	for (i = 0; i < __arraycount(sis_hostbr_type); i++) {
		if (PCI_PRODUCT(id) == sis_hostbr_type[i].id &&
		    PCI_REVISION(pa->pa_class) >= sis_hostbr_type[i].rev)
			sis_hostbr_type_match = &sis_hostbr_type[i];
	}
	return (sis_hostbr_type_match != NULL);
}

static int
sis_south_match(const struct pci_attach_args *pa)
{

	return (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SIS &&
		PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SIS_85C503 &&
		PCI_REVISION(pa->pa_class) >= 0x10);
}

static void
sis_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	int channel;
	u_int8_t sis_ctr0 = pciide_pci_read(sc->sc_pc, sc->sc_tag, SIS_CTRL0);
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	pcireg_t rev = PCI_REVISION(pa->pa_class);

	if (pciide_chipen(sc, pa) == 0)
		return;

	aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "Silicon Integrated Systems ");
	pci_find_device(NULL, sis_hostbr_match);
	if (sis_hostbr_type_match) {
		if (sis_hostbr_type_match->type == SIS_TYPE_SOUTH) {
			pciide_pci_write(sc->sc_pc, sc->sc_tag, SIS_REG_57,
			    pciide_pci_read(sc->sc_pc, sc->sc_tag,
			    SIS_REG_57) & 0x7f);
			if (PCI_PRODUCT(pci_conf_read(sc->sc_pc, sc->sc_tag,
			    PCI_ID_REG)) == SIS_PRODUCT_5518) {
				aprint_normal("96X UDMA%d ",
				    sis_hostbr_type_match->udma_mode);
				sc->sis_type = SIS_TYPE_133NEW;
				sc->sc_wdcdev.sc_atac.atac_udma_cap =
			    	    sis_hostbr_type_match->udma_mode;
			} else {
				if (pci_find_device(NULL, sis_south_match)) {
					sc->sis_type = SIS_TYPE_133OLD;
					sc->sc_wdcdev.sc_atac.atac_udma_cap =
				    	    sis_hostbr_type_match->udma_mode;
				} else {
					sc->sis_type = SIS_TYPE_100NEW;
					sc->sc_wdcdev.sc_atac.atac_udma_cap =
					    sis_hostbr_type_match->udma_mode;
				}
			}
		} else {
			sc->sis_type = sis_hostbr_type_match->type;
			sc->sc_wdcdev.sc_atac.atac_udma_cap =
		    	    sis_hostbr_type_match->udma_mode;
		}
		aprint_normal("%s", sis_hostbr_type_match->name);
	} else {
		aprint_normal("5597/5598");
		if (rev >= 0xd0) {
			sc->sc_wdcdev.sc_atac.atac_udma_cap = 2;
			sc->sis_type = SIS_TYPE_66;
		} else {
			sc->sc_wdcdev.sc_atac.atac_udma_cap = 0;
			sc->sis_type = SIS_TYPE_NOUDMA;
		}
	}
	aprint_normal(" IDE controller (rev. 0x%02x)\n",
	    PCI_REVISION(pa->pa_class));
	aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "bus-master DMA support present");
	pciide_mapreg_dma(sc, pa);
	aprint_verbose("\n");

	sc->sc_wdcdev.sc_atac.atac_cap = ATAC_CAP_DATA16 | ATAC_CAP_DATA32;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA;
		sc->sc_wdcdev.irqack = pciide_irqack;
		if (sc->sis_type >= SIS_TYPE_66)
			sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_UDMA;
	}

	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;

	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.wdc_maxdrives = 2;
	switch(sc->sis_type) {
	case SIS_TYPE_NOUDMA:
	case SIS_TYPE_66:
	case SIS_TYPE_100OLD:
		sc->sc_wdcdev.sc_atac.atac_set_modes = sis_setup_channel;
		pciide_pci_write(sc->sc_pc, sc->sc_tag, SIS_MISC,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, SIS_MISC) |
		    SIS_MISC_TIM_SEL | SIS_MISC_FIFO_SIZE | SIS_MISC_GTC);
		break;
	case SIS_TYPE_100NEW:
	case SIS_TYPE_133OLD:
		sc->sc_wdcdev.sc_atac.atac_set_modes = sis_setup_channel;
		pciide_pci_write(sc->sc_pc, sc->sc_tag, SIS_REG_49,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, SIS_REG_49) | 0x01);
		break;
	case SIS_TYPE_133NEW:
		sc->sc_wdcdev.sc_atac.atac_set_modes = sis96x_setup_channel;
		pciide_pci_write(sc->sc_pc, sc->sc_tag, SIS_REG_50,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, SIS_REG_50) & 0xf7);
		pciide_pci_write(sc->sc_pc, sc->sc_tag, SIS_REG_52,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, SIS_REG_52) & 0xf7);
		break;
	}

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		if ((channel == 0 && (sis_ctr0 & SIS_CTRL0_CHAN0_EN) == 0) ||
		    (channel == 1 && (sis_ctr0 & SIS_CTRL0_CHAN1_EN) == 0)) {
			aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "%s channel ignored (disabled)\n", cp->name);
			cp->ata_channel.ch_flags |= ATACH_DISABLED;
			continue;
		}
		pciide_mapchan(pa, cp, interface, pciide_pci_intr);
	}
}

static void
sis96x_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	int drive, s;
	u_int32_t sis_tim;
	u_int32_t idedma_ctl;
	int regtim;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);

	sis_tim = 0;
	idedma_ctl = 0;
	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		regtim = SIS_TIM133(
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, SIS_REG_57),
		    chp->ch_channel, drive);
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;
		/* add timing values, setup DMA if needed */
		if (drvp->drive_flags & ATA_DRIVE_UDMA) {
			/* use Ultra/DMA */
			s = splbio();
			drvp->drive_flags &= ~ATA_DRIVE_DMA;
			splx(s);
			if (pciide_pci_read(sc->sc_pc, sc->sc_tag,
			    SIS96x_REG_CBL(chp->ch_channel)) & SIS96x_REG_CBL_33) {
				if (drvp->UDMA_mode > 2)
					drvp->UDMA_mode = 2;
			}
			sis_tim |= sis_udma133new_tim[drvp->UDMA_mode];
			sis_tim |= sis_pio133new_tim[drvp->PIO_mode];
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else if (drvp->drive_flags & ATA_DRIVE_DMA) {
			/*
			 * use Multiword DMA
			 * Timings will be used for both PIO and DMA,
			 * so adjust DMA mode if needed
			 */
			if (drvp->PIO_mode > (drvp->DMA_mode + 2))
				drvp->PIO_mode = drvp->DMA_mode + 2;
			if (drvp->DMA_mode + 2 > (drvp->PIO_mode))
				drvp->DMA_mode = (drvp->PIO_mode > 2) ?
				    drvp->PIO_mode - 2 : 0;
			sis_tim |= sis_dma133new_tim[drvp->DMA_mode];
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else {
			sis_tim |= sis_pio133new_tim[drvp->PIO_mode];
		}
		ATADEBUG_PRINT(("sis96x_setup_channel: new timings reg for "
		    "channel %d drive %d: 0x%x (reg 0x%x)\n",
		    chp->ch_channel, drive, sis_tim, regtim), DEBUG_PROBE);
		pci_conf_write(sc->sc_pc, sc->sc_tag, regtim, sis_tim);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
		    idedma_ctl);
	}
}

static void
sis_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	int drive, s;
	u_int32_t sis_tim;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);

	ATADEBUG_PRINT(("sis_setup_channel: old timings reg for "
	    "channel %d 0x%x\n", chp->ch_channel,
	    pci_conf_read(sc->sc_pc, sc->sc_tag, SIS_TIM(chp->ch_channel))),
	    DEBUG_PROBE);
	sis_tim = 0;
	idedma_ctl = 0;
	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;
		/* add timing values, setup DMA if needed */
		if ((drvp->drive_flags & ATA_DRIVE_DMA) == 0 &&
		    (drvp->drive_flags & ATA_DRIVE_UDMA) == 0)
			goto pio;

		if (drvp->drive_flags & ATA_DRIVE_UDMA) {
			/* use Ultra/DMA */
			s = splbio();
			drvp->drive_flags &= ~ATA_DRIVE_DMA;
			splx(s);
			if (pciide_pci_read(sc->sc_pc, sc->sc_tag,
			    SIS_REG_CBL) & SIS_REG_CBL_33(chp->ch_channel)) {
				if (drvp->UDMA_mode > 2)
					drvp->UDMA_mode = 2;
			}
			switch (sc->sis_type) {
			case SIS_TYPE_66:
			case SIS_TYPE_100OLD:
				sis_tim |= sis_udma66_tim[drvp->UDMA_mode] <<
				    SIS_TIM66_UDMA_TIME_OFF(drive);
				break;
			case SIS_TYPE_100NEW:
				sis_tim |=
				    sis_udma100new_tim[drvp->UDMA_mode] <<
				    SIS_TIM100_UDMA_TIME_OFF(drive);
			case SIS_TYPE_133OLD:
				sis_tim |=
				    sis_udma133old_tim[drvp->UDMA_mode] <<
				    SIS_TIM100_UDMA_TIME_OFF(drive);
				break;
			default:
				aprint_error("unknown SiS IDE type %d\n",
				    sc->sis_type);
			}
		} else {
			/*
			 * use Multiword DMA
			 * Timings will be used for both PIO and DMA,
			 * so adjust DMA mode if needed
			 */
			if (drvp->PIO_mode > (drvp->DMA_mode + 2))
				drvp->PIO_mode = drvp->DMA_mode + 2;
			if (drvp->DMA_mode + 2 > (drvp->PIO_mode))
				drvp->DMA_mode = (drvp->PIO_mode > 2) ?
				    drvp->PIO_mode - 2 : 0;
			if (drvp->DMA_mode == 0)
				drvp->PIO_mode = 0;
		}
		idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
pio:		switch (sc->sis_type) {
		case SIS_TYPE_NOUDMA:
		case SIS_TYPE_66:
		case SIS_TYPE_100OLD:
			sis_tim |= sis_pio_act[drvp->PIO_mode] <<
			    SIS_TIM66_ACT_OFF(drive);
			sis_tim |= sis_pio_rec[drvp->PIO_mode] <<
			    SIS_TIM66_REC_OFF(drive);
			break;
		case SIS_TYPE_100NEW:
		case SIS_TYPE_133OLD:
			sis_tim |= sis_pio_act[drvp->PIO_mode] <<
			    SIS_TIM100_ACT_OFF(drive);
			sis_tim |= sis_pio_rec[drvp->PIO_mode] <<
			    SIS_TIM100_REC_OFF(drive);
			break;
		default:
			aprint_error("unknown SiS IDE type %d\n",
			    sc->sis_type);
		}
	}
	ATADEBUG_PRINT(("sis_setup_channel: new timings reg for "
	    "channel %d 0x%x\n", chp->ch_channel, sis_tim), DEBUG_PROBE);
	pci_conf_write(sc->sc_pc, sc->sc_tag, SIS_TIM(chp->ch_channel),
		       sis_tim);
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
		    idedma_ctl);
	}
}

static void
sis_sata_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	int channel;

	if (pciide_chipen(sc, pa) == 0)
		return;

	if (interface == 0) {
		ATADEBUG_PRINT(("sis_sata_chip_map interface == 0\n"),
		    DEBUG_PROBE);
		interface = PCIIDE_INTERFACE_BUS_MASTER_DMA |
		    PCIIDE_INTERFACE_PCI(0) | PCIIDE_INTERFACE_PCI(1);
	}

	aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "Silicon Integrated Systems 180/96X SATA controller "
	    "(rev. 0x%02x)\n", PCI_REVISION(pa->pa_class));

	aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "bus-master DMA support present");
	pciide_mapreg_dma(sc, pa);
	aprint_verbose("\n");

	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_UDMA | ATAC_CAP_DMA;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;
	sc->sc_wdcdev.sc_atac.atac_udma_cap = 6;

	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DATA16 | ATAC_CAP_DATA32;
	sc->sc_wdcdev.sc_atac.atac_set_modes = sata_setup_channel;
	sc->sc_wdcdev.wdc_maxdrives = 2;

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		pciide_mapchan(pa, cp, interface, pciide_pci_intr);
	}
}
