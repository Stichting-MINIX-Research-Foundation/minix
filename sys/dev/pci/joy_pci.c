/*	$NetBSD: joy_pci.c,v 1.21 2014/05/07 19:30:09 joerg Exp $	*/

/*-
 * Copyright (c) 2000, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Husemann <martin@NetBSD.org>.
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
__KERNEL_RCSID(0, "$NetBSD: joy_pci.c,v 1.21 2014/05/07 19:30:09 joerg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <sys/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/joyvar.h>

struct joy_pci_softc {
	struct joy_softc sc_joy;
	kmutex_t sc_lock;
};

static int bar_is_io(pci_chipset_tag_t pc, pcitag_t tag, int reg);

static int
joy_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_INPUT &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_INPUT_GAMEPORT &&
	    PCI_INTERFACE(pa->pa_class) == 0x10)
		return 1;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_CREATIVELABS &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CREATIVELABS_SBJOY ||
	     PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CREATIVELABS_SBJOY2))
		return 1;

	return 0;
}

/* check if this BAR assigns/requests IO space */
static int
bar_is_io(pci_chipset_tag_t pc, pcitag_t tag, int reg)
{
	pcireg_t address, mask;

	address = pci_conf_read(pc, tag, reg);
	pci_conf_write(pc, tag, reg, 0xffffffff);
	mask = pci_conf_read(pc, tag, reg);
	pci_conf_write(pc, tag, reg, address);

	return (PCI_MAPREG_TYPE(address) == PCI_MAPREG_TYPE_IO &&
	    PCI_MAPREG_IO_SIZE(mask) > 0);
}

static void
joy_pci_attach(device_t parent, device_t self, void *aux)
{
	struct joy_pci_softc *psc = device_private(self);
	struct joy_softc *sc = &psc->sc_joy;
	struct pci_attach_args *pa = aux;
	bus_size_t mapsize;
	int reg;

	pci_aprint_devinfo(pa, NULL);
	
	for (reg = PCI_MAPREG_START; reg < PCI_MAPREG_END;
	     reg += sizeof(pcireg_t))
		if (bar_is_io(pa->pa_pc, pa->pa_tag, reg))
			break;
	if (reg >= PCI_MAPREG_END) {
		aprint_error_dev(self,
		    "violates PCI spec, no IO region found\n");
		return;
	}

	if (pci_mapreg_map(pa, reg, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_iot, &sc->sc_ioh, NULL, &mapsize)) {
		aprint_error_dev(self, "could not map IO space\n");
		return;
	}

	if (mapsize != 2) {
		if (bus_space_subregion(sc->sc_iot, sc->sc_ioh, 1, 1,
		    &sc->sc_ioh)) {
			aprint_error_dev(self, "error mapping subregion\n");
			return;
		}
	}

	mutex_init(&psc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	sc->sc_dev = self;
	sc->sc_lock = &psc->sc_lock;

	joyattach(sc);
}

CFATTACH_DECL_NEW(joy_pci, sizeof(struct joy_pci_softc),
    joy_pci_match, joy_pci_attach, NULL, NULL);
