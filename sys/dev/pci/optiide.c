/*	$NetBSD: optiide.c,v 1.25 2013/10/07 19:51:55 jakllsch Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
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
__KERNEL_RCSID(0, "$NetBSD: optiide.c,v 1.25 2013/10/07 19:51:55 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_opti_reg.h>

static void opti_chip_map(struct pciide_softc*, const struct pci_attach_args*);
static void opti_setup_channel(struct ata_channel*);

static int  optiide_match(device_t, cfdata_t, void *);
static void optiide_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(optiide, sizeof(struct pciide_softc),
    optiide_match, optiide_attach, pciide_detach, NULL);

static const struct pciide_product_desc pciide_opti_products[] =  {
	{ PCI_PRODUCT_OPTI_82C621,
	  0,
	  "OPTi 82c621 PCI IDE controller",
	  opti_chip_map,
	},
	{ PCI_PRODUCT_OPTI_82C568,
	  0,
	  "OPTi 82c568 (82c621 compatible) PCI IDE controller",
	  opti_chip_map,
	},
	{ PCI_PRODUCT_OPTI_82D568,
	  0,
	  "OPTi 82d568 (82c621 compatible) PCI IDE controller",
	  opti_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

static int
optiide_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_OPTI &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_MASS_STORAGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_IDE) {
		if (pciide_lookup_product(pa->pa_id, pciide_opti_products))
			return (2);
	}
	return (0);
}

static void
optiide_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_opti_products));

}

static void
opti_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	pcireg_t interface;
	u_int8_t init_ctrl;
	int channel;

	if (pciide_chipen(sc, pa) == 0)
		return;

	aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "bus-master DMA support present");

	/*
	 * XXXSCW:
	 * There seem to be a couple of buggy revisions/implementations
	 * of the OPTi pciide chipset. This kludge seems to fix one of
	 * the reported problems (PR/11644) but still fails for the
	 * other (PR/13151), although the latter may be due to other
	 * issues too...
	 */
	if (PCI_REVISION(pa->pa_class) <= 0x12) {
		aprint_verbose(" but disabled due to chip rev. <= 0x12");
		sc->sc_dma_ok = 0;
	} else
		pciide_mapreg_dma(sc, pa);

	aprint_verbose("\n");

	sc->sc_wdcdev.sc_atac.atac_cap = ATAC_CAP_DATA32 | ATAC_CAP_DATA16;
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA;
		sc->sc_wdcdev.irqack = pciide_irqack;
		sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;
	}
	sc->sc_wdcdev.sc_atac.atac_set_modes = opti_setup_channel;

	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.wdc_maxdrives = 2;

	init_ctrl = pciide_pci_read(sc->sc_pc, sc->sc_tag,
	    OPTI_REG_INIT_CONTROL);

	interface = PCI_INTERFACE(pa->pa_class);

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		if (channel == 1 &&
		    (init_ctrl & OPTI_INIT_CONTROL_CH2_DISABLE) != 0) {
			aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "%s channel ignored (disabled)\n", cp->name);
			cp->ata_channel.ch_flags |= ATACH_DISABLED;
			continue;
		}
		pciide_mapchan(pa, cp, interface, pciide_pci_intr);
	}
}

static void
opti_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	int drive, spd, s;
	int mode[2];
	u_int8_t rv, mr;

	/*
	 * The `Delay' and `Address Setup Time' fields of the
	 * Miscellaneous Register are always zero initially.
	 */
	mr = opti_read_config(chp, OPTI_REG_MISC) & ~OPTI_MISC_INDEX_MASK;
	mr &= ~(OPTI_MISC_DELAY_MASK |
		OPTI_MISC_ADDR_SETUP_MASK |
		OPTI_MISC_INDEX_MASK);

	/* Prime the control register before setting timing values */
	opti_write_config(chp, OPTI_REG_CONTROL, OPTI_CONTROL_DISABLE);

	/* Determine the clockrate of the PCIbus the chip is attached to */
	spd = (int) opti_read_config(chp, OPTI_REG_STRAP);
	spd &= OPTI_STRAP_PCI_SPEED_MASK;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE) {
			mode[drive] = -1;
			continue;
		}

		if ((drvp->drive_flags & ATA_DRIVE_DMA)) {
			/*
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

			mode[drive] = drvp->DMA_mode + 5;
		} else
			mode[drive] = drvp->PIO_mode;

		if (drive && mode[0] >= 0 &&
		    (opti_tim_as[spd][mode[0]] != opti_tim_as[spd][mode[1]])) {
			/*
			 * Can't have two drives using different values
			 * for `Address Setup Time'.
			 * Slow down the faster drive to compensate.
			 */
			int d = (opti_tim_as[spd][mode[0]] >
				 opti_tim_as[spd][mode[1]]) ?  0 : 1;

			mode[d] = mode[1-d];
			chp->ch_drive[d].PIO_mode = chp->ch_drive[1-d].PIO_mode;
			chp->ch_drive[d].DMA_mode = 0;
			s = splbio();
			chp->ch_drive[d].drive_flags &= ~ATA_DRIVE_DMA;
			splx(s);
		}
	}

	for (drive = 0; drive < 2; drive++) {
		int m;
		if ((m = mode[drive]) < 0)
			continue;

		/* Set the Address Setup Time and select appropriate index */
		rv = opti_tim_as[spd][m] << OPTI_MISC_ADDR_SETUP_SHIFT;
		rv |= OPTI_MISC_INDEX(drive);
		opti_write_config(chp, OPTI_REG_MISC, mr | rv);

		/* Set the pulse width and recovery timing parameters */
		rv  = opti_tim_cp[spd][m] << OPTI_PULSE_WIDTH_SHIFT;
		rv |= opti_tim_rt[spd][m] << OPTI_RECOVERY_TIME_SHIFT;
		opti_write_config(chp, OPTI_REG_READ_CYCLE_TIMING, rv);
		opti_write_config(chp, OPTI_REG_WRITE_CYCLE_TIMING, rv);

		/* Set the Enhanced Mode register appropriately */
	    	rv = pciide_pci_read(sc->sc_pc, sc->sc_tag, OPTI_REG_ENH_MODE);
		rv &= ~OPTI_ENH_MODE_MASK(chp->ch_channel, drive);
		rv |= OPTI_ENH_MODE(chp->ch_channel, drive, opti_tim_em[m]);
		pciide_pci_write(sc->sc_pc, sc->sc_tag, OPTI_REG_ENH_MODE, rv);
	}

	/* Finally, enable the timings */
	opti_write_config(chp, OPTI_REG_CONTROL, OPTI_CONTROL_ENABLE);
}
