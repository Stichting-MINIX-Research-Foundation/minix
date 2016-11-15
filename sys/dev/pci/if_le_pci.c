/*	$NetBSD: if_le_pci.c,v 1.52 2014/03/29 19:28:24 christos Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_le.c	8.2 (Berkeley) 11/16/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_le_pci.c,v 1.52 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/lancereg.h>
#include <dev/ic/lancevar.h>
#include <dev/ic/am79900reg.h>
#include <dev/ic/am79900var.h>

#include <dev/pci/if_levar.h>

static int	le_pci_match(device_t, cfdata_t, void *);
static void	le_pci_attach(device_t, device_t, void *);
static int	le_pci_mediachange(struct lance_softc *);

CFATTACH_DECL_NEW(le_pci, sizeof(struct le_softc),
    le_pci_match, le_pci_attach, NULL, NULL);

/*
 * PCI constants.
 * XXX These should be in a common file!
 */
#define PCI_CBIO PCI_BAR(0)		/* Configuration Base IO Address */

#define	LE_PCI_MEMSIZE	16384

static int le_pci_supmedia[] = {
	IFM_ETHER|IFM_AUTO,
	IFM_ETHER|IFM_AUTO|IFM_FDX,
	IFM_ETHER|IFM_10_T,
	IFM_ETHER|IFM_10_T|IFM_FDX,
	IFM_ETHER|IFM_10_5,
	IFM_ETHER|IFM_10_5|IFM_FDX,
};

static void
le_pci_wrcsr(struct lance_softc *sc, uint16_t port, uint16_t val)
{
	struct le_softc *lesc = (struct le_softc *)sc;
	bus_space_tag_t iot = lesc->sc_iot;
	bus_space_handle_t ioh = lesc->sc_ioh;

	bus_space_write_2(iot, ioh, lesc->sc_rap, port);
	bus_space_write_2(iot, ioh, lesc->sc_rdp, val);
}

static uint16_t
le_pci_rdcsr(struct lance_softc *sc, uint16_t port)
{
	struct le_softc *lesc = (struct le_softc *)sc;
	bus_space_tag_t iot = lesc->sc_iot;
	bus_space_handle_t ioh = lesc->sc_ioh;
	uint16_t val;

	bus_space_write_2(iot, ioh, lesc->sc_rap, port);
	val = bus_space_read_2(iot, ioh, lesc->sc_rdp);
	return (val);
}

static int
le_pci_mediachange(struct lance_softc *sc)
{
	struct le_softc *lesc = (struct le_softc *)sc;
	bus_space_tag_t iot = lesc->sc_iot;
	bus_space_handle_t ioh = lesc->sc_ioh;
	int newmedia = sc->sc_media.ifm_media;
	uint16_t reg;

	if (IFM_SUBTYPE(newmedia) !=
	    IFM_SUBTYPE(lesc->sc_currentmedia)) {
		if (IFM_SUBTYPE(newmedia) == IFM_AUTO) {
			/* switch to autoselect - BCR2 bit 1 */
			bus_space_write_2(iot, ioh, PCNET_PCI_RAP, 2);
			reg = bus_space_read_2(iot, ioh, PCNET_PCI_BDP);
			reg |= 2;
			bus_space_write_2(iot, ioh, PCNET_PCI_RAP, 2);
			bus_space_write_2(iot, ioh, PCNET_PCI_BDP, reg);
		} else {
			/* force media type (in init block) */
			lance_reset(sc);
			if (IFM_SUBTYPE(newmedia) == IFM_10_T)
				sc->sc_initmodemedia = 1; /* UTP */
			else
				sc->sc_initmodemedia = 0; /* AUI */
			lance_init(&sc->sc_ethercom.ec_if);

			if (IFM_SUBTYPE(lesc->sc_currentmedia) == IFM_AUTO) {
				/* take away autoselect - BCR2 bit 1 */
				bus_space_write_2(iot, ioh, PCNET_PCI_RAP, 2);
				reg = bus_space_read_2(iot, ioh, PCNET_PCI_BDP);
				reg &= ~2;
				bus_space_write_2(iot, ioh, PCNET_PCI_RAP, 2);
				bus_space_write_2(iot, ioh, PCNET_PCI_BDP, reg);
			}
		}

	}

	if ((IFM_OPTIONS(newmedia) ^ IFM_OPTIONS(lesc->sc_currentmedia))
	    & IFM_FDX) {
		/* toggle full duplex - BCR9 */
		bus_space_write_2(iot, ioh, PCNET_PCI_RAP, 9);
		reg = bus_space_read_2(iot, ioh, PCNET_PCI_BDP);
		if (IFM_OPTIONS(newmedia) & IFM_FDX) {
			reg |= 1; /* FDEN */
			/* allow FDX on AUI only if explicitly chosen,
			 not in autoselect mode */
			if (IFM_SUBTYPE(newmedia) == IFM_10_5)
				reg |= 2; /* AUIFD */
			else
				reg &= ~2;
		} else
			reg &= ~1;
		bus_space_write_2(iot, ioh, PCNET_PCI_RAP, 9);
		bus_space_write_2(iot, ioh, PCNET_PCI_BDP, reg);
	}

	lesc->sc_currentmedia = newmedia;
	return (0);
}

static int
le_pci_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_AMD)
		return (0);

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_AMD_PCNET_PCI:
		return (1);
	}

	return (0);
}

static void
le_pci_attach(device_t parent, device_t self, void *aux)
{
	struct le_softc *lesc = device_private(self);
	struct lance_softc *sc = &lesc->sc_am79900.lsc;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_dma_tag_t dmat = pa->pa_dmat;
	bus_dma_segment_t seg;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcireg_t csr;
	int i, rseg;
	const char *model, *intrstr;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_AMD_PCNET_PCI:
		model = "PCnet-PCI Ethernet";
		lesc->sc_rap = PCNET_PCI_RAP;
		lesc->sc_rdp = PCNET_PCI_RDP;
		break;

	default:
		model = "unknown model!";
	}

	aprint_normal(": %s\n", model);

	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, NULL)) {
		aprint_error_dev(self, "can't map I/O space\n");
		return;
	}

	/*
	 * Extract the physical MAC address from the ROM.
	 */
	for (i = 0; i < sizeof(sc->sc_enaddr); i++)
		sc->sc_enaddr[i] = bus_space_read_1(iot, ioh, i);

	lesc->sc_iot = iot;
	lesc->sc_ioh = ioh;
	lesc->sc_dmat = dmat;

	/*
	 * Allocate a DMA area for the card.
	 */
	if (bus_dmamem_alloc(dmat, LE_PCI_MEMSIZE, PAGE_SIZE, 0, &seg, 1,
	    &rseg, BUS_DMA_NOWAIT)) {
		aprint_error_dev(self, "couldn't allocate memory for card\n");
		return;
	}
	if (bus_dmamem_map(dmat, &seg, rseg, LE_PCI_MEMSIZE,
	    (void **)&sc->sc_mem,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		aprint_error_dev(self, "couldn't map memory for card\n");
		return;
	}

	/*
	 * Create and load the DMA map for the DMA area.
	 */
	if (bus_dmamap_create(dmat, LE_PCI_MEMSIZE, 1,
	    LE_PCI_MEMSIZE, 0, BUS_DMA_NOWAIT, &lesc->sc_dmam)) {
		aprint_error_dev(self, "couldn't create DMA map\n");
		bus_dmamem_free(dmat, &seg, rseg);
		return;
	}
	if (bus_dmamap_load(dmat, lesc->sc_dmam,
	    sc->sc_mem, LE_PCI_MEMSIZE, NULL, BUS_DMA_NOWAIT)) {
		aprint_error_dev(self, "coundn't load DMA map\n");
		bus_dmamem_free(dmat, &seg, rseg);
		return;
	}

	sc->sc_conf3 = 0;
	sc->sc_addr = lesc->sc_dmam->dm_segs[0].ds_addr;
	sc->sc_memsize = LE_PCI_MEMSIZE;

	sc->sc_copytodesc = lance_copytobuf_contig;
	sc->sc_copyfromdesc = lance_copyfrombuf_contig;
	sc->sc_copytobuf = lance_copytobuf_contig;
	sc->sc_copyfrombuf = lance_copyfrombuf_contig;
	sc->sc_zerobuf = lance_zerobuf_contig;

	sc->sc_rdcsr = le_pci_rdcsr;
	sc->sc_wrcsr = le_pci_wrcsr;
	sc->sc_hwinit = NULL;

	sc->sc_supmedia = le_pci_supmedia;
	sc->sc_nsupmedia = sizeof(le_pci_supmedia) / sizeof(int);
	sc->sc_defaultmedia = le_pci_supmedia[0];
	sc->sc_mediachange = le_pci_mediachange;
	lesc->sc_currentmedia = le_pci_supmedia[0];

	aprint_normal("%s", device_xname(self));
	am79900_config(&lesc->sc_am79900);

	/* Chip is stopped. Set "software style" to 32-bit. */
	bus_space_write_2(iot, ioh, PCNET_PCI_RAP, 20);
	bus_space_write_2(iot, ioh, PCNET_PCI_BDP, 2);

	/* Enable the card. */
	csr = pci_conf_read(pc, pa->pa_tag,
	    PCI_COMMAND_STATUS_REG);
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	lesc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, am79900_intr, sc);
	if (lesc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);
}
