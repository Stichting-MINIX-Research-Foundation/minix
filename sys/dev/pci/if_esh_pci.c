/*	$NetBSD: if_esh_pci.c,v 1.31 2014/03/29 19:28:24 christos Exp $	*/

/*
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code contributed to The NetBSD Foundation by Kevin M. Lahey
 * of the Numerical Aerospace Simulation Facility, NASA Ames Research
 * Center.
 *
 * Partially based on a HIPPI driver written by Essential Communications
 * Corporation.
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
__KERNEL_RCSID(0, "$NetBSD: if_esh_pci.c,v 1.31 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/bufq.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_hippi.h>
#include <net/if_media.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/rrunnerreg.h>
#include <dev/ic/rrunnervar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

/*
 * PCI constants.
 * XXX These should be in a common file!
 */
#define PCI_CONN		0x48    /* Connector type */
#define PCI_CBIO PCI_BAR(0)    /* Configuration Base IO Address */

#define MEM_MAP_REG PCI_BAR(0)

static int	esh_pci_match(device_t, cfdata_t, void *);
static void	esh_pci_attach(device_t, device_t, void *);
static u_int8_t	esh_pci_bist_read(struct esh_softc *);
static void	esh_pci_bist_write(struct esh_softc *, u_int8_t);


CFATTACH_DECL_NEW(esh_pci, sizeof(struct esh_softc),
    esh_pci_match, esh_pci_attach, NULL, NULL);

static int
esh_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_ESSENTIAL)
		return 0;

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_ESSENTIAL_RR_HIPPI:
	case PCI_PRODUCT_ESSENTIAL_RR_GIGE:
		break;
	default:
		return 0;
	}
	return 1;
}

static void
esh_pci_attach(device_t parent, device_t self, void *aux)
{
	struct esh_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *model;
	const char *intrstr = NULL;
	char intrbuf[PCI_INTRSTR_LEN];

	aprint_naive(": HIPPI controller\n");

	if (pci_mapreg_map(pa, MEM_MAP_REG,
			   PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
			   &sc->sc_iot, &sc->sc_ioh, NULL, NULL) != 0) {
	    aprint_error(": unable to map memory device registers\n");
	    return;
	}

	sc->sc_dev = self;
	sc->sc_dmat = pa->pa_dmat;

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_ESSENTIAL_RR_HIPPI:
		model = "RoadRunner HIPPI";
		break;
	case PCI_PRODUCT_ESSENTIAL_RR_GIGE:
		model = "RoadRunner Gig-E";
		break;
	default:
		model = "unknown model";
		break;
	}

	aprint_normal(": %s\n", model);

	sc->sc_bist_read = esh_pci_bist_read;
	sc->sc_bist_write = esh_pci_bist_write;

	eshconfig(sc);

	/* Enable the card. */
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
	    PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, eshintr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);
}

static u_int8_t
esh_pci_bist_read(struct esh_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int32_t pci_bist;

	pci_bist = bus_space_read_4(iot, ioh, RR_PCI_BIST);

	return ((u_int8_t) (pci_bist >> 24));
}

static void
esh_pci_bist_write(struct esh_softc *sc, u_int8_t value)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int32_t pci_bist;
	u_int32_t new_bist;

	pci_bist = bus_space_read_4(iot, ioh, RR_PCI_BIST);
	new_bist = ((u_int32_t) value << 24) | (pci_bist & 0x00ffffff);

	bus_space_write_4(iot, ioh, RR_PCI_BIST, new_bist);
}
