/*	$NetBSD: ahc_cardbus.c,v 1.35 2011/08/01 11:20:27 drochner Exp $	*/

/*-
 * Copyright (c) 2000, 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

/*
 * CardBus front-end for the Adaptec AIC-7xxx family of SCSI controllers.
 *
 * TODO:
 *	- power management
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ahc_cardbus.c,v 1.35 2011/08/01 11:20:27 drochner Exp $");

#include "opt_ahc_cardbus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/pci/pcireg.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/aic7xxx_osm.h>
#include <dev/ic/aic7xxx_inline.h>


#ifndef	AHC_CARDBUS_DEFAULT_SCSI_ID
#define	AHC_CARDBUS_DEFAULT_SCSI_ID	0x7
#endif

#define	AHC_CARDBUS_IOBA	0x10
#define	AHC_CARDBUS_MMBA	0x14

struct ahc_cardbus_softc {
	struct ahc_softc sc_ahc;	/* real AHC */

	/* CardBus-specific goo. */
	cardbus_devfunc_t sc_ct;	/* our CardBus devfuncs */
	pcitag_t sc_tag;

	int	sc_bar;
	pcireg_t	sc_csr;
	bus_size_t sc_size;
};

int	ahc_cardbus_match(device_t, cfdata_t, void *);
void	ahc_cardbus_attach(device_t, device_t, void *);
int	ahc_cardbus_detach(device_t, int);

CFATTACH_DECL_NEW(ahc_cardbus, sizeof(struct ahc_cardbus_softc),
    ahc_cardbus_match, ahc_cardbus_attach, ahc_cardbus_detach, NULL);

int
ahc_cardbus_match(device_t parent, cfdata_t match, void *aux)
{
	struct cardbus_attach_args *ca = aux;

	if (PCI_VENDOR(ca->ca_id) == PCI_VENDOR_ADP &&
	    PCI_PRODUCT(ca->ca_id) == PCI_PRODUCT_ADP_APA1480)
		return (1);

	return (0);
}

void
ahc_cardbus_attach(device_t parent, device_t self, void *aux)
{
	struct cardbus_attach_args *ca = aux;
	struct ahc_cardbus_softc *csc = device_private(self);
	struct ahc_softc *ahc = &csc->sc_ahc;
	cardbus_devfunc_t ct = ca->ca_ct;
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	pcireg_t reg;
	u_int sxfrctl1 = 0;
	u_char sblkctl;


	ahc->sc_dev = self;
	csc->sc_ct = ct;
	csc->sc_tag = ca->ca_tag;

	printf(": Adaptec ADP-1480 SCSI\n");

	/*
	 * Map the device.
	 */
	csc->sc_csr = PCI_COMMAND_MASTER_ENABLE;
	if (Cardbus_mapreg_map(csc->sc_ct, AHC_CARDBUS_MMBA,
	    PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &bst, &bsh, NULL, &csc->sc_size) == 0) {
		csc->sc_bar = AHC_CARDBUS_MMBA;
		csc->sc_csr |= PCI_COMMAND_MEM_ENABLE;
	} else if (Cardbus_mapreg_map(csc->sc_ct, AHC_CARDBUS_IOBA,
	    PCI_MAPREG_TYPE_IO, 0, &bst, &bsh, NULL, &csc->sc_size) == 0) {
		csc->sc_bar = AHC_CARDBUS_IOBA;
		csc->sc_csr |= PCI_COMMAND_IO_ENABLE;
	} else {
		csc->sc_bar = 0;
		printf("%s: unable to map device registers\n",
		    ahc_name(ahc));
		return;
	}

	/* Enable the appropriate bits in the PCI CSR. */
	reg = Cardbus_conf_read(ct, ca->ca_tag, PCI_COMMAND_STATUS_REG);
	reg &= ~(PCI_COMMAND_IO_ENABLE|PCI_COMMAND_MEM_ENABLE);
	reg |= csc->sc_csr;
	Cardbus_conf_write(ct, ca->ca_tag, PCI_COMMAND_STATUS_REG, reg);

	/*
	 * Make sure the latency timer is set to some reasonable
	 * value.
	 */
	reg = Cardbus_conf_read(ct, ca->ca_tag, PCI_BHLC_REG);
	if (PCI_LATTIMER(reg) < 0x20) {
		reg &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
		reg |= (0x20 << PCI_LATTIMER_SHIFT);
		Cardbus_conf_write(ct, ca->ca_tag, PCI_BHLC_REG, reg);
	}

	ahc_set_name(ahc, device_xname(ahc->sc_dev));

	ahc->parent_dmat = ca->ca_dmat;
	ahc->tag = bst;
	ahc->bsh = bsh;

	/*
	 * ADP-1480 is always an AIC-7860.
	 */
	ahc->chip = AHC_AIC7860 | AHC_PCI;
	ahc->features = AHC_AIC7860_FE|AHC_REMOVABLE;
	ahc->bugs |= AHC_TMODE_WIDEODD_BUG|AHC_CACHETHEN_BUG|AHC_PCI_MWI_BUG;
	if (PCI_REVISION(ca->ca_class) >= 1)
		ahc->bugs |= AHC_PCI_2_1_RETRY_BUG;

	if (ahc_softc_init(ahc) != 0)
		return;

	/*
	 * On all CardBus adapters, we allow SCB paging.
	 */
	ahc->flags = AHC_PAGESCBS;

	ahc->channel = 'A';

	ahc_intr_enable(ahc, FALSE);

	ahc_reset(ahc);

	/*
	 * Establish the interrupt.
	 */
	ahc->ih = Cardbus_intr_establish(ct, IPL_BIO, ahc_intr, ahc);
	if (ahc->ih == NULL) {
		printf("%s: unable to establish interrupt\n",
		    ahc_name(ahc));
		return;
	}

	ahc->seep_config = malloc(sizeof(*ahc->seep_config),
				  M_DEVBUF, M_NOWAIT);
	if (ahc->seep_config == NULL)
		return;

	ahc_check_extport(ahc, &sxfrctl1);
	/*
	 * Take the LED out of diagnostic mode.
	 */
	sblkctl = ahc_inb(ahc, SBLKCTL);
	ahc_outb(ahc, SBLKCTL, (sblkctl & ~(DIAGLEDEN|DIAGLEDON)));

	/*
	 * I don't know where this is set in the SEEPROM or by the
	 * BIOS, so we default to 100%.
	 */
	ahc_outb(ahc, DSPCISTATUS, DFTHRSH_100);

	if (ahc->flags & AHC_USEDEFAULTS) {
		int our_id;
		/*
		 * Assume only one connector and always turn
		 * on termination.
		 */
		our_id = AHC_CARDBUS_DEFAULT_SCSI_ID;
		sxfrctl1 = STPWEN;
		ahc_outb(ahc, SCSICONF, our_id | ENSPCHK | RESET_SCSI);
		ahc->our_id = our_id;
	}

	printf("%s: aic7860", ahc_name(ahc));

	/*
	 * Record our termination setting for the
	 * generic initialization routine.
	 */
        if ((sxfrctl1 & STPWEN) != 0)
		ahc->flags |= AHC_TERM_ENB_A;

	if (ahc_init(ahc)) {
		ahc_free(ahc);
		return;
	}

	ahc_attach(ahc);
}

int
ahc_cardbus_detach(device_t self, int flags)
{
	struct ahc_cardbus_softc *csc = device_private(self);
	struct ahc_softc *ahc = &csc->sc_ahc;

	int rv;

	rv = ahc_detach(ahc, flags);
	if (rv)
		return rv;

	if (ahc->ih) {
		Cardbus_intr_disestablish(csc->sc_ct, ahc->ih);
		ahc->ih = 0;
	}

	if (csc->sc_bar != 0) {
		Cardbus_mapreg_unmap(csc->sc_ct, csc->sc_bar,
			ahc->tag, ahc->bsh, csc->sc_size);
		csc->sc_bar = 0;
	}

	return (0);
}
