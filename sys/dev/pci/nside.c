/*	$NetBSD: nside.c,v 1.9 2013/10/07 19:51:55 jakllsch Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: nside.c,v 1.9 2013/10/07 19:51:55 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_natsemi_reg.h>

static void natsemi_chip_map(struct pciide_softc *,
    const struct pci_attach_args *);
static void natsemi_setup_channel(struct ata_channel *);
static int  natsemi_pci_intr(void *);
static void natsemi_irqack(struct ata_channel *);

static int  nside_match(device_t, cfdata_t, void *);
static void nside_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(nside, sizeof(struct pciide_softc),
    nside_match, nside_attach, pciide_detach, NULL);

static const struct pciide_product_desc pciide_natsemi_products[] =  {
	{ PCI_PRODUCT_NS_PC87415,       /* National Semi PC87415 IDE */
	  0,
	  "National Semiconductor PC87415 IDE Controller",
          natsemi_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

static int
nside_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_NS &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_MASS_STORAGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_IDE) {
		if (pciide_lookup_product(pa->pa_id, pciide_natsemi_products))
			return 2;
	}
	return 0;
}

static void
nside_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_natsemi_products));
}

static void
natsemi_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	int channel;
	pcireg_t interface, ctl;

	if (pciide_chipen(sc, pa) == 0)
		return;

	aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "bus-master DMA support present");
	pciide_mapreg_dma(sc, pa);
	aprint_verbose("\n");

	sc->sc_wdcdev.sc_atac.atac_cap = ATAC_CAP_DATA16;

	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA;
		sc->sc_wdcdev.irqack = natsemi_irqack;
	}

	pciide_pci_write(sc->sc_pc, sc->sc_tag, NATSEMI_CCBT, 0xb7);

	/*
	 * Mask off interrupts from both channels, appropriate channel(s)
	 * will be unmasked later.
	 */
	pciide_pci_write(sc->sc_pc, sc->sc_tag, NATSEMI_CTRL2,
	    pciide_pci_read(sc->sc_pc, sc->sc_tag, NATSEMI_CTRL2) |
	    NATSEMI_CHMASK(0) | NATSEMI_CHMASK(1));

	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;
	sc->sc_wdcdev.sc_atac.atac_set_modes = natsemi_setup_channel;
	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.wdc_maxdrives = 2;

        interface = PCI_INTERFACE(pa->pa_class);
	interface &= ~PCIIDE_CHANSTATUS_EN;	/* Reserved on PC87415 */

	/* If we're in PCIIDE mode, unmask INTA, otherwise mask it. */
	ctl = pciide_pci_read(sc->sc_pc, sc->sc_tag, NATSEMI_CTRL1);
	if (interface & (PCIIDE_INTERFACE_PCI(0) | PCIIDE_INTERFACE_PCI(1)))
		ctl &= ~NATSEMI_CTRL1_INTAMASK;
	else
		ctl |= NATSEMI_CTRL1_INTAMASK;
	pciide_pci_write(sc->sc_pc, sc->sc_tag, NATSEMI_CTRL1, ctl);

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;

		pciide_mapchan(pa, cp, interface, natsemi_pci_intr);

		pciide_pci_write(sc->sc_pc, sc->sc_tag, NATSEMI_CTRL2,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, NATSEMI_CTRL2) &
		    ~(NATSEMI_CHMASK(channel)));
	}
}

void
natsemi_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	int drive, ndrives = 0;
	uint32_t idedma_ctl = 0;
        struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
        struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	uint8_t tim;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;

		ndrives++;
		/* add timing values, setup DMA if needed */
		if ((drvp->drive_flags & ATA_DRIVE_DMA) == 0) {
			tim = natsemi_pio_pulse[drvp->PIO_mode] |
			    (natsemi_pio_recover[drvp->PIO_mode] << 4);
		} else {
			/*
			 * use Multiword DMA
			 * Timings will be used for both PIO and DMA,
			 * so adjust DMA mode if needed
			 */
			if (drvp->PIO_mode >= 3 &&
			    (drvp->DMA_mode + 2) > drvp->PIO_mode) {
				drvp->DMA_mode = drvp->PIO_mode - 2;
			}
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
			tim = natsemi_dma_pulse[drvp->DMA_mode] |
			    (natsemi_dma_recover[drvp->DMA_mode] << 4);

		}

		pciide_pci_write(sc->sc_pc, sc->sc_tag,
		    NATSEMI_RTREG(chp->ch_channel, drive), tim);
		pciide_pci_write(sc->sc_pc, sc->sc_tag,
		    NATSEMI_WTREG(chp->ch_channel, drive), tim);
	}

	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
		    idedma_ctl);

	}
	/* Go ahead and ack interrupts generated during probe. */
	natsemi_irqack(chp);
}

void
natsemi_irqack(struct ata_channel *chp)
{
        struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
        struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	uint8_t clr;

	/* Errata: The "clear" bits are in the wrong register *sigh* */
	clr = bus_space_read_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CMD], 0);
	clr |= bus_space_read_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0) &
	    (IDEDMA_CTL_ERR | IDEDMA_CTL_INTR);
	bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CMD], 0, clr);
}

int
natsemi_pci_intr(void *arg)
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct ata_channel *wdc_cp;
	int i, rv, crv;
	uint8_t msk;

	rv = 0;
	msk = pciide_pci_read(sc->sc_pc, sc->sc_tag, NATSEMI_CTRL2);
	for (i = 0; i <  sc->sc_wdcdev.sc_atac.atac_nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->ata_channel;

		/* If a compat channel skip. */
		if (cp->compat)
			continue;

		/* If this channel is masked, skip it. */
		if (msk & NATSEMI_CHMASK(i))
			continue;

		crv = wdcintr(wdc_cp);
		if (crv == 0)
			;	/* leave alone */
		else if (crv == 1)
			rv = 1;		/* claim the intr */
		else if (rv == 0)	/* crv should be -1 in this case */
			rv = crv;	/* if we've done no better, take it */
	}
	return (rv);
}
