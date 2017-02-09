/*	$NetBSD: if_le_isa.c,v 1.49 2010/11/13 13:52:03 uebayasi Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: if_le_isa.c,v 1.49 2010/11/13 13:52:03 uebayasi Exp $");

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
#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/lancereg.h>
#include <dev/ic/lancevar.h>
#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#include <dev/isa/if_levar.h>

int ne2100_isa_probe(device_t, cfdata_t, void *);
int bicc_isa_probe(device_t, cfdata_t, void *);
void le_dummyattach(device_t, device_t, void *);
int le_dummyprobe(device_t, cfdata_t, void *);
void le_ne2100_attach(device_t, device_t, void *);
void le_bicc_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(nele, 0,
    ne2100_isa_probe, le_dummyattach, NULL, NULL);

CFATTACH_DECL_NEW(le_nele, sizeof(struct le_softc),
    le_dummyprobe, le_ne2100_attach, NULL, NULL);

CFATTACH_DECL_NEW(bicc, 0,
    bicc_isa_probe, le_dummyattach, NULL, NULL);

CFATTACH_DECL_NEW(le_bicc, sizeof(struct le_softc),
    le_dummyprobe, le_bicc_attach, NULL, NULL);

struct le_isa_params {
	const char *name;
	int iosize, rap, rdp;
	int macstart, macstride;
} ne2100_params = {
	"NE2100",
	24, NE2100_RAP, NE2100_RDP,
	0, 1
}, bicc_params = {
	"BICC Isolan",
	16, BICC_RAP, BICC_RDP,
	0, 2
};

int lance_isa_probe(struct isa_attach_args *, struct le_isa_params *, int);
void le_isa_attach(device_t, struct le_softc *,
    struct isa_attach_args *, struct le_isa_params *);

int le_isa_intredge(void *);

#if defined(_KERNEL_OPT)
#include "opt_ddb.h"
#endif

#ifdef DDB
#define	integrate
#define hide
#else
#define	integrate	static inline
#define hide		static
#endif

hide void le_isa_wrcsr(struct lance_softc *, uint16_t, uint16_t);
hide uint16_t le_isa_rdcsr(struct lance_softc *, uint16_t);

#define	LE_ISA_MEMSIZE	16384

hide void
le_isa_wrcsr(struct lance_softc *sc, uint16_t port, uint16_t val)
{
	struct le_softc *lesc = (struct le_softc *)sc;
	bus_space_tag_t iot = lesc->sc_iot;
	bus_space_handle_t ioh = lesc->sc_ioh;

	bus_space_write_2(iot, ioh, lesc->sc_rap, port);
	bus_space_write_2(iot, ioh, lesc->sc_rdp, val);
}

hide uint16_t
le_isa_rdcsr(struct lance_softc *sc, uint16_t port)
{
	struct le_softc *lesc = (struct le_softc *)sc;
	bus_space_tag_t iot = lesc->sc_iot;
	bus_space_handle_t ioh = lesc->sc_ioh;
	uint16_t val;

	bus_space_write_2(iot, ioh, lesc->sc_rap, port);
	val = bus_space_read_2(iot, ioh, lesc->sc_rdp);
	return (val);
}

int
ne2100_isa_probe(device_t parent, cfdata_t cf, void *aux)
{

	return (lance_isa_probe(aux, &ne2100_params, cf->cf_flags));
}

int
bicc_isa_probe(device_t parent, cfdata_t cf, void *aux)
{

	return (lance_isa_probe(aux, &bicc_params, cf->cf_flags));
}

/*
 * Determine which chip is present on the card.
 */
int
lance_isa_probe(struct isa_attach_args *ia, struct le_isa_params *p, int flags)
{
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int rap, rdp;
	int rv = 0;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);
	if (ia->ia_ndrq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* Disallow wildcarded i/o address. */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);
	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ)
		return (0);
	if ((flags & LANCEISA_FLAG_LOCALBUS) == 0 &&
	    ia->ia_drq[0].ir_drq == ISA_UNKNOWN_DRQ)
		return (0);

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_io[0].ir_addr, p->iosize, 0, &ioh))
		return (0);

	rap = p->rap;
	rdp = p->rdp;

	/* Stop the LANCE chip and put it in a known state. */
	bus_space_write_2(iot, ioh, rap, LE_CSR0);
	bus_space_write_2(iot, ioh, rdp, LE_C0_STOP);
	delay(100);

	bus_space_write_2(iot, ioh, rap, LE_CSR0);
	if (bus_space_read_2(iot, ioh, rdp) != LE_C0_STOP)
		goto bad;

	bus_space_write_2(iot, ioh, rap, LE_CSR3);
	bus_space_write_2(iot, ioh, rdp, 0);

	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = p->iosize;

	ia->ia_nirq = 1;

	if ((flags & LANCEISA_FLAG_LOCALBUS) != 0 &&
	    ia->ia_drq[0].ir_drq == ISA_UNKNOWN_DRQ)
	    ia->ia_ndrq = 0;
	else
	    ia->ia_ndrq = 1;

	ia->ia_niomem = 0;

	rv = 1;

bad:
	bus_space_unmap(iot, ioh, p->iosize);
	return (rv);
}

void
le_dummyattach(device_t parent, device_t self, void *aux)
{

	aprint_normal("\n");

	config_found(self, aux, 0);
}

int
le_dummyprobe(device_t parent, cfdata_t cf, void *aux)
{

	return (1);
}

void
le_ne2100_attach(device_t parent, device_t self, void *aux)
{
	struct le_softc *lesc = device_private(self);
	struct lance_softc *sc = &lesc->sc_am7990.lsc;

	sc->sc_dev = self;
	le_isa_attach(parent, lesc, aux, &ne2100_params);
}

void
le_bicc_attach(device_t parent, device_t self, void *aux)
{
	struct le_softc *lesc = device_private(self);
	struct lance_softc *sc = &lesc->sc_am7990.lsc;

	sc->sc_dev = self;
	le_isa_attach(parent, lesc, aux, &bicc_params);
}

void
le_isa_attach(device_t parent, struct le_softc *lesc,
    struct isa_attach_args *ia, struct le_isa_params *p)
{
	struct lance_softc *sc = &lesc->sc_am7990.lsc;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	bus_dma_tag_t dmat = ia->ia_dmat;
	bus_dma_segment_t seg;
	int i, rseg, error;

	aprint_normal(": %s Ethernet\n", p->name);

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, p->iosize, 0, &ioh))
		panic("%s: can't map io", device_xname(sc->sc_dev));

	/*
	 * Extract the physical MAC address from the ROM.
	 */
	for (i = 0; i < sizeof(sc->sc_enaddr); i++)
		sc->sc_enaddr[i] =
		    bus_space_read_1(iot, ioh, p->macstart + i * p->macstride);

	lesc->sc_iot = iot;
	lesc->sc_ioh = ioh;
	lesc->sc_dmat = dmat;
	lesc->sc_rap = p->rap;
	lesc->sc_rdp = p->rdp;

	/*
	 * Allocate a DMA area for the card.
	 */
	if (bus_dmamem_alloc(dmat, LE_ISA_MEMSIZE, PAGE_SIZE, 0, &seg, 1,
			     &rseg, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev,
		    "couldn't allocate memory for card\n");
		return;
	}
	if (bus_dmamem_map(dmat, &seg, rseg, LE_ISA_MEMSIZE,
			   (void **)&sc->sc_mem,
			   BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		aprint_error_dev(sc->sc_dev, "couldn't map memory for card\n");
		return;
	}

	/*
	 * Create and load the DMA map for the DMA area.
	 */
	if (bus_dmamap_create(dmat, LE_ISA_MEMSIZE, 1,
			LE_ISA_MEMSIZE, 0, BUS_DMA_NOWAIT, &lesc->sc_dmam)) {
		aprint_error_dev(sc->sc_dev, "couldn't create DMA map\n");
		bus_dmamem_free(dmat, &seg, rseg);
		return;
	}
	if (bus_dmamap_load(dmat, lesc->sc_dmam,
			sc->sc_mem, LE_ISA_MEMSIZE, NULL, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev, "coundn't load DMA map\n");
		bus_dmamem_free(dmat, &seg, rseg);
		return;
	}

	sc->sc_conf3 = 0;
	sc->sc_addr = lesc->sc_dmam->dm_segs[0].ds_addr;
	sc->sc_memsize = LE_ISA_MEMSIZE;

	sc->sc_copytodesc = lance_copytobuf_contig;
	sc->sc_copyfromdesc = lance_copyfrombuf_contig;
	sc->sc_copytobuf = lance_copytobuf_contig;
	sc->sc_copyfrombuf = lance_copyfrombuf_contig;
	sc->sc_zerobuf = lance_zerobuf_contig;

	sc->sc_rdcsr = le_isa_rdcsr;
	sc->sc_wrcsr = le_isa_wrcsr;
	sc->sc_hwinit = NULL;

	if (ia->ia_ndrq > 0) {
		if ((error = isa_dmacascade(ia->ia_ic,
					    ia->ia_drq[0].ir_drq)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to cascade DRQ, error = %d\n", error);
			return;
		}
	}

	lesc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_NET, le_isa_intredge, sc);

	aprint_normal("%s", device_xname(sc->sc_dev));
	am7990_config(&lesc->sc_am7990);
}

/*
 * Controller interrupt.
 */
int
le_isa_intredge(void *arg)
{

	if (am7990_intr(arg) == 0)
		return (0);
	for (;;)
		if (am7990_intr(arg) == 0)
			return (1);
}
