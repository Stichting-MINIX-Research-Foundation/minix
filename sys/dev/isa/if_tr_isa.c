/*	$NetBSD: if_tr_isa.c,v 1.24 2012/10/27 17:18:24 chs Exp $	*/

/* XXXJRT changes isa_attach_args too early!! */

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Onno van der Linden.
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
__KERNEL_RCSID(0, "$NetBSD: if_tr_isa.c,v 1.24 2012/10/27 17:18:24 chs Exp $");

#undef TRISADEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/isa/isavar.h>

#include <dev/ic/tropicreg.h>
#include <dev/ic/tropicvar.h>


int	tr_isa_probe(device_t, cfdata_t, void *);
int	trtcm_isa_probe(device_t, cfdata_t, void *);
int	tribm_isa_probe(device_t, cfdata_t, void *);
void	tr_isa_attach(device_t, device_t, void *);
int	tr_isa_map_io(struct isa_attach_args *, bus_space_handle_t *,
	    bus_space_handle_t *);
void	tr_isa_unmap_io(struct isa_attach_args *, bus_space_handle_t,
	    bus_space_handle_t);
int	trtcm_isa_mediachange(struct tr_softc *);
void	trtcm_isa_mediastatus(struct tr_softc *, struct ifmediareq *);
#ifdef TRISADEBUG
void	tr_isa_dumpaip(bus_space_tag_t, bus_space_handle_t);
#endif

/*
 * List of manufacturer specific probe routines.  Order is important.
 */
int	(*tr_isa_probe_list[])(device_t, cfdata_t, void *) = {
		trtcm_isa_probe,
		tribm_isa_probe,
		0
	};

CFATTACH_DECL_NEW(tr_isa, sizeof(struct tr_softc),
    tr_isa_probe, tr_isa_attach, NULL, NULL);

int
tr_isa_map_io(struct isa_attach_args *ia, bus_space_handle_t *pioh, bus_space_handle_t *mmioh)
{
	bus_size_t mmio;
	u_int8_t s;

	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr,
	    ia->ia_io[0].ir_size, 0, pioh)) {
		printf("tr_isa_map_io: can't map PIO ports\n");
		return 1;
	}

	/* Read adapter switches and calculate addresses of MMIO. */
	s = bus_space_read_1(ia->ia_iot, *pioh, TR_SWITCH);

	if ((s & 0xfc) < ((TR_MMIO_MINADDR - TR_MMIO_OFFSET) >> 11) ||
	    (s & 0xfc) > ((TR_MMIO_MAXADDR - TR_MMIO_OFFSET) >> 11)) {
		bus_space_unmap(ia->ia_iot, *pioh, ia->ia_io[0].ir_size);
		return 1;
	}

	mmio = ((s & 0xfc) << 11) + TR_MMIO_OFFSET;
	if (bus_space_map(ia->ia_memt, mmio, TR_MMIO_SIZE, 0, mmioh)) {
		printf("tr_isa_map_io: can't map MMIO region 0x%05lx/%d\n",
			(u_long)mmio, TR_MMIO_SIZE);
		bus_space_unmap(ia->ia_iot, *pioh, ia->ia_io[0].ir_size);
		return 1;
	}
	return 0;
}

void
tr_isa_unmap_io(struct isa_attach_args *ia, bus_space_handle_t pioh, bus_space_handle_t mmioh)
{
	bus_space_unmap(ia->ia_memt, mmioh, TR_MMIO_SIZE);
	bus_space_unmap(ia->ia_iot, pioh, ia->ia_io[0].ir_size);
}

static u_char tr_isa_id[] = {
	5, 0, 4, 9, 4, 3, 4, 15, 3, 6, 3, 1, 3, 1, 3, 0, 3, 9, 3, 9, 3, 0, 2, 0
};

/*
 * XXX handle multiple IBM TR cards (sram mapping !!)
 */

int
tr_isa_probe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	int	i;
	bus_size_t	tr_id;
	bus_space_handle_t sramh, pioh, mmioh;
	int probecode;
	int matched = 0;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_niomem < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	for (i = 0; tr_isa_probe_list[i] != 0; i++) {
		probecode = tr_isa_probe_list[i](parent, match, aux);
		if (probecode < 0)
			return 0;	/* Fail instantly. */
		if (probecode > 0)
			break;		/* We have a match. */
	}
	if (tr_isa_probe_list[i] == 0)
		return 0;		/* Nothing matched. */
	if (tr_isa_map_io(ia, &pioh, &mmioh))
		return 0;
	tr_id = TR_ID_OFFSET;
	matched = 1;
	for (i = 0; i < sizeof(tr_isa_id); i++) {
		if (bus_space_read_1(ia->ia_memt, mmioh, tr_id) !=
		    tr_isa_id[i])
			matched = 0;
		tr_id += 2;
	}
#ifdef TRISADEBUG
	tr_isa_dumpaip(ia->ia_memt, mmioh);
#endif
	tr_isa_unmap_io(ia, pioh, mmioh);
	if (!matched) {
		return 0;
	}
	if (bus_space_map(ia->ia_memt, ia->ia_iomem[0].ir_addr,
	    ia->ia_iomem[0].ir_size, 0, &sramh)) {
		printf("tr_isa_probe: can't map shared ram\n");
		return 0;
	}
	bus_space_unmap(ia->ia_memt, sramh, ia->ia_iomem[0].ir_size);

	ia->ia_nio = 1;
	ia->ia_niomem = 1;
	ia->ia_nirq = 1;

	ia->ia_ndrq = 0;

	return 1;
}

int trtcm_setspeed(struct tr_softc *, int);

void
tr_isa_attach(device_t parent, device_t self, void *aux)
{
	struct tr_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;

	printf("\n");

	sc->sc_dev = self;
	sc->sc_piot = ia->ia_iot;
	sc->sc_memt = ia->ia_memt;
	if (tr_isa_map_io(ia, &sc->sc_pioh, &sc->sc_mmioh)) {
		printf("tr_isa_attach: IO space vanished\n");
		return;
	}
	if (bus_space_map(sc->sc_memt, ia->ia_iomem[0].ir_addr,
	    ia->ia_iomem[0].ir_size, 0, &sc->sc_sramh)) {
		printf("tr_isa_attach: shared ram space vanished\n");
		return;
	}
	/* set ACA offset */
	sc->sc_aca = TR_ACA_OFFSET;
	sc->sc_memwinsz = ia->ia_iomem[0].ir_size;
	sc->sc_maddr = ia->ia_iomem[0].ir_addr;
	/*
	 * Determine total RAM on adapter and decide how much to use.
	 * XXX Since we don't use RAM paging, use sc_memwinsz for now.
	 */
	sc->sc_memsize = sc->sc_memwinsz;
	sc->sc_memreserved = 0;

	if (tr_reset(sc) != 0)
		return;

	if (ia->ia_aux != NULL) {
		sc->sc_mediastatus = trtcm_isa_mediastatus;
		sc->sc_mediachange = trtcm_isa_mediachange;
	}
	else {
		sc->sc_mediastatus = NULL;
		sc->sc_mediachange = NULL;
	}

	if (tr_attach(sc) != 0)
		return;

/*
 * XXX 3Com 619 can use LEVEL intr
 */
	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_NET, tr_intr, sc);
}

#ifdef TRISADEBUG
/*
 * Dump the adapters AIP
 */
void
tr_isa_dumpaip(bus_space_tag_t memt, bus_space_handle_t mmioh)
{
	unsigned int off, val;
	printf("AIP contents:");
	for (off=0; off < 256; off++) {
		val = bus_space_read_1(memt, mmioh, TR_MAC_OFFSET + off);
		if ((off % 16) == 0)
			printf("\n");
		printf("%02x ", val);
	}
	printf("\n");
}
#endif
