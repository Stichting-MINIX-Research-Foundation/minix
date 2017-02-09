/*	$NetBSD: if_ai.c,v 1.33 2011/06/03 16:28:40 tsutsui Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Rafal K. Boni.
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
__KERNEL_RCSID(0, "$NetBSD: if_ai.c,v 1.33 2011/06/03 16:28:40 tsutsui Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/ic/i82586reg.h>
#include <dev/ic/i82586var.h>
#include <dev/isa/if_aireg.h>

#ifdef AI_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

struct ai_softc {
	struct ie_softc sc_ie;

	bus_space_tag_t sc_regt;	/* space tag for registers */
	bus_space_handle_t sc_regh;	/* space handle for registers */

	u_int8_t	card_rev;
	u_int8_t	card_type;

	void		*sc_ih;		/* interrupt handle */
};

const char *ai_names[] = {
        "StarLAN 10",
        "EN100",
        "StarLAN Fiber",
};

/* Functions required by the i82586 MI driver */
static void 	ai_reset(struct ie_softc *, int);
static void 	ai_atten(struct ie_softc *, int);

static void	ai_copyin(struct ie_softc *, void *, int, size_t);
static void	ai_copyout(struct ie_softc *, const void *, int, size_t);

static u_int16_t ai_read_16(struct ie_softc *, int);
static void	ai_write_16(struct ie_softc *, int, u_int16_t);
static void	ai_write_24(struct ie_softc *, int, int);

/* Local support functions */
static int 	check_ie_present(struct ie_softc*, bus_space_tag_t,
					bus_space_handle_t, bus_size_t);
static int	ai_find_mem_size(struct ai_softc*, bus_space_tag_t,
					bus_size_t);

int ai_match(device_t, cfdata_t, void *);
void ai_attach(device_t, device_t, void *);

/*
 * AT&T StarLan support routines
 */
static void
ai_reset(struct ie_softc *sc, int why)
{
	struct ai_softc* asc = (struct ai_softc *) sc;

	switch (why) {
	case CHIP_PROBE:
		/* reset to chip to see if it responds */
		bus_space_write_1(asc->sc_regt, asc->sc_regh, AI_RESET, 0);
		DELAY(100);
		break;

	case CARD_RESET:
		/*
		 * this takes around 10sec, and we can get
		 * by quite well w/out it...
		 */
		break;
	}
}

static void
ai_atten(struct ie_softc *sc, int why)
{
    struct ai_softc* asc = (struct ai_softc *) sc;
    bus_space_write_1(asc->sc_regt, asc->sc_regh, AI_ATTN, 0);
}

static void
ai_copyin (struct ie_softc *sc, void *dst, int offset, size_t size)
{
	int dribble;
	u_int8_t* bptr = dst;

	bus_space_barrier(sc->bt, sc->bh, offset, size,
			  BUS_SPACE_BARRIER_READ);

	if (offset % 2) {
		*bptr = bus_space_read_1(sc->bt, sc->bh, offset);
		offset++; bptr++; size--;
	}

	dribble = size % 2;
	bus_space_read_region_2(sc->bt, sc->bh, offset, (u_int16_t *) bptr,
				size >> 1);

	if (dribble) {
		bptr += size - 1;
		offset += size - 1;
		*bptr = bus_space_read_1(sc->bt, sc->bh, offset);
	}
}

static void
ai_copyout (struct ie_softc *sc, const void *src, int offset, size_t size)
{
	int dribble;
	int osize = size;
	int ooffset = offset;
	const u_int8_t* bptr = src;

	if (offset % 2) {
		bus_space_write_1(sc->bt, sc->bh, offset, *bptr);
		offset++; bptr++; size--;
	}

	dribble = size % 2;
	bus_space_write_region_2(sc->bt, sc->bh, offset,
	    (const u_int16_t *)bptr, size >> 1);
	if (dribble) {
		bptr += size - 1;
		offset += size - 1;
		bus_space_write_1(sc->bt, sc->bh, offset, *bptr);
	}

	bus_space_barrier(sc->bt, sc->bh, ooffset, osize,
			  BUS_SPACE_BARRIER_WRITE);
}

static u_int16_t
ai_read_16 (struct ie_softc *sc, int offset)
{
	bus_space_barrier(sc->bt, sc->bh, offset, 2, BUS_SPACE_BARRIER_READ);
        return bus_space_read_2(sc->bt, sc->bh, offset);
}

static void
ai_write_16 (struct ie_softc *sc, int offset, u_int16_t value)
{
        bus_space_write_2(sc->bt, sc->bh, offset, value);
	bus_space_barrier(sc->bt, sc->bh, offset, 2, BUS_SPACE_BARRIER_WRITE);
}

static void
ai_write_24 (struct ie_softc *sc, int offset, int addr)
{
        bus_space_write_4(sc->bt, sc->bh, offset, addr +
                                (u_long) sc->sc_maddr - (u_long) sc->sc_iobase);
	bus_space_barrier(sc->bt, sc->bh, offset, 4, BUS_SPACE_BARRIER_WRITE);
}

int
ai_match(device_t parent, cfdata_t cf, void *aux)
{
	int rv = 0;
	u_int8_t val, type;
	bus_size_t memsize;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct isa_attach_args * const ia = aux;
	struct ai_softc asc;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_niomem < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* Punt if wildcarded port, IRQ or memory address */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT ||
	    ia->ia_iomem[0].ir_addr == ISA_UNKNOWN_IOMEM ||
	    ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ) {
		DPRINTF((
		 "ai_match: wildcarded IRQ, IOAddr, or memAddr, skipping\n"));
		return (0);
	}

	iot = ia->ia_iot;

	/*
	 * This probe is horribly bad, but I have no info on this card other
	 * than the former driver, and it was just as bad!
	 */
	if (bus_space_map(iot, ia->ia_io[0].ir_addr,
			  AI_IOSIZE, 0, &ioh) != 0) {

		DPRINTF(("ai_match: cannot map %d IO ports @ 0x%x\n",
			 AI_IOSIZE, ia->ia_iobase));
		return (0);
	}

	val = bus_space_read_1(iot, ioh, AI_REVISION);

	type = SL_BOARD(val);
	if (type != SL10_BOARD && type != EN100_BOARD &&
	    type != SLFIBER_BOARD) {
		DPRINTF(("ai_match: unknown board code 0x%02x @ 0x%x\n",
			 type, ia->ia_iobase));
		goto out;
	}

	/*
	 * Fill in just about enough of our local `ai_softc' for
	 * ai_find_mem_size() to do its job.
	 */
	memset(&asc, 0, sizeof asc);
	asc.sc_regt = iot;
	asc.sc_regh = ioh;

	if ((memsize = ai_find_mem_size(&asc, ia->ia_memt,
	     ia->ia_iomem[0].ir_addr)) == 0) {
		DPRINTF(("ai_match: cannot size memory of board @ 0x%x\n",
			 ia->ia_io[0].ir_addr));
		goto out;
	}

	if (ia->ia_iomem[0].ir_size != 0 &&
	    ia->ia_iomem[0].ir_size != memsize) {
		DPRINTF((
		   "ai_match: memsize of board @ 0x%x doesn't match config\n",
		   ia->ia_iobase));
		goto out;
	}

	rv = 1;

	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = AI_IOSIZE;

	ia->ia_niomem = 1;
	ia->ia_iomem[0].ir_size = memsize;

	ia->ia_nirq = 1;

	ia->ia_ndrq = 0;

	DPRINTF(("ai_match: found board @ 0x%x\n", ia->ia_iobase));

out:
	bus_space_unmap(iot, ioh, AI_IOSIZE);
	return rv;
}

void
ai_attach(device_t parent, device_t self, void *aux)
{
	struct ai_softc *asc = device_private(self);
	struct ie_softc *sc = &asc->sc_ie;
	struct isa_attach_args *ia = aux;

	u_int8_t val = 0;
	bus_space_handle_t ioh, memh;
	u_int8_t ethaddr[ETHER_ADDR_LEN];
	char name[80];

	sc->sc_dev = self;

	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr,
			  ia->ia_io[0].ir_size, 0, &ioh) != 0) {
		DPRINTF(("\n%s: can't map i/o space 0x%x-0x%x\n",
			 device_xname(self),
		         ia->ia_io[0].ir_addr, ia->ia_io[0].ir_addr +
		         ia->ia_io[0].ir_size - 1));
		return;
	}

	if (bus_space_map(ia->ia_memt, ia->ia_iomem[0].ir_addr,
			  ia->ia_iomem[0].ir_size, 0, &memh) != 0) {
		DPRINTF(("\n%s: can't map iomem space 0x%x-0x%x\n",
			 device_xname(self),
			 ia->ia_iomem[0].ir_addr, ia->ia_iomem[0].ir_addr +
			 ia->ia_iomem[0].ir_size - 1));
		bus_space_unmap(ia->ia_iot, ioh, ia->ia_io[0].ir_size);
		return;
	}

	asc->sc_regt = ia->ia_iot;
	asc->sc_regh = ioh;

	sc->hwinit = NULL;
	sc->intrhook = NULL;
	sc->hwreset = ai_reset;
	sc->chan_attn = ai_atten;

	sc->ie_bus_barrier = NULL;

	sc->memcopyin = ai_copyin;
	sc->memcopyout = ai_copyout;
	sc->ie_bus_read16 = ai_read_16;
	sc->ie_bus_write16 = ai_write_16;
	sc->ie_bus_write24 = ai_write_24;

	sc->do_xmitnopchain = 0;

	sc->sc_mediachange = NULL;
	sc->sc_mediastatus = NULL;

	sc->bt = ia->ia_memt;
	sc->bh = memh;

	/* Map i/o space. */
	sc->sc_msize = ia->ia_iomem[0].ir_size;
	sc->sc_maddr = (void *)memh;
	sc->sc_iobase = (char *)sc->sc_maddr + sc->sc_msize - (1 << 24);

	/* set up pointers to important on-card control structures */
	sc->iscp = 0;
	sc->scb = IE_ISCP_SZ;
	sc->scp = sc->sc_msize + IE_SCP_ADDR - (1 << 24);

	sc->buf_area = sc->scb + IE_SCB_SZ;
	sc->buf_area_sz = sc->sc_msize - IE_ISCP_SZ - IE_SCB_SZ - IE_SCP_SZ;

	/* zero card memory */
	bus_space_set_region_1(sc->bt, sc->bh, 0, 0, sc->sc_msize);

	/* set card to 16-bit bus mode */
	bus_space_write_1(sc->bt, sc->bh, IE_SCP_BUS_USE((u_long)sc->scp),
			  IE_SYSBUS_16BIT);

	/* set up pointers to key structures */
	ai_write_24(sc, IE_SCP_ISCP((u_long)sc->scp), (u_long) sc->iscp);
	ai_write_16(sc, IE_ISCP_SCB((u_long)sc->iscp), (u_long) sc->scb);
	ai_write_24(sc, IE_ISCP_BASE((u_long)sc->iscp), (u_long) sc->iscp);

	/* flush setup of pointers, check if chip answers */
	bus_space_barrier(sc->bt, sc->bh, 0, sc->sc_msize,
			  BUS_SPACE_BARRIER_WRITE);
	if (!i82586_proberam(sc)) {
		DPRINTF(("\n%s: can't talk to i82586!\n",
			device_xname(self)));
		bus_space_unmap(ia->ia_iot, ioh, ia->ia_io[0].ir_size);
		bus_space_unmap(ia->ia_memt, memh, ia->ia_iomem[0].ir_size);
		return;
	}

	val = bus_space_read_1(asc->sc_regt, asc->sc_regh, AI_REVISION);
	asc->card_rev = SL_REV(val);
	asc->card_type = SL_BOARD(val) - 1;
	snprintf(name, sizeof(name), "%s, rev. %d",
	    ai_names[asc->card_type], asc->card_rev);

	i82586_attach(sc, name, ethaddr, NULL, 0, 0);

	asc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_NET, i82586_intr, sc);
	if (asc->sc_ih == NULL) {
		DPRINTF(("\n%s: can't establish interrupt\n",
			device_xname(self)));
	}
}

/*
 * Divine the memory size of this board.
 * Better hope there's nothing important hiding just below the card...
 */
static int
ai_find_mem_size(struct ai_softc* asc, bus_space_tag_t memt, bus_size_t maddr)
{
	int size;
	bus_space_handle_t memh;
	struct ie_softc* sc = &asc->sc_ie;

	for (size = 65536; size >= 16384; size -= 16384) {
		if (bus_space_map(memt, maddr, size, 0, &memh) == 0) {
			size = check_ie_present(sc, memt, maddr, size);
			bus_space_unmap(memt, memh, size);

			if (size != 0)
				return size;
		}
	}

	return (0);
}

/*
 * Check to see if there's an 82586 out there.
 */
static int
check_ie_present(struct ie_softc* sc, bus_space_tag_t memt, bus_space_handle_t memh, bus_size_t size)
{
	sc->hwreset = ai_reset;
	sc->chan_attn = ai_atten;
	sc->ie_bus_read16 = ai_read_16;
	sc->ie_bus_write16 = ai_write_16;

	sc->bt = memt;
	sc->bh = memh;
	sc->sc_iobase = (char *)memh + size - (1 << 24);

	sc->scp = size + IE_SCP_ADDR - (1 << 24);
	bus_space_set_region_1(memt, memh, (u_long) sc->scp, 0, IE_SCP_SZ);

	sc->iscp = 0;
	bus_space_set_region_1(memt, memh, (u_long) sc->iscp, 0, IE_ISCP_SZ);

	sc->scb = IE_ISCP_SZ;
	bus_space_set_region_1(memt, memh, sc->scb, 0, IE_SCB_SZ);

	/* set card to 16-bit bus mode */
	bus_space_write_1(sc->bt, sc->bh, IE_SCP_BUS_USE((u_long)sc->scp),
			  IE_SYSBUS_16BIT);

	/* set up pointers to key structures */
	ai_write_24(sc, IE_SCP_ISCP((u_long)sc->scp), (u_long) sc->iscp);
	ai_write_16(sc, IE_ISCP_SCB((u_long)sc->iscp), (u_long) sc->scb);
	ai_write_24(sc, IE_ISCP_BASE((u_long)sc->iscp), (u_long) sc->iscp);

	/* flush setup of pointers, check if chip answers */
	bus_space_barrier(sc->bt, sc->bh, 0, sc->sc_msize,
			  BUS_SPACE_BARRIER_WRITE);

	if (!i82586_proberam(sc))
		return (0);

	return (size);
}

CFATTACH_DECL_NEW(ai, sizeof(struct ai_softc),
    ai_match, ai_attach, NULL, NULL);
