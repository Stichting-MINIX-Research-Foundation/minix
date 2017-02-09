/*	$NetBSD: schide.c,v 1.8 2013/10/07 19:51:55 jakllsch Exp $	*/
/*	$OpenBSD: pciide.c,v 1.305 2009/11/01 01:50:15 dlg Exp $	*/

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

/*
 * Copyright (c) 1996, 1998 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
__KERNEL_RCSID(0, "$NetBSD: schide.c,v 1.8 2013/10/07 19:51:55 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_sch_reg.h>

static void sch_chip_map(struct pciide_softc*, const struct pci_attach_args*);
static void sch_setup_channel(struct ata_channel*);
static int  schide_match(device_t, cfdata_t, void *);
static void schide_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(schide, sizeof(struct pciide_softc),
    schide_match, schide_attach, pciide_detach, NULL);

static const struct pciide_product_desc pciide_sch_products[] =  {
	{ PCI_PRODUCT_INTEL_SCH_IDE,
	  0,
	  "Intel SCH IDE Controller",
	  sch_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

static int
schide_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL) {
		if (pciide_lookup_product(pa->pa_id, pciide_sch_products))
			return (2);
	}
	return (0);
}

static void
schide_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_sch_products));

}

static void
sch_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	pcireg_t interface;
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
	sc->sc_wdcdev.sc_atac.atac_udma_cap = 5;
	sc->sc_wdcdev.sc_atac.atac_set_modes = sch_setup_channel;
	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = 1;
	sc->sc_wdcdev.wdc_maxdrives = 2;


	ATADEBUG_PRINT(("sch_setup_chip: old d0tim=0x%x, d1tim=0x%x\n",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, SCH_D0TIM),
	    pci_conf_read(sc->sc_pc, sc->sc_tag, SCH_D1TIM)),
	    DEBUG_PROBE);

	interface = PCI_INTERFACE(pa->pa_class);

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		pciide_mapchan(pa, cp, interface, pciide_pci_intr);
	}

	ATADEBUG_PRINT(("sch_setup_chip: d0tim=0x%x, d1tim=0x%x\n",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, SCH_D0TIM),
	    pci_conf_read(sc->sc_pc, sc->sc_tag, SCH_D1TIM)),
	    DEBUG_PROBE);
}

static void
sch_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	u_int32_t tim, timaddr, idedma_ctl;
	struct atac_softc *atac = chp->ch_atac;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	int drive, s;

	idedma_ctl = 0;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	/* Per drive settings */
	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;

		timaddr = (drive == 0) ? SCH_D0TIM : SCH_D1TIM;
		tim = pci_conf_read(sc->sc_pc, sc->sc_tag, timaddr);
		tim &= ~SCH_TIM_MASK;

		if (((drvp->drive_flags & ATA_DRIVE_DMA) == 0 &&
		    (drvp->drive_flags & ATA_DRIVE_UDMA) == 0))
			goto pio;

		/* add timing values, setup DMA if needed */
		if ((atac->atac_cap & ATAC_CAP_UDMA) &&
		    (drvp->drive_flags & ATA_DRIVE_UDMA)) {
			/* use Ultra/DMA */
			tim |= (drvp->UDMA_mode << 16) | SCH_TIM_SYNCDMA;
		} else {
			/* use Multiword DMA */
			s = splbio();
			drvp->drive_flags &= ~ATA_DRIVE_UDMA;
			splx(s);
			tim &= ~SCH_TIM_SYNCDMA;
		}
		idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);

pio:		/* use PIO mode */

		tim |= (drvp->DMA_mode << 8) | (drvp->PIO_mode);
		pci_conf_write(sc->sc_pc, sc->sc_tag, timaddr, tim);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
		    idedma_ctl);
	}
}
