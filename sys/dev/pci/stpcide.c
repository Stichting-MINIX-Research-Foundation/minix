/*	$NetBSD: stpcide.c,v 1.27 2013/10/07 19:51:55 jakllsch Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tohru Nishimura.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: stpcide.c,v 1.27 2013/10/07 19:51:55 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>

static void stpc_chip_map(struct pciide_softc *,
    const struct pci_attach_args *);
static void stpc_setup_channel(struct ata_channel *);

static int  stpcide_match(device_t, cfdata_t, void *);
static void stpcide_attach(device_t, device_t, void *);

const struct pciide_product_desc pciide_stpc_products[] = {
	{ 0x0228,
	  0,
	  "STMicroelectronics STPC IDE Controller",
	  stpc_chip_map,
	},
	{ 0, 0, NULL, NULL },
};

CFATTACH_DECL_NEW(stpcide, sizeof(struct pciide_softc),
    stpcide_match, stpcide_attach, pciide_detach, NULL);

static int
stpcide_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SGSTHOMSON) {
		if (pciide_lookup_product(pa->pa_id, pciide_stpc_products))
			return (2);
	}
	return (0);
}

static void
stpcide_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_stpc_products));

}

static void
stpc_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	int channel;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);

	if (pciide_chipen(sc, pa) == 0)
		return;

	aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "bus-master DMA support present");
	pciide_mapreg_dma(sc, pa);
	aprint_verbose("\n");
	sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DATA16 | ATAC_CAP_DATA32;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;
	sc->sc_wdcdev.sc_atac.atac_udma_cap = 0;
	sc->sc_wdcdev.sc_atac.atac_set_modes = stpc_setup_channel;
	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;
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

/*
 * IDE timing register (0x40, 0x42, 0x44, and 0x46) assignment.
 * 33MHz PCI system will have;
 *	DMA0 01-11-11
 *	DMA1 00-01-10
 *	DMA2 00-00-10
 *	PIO0          111-100
 *	PIO1          100-011
 *	PIO2          011-010
 *	PIO3          010-001
 *	PIO4          000-001
 *	MISC                  XYZW
 */
static const u_int16_t dmatbl[] = { 0x7C00, 0x1800, 0x0800 };
static const u_int16_t piotbl[] = { 0x03C0, 0x0230, 0x01A0, 0x0110, 0x0010 };

static void
stpc_setup_channel(struct ata_channel *chp)
{
	struct atac_softc *atac = chp->ch_atac;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	int channel = chp->ch_channel;
	struct ata_drive_datas *drvp;
	u_int32_t idedma_ctl, idetim;
	int drive, bits[2], s;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	idedma_ctl = 0;
	bits[0] = bits[1] = 0x7F60; /* assume PIO2/DMA0 */

	/* Per drive settings */
	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;
		/* add timing values, setup DMA if needed */
		if ((atac->atac_cap & ATAC_CAP_DMA) &&
		    (drvp->drive_flags & ATA_DRIVE_DMA)) {
			/* use Multiword DMA */
			s = splbio();
			drvp->drive_flags &= ~ATA_DRIVE_UDMA;
			splx(s);
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
			bits[drive] = 0xe; /* IOCHRDY,wr/post,rd/prefetch */
		}
		else {
			/* PIO only */
			s = splbio();
			drvp->drive_flags &= ~(ATA_DRIVE_UDMA | ATA_DRIVE_DMA);
			splx(s);
			bits[drive] = 0x8; /* IOCHRDY */
		}
		bits[drive] |= dmatbl[drvp->DMA_mode] | piotbl[drvp->PIO_mode];
	}
#if 0
	idetim = pci_conf_read(sc->sc_pc, sc->sc_tag,
	    (channel == 0) ? 0x40 : 0x44);
	aprint_normal("wdc%d: IDETIM %08x -> %08x\n",
	    channel, idetim, (bits[1] << 16) | bits[0]);
#endif
	idetim = (bits[1] << 16) | bits[0];
	pci_conf_write(sc->sc_pc, sc->sc_tag,
	    (channel == 0) ? 0x40 : 0x44, idetim);

	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
		    idedma_ctl);
	}
}
