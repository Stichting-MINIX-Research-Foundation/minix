/*	$NetBSD: cmdide.c,v 1.38 2012/09/03 15:38:17 kiyohara Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: cmdide.c,v 1.38 2012/09/03 15:38:17 kiyohara Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_cmd_reg.h>


static int  cmdide_match(device_t, cfdata_t, void *);
static void cmdide_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(cmdide, sizeof(struct pciide_softc),
    cmdide_match, cmdide_attach, pciide_detach, NULL);

static void cmd_chip_map(struct pciide_softc*, const struct pci_attach_args*);
static void cmd0643_9_chip_map(struct pciide_softc*,
			       const struct pci_attach_args*);
static void cmd0643_9_setup_channel(struct ata_channel*);
static void cmd_channel_map(const struct pci_attach_args *,
			    struct pciide_softc *, int);
static int  cmd_pci_intr(void *);
static void cmd646_9_irqack(struct ata_channel *);
static void cmd680_chip_map(struct pciide_softc*,
			    const struct pci_attach_args*);
static void cmd680_setup_channel(struct ata_channel*);
static void cmd680_channel_map(const struct pci_attach_args *,
			       struct pciide_softc *, int);

static const struct pciide_product_desc pciide_cmd_products[] =  {
	{ PCI_PRODUCT_CMDTECH_640,
	  0,
	  "CMD Technology PCI0640",
	  cmd_chip_map
	},
	{ PCI_PRODUCT_CMDTECH_643,
	  0,
	  "CMD Technology PCI0643",
	  cmd0643_9_chip_map,
	},
	{ PCI_PRODUCT_CMDTECH_646,
	  0,
	  "CMD Technology PCI0646",
	  cmd0643_9_chip_map,
	},
	{ PCI_PRODUCT_CMDTECH_648,
	  0,
	  "CMD Technology PCI0648",
	  cmd0643_9_chip_map,
	},
	{ PCI_PRODUCT_CMDTECH_649,
	  0,
	  "CMD Technology PCI0649",
	  cmd0643_9_chip_map,
	},
	{ PCI_PRODUCT_CMDTECH_680,
	  0,
	  "Silicon Image 0680",
	  cmd680_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

static int
cmdide_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_CMDTECH) {
		if (pciide_lookup_product(pa->pa_id, pciide_cmd_products))
			return (2);
	}
	return (0);
}

static void
cmdide_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_cmd_products));

}

static void
cmd_channel_map(const struct pci_attach_args *pa, struct pciide_softc *sc,
    int channel)
{
	struct pciide_channel *cp = &sc->pciide_channels[channel];
	u_int8_t ctrl = pciide_pci_read(sc->sc_pc, sc->sc_tag, CMD_CTRL);
	int interface, one_channel;

	/*
	 * The 0648/0649 can be told to identify as a RAID controller.
	 * In this case, we have to fake interface
	 */
	if (PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_MASS_STORAGE_IDE) {
		interface = PCIIDE_INTERFACE_SETTABLE(0) |
		    PCIIDE_INTERFACE_SETTABLE(1);
		if (pciide_pci_read(pa->pa_pc, pa->pa_tag, CMD_CONF) &
		    CMD_CONF_DSA1)
			interface |= PCIIDE_INTERFACE_PCI(0) |
			    PCIIDE_INTERFACE_PCI(1);
	} else {
		interface = PCI_INTERFACE(pa->pa_class);
	}

	sc->wdc_chanarray[channel] = &cp->ata_channel;
	cp->name = PCIIDE_CHANNEL_NAME(channel);
	cp->ata_channel.ch_channel = channel;
	cp->ata_channel.ch_atac = &sc->sc_wdcdev.sc_atac;

	/*
	 * Older CMD64X doesn't have independent channels
	 */
	switch (sc->sc_pp->ide_product) {
	case PCI_PRODUCT_CMDTECH_649:
		one_channel = 0;
		break;
	default:
		one_channel = 1;
		break;
	}

	if (channel > 0 && one_channel) {
		cp->ata_channel.ch_queue =
		    sc->pciide_channels[0].ata_channel.ch_queue;
	} else {
		cp->ata_channel.ch_queue =
		    malloc(sizeof(struct ata_queue), M_DEVBUF, M_NOWAIT);
	}
	if (cp->ata_channel.ch_queue == NULL) {
		aprint_error("%s %s channel: "
		    "can't allocate memory for command queue",
		    device_xname(sc->sc_wdcdev.sc_atac.atac_dev), cp->name);
		    return;
	}

	aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "%s channel %s to %s mode\n", cp->name,
	    (interface & PCIIDE_INTERFACE_SETTABLE(channel)) ?
	    "configured" : "wired",
	    (interface & PCIIDE_INTERFACE_PCI(channel)) ?
	    "native-PCI" : "compatibility");

	/*
	 * with a CMD PCI64x, if we get here, the first channel is enabled:
	 * there's no way to disable the first channel without disabling
	 * the whole device
	 */
	if (channel != 0 && (ctrl & CMD_CTRL_2PORT) == 0) {
		aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "%s channel ignored (disabled)\n", cp->name);
		cp->ata_channel.ch_flags |= ATACH_DISABLED;
		return;
	}

	pciide_mapchan(pa, cp, interface, cmd_pci_intr);
}

static int
cmd_pci_intr(void *arg)
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct ata_channel *wdc_cp;
	int i, rv, crv;
	u_int32_t priirq, secirq;

	rv = 0;
	priirq = pciide_pci_read(sc->sc_pc, sc->sc_tag, CMD_CONF);
	secirq = pciide_pci_read(sc->sc_pc, sc->sc_tag, CMD_ARTTIM23);
	for (i = 0; i < sc->sc_wdcdev.sc_atac.atac_nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->ata_channel;
		/* If a compat channel skip. */
		if (cp->compat)
			continue;
		if ((i == 0 && (priirq & CMD_CONF_DRV0_INTR)) ||
		    (i == 1 && (secirq & CMD_ARTTIM23_IRQ))) {
			crv = wdcintr(wdc_cp);
			if (crv == 0) {
				aprint_error("%s:%d: bogus intr\n",
				    device_xname(
				      sc->sc_wdcdev.sc_atac.atac_dev), i);
				sc->sc_wdcdev.irqack(wdc_cp);
			} else
				rv = 1;
		}
	}
	return rv;
}

static void
cmd_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	int channel;

	/*
	 * For a CMD PCI064x, the use of PCI_COMMAND_IO_ENABLE
	 * and base addresses registers can be disabled at
	 * hardware level. In this case, the device is wired
	 * in compat mode and its first channel is always enabled,
	 * but we can't rely on PCI_COMMAND_IO_ENABLE.
	 * In fact, it seems that the first channel of the CMD PCI0640
	 * can't be disabled.
	 */

#ifdef PCIIDE_CMD064x_DISABLE
	if (pciide_chipen(sc, pa) == 0)
		return;
#endif

	aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "hardware does not support DMA\n");
	sc->sc_dma_ok = 0;

	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.sc_atac.atac_cap = ATAC_CAP_DATA16;
	sc->sc_wdcdev.wdc_maxdrives = 2;

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cmd_channel_map(pa, sc, channel);
	}
}

static void
cmd0643_9_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	int channel;
	pcireg_t rev = PCI_REVISION(pa->pa_class);

	/*
	 * For a CMD PCI064x, the use of PCI_COMMAND_IO_ENABLE
	 * and base addresses registers can be disabled at
	 * hardware level. In this case, the device is wired
	 * in compat mode and its first channel is always enabled,
	 * but we can't rely on PCI_COMMAND_IO_ENABLE.
	 * In fact, it seems that the first channel of the CMD PCI0640
	 * can't be disabled.
	 */

#ifdef PCIIDE_CMD064x_DISABLE
	if (pciide_chipen(sc, pa) == 0)
		return;
#endif

	aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "bus-master DMA support present");
	pciide_mapreg_dma(sc, pa);
	aprint_verbose("\n");
	sc->sc_wdcdev.sc_atac.atac_cap = ATAC_CAP_DATA16 | ATAC_CAP_DATA32;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA;
		switch (sc->sc_pp->ide_product) {
		case PCI_PRODUCT_CMDTECH_649:
			sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_UDMA;
			sc->sc_wdcdev.sc_atac.atac_udma_cap = 5;
			sc->sc_wdcdev.irqack = cmd646_9_irqack;
			break;
		case PCI_PRODUCT_CMDTECH_648:
			sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_UDMA;
			sc->sc_wdcdev.sc_atac.atac_udma_cap = 4;
			sc->sc_wdcdev.irqack = cmd646_9_irqack;
			break;
		case PCI_PRODUCT_CMDTECH_646:
			if (rev >= CMD0646U2_REV) {
				sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_UDMA;
				sc->sc_wdcdev.sc_atac.atac_udma_cap = 2;
			} else if (rev >= CMD0646U_REV) {
			/*
			 * Linux's driver claims that the 646U is broken
			 * with UDMA. Only enable it if we know what we're
			 * doing
			 */
#ifdef PCIIDE_CMD0646U_ENABLEUDMA
				sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_UDMA;
				sc->sc_wdcdev.sc_atac.atac_udma_cap = 2;
#endif
				/* explicitly disable UDMA */
				pciide_pci_write(sc->sc_pc, sc->sc_tag,
				    CMD_UDMATIM(0), 0);
				pciide_pci_write(sc->sc_pc, sc->sc_tag,
				    CMD_UDMATIM(1), 0);
			}
			sc->sc_wdcdev.irqack = cmd646_9_irqack;
			break;
		default:
			sc->sc_wdcdev.irqack = pciide_irqack;
		}
	}

	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;
	sc->sc_wdcdev.sc_atac.atac_set_modes = cmd0643_9_setup_channel;
	sc->sc_wdcdev.wdc_maxdrives = 2;

	ATADEBUG_PRINT(("cmd0643_9_chip_map: old timings reg 0x%x 0x%x\n",
		pci_conf_read(sc->sc_pc, sc->sc_tag, 0x54),
		pci_conf_read(sc->sc_pc, sc->sc_tag, 0x58)),
		DEBUG_PROBE);

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++)
		cmd_channel_map(pa, sc, channel);

	/*
	 * note - this also makes sure we clear the irq disable and reset
	 * bits
	 */
	pciide_pci_write(sc->sc_pc, sc->sc_tag, CMD_DMA_MODE, CMD_DMA_MULTIPLE);
	ATADEBUG_PRINT(("cmd0643_9_chip_map: timings reg now 0x%x 0x%x\n",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, 0x54),
	    pci_conf_read(sc->sc_pc, sc->sc_tag, 0x58)),
	    DEBUG_PROBE);
}

static void
cmd0643_9_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	u_int8_t tim;
	u_int32_t idedma_ctl, udma_reg;
	int drive, s;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);

	idedma_ctl = 0;
	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;
		/* add timing values, setup DMA if needed */
		tim = cmd0643_9_data_tim_pio[drvp->PIO_mode];
		if (drvp->drive_flags & (ATA_DRIVE_DMA | ATA_DRIVE_UDMA)) {
			if (drvp->drive_flags & ATA_DRIVE_UDMA) {
				/* UltraDMA on a 646U2, 0648 or 0649 */
				s = splbio();
				drvp->drive_flags &= ~ATA_DRIVE_DMA;
				splx(s);
				udma_reg = pciide_pci_read(sc->sc_pc,
				    sc->sc_tag, CMD_UDMATIM(chp->ch_channel));
				if (drvp->UDMA_mode > 2 &&
				    (pciide_pci_read(sc->sc_pc, sc->sc_tag,
				    CMD_BICSR) &
				    CMD_BICSR_80(chp->ch_channel)) == 0)
					drvp->UDMA_mode = 2;
				if (drvp->UDMA_mode > 2)
					udma_reg &= ~CMD_UDMATIM_UDMA33(drive);
				else if (sc->sc_wdcdev.sc_atac.atac_udma_cap > 2)
					udma_reg |= CMD_UDMATIM_UDMA33(drive);
				udma_reg |= CMD_UDMATIM_UDMA(drive);
				udma_reg &= ~(CMD_UDMATIM_TIM_MASK <<
				    CMD_UDMATIM_TIM_OFF(drive));
				udma_reg |=
				    (cmd0646_9_tim_udma[drvp->UDMA_mode] <<
				    CMD_UDMATIM_TIM_OFF(drive));
				pciide_pci_write(sc->sc_pc, sc->sc_tag,
				    CMD_UDMATIM(chp->ch_channel), udma_reg);
			} else {
				/*
				 * use Multiword DMA.
				 * Timings will be used for both PIO and DMA,
				 * so adjust DMA mode if needed
				 * if we have a 0646U2/8/9, turn off UDMA
				 */
				if (sc->sc_wdcdev.sc_atac.atac_cap & ATAC_CAP_UDMA) {
					udma_reg = pciide_pci_read(sc->sc_pc,
					    sc->sc_tag,
					    CMD_UDMATIM(chp->ch_channel));
					udma_reg &= ~CMD_UDMATIM_UDMA(drive);
					pciide_pci_write(sc->sc_pc, sc->sc_tag,
					    CMD_UDMATIM(chp->ch_channel),
					    udma_reg);
				}
				if (drvp->PIO_mode >= 3 &&
				    (drvp->DMA_mode + 2) > drvp->PIO_mode) {
					drvp->DMA_mode = drvp->PIO_mode - 2;
				}
				tim = cmd0643_9_data_tim_dma[drvp->DMA_mode];
			}
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		}
		pciide_pci_write(sc->sc_pc, sc->sc_tag,
		    CMD_DATA_TIM(chp->ch_channel, drive), tim);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
		    idedma_ctl);
	}
}

static void
cmd646_9_irqack(struct ata_channel *chp)
{
	u_int32_t priirq, secirq;
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);

	if (chp->ch_channel == 0) {
		priirq = pciide_pci_read(sc->sc_pc, sc->sc_tag, CMD_CONF);
		pciide_pci_write(sc->sc_pc, sc->sc_tag, CMD_CONF, priirq);
	} else {
		secirq = pciide_pci_read(sc->sc_pc, sc->sc_tag, CMD_ARTTIM23);
		pciide_pci_write(sc->sc_pc, sc->sc_tag, CMD_ARTTIM23, secirq);
	}
	pciide_irqack(chp);
}

static void
cmd680_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	int channel;

	if (pciide_chipen(sc, pa) == 0)
		return;

	aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "bus-master DMA support present");
	pciide_mapreg_dma(sc, pa);
	aprint_verbose("\n");
	sc->sc_wdcdev.sc_atac.atac_cap = ATAC_CAP_DATA16 | ATAC_CAP_DATA32;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA;
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_UDMA;
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 6;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}

	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;
	sc->sc_wdcdev.sc_atac.atac_set_modes = cmd680_setup_channel;
	sc->sc_wdcdev.wdc_maxdrives = 2;

	pciide_pci_write(sc->sc_pc, sc->sc_tag, 0x80, 0x00);
	pciide_pci_write(sc->sc_pc, sc->sc_tag, 0x84, 0x00);
	pciide_pci_write(sc->sc_pc, sc->sc_tag, 0x8a,
	    pciide_pci_read(sc->sc_pc, sc->sc_tag, 0x8a) | 0x01);

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++)
		cmd680_channel_map(pa, sc, channel);
}

static void
cmd680_channel_map(const struct pci_attach_args *pa, struct pciide_softc *sc,
    int channel)
{
	struct pciide_channel *cp = &sc->pciide_channels[channel];
	int interface, i, reg;
	static const u_int8_t init_val[] =
	    {             0x8a, 0x32, 0x8a, 0x32, 0x8a, 0x32,
	      0x92, 0x43, 0x92, 0x43, 0x09, 0x40, 0x09, 0x40 };

	if (PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_MASS_STORAGE_IDE) {
		interface = PCIIDE_INTERFACE_SETTABLE(0) |
		    PCIIDE_INTERFACE_SETTABLE(1);
		interface |= PCIIDE_INTERFACE_PCI(0) |
		    PCIIDE_INTERFACE_PCI(1);
	} else {
		interface = PCI_INTERFACE(pa->pa_class);
	}

	sc->wdc_chanarray[channel] = &cp->ata_channel;
	cp->name = PCIIDE_CHANNEL_NAME(channel);
	cp->ata_channel.ch_channel = channel;
	cp->ata_channel.ch_atac = &sc->sc_wdcdev.sc_atac;

	cp->ata_channel.ch_queue =
	    malloc(sizeof(struct ata_queue), M_DEVBUF, M_NOWAIT);
	if (cp->ata_channel.ch_queue == NULL) {
		aprint_error("%s %s channel: "
		    "can't allocate memory for command queue",
		    device_xname(sc->sc_wdcdev.sc_atac.atac_dev), cp->name);
		    return;
	}

	/* XXX */
	reg = 0xa2 + channel * 16;
	for (i = 0; i < sizeof(init_val); i++)
		pciide_pci_write(sc->sc_pc, sc->sc_tag, reg + i, init_val[i]);

	aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "%s channel %s to %s mode\n", cp->name,
	    (interface & PCIIDE_INTERFACE_SETTABLE(channel)) ?
	    "configured" : "wired",
	    (interface & PCIIDE_INTERFACE_PCI(channel)) ?
	    "native-PCI" : "compatibility");

	pciide_mapchan(pa, cp, interface, pciide_pci_intr);
}

static void
cmd680_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	u_int8_t mode, off, scsc;
	u_int16_t val;
	u_int32_t idedma_ctl;
	int drive, s;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	pci_chipset_tag_t pc = sc->sc_pc;
	pcitag_t pa = sc->sc_tag;
	static const u_int8_t udma2_tbl[] =
	    { 0x0f, 0x0b, 0x07, 0x06, 0x03, 0x02, 0x01 };
	static const u_int8_t udma_tbl[] =
	    { 0x0c, 0x07, 0x05, 0x04, 0x02, 0x01, 0x00 };
	static const u_int16_t dma_tbl[] =
	    { 0x2208, 0x10c2, 0x10c1 };
	static const u_int16_t pio_tbl[] =
	    { 0x328a, 0x2283, 0x1104, 0x10c3, 0x10c1 };

	idedma_ctl = 0;
	pciide_channel_dma_setup(cp);
	mode = pciide_pci_read(pc, pa, 0x80 + chp->ch_channel * 4);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;
		mode &= ~(0x03 << (drive * 4));
		if (drvp->drive_flags & ATA_DRIVE_UDMA) {
			s = splbio();
			drvp->drive_flags &= ~ATA_DRIVE_DMA;
			splx(s);
			off = 0xa0 + chp->ch_channel * 16;
			if (drvp->UDMA_mode > 2 &&
			    (pciide_pci_read(pc, pa, off) & 0x01) == 0)
				drvp->UDMA_mode = 2;
			scsc = pciide_pci_read(pc, pa, 0x8a);
			if (drvp->UDMA_mode == 6 && (scsc & 0x30) == 0) {
				pciide_pci_write(pc, pa, 0x8a, scsc | 0x01);
				scsc = pciide_pci_read(pc, pa, 0x8a);
				if ((scsc & 0x30) == 0)
					drvp->UDMA_mode = 5;
			}
			mode |= 0x03 << (drive * 4);
			off = 0xac + chp->ch_channel * 16 + drive * 2;
			val = pciide_pci_read(pc, pa, off) & ~0x3f;
			if (scsc & 0x30)
				val |= udma2_tbl[drvp->UDMA_mode];
			else
				val |= udma_tbl[drvp->UDMA_mode];
			pciide_pci_write(pc, pa, off, val);
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else if (drvp->drive_flags & ATA_DRIVE_DMA) {
			mode |= 0x02 << (drive * 4);
			off = 0xa8 + chp->ch_channel * 16 + drive * 2;
			val = dma_tbl[drvp->DMA_mode];
			pciide_pci_write(pc, pa, off, val & 0xff);
			pciide_pci_write(pc, pa, off+1, val >> 8);
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else {
			mode |= 0x01 << (drive * 4);
			off = 0xa4 + chp->ch_channel * 16 + drive * 2;
			val = pio_tbl[drvp->PIO_mode];
			pciide_pci_write(pc, pa, off, val & 0xff);
			pciide_pci_write(pc, pa, off+1, val >> 8);
		}
	}

	pciide_pci_write(pc, pa, 0x80 + chp->ch_channel * 4, mode);
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
		    idedma_ctl);
	}
}
