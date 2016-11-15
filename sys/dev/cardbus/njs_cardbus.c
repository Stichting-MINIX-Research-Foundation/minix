/*	$NetBSD: njs_cardbus.c,v 1.17 2011/08/01 11:20:28 drochner Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by ITOH Yasufumi.
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
__KERNEL_RCSID(0, "$NetBSD: njs_cardbus.c,v 1.17 2011/08/01 11:20:28 drochner Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/ninjascsi32reg.h>
#include <dev/ic/ninjascsi32var.h>

#define NJSC32_CARDBUS_BASEADDR_IO	PCI_BAR0
#define NJSC32_CARDBUS_BASEADDR_MEM	PCI_BAR1

struct njsc32_cardbus_softc {
	struct njsc32_softc	sc_njsc32;

	/* CardBus-specific goo */
	cardbus_devfunc_t	sc_ct;		/* our CardBus devfuncs */
	pcitag_t		sc_tag;

	bus_space_handle_t	sc_regmaph;
	bus_size_t		sc_regmap_size;
};

static int	njs_cardbus_match(device_t, cfdata_t, void *);
static void	njs_cardbus_attach(device_t, device_t, void *);
static int	njs_cardbus_detach(device_t, int);

CFATTACH_DECL_NEW(njs_cardbus, sizeof(struct njsc32_cardbus_softc),
    njs_cardbus_match, njs_cardbus_attach, njs_cardbus_detach, NULL);

static const struct njsc32_cardbus_product {
	pci_vendor_id_t		p_vendor;
	pci_product_id_t	p_product;
	njsc32_model_t		p_model;
	int			p_clk;		/* one of NJSC32_CLK_* */
} njsc32_cardbus_products[] = {
	{ PCI_VENDOR_IODATA,	PCI_PRODUCT_IODATA_CBSCII,
	  NJSC32_MODEL_32BI,	NJSC32_CLK_40M },
	{ PCI_VENDOR_WORKBIT,	PCI_PRODUCT_WORKBIT_NJSC32BI,
	  NJSC32_MODEL_32BI,	NJSC32_CLK_40M },
	{ PCI_VENDOR_WORKBIT,	PCI_PRODUCT_WORKBIT_NJSC32UDE,
	  NJSC32_MODEL_32UDE | NJSC32_FLAG_DUALEDGE,	NJSC32_CLK_40M },
	{ PCI_VENDOR_WORKBIT,	PCI_PRODUCT_WORKBIT_NJSC32BI_KME,
	  NJSC32_MODEL_32BI,	NJSC32_CLK_40M },

	{ 0,				0,
	  NJSC32_MODEL_INVALID,		0 },
};

static const struct njsc32_cardbus_product *
njs_cardbus_lookup(const struct cardbus_attach_args *ca)
{
	const struct njsc32_cardbus_product *p;

	for (p = njsc32_cardbus_products;
	    p->p_model != NJSC32_MODEL_INVALID; p++) {
		if (PCI_VENDOR(ca->ca_id) == p->p_vendor &&
		    PCI_PRODUCT(ca->ca_id) == p->p_product)
			return p;
	}

	return NULL;
}

static int
njs_cardbus_match(device_t parent, cfdata_t match, void *aux)
{
	struct cardbus_attach_args *ca = aux;

	if (njs_cardbus_lookup(ca))
		return 1;

	return 0;
}

static void
njs_cardbus_attach(device_t parent, device_t self, void *aux)
{
	struct cardbus_attach_args *ca = aux;
	struct njsc32_cardbus_softc *csc = device_private(self);
	struct njsc32_softc *sc = &csc->sc_njsc32;
	const struct njsc32_cardbus_product *prod;
	cardbus_devfunc_t ct = ca->ca_ct;
	pcireg_t csr, reg;
	u_int8_t latency = 0x20;

	if ((prod = njs_cardbus_lookup(ca)) == NULL)
		panic("njs_cardbus_attach");

	printf(": Workbit NinjaSCSI-32 SCSI adapter\n");
	sc->sc_dev = self;
	sc->sc_model = prod->p_model;
	sc->sc_clk = prod->p_clk;

	csc->sc_ct = ct;
	csc->sc_tag = ca->ca_tag;

	/*
	 * Map the device.
	 */
	csr = PCI_COMMAND_MASTER_ENABLE;

	/*
	 * Map registers.
	 * Try memory map first, and then try I/O.
	 */
	if (Cardbus_mapreg_map(csc->sc_ct, NJSC32_CARDBUS_BASEADDR_MEM,
	    PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &sc->sc_regt, &csc->sc_regmaph, NULL, &csc->sc_regmap_size) == 0) {
		if (bus_space_subregion(sc->sc_regt, csc->sc_regmaph,
		    NJSC32_MEMOFFSET_REG, NJSC32_REGSIZE, &sc->sc_regh) != 0) {
			/* failed -- undo map and try I/O */
			Cardbus_mapreg_unmap(csc->sc_ct,
			    NJSC32_CARDBUS_BASEADDR_MEM,
			    sc->sc_regt, csc->sc_regmaph, csc->sc_regmap_size);
			goto try_io;
		}
#ifdef NJSC32_DEBUG
		printf("%s: memory space mapped\n", device_xname(self));
#endif
		csr |= PCI_COMMAND_MEM_ENABLE;
		sc->sc_flags = NJSC32_MEM_MAPPED;
	} else {
	try_io:
		if (Cardbus_mapreg_map(csc->sc_ct, NJSC32_CARDBUS_BASEADDR_IO,
		    PCI_MAPREG_TYPE_IO, 0, &sc->sc_regt, &sc->sc_regh,
		    NULL, &csc->sc_regmap_size) == 0) {
#ifdef NJSC32_DEBUG
			printf("%s: io space mapped\n", device_xname(self));
#endif
			csr |= PCI_COMMAND_IO_ENABLE;
			sc->sc_flags = NJSC32_IO_MAPPED;
		} else {
			aprint_error_dev(self, "unable to map device registers\n");
			return;
		}
	}

	/* Enable the appropriate bits in the PCI CSR. */
	reg = Cardbus_conf_read(ct, ca->ca_tag, PCI_COMMAND_STATUS_REG);
	reg &= ~(PCI_COMMAND_IO_ENABLE|PCI_COMMAND_MEM_ENABLE);
	reg |= csr;
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

	sc->sc_dmat = ca->ca_dmat;

	/*
	 * Establish the interrupt.
	 */
	sc->sc_ih = Cardbus_intr_establish(ct, IPL_BIO, njsc32_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self,
				 "unable to establish interrupt\n");
		return;
	}

	/* CardBus device cannot supply termination power. */
	sc->sc_flags |= NJSC32_CANNOT_SUPPLY_TERMPWR;

	/* attach */
	njsc32_attach(sc);
}

static int
njs_cardbus_detach(device_t self, int flags)
{
	struct njsc32_cardbus_softc *csc = device_private(self);
	struct njsc32_softc *sc = &csc->sc_njsc32;
	int rv;

	rv = njsc32_detach(sc, flags);
	if (rv)
		return rv;

	if (sc->sc_ih)
		Cardbus_intr_disestablish(csc->sc_ct, sc->sc_ih);

	if (sc->sc_flags & NJSC32_IO_MAPPED)
		Cardbus_mapreg_unmap(csc->sc_ct, NJSC32_CARDBUS_BASEADDR_IO,
		    sc->sc_regt, sc->sc_regh, csc->sc_regmap_size);
	if (sc->sc_flags & NJSC32_MEM_MAPPED)
		Cardbus_mapreg_unmap(csc->sc_ct, NJSC32_CARDBUS_BASEADDR_MEM,
		    sc->sc_regt, csc->sc_regmaph, csc->sc_regmap_size);

	return 0;
}
