/*	$NetBSD: if_ef.c,v 1.31 2011/06/03 16:28:40 tsutsui Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: if_ef.c,v 1.31 2011/06/03 16:28:40 tsutsui Exp $");

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
#include <dev/isa/if_efreg.h>
#include <dev/isa/elink.h>

#ifdef EF_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

struct ef_softc {
	struct ie_softc sc_ie;

	bus_space_tag_t sc_regt;	/* space tag for registers */
	bus_space_handle_t sc_regh;	/* space handle for registers */

	void* sc_ih;			/* interrupt handle */

	u_int8_t card_rev;		/* hardware revision */
	u_int8_t card_type;		/* card model -- AUI/BNC or TP */
};

int ef_media[] = {
	IFM_ETHER | IFM_10_5,
	IFM_ETHER | IFM_10_2,
};
#define NEF_MEDIA       (sizeof(ef_media) / sizeof(ef_media[0]))

int eftp_media[] = {
	IFM_ETHER | IFM_10_T,
};
#define NEFTP_MEDIA       (sizeof(eftp_media) / sizeof(eftp_media[0]))

/* Routines required by the MI i82586 driver API */
static void 	ef_reset(struct ie_softc *, int);
static void 	ef_hwinit(struct ie_softc *);
static void 	ef_atten(struct ie_softc *, int);
static int 	ef_intrhook(struct ie_softc *, int);

static void	ef_copyin(struct ie_softc *, void *, int, size_t);
static void	ef_copyout(struct ie_softc *, const void *, int, size_t);

static u_int16_t ef_read_16(struct ie_softc *, int);
static void	ef_write_16(struct ie_softc *, int, u_int16_t);
static void	ef_write_24(struct ie_softc *, int, int);

static void	ef_mediastatus(struct ie_softc *, struct ifmediareq *);

/* Local routines */
static int 	ef_port_check(bus_space_tag_t, bus_space_handle_t);

int ef_match(device_t, cfdata_t, void *);
void ef_attach(device_t, device_t, void *);

/*
 * This keeps track of which ISAs have been through an ie probe sequence.
 * A simple static variable isn't enough, since it's conceivable that
 * a system might have more than one ISA bus.
 *
 * The "isa_bus" member is a pointer to the parent ISA bus device struct
 * which will unique per ISA bus.
 */

#define MAXCARDS_PER_ISABUS     8       /* if you have more than 8, you lose */

struct ef_isabus {
	LIST_ENTRY(ef_isabus) isa_link;
	device_t isa_bus;

	int bus_state;

	struct card {
		bus_addr_t iobase;
		bus_addr_t maddr;
		bus_size_t msize;
		int irq;
		int available;
	} isa_cards[MAXCARDS_PER_ISABUS];
};

static LIST_HEAD(, ef_isabus) ef_isa_buses;
static int ef_isa_buses_inited;

static void
ef_card_add(
    struct ef_isabus *bus,
    bus_addr_t iobase,
    bus_addr_t maddr,
    bus_size_t msiz,
    int irq)
{
	int idx;

	DPRINTF(("Adding 3c507 at 0x%x, IRQ %d, Mem 0x%lx/%ld\n",
		 (u_int) iobase, irq, (u_long) maddr, msiz));

	for (idx = 0; idx < MAXCARDS_PER_ISABUS; idx++) {
		if (bus->isa_cards[idx].available == 0) {
			bus->isa_cards[idx].iobase = iobase;
			bus->isa_cards[idx].maddr = maddr;
			bus->isa_cards[idx].msize = msiz;
			bus->isa_cards[idx].irq = irq;
			bus->isa_cards[idx].available = 1;
			break;
		}
	}
}

/*
 * 3C507 support routines
 */
static void
ef_reset(struct ie_softc *sc, int why)
{
	struct ef_softc* esc = (struct ef_softc *) sc;

	switch (why) {
	case CHIP_PROBE:
		/* reset to chip to see if it responds */
		bus_space_write_1(esc->sc_regt, esc->sc_regh,
				  EF_CTRL, EF_CTRL_RESET);
		DELAY(100);
		bus_space_write_1(esc->sc_regt, esc->sc_regh,
				  EF_CTRL, EF_CTRL_NORMAL);
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
ef_atten(struct ie_softc *sc, int why)
{
	struct ef_softc* esc = (struct ef_softc *) sc;
	bus_space_write_1(esc->sc_regt, esc->sc_regh, EF_ATTN, 1);
}

static void
ef_hwinit(struct ie_softc *sc)
{
	struct ef_softc* esc = (struct ef_softc *) sc;
	bus_space_write_1(esc->sc_regt, esc->sc_regh, EF_ICTRL, 1);
}

static int
ef_intrhook(struct ie_softc *sc, int where)
{
	unsigned char cr;
	struct ef_softc* esc = (struct ef_softc *) sc;

	switch (where) {
	case INTR_ENTER:
		/* entering ISR: disable, ack card interrupts */
		cr = bus_space_read_1(esc->sc_regt, esc->sc_regh, EF_CTRL);
		bus_space_write_1(esc->sc_regt, esc->sc_regh, EF_CTRL,
				  cr & ~EF_CTRL_IEN);
		bus_space_write_1(esc->sc_regt, esc->sc_regh, EF_ICTRL, 1);
		break;

	case INTR_EXIT:
		/* exiting ISR: re-enable card interrupts */
		cr = bus_space_read_1(esc->sc_regt, esc->sc_regh, EF_CTRL);
		bus_space_write_1(esc->sc_regt, esc->sc_regh, EF_CTRL,
				  cr | EF_CTRL_IEN);
		break;

	case INTR_LOOP:
		/* looping in ISR: ack new interrupts */
		bus_space_write_1(esc->sc_regt, esc->sc_regh, EF_ICTRL, 1);
		break;
    }

    return 1;
}

static u_int16_t
ef_read_16 (struct ie_softc *sc, int offset)
{
	bus_space_barrier(sc->bt, sc->bh, offset, 2, BUS_SPACE_BARRIER_READ);
	return bus_space_read_2(sc->bt, sc->bh, offset);
}

static void
ef_copyin (struct ie_softc *sc, void *dst, int offset, size_t size)
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
ef_copyout (struct ie_softc *sc, const void *src, int offset, size_t size)
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

static void
ef_write_16 (struct ie_softc *sc, int offset, u_int16_t value)
{
	bus_space_write_2(sc->bt, sc->bh, offset, value);
	bus_space_barrier(sc->bt, sc->bh, offset, 2, BUS_SPACE_BARRIER_WRITE);
}

static void
ef_write_24 (struct ie_softc *sc, int offset, int addr)
{
	bus_space_write_4(sc->bt, sc->bh, offset, addr +
			  (u_long) sc->sc_maddr - (u_long) sc->sc_iobase);
	bus_space_barrier(sc->bt, sc->bh, offset, 4, BUS_SPACE_BARRIER_WRITE);
}

static void
ef_mediastatus(struct ie_softc *sc, struct ifmediareq *ifmr)
{
        struct ifmedia *ifm = &sc->sc_media;

        /*
         * The currently selected media is always the active media.
         */
        ifmr->ifm_active = ifm->ifm_cur->ifm_media;
}

int
ef_match(device_t parent, cfdata_t cf, void *aux)
{
	struct isa_attach_args * const ia = aux;

	int idx;
	struct ef_isabus *bus;

	bus_space_handle_t ioh;
	bus_space_tag_t iot = ia->ia_iot;

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	if (ef_isa_buses_inited == 0) {
		LIST_INIT(&ef_isa_buses);
		ef_isa_buses_inited = 1;
	}

	/*
	 * Probe this bus if we haven't done so already.
	 */
	for (bus = ef_isa_buses.lh_first; bus != NULL;
	     bus = bus->isa_link.le_next) {
		if (bus->isa_bus == parent)
			break;
	}

	if (bus == NULL) {
		bus_addr_t iobase;

		/*
		 * Mark this bus so we don't probe it again.
		 */
		bus = (struct ef_isabus *)
			malloc(sizeof(struct ef_isabus), M_DEVBUF, M_NOWAIT);
		if (bus == NULL)
		    panic("ef_isa_probe: can't allocate state storage for %s",
			  device_xname(parent));

		bus->bus_state = 0;		/* nothing done yet */
		bus->isa_bus = parent;

		LIST_INSERT_HEAD(&ef_isa_buses, bus, isa_link);

		if (bus_space_map(iot, ELINK_ID_PORT, 1, 0, &ioh)) {
			DPRINTF(("3c507 probe: can't map Etherlink ID port\n"));
			return 0;
		}

		/*
		 * Reset and put card in CONFIG state without
		 * changing address.
		 */
		elink_reset(iot, ioh, device_unit(parent));
		elink_idseq(iot, ioh, ELINK_507_POLY);
		elink_idseq(iot, ioh, ELINK_507_POLY);
		bus_space_write_1(iot, ioh, 0, 0xff);

		/* Unmap the ID port */
		bus_space_unmap(iot, ioh, 1);

		bus->bus_state++;	/* cards now in CONFIG state */

		for (iobase = EF_IOBASE_LOW; iobase <= EF_IOBASE_HIGH;
		     iobase += EF_IOSIZE) {
			/*
			 * Map the 507's port-space for the probe sequence.
			 */
			if (bus_space_map(iot, iobase, EF_IOSIZE,
					  0, &ioh) != 0)
				continue;

			/* Now look for the 3Com magic bytes */

			if (ef_port_check(iot, ioh)) {
				int irq;
				u_int8_t v;
				bus_addr_t maddr;
				bus_addr_t msiz1;
				bus_space_handle_t memh;

				irq = bus_space_read_1(iot, ioh, EF_IRQ) &
					EF_IRQ_MASK;

				v = bus_space_read_1(iot, ioh, EF_MADDR);
				maddr = EF_MADDR_BASE +
				      ((v & EF_MADDR_MASK) << EF_MADDR_SHIFT);
				msiz1 = ((v & EF_MSIZE_MASK) + 1) *
					EF_MSIZE_STEP;

				if (bus_space_map(ia->ia_memt, maddr,
						  msiz1, 0, &memh) == 0) {
					    ef_card_add(bus, iobase, maddr,
							msiz1, irq);
					    bus_space_unmap(ia->ia_memt,
							    memh, msiz1);
				}
			}
			bus_space_unmap(iot, ioh, EF_IOSIZE);
		}
	}

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_niomem < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	for (idx = 0; idx < MAXCARDS_PER_ISABUS; idx++) {
		if (bus->isa_cards[idx].available != 1)
			continue;

		if (ia->ia_io[0].ir_addr != ISA_UNKNOWN_PORT &&
		    ia->ia_io[0].ir_addr != bus->isa_cards[idx].iobase)
			continue;

		if (ia->ia_iomem[0].ir_addr != ISA_UNKNOWN_IOMEM &&
		    ia->ia_iomem[0].ir_addr != bus->isa_cards[idx].maddr)
			continue;

		if (ia->ia_irq[0].ir_irq != ISA_UNKNOWN_IRQ &&
		    ia->ia_irq[0].ir_irq != bus->isa_cards[idx].irq)
			continue;

		break;
	}

	if (idx == MAXCARDS_PER_ISABUS)
		return (0);

	bus->isa_cards[idx].available++;

	ia->ia_nio = 1;
	ia->ia_io[0].ir_addr = bus->isa_cards[idx].iobase;
	ia->ia_io[0].ir_size = EF_IOSIZE;

	ia->ia_niomem = 1;
	ia->ia_iomem[0].ir_addr = bus->isa_cards[idx].maddr;
	ia->ia_iomem[0].ir_size = bus->isa_cards[idx].msize;

	ia->ia_nirq = 1;
	ia->ia_irq[0].ir_irq = bus->isa_cards[idx].irq;

	ia->ia_ndrq = 0;

	return (1);
}

void
ef_attach(device_t parent, device_t self, void *aux)
{
	struct ef_softc *esc = device_private(self);
	struct ie_softc *sc = &esc->sc_ie;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;

	int i;
	char vers[20];
	struct ef_isabus *bus;
	u_int8_t partno[EF_TYPE_LEN];
	bus_space_handle_t ioh, memh;
	u_int8_t ethaddr[ETHER_ADDR_LEN];

	sc->sc_dev = self;
	sc->hwinit = ef_hwinit;
	sc->hwreset = ef_reset;
	sc->chan_attn = ef_atten;
	sc->intrhook = ef_intrhook;

	sc->ie_bus_barrier = NULL;

	sc->memcopyin = ef_copyin;
	sc->memcopyout = ef_copyout;
	sc->ie_bus_read16 = ef_read_16;
	sc->ie_bus_write16 = ef_write_16;
	sc->ie_bus_write24 = ef_write_24;

	sc->sc_msize = 0;

	/*
	 * NOP chains don't give any advantage on this card, in fact they
	 * seem to slow it down some.  As the doctor says, "if it hurts,
	 * don't do it".
	 */
	sc->do_xmitnopchain = 0;

	sc->sc_mediachange = NULL;
	sc->sc_mediastatus = ef_mediastatus;

	/* Find the cards parent bus */
	for (bus = ef_isa_buses.lh_first; bus != NULL;
	     bus = bus->isa_link.le_next) {

		if (bus->isa_bus == parent)
			break;
	}

	if (bus == NULL)
		panic("%s: Can't find parent bus!", device_xname(self));


	/* If the bus hasn't been transitioned to the RUN state, do so now */
	if (bus->bus_state == 1) {
		if (bus_space_map(iot, ELINK_ID_PORT, 1, 0, &ioh) != 0) {
			DPRINTF(("\n%s: Can't map Elink ID port!\n",
				device_xname(self)));
			return;
		}

		bus_space_write_1(ia->ia_iot, ioh, 0, 0x00);
		elink_idseq(ia->ia_iot, ioh, ELINK_507_POLY);
		bus_space_write_1(ia->ia_iot, ioh, 0, 0x00);
		bus_space_unmap(ia->ia_iot, ioh, 1);

		bus->bus_state++;
	}

	/* Map i/o space. */
	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr,
			  ia->ia_io[0].ir_size, 0, &ioh) != 0) {

		DPRINTF(("\n%s: can't map i/o space 0x%x-0x%x\n",
			  device_xname(self), ia->ia_io[0].ir_addr,
			  ia->ia_io[0].ir_addr + ia->ia_io[0].ir_size - 1));
		return;
	}

	esc->sc_regt = ia->ia_iot;
	esc->sc_regh = ioh;

	if (bus_space_map(ia->ia_memt, ia->ia_iomem[0].ir_addr,
			  ia->ia_iomem[0].ir_size, 0, &memh) != 0) {

		DPRINTF(("\n%s: can't map iomem space 0x%x-0x%x\n",
			device_xname(self), ia->ia_maddr,
			ia->ia_maddr + ia->ia_msize - 1));
		bus_space_unmap(ia->ia_iot, ioh, ia->ia_io[0].ir_size);
		return;
	}

	sc->bt = ia->ia_memt;
	sc->bh = memh;

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
	ef_write_24(sc, IE_SCP_ISCP((u_long)sc->scp), (u_long) sc->iscp);
	ef_write_16(sc, IE_ISCP_SCB((u_long)sc->iscp), (u_long) sc->scb);
	ef_write_24(sc, IE_ISCP_BASE((u_long)sc->iscp), (u_long) sc->iscp);

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

	/* set bank 2 for card part number and revision */
	bus_space_write_1(esc->sc_regt, esc->sc_regh, EF_CTRL,
			  EF_CTRL_NRST | EF_CTRL_BNK2);

	/* card revision is encoded in BCD */
	i = bus_space_read_1(esc->sc_regt, esc->sc_regh, EF_REV);
	esc->card_rev = 10 * (i / 16) + (i % 16);

	for (i = 0; i < EF_TYPE_LEN; i++)
		partno[i] = bus_space_read_1(esc->sc_regt, esc->sc_regh,
					     EF_TYPE + i);

	/* use part number to guess if card is TP or AUI/BNC model */
	esc->card_type = EF_IS_TP(partno) ? EF_CARD_TP : EF_CARD_BNC;

	/* set bank 0 for ethernet address */
	bus_space_write_1(esc->sc_regt, esc->sc_regh,
			  EF_CTRL, EF_CTRL_NORMAL);

	for (i = 0; i < EF_ADDR_LEN; i++)
		ethaddr[i] = bus_space_read_1(esc->sc_regt, esc->sc_regh,
					      EF_ADDR + i);

	snprintf(vers, sizeof(vers), "%s, rev. %d",
		(esc->card_type == EF_CARD_TP) ? "3C507-TP" : "3C507",
		esc->card_rev);

	if (esc->card_type == EF_CARD_TP)
		i82586_attach(sc, vers, ethaddr, eftp_media, NEFTP_MEDIA,
			      eftp_media[0]);
	else {
		u_int8_t media = bus_space_read_1(esc->sc_regt, esc->sc_regh,
						  EF_MEDIA);
		media = (media & EF_MEDIA_MASK) >> EF_MEDIA_SHIFT;

		i82586_attach(sc, vers, ethaddr, ef_media, NEF_MEDIA,
			      ef_media[media]);
	}

	/* Clear the interrupt latch just in case. */
	bus_space_write_1(esc->sc_regt, esc->sc_regh, EF_ICTRL, 1);

	esc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_NET, i82586_intr, sc);
	if (esc->sc_ih == NULL) {
		DPRINTF(("\n%s: can't establish interrupt\n",
			device_xname(self)));
	}
}

static int
ef_port_check(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	int i;
        u_char ch;
	const u_char* signature = EF_SIGNATURE;

	for (i = 0; i < strlen(signature); i++) {
		ch = bus_space_read_1(iot, ioh, i);
		if (ch != signature[i])
			return 0;
	}

	/* If card is mapped in high memory (above 15Meg), we can't use it */
	ch = bus_space_read_1(iot, ioh, EF_MADDR);
	if (ch & EF_MADDR_HIGH)
	    return 0;			/* XXX: maybe we should panic?? */

	return 1;
}

CFATTACH_DECL_NEW(ef, sizeof(struct ef_softc),
    ef_match, ef_attach, NULL, NULL);

