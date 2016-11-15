/*	$NetBSD: njs_pci.c,v 1.10 2014/03/29 19:28:25 christos Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: njs_pci.c,v 1.10 2014/03/29 19:28:25 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/ninjascsi32reg.h>
#include <dev/ic/ninjascsi32var.h>

#define NJSC32_PCI_BASEADDR_IO		PCI_MAPREG_START
#define NJSC32_PCI_BASEADDR_MEM		(PCI_MAPREG_START + 4)

struct njsc32_pci_softc {
	struct njsc32_softc	sc_njsc32;

	pci_chipset_tag_t	sc_pc;

	bus_space_handle_t	sc_regmaph;
	bus_size_t		sc_regmap_size;
};

static int	njs_pci_match(device_t, cfdata_t, void *);
static void	njs_pci_attach(device_t, device_t, void *);
static int	njs_pci_detach(device_t, int);

CFATTACH_DECL_NEW(njs_pci, sizeof(struct njsc32_pci_softc),
    njs_pci_match, njs_pci_attach, njs_pci_detach, NULL);

static const struct njsc32_pci_product {
	pci_vendor_id_t		p_vendor;
	pci_product_id_t	p_product;
	njsc32_model_t		p_model;
	int			p_clk;		/* one of NJSC32_CLK_* */
} njsc32_pci_products[] = {
	{ PCI_VENDOR_WORKBIT,	PCI_PRODUCT_WORKBIT_NJSC32UDE_IODATA,
	  NJSC32_MODEL_32UDE | NJSC32_FLAG_DUALEDGE,	NJSC32_CLK_40M },
	{ PCI_VENDOR_WORKBIT,	PCI_PRODUCT_WORKBIT_NJSC32UDE_LOGITEC,
	  NJSC32_MODEL_32UDE | NJSC32_FLAG_DUALEDGE,	NJSC32_CLK_40M },
	{ PCI_VENDOR_WORKBIT,	PCI_PRODUCT_WORKBIT_NJSC32UDE_LOGITEC2,
	  NJSC32_MODEL_32UDE | NJSC32_FLAG_DUALEDGE,	NJSC32_CLK_40M },
	{ PCI_VENDOR_WORKBIT,	PCI_PRODUCT_WORKBIT_NJSC32UDE_BUFFALO,
	  NJSC32_MODEL_32UDE | NJSC32_FLAG_DUALEDGE,	NJSC32_CLK_40M },

	{ 0,				0,
	  NJSC32_MODEL_INVALID,		0 },
};

static const struct njsc32_pci_product *
njs_pci_lookup(const struct pci_attach_args *pa)
{
	const struct njsc32_pci_product *p;

	for (p = njsc32_pci_products;
	    p->p_model != NJSC32_MODEL_INVALID; p++) {
		if (PCI_VENDOR(pa->pa_id) == p->p_vendor &&
		    PCI_PRODUCT(pa->pa_id) == p->p_product)
			return p;
	}

	return NULL;
}

static int
njs_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (njs_pci_lookup(pa))
		return 1;

	return 0;
}

static void
njs_pci_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct njsc32_pci_softc *psc = device_private(self);
	struct njsc32_softc *sc = &psc->sc_njsc32;
	const struct njsc32_pci_product *prod;
	pci_intr_handle_t ih;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcireg_t reg;
	const char *str_intr, *str_at;
	char intrbuf[PCI_INTRSTR_LEN];

	aprint_naive(": SCSI controller\n");
	if ((prod = njs_pci_lookup(pa)) == NULL)
		panic("njs_pci_attach");

	aprint_normal(": Workbit NinjaSCSI-32 SCSI adapter\n");
	sc->sc_dev = self;
	sc->sc_model = prod->p_model;
	sc->sc_clk = prod->p_clk;

	psc->sc_pc = pc;

	/* enable device and DMA */
	reg = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	reg |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
	    PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, reg);

	/*
	 * Map registers.
	 * Try memory map first, and then try I/O.
	 */
	if (pci_mapreg_map(pa, NJSC32_PCI_BASEADDR_MEM,
	    PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &sc->sc_regt, &psc->sc_regmaph, NULL, &psc->sc_regmap_size) == 0) {
		if (bus_space_subregion(sc->sc_regt, psc->sc_regmaph,
		    NJSC32_MEMOFFSET_REG, NJSC32_REGSIZE, &sc->sc_regh) != 0) {
			/* failed -- undo map and try I/O */
			bus_space_unmap(sc->sc_regt, psc->sc_regmaph,
			    psc->sc_regmap_size);
			goto try_io;
		}
#ifdef NJSC32_DEBUG
		printf("%s: memory space mapped\n", device_xname(self));
#endif
		sc->sc_flags = NJSC32_MEM_MAPPED;
	} else {
	try_io:
		if (pci_mapreg_map(pa, NJSC32_PCI_BASEADDR_IO,
		    PCI_MAPREG_TYPE_IO, 0, &sc->sc_regt, &sc->sc_regh,
		    NULL, &psc->sc_regmap_size) == 0) {
#ifdef NJSC32_DEBUG
			printf("%s: io space mapped\n", device_xname(self));
#endif
			sc->sc_flags = NJSC32_IO_MAPPED;
		} else {
			aprint_error_dev(self, "unable to map device registers\n");
			return;
		}
	}

	sc->sc_dmat = pa->pa_dmat;

	/* map interrupt */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}

	str_intr = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));
	str_at = " at ";
	if (str_intr == NULL)
		str_at = str_intr = "";

	/* setup interrupt handler */
	if ((sc->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, njsc32_intr, sc))
	    == NULL) {
		aprint_error_dev(self, "unable to establish interrupt%s%s\n",
		    str_at, str_intr);
		return;
	}
	printf("%s: interrupting%s%s\n", device_xname(self), str_at, str_intr);

	/* attach */
	njsc32_attach(sc);
}

static int
njs_pci_detach(device_t self, int flags)
{
	struct njsc32_pci_softc *psc = device_private(self);
	struct njsc32_softc *sc = &psc->sc_njsc32;
	int rv;

	rv = njsc32_detach(sc, flags);
	if (rv)
		return rv;

	if (sc->sc_ih)
		pci_intr_disestablish(psc->sc_pc, sc->sc_ih);

	if (sc->sc_flags & NJSC32_IO_MAPPED)
		bus_space_unmap(sc->sc_regt, sc->sc_regh, psc->sc_regmap_size);
	if (sc->sc_flags & NJSC32_MEM_MAPPED)
		bus_space_unmap(sc->sc_regt, psc->sc_regmaph,
		    psc->sc_regmap_size);

	return 0;
}
