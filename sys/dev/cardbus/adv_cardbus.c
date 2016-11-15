/*	$NetBSD: adv_cardbus.c,v 1.29 2012/10/27 17:18:15 chs Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
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
 * this file was brought from ahc_cardbus.c and adv_pci.c
 * and modified by YAMAMOTO Takashi.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: adv_cardbus.c,v 1.29 2012/10/27 17:18:15 chs Exp $");

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
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/advlib.h>
#include <dev/ic/adv.h>

#define ADV_CARDBUS_IOBA PCI_BAR0
#define ADV_CARDBUS_MMBA PCI_BAR1

#define ADV_CARDBUS_DEBUG
#define ADV_CARDBUS_ALLOW_MEMIO

#define DEVNAME(sc) device_xname((sc)->sc_dev)

struct adv_cardbus_softc {
	struct asc_softc sc_adv;	/* real ADV */

	/* CardBus-specific goo. */
	cardbus_devfunc_t sc_ct;	/* our CardBus devfuncs */
	pcitag_t sc_tag;

	int	sc_bar;
	pcireg_t	sc_csr;
	bus_size_t sc_size;
};

int	adv_cardbus_match(device_t, cfdata_t, void *);
void	adv_cardbus_attach(device_t, device_t, void *);
int	adv_cardbus_detach(device_t, int);

CFATTACH_DECL_NEW(adv_cardbus, sizeof(struct adv_cardbus_softc),
    adv_cardbus_match, adv_cardbus_attach, adv_cardbus_detach, NULL);

int
adv_cardbus_match(device_t parent, cfdata_t match, void *aux)
{
	struct cardbus_attach_args *ca = aux;

	if (PCI_VENDOR(ca->ca_id) == PCI_VENDOR_ADVSYS &&
	    PCI_PRODUCT(ca->ca_id) == PCI_PRODUCT_ADVSYS_ULTRA)
		return (1);

	return (0);
}

void
adv_cardbus_attach(device_t parent, device_t self, void *aux)
{
	struct cardbus_attach_args *ca = aux;
	struct adv_cardbus_softc *csc = device_private(self);
	struct asc_softc *sc = &csc->sc_adv;
	cardbus_devfunc_t ct = ca->ca_ct;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	pcireg_t reg;
	u_int8_t latency = 0x20;

	sc->sc_dev = self;
	sc->sc_flags = 0;

	if (PCI_VENDOR(ca->ca_id) == PCI_VENDOR_ADVSYS) {
		switch (PCI_PRODUCT(ca->ca_id)) {
		case PCI_PRODUCT_ADVSYS_1200A:
			printf(": AdvanSys ASC1200A SCSI adapter\n");
			latency = 0;
			break;

		case PCI_PRODUCT_ADVSYS_1200B:
			printf(": AdvanSys ASC1200B SCSI adapter\n");
			latency = 0;
			break;

		case PCI_PRODUCT_ADVSYS_ULTRA:
			switch (PCI_REVISION(ca->ca_class)) {
			case ASC_PCI_REVISION_3050:
				printf(": AdvanSys ABP-9xxUA SCSI adapter\n");
				break;

			case ASC_PCI_REVISION_3150:
				printf(": AdvanSys ABP-9xxU SCSI adapter\n");
				break;
			}
			break;

		default:
			printf(": unknown model!\n");
			return;
		}
	}

	csc->sc_ct = ct;
	csc->sc_tag = ca->ca_tag;

	/*
	 * Map the device.
	 */
	csc->sc_csr = PCI_COMMAND_MASTER_ENABLE;

#ifdef ADV_CARDBUS_ALLOW_MEMIO
	if (Cardbus_mapreg_map(csc->sc_ct, ADV_CARDBUS_MMBA,
	    PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &iot, &ioh, NULL, &csc->sc_size) == 0) {
#ifdef ADV_CARDBUS_DEBUG
		printf("%s: memio enabled\n", DEVNAME(sc));
#endif
		csc->sc_bar = ADV_CARDBUS_MMBA;
		csc->sc_csr |= PCI_COMMAND_MEM_ENABLE;
	} else
#endif
	if (Cardbus_mapreg_map(csc->sc_ct, ADV_CARDBUS_IOBA,
	    PCI_MAPREG_TYPE_IO, 0, &iot, &ioh, NULL, &csc->sc_size) == 0) {
#ifdef ADV_CARDBUS_DEBUG
		printf("%s: io enabled\n", DEVNAME(sc));
#endif
		csc->sc_bar = ADV_CARDBUS_IOBA;
		csc->sc_csr |= PCI_COMMAND_IO_ENABLE;
	} else {
		csc->sc_bar = 0;
		aprint_error_dev(sc->sc_dev, "unable to map device registers\n");
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
	if (PCI_LATTIMER(reg) < latency) {
		reg &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
		reg |= (latency << PCI_LATTIMER_SHIFT);
		Cardbus_conf_write(ct, ca->ca_tag, PCI_BHLC_REG, reg);
	}

	ASC_SET_CHIP_CONTROL(iot, ioh, ASC_CC_HALT);
	ASC_SET_CHIP_STATUS(iot, ioh, 0);

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = ca->ca_dmat;
	sc->pci_device_id = ca->ca_id;
	sc->bus_type = ASC_IS_PCI;
	sc->chip_version = ASC_GET_CHIP_VER_NO(iot, ioh);

	/*
	 * Initialize the board
	 */
	if (adv_init(sc)) {
		printf("adv_init failed\n");
		return;
	}

	/*
	 * Establish the interrupt.
	 */
	sc->sc_ih = Cardbus_intr_establish(ct, IPL_BIO, adv_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev,
				 "unable to establish interrupt\n");
		return;
	}

	/*
	 * Attach.
	 */
	adv_attach(sc);
}

int
adv_cardbus_detach(device_t self, int flags)
{
	struct adv_cardbus_softc *csc = device_private(self);
	struct asc_softc *sc = &csc->sc_adv;

	int rv;

	rv = adv_detach(sc, flags);
	if (rv)
		return rv;

	if (sc->sc_ih) {
		Cardbus_intr_disestablish(csc->sc_ct, sc->sc_ih);
		sc->sc_ih = 0;
	}

	if (csc->sc_bar != 0) {
		Cardbus_mapreg_unmap(csc->sc_ct, csc->sc_bar,
		    sc->sc_iot, sc->sc_ioh, csc->sc_size);
		csc->sc_bar = 0;
	}

	return 0;
}
