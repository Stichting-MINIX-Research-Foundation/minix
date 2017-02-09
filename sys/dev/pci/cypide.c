/*	$NetBSD: cypide.c,v 1.30 2013/10/07 19:51:55 jakllsch Exp $	*/

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
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cypide.c,v 1.30 2013/10/07 19:51:55 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_cy693_reg.h>
#include <dev/pci/cy82c693var.h>

static void cy693_chip_map(struct pciide_softc*, const struct pci_attach_args*);
static void cy693_setup_channel(struct ata_channel*);

static int  cypide_match(device_t, cfdata_t, void *);
static void cypide_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(cypide, sizeof(struct pciide_softc),
    cypide_match, cypide_attach, pciide_detach, NULL);

static const struct pciide_product_desc pciide_cypress_products[] =  {
	{ PCI_PRODUCT_CONTAQ_82C693,
	  IDE_16BIT_IOSPACE,
	  "Cypress 82C693 IDE Controller",
	  cy693_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

static int
cypide_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_CONTAQ &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_MASS_STORAGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_IDE) {
		if (pciide_lookup_product(pa->pa_id, pciide_cypress_products))
			return (2);
	}
	return (0);
}

static void
cypide_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_cypress_products));

}

static void
cy693_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);

	if (pciide_chipen(sc, pa) == 0)
		return;

	/*
	 * this chip has 2 PCI IDE functions, one for primary and one for
	 * secondary. So we need to call pciide_mapregs_compat() with
	 * the real channel
	 */
	if (pa->pa_function == 1) {
		sc->sc_cy_compatchan = 0;
	} else if (pa->pa_function == 2) {
		sc->sc_cy_compatchan = 1;
	} else {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "unexpected PCI function %d\n", pa->pa_function);
		return;
	}
	if (interface & PCIIDE_INTERFACE_BUS_MASTER_DMA) {
		aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "bus-master DMA support present\n");
		pciide_mapreg_dma(sc, pa);
	} else {
		aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "hardware does not support DMA\n");
		sc->sc_dma_ok = 0;
	}

	sc->sc_cy_handle = cy82c693_init(pa->pa_iot);
	if (sc->sc_cy_handle == NULL) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "unable to map hyperCache control registers\n");
		sc->sc_dma_ok = 0;
	}

	sc->sc_wdcdev.sc_atac.atac_cap = ATAC_CAP_DATA16 | ATAC_CAP_DATA32;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;
	sc->sc_wdcdev.sc_atac.atac_set_modes = cy693_setup_channel;

	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = 1;
	sc->sc_wdcdev.wdc_maxdrives = 2;

	wdc_allocate_regs(&sc->sc_wdcdev);

	/* Only one channel for this chip; if we are here it's enabled */
	cp = &sc->pciide_channels[0];
	sc->wdc_chanarray[0] = &cp->ata_channel;
	cp->name = PCIIDE_CHANNEL_NAME(0);
	cp->ata_channel.ch_channel = 0;
	cp->ata_channel.ch_atac = &sc->sc_wdcdev.sc_atac;
	cp->ata_channel.ch_queue =
	    malloc(sizeof(struct ata_queue), M_DEVBUF, M_NOWAIT);
	if (cp->ata_channel.ch_queue == NULL) {
		aprint_error("%s primary channel: "
		    "can't allocate memory for command queue",
		    device_xname(sc->sc_wdcdev.sc_atac.atac_dev));
		return;
	}
	aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "primary channel %s to ",
	    (interface & PCIIDE_INTERFACE_SETTABLE(0)) ?
	    "configured" : "wired");
	if (interface & PCIIDE_INTERFACE_PCI(0)) {
		aprint_normal("native-PCI mode\n");
		pciide_mapregs_native(pa, cp, pciide_pci_intr);
	} else {
		aprint_normal("compatibility mode\n");
		pciide_mapregs_compat(pa, cp, sc->sc_cy_compatchan);
		if ((cp->ata_channel.ch_flags & ATACH_DISABLED) == 0)
			pciide_map_compat_intr(pa, cp, sc->sc_cy_compatchan);
	}
	wdcattach(&cp->ata_channel);
}

static void
cy693_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	int drive;
	u_int32_t cy_cmd_ctrl;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	int dma_mode = -1;

	ATADEBUG_PRINT(("cy693_chip_map: old timings reg 0x%x\n",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, CY_CMD_CTRL)),DEBUG_PROBE);

	cy_cmd_ctrl = idedma_ctl = 0;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;
		/* add timing values, setup DMA if needed */
		if (drvp->drive_flags & ATA_DRIVE_DMA) {
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
			/* use Multiword DMA */
			if (dma_mode == -1 || dma_mode > drvp->DMA_mode)
				dma_mode = drvp->DMA_mode;
		}
		cy_cmd_ctrl |= (cy_pio_pulse[drvp->PIO_mode] <<
		    CY_CMD_CTRL_IOW_PULSE_OFF(drive));
		cy_cmd_ctrl |= (cy_pio_rec[drvp->PIO_mode] <<
		    CY_CMD_CTRL_IOW_REC_OFF(drive));
		cy_cmd_ctrl |= (cy_pio_pulse[drvp->PIO_mode] <<
		    CY_CMD_CTRL_IOR_PULSE_OFF(drive));
		cy_cmd_ctrl |= (cy_pio_rec[drvp->PIO_mode] <<
		    CY_CMD_CTRL_IOR_REC_OFF(drive));
	}
	pci_conf_write(sc->sc_pc, sc->sc_tag, CY_CMD_CTRL, cy_cmd_ctrl);
	chp->ch_drive[0].DMA_mode = dma_mode;
	chp->ch_drive[1].DMA_mode = dma_mode;

	if (dma_mode == -1)
		dma_mode = 0;

	if (sc->sc_cy_handle != NULL) {
		/* Note: `multiple' is implied. */
		cy82c693_write(sc->sc_cy_handle,
		    (sc->sc_cy_compatchan == 0) ?
		    CY_DMA_IDX_PRIMARY : CY_DMA_IDX_SECONDARY, dma_mode);
	}

	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
		    idedma_ctl);
	}
	ATADEBUG_PRINT(("cy693_chip_map: new timings reg 0x%x\n",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, CY_CMD_CTRL)), DEBUG_PROBE);
}
