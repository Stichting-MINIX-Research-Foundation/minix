/*	$NetBSD: if_ix.c,v 1.34 2011/06/03 16:28:40 tsutsui Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: if_ix.c,v 1.34 2011/06/03 16:28:40 tsutsui Exp $");

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
#include <dev/isa/if_ixreg.h>

#ifdef IX_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

int ix_media[] = {
	IFM_ETHER | IFM_10_5,
	IFM_ETHER | IFM_10_2,
	IFM_ETHER | IFM_10_T,
};
#define NIX_MEDIA       (sizeof(ix_media) / sizeof(ix_media[0]))

struct ix_softc {
	struct ie_softc sc_ie;

	bus_space_tag_t sc_regt;	/* space tag for registers */
	bus_space_handle_t sc_regh;	/* space handle for registers */

	u_int8_t	use_pio;	/* use PIO rather than shared mem */
	u_int16_t	irq_encoded;	/* encoded IRQ */
	void		*sc_ih;		/* interrupt handle */
};

static void 	ix_reset(struct ie_softc *, int);
static void 	ix_atten(struct ie_softc *, int);
static int 	ix_intrhook(struct ie_softc *, int);

static void     ix_copyin(struct ie_softc *, void *, int, size_t);
static void     ix_copyout(struct ie_softc *, const void *, int, size_t);

static void	ix_bus_barrier(struct ie_softc *, int, int, int);

static u_int16_t ix_read_16(struct ie_softc *, int);
static void	ix_write_16(struct ie_softc *, int, u_int16_t);
static void	ix_write_24(struct ie_softc *, int, int);
static void	ix_zeromem (struct ie_softc *, int, int);

static void	ix_mediastatus(struct ie_softc *, struct ifmediareq *);

static u_int16_t ix_read_eeprom(bus_space_tag_t, bus_space_handle_t, int);
static void	ix_eeprom_outbits(bus_space_tag_t, bus_space_handle_t, int, int);
static int	ix_eeprom_inbits (bus_space_tag_t, bus_space_handle_t);
static void	ix_eeprom_clock  (bus_space_tag_t, bus_space_handle_t, int);

int ix_match(device_t, cfdata_t, void *);
void ix_attach(device_t, device_t, void *);

/*
 * EtherExpress/16 support routines
 */
static void
ix_reset(struct ie_softc *sc, int why)
{
	struct ix_softc* isc = (struct ix_softc *) sc;

	switch (why) {
	case CHIP_PROBE:
		bus_space_write_1(isc->sc_regt, isc->sc_regh, IX_ECTRL,
				  IX_RESET_586);
		delay(100);
		bus_space_write_1(isc->sc_regt, isc->sc_regh, IX_ECTRL, 0);
		delay(100);
		break;

	case CARD_RESET:
		break;
    }
}

static void
ix_atten(struct ie_softc *sc, int why)
{
	struct ix_softc* isc = (struct ix_softc *) sc;
	bus_space_write_1(isc->sc_regt, isc->sc_regh, IX_ATTN, 0);
}

static u_int16_t
ix_read_eeprom(bus_space_tag_t iot, bus_space_handle_t ioh, int location)
{
	int ectrl, edata;

	ectrl = bus_space_read_1(iot, ioh, IX_ECTRL);
	ectrl &= IX_ECTRL_MASK;
	ectrl |= IX_ECTRL_EECS;
	bus_space_write_1(iot, ioh, IX_ECTRL, ectrl);

	ix_eeprom_outbits(iot, ioh, IX_EEPROM_READ, IX_EEPROM_OPSIZE1);
	ix_eeprom_outbits(iot, ioh, location, IX_EEPROM_ADDR_SIZE);
	edata = ix_eeprom_inbits(iot, ioh);
	ectrl = bus_space_read_1(iot, ioh, IX_ECTRL);
	ectrl &= ~(IX_RESET_ASIC | IX_ECTRL_EEDI | IX_ECTRL_EECS);
	bus_space_write_1(iot, ioh, IX_ECTRL, ectrl);
	ix_eeprom_clock(iot, ioh, 1);
	ix_eeprom_clock(iot, ioh, 0);
	return (edata);
}

static void
ix_eeprom_outbits(bus_space_tag_t iot, bus_space_handle_t ioh, int edata, int count)
{
	int ectrl, i;

	ectrl = bus_space_read_1(iot, ioh, IX_ECTRL);
	ectrl &= ~IX_RESET_ASIC;
	for (i = count - 1; i >= 0; i--) {
		ectrl &= ~IX_ECTRL_EEDI;
		if (edata & (1 << i)) {
			ectrl |= IX_ECTRL_EEDI;
		}
		bus_space_write_1(iot, ioh, IX_ECTRL, ectrl);
		delay(1);	/* eeprom data must be setup for 0.4 uSec */
		ix_eeprom_clock(iot, ioh, 1);
		ix_eeprom_clock(iot, ioh, 0);
	}
	ectrl &= ~IX_ECTRL_EEDI;
	bus_space_write_1(iot, ioh, IX_ECTRL, ectrl);
	delay(1);		/* eeprom data must be held for 0.4 uSec */
}

static int
ix_eeprom_inbits(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	int ectrl, edata, i;

	ectrl = bus_space_read_1(iot, ioh, IX_ECTRL);
	ectrl &= ~IX_RESET_ASIC;
	for (edata = 0, i = 0; i < 16; i++) {
		edata = edata << 1;
		ix_eeprom_clock(iot, ioh, 1);
		ectrl = bus_space_read_1(iot, ioh, IX_ECTRL);
		if (ectrl & IX_ECTRL_EEDO) {
			edata |= 1;
		}
		ix_eeprom_clock(iot, ioh, 0);
	}
	return (edata);
}

static void
ix_eeprom_clock(bus_space_tag_t iot, bus_space_handle_t ioh, int state)
{
	int ectrl;

	ectrl = bus_space_read_1(iot, ioh, IX_ECTRL);
	ectrl &= ~(IX_RESET_ASIC | IX_ECTRL_EESK);
	if (state) {
		ectrl |= IX_ECTRL_EESK;
	}
	bus_space_write_1(iot, ioh, IX_ECTRL, ectrl);
	delay(9);		/* EESK must be stable for 8.38 uSec */
}

static int
ix_intrhook(struct ie_softc *sc, int where)
{
	struct ix_softc* isc = (struct ix_softc *) sc;

	switch (where) {
	case INTR_ENTER:
		/* entering ISR: disable card interrupts */
		bus_space_write_1(isc->sc_regt, isc->sc_regh,
				  IX_IRQ, isc->irq_encoded);
		break;

	case INTR_EXIT:
		/* exiting ISR: re-enable card interrupts */
		bus_space_write_1(isc->sc_regt, isc->sc_regh, IX_IRQ,
    				  isc->irq_encoded | IX_IRQ_ENABLE);
	break;
    }

    return 1;
}


static void
ix_copyin (struct ie_softc *sc, void *dst, int offset, size_t size)
{
	int i, dribble;
	u_int8_t* bptr = dst;
	u_int16_t* wptr = dst;
	struct ix_softc* isc = (struct ix_softc *) sc;

	if (isc->use_pio) {
		/* Reset read pointer to the specified offset */
		bus_space_barrier(sc->bt, sc->bh, IX_DATAPORT, 2,
						  BUS_SPACE_BARRIER_READ);
		bus_space_write_2(sc->bt, sc->bh, IX_READPTR, offset);
		bus_space_barrier(sc->bt, sc->bh, IX_READPTR, 2,
						  BUS_SPACE_BARRIER_WRITE);
	} else {
	bus_space_barrier(sc->bt, sc->bh, offset, size,
			  BUS_SPACE_BARRIER_READ);
	}

	if (offset % 2) {
		if (isc->use_pio)
			*bptr = bus_space_read_1(sc->bt, sc->bh, IX_DATAPORT);
		else
		*bptr = bus_space_read_1(sc->bt, sc->bh, offset);
		offset++; bptr++; size--;
	}

	dribble = size % 2;
	wptr = (u_int16_t*) bptr;

	if (isc->use_pio) {
		for(i = 0; i <  size / 2; i++) {
			*wptr = bus_space_read_2(sc->bt, sc->bh, IX_DATAPORT);
			wptr++;
		}
	} else {
		bus_space_read_region_2(sc->bt, sc->bh, offset,
					(u_int16_t *) bptr, size / 2);
	}

	if (dribble) {
		bptr += size - 1;
		offset += size - 1;

		if (isc->use_pio)
			*bptr = bus_space_read_1(sc->bt, sc->bh, IX_DATAPORT);
		else
		*bptr = bus_space_read_1(sc->bt, sc->bh, offset);
	}
}

static void
ix_copyout (struct ie_softc *sc, const void *src, int offset, size_t size)
{
	int i, dribble;
	int osize = size;
	int ooffset = offset;
	const u_int8_t* bptr = src;
	const u_int16_t* wptr = src;
	struct ix_softc* isc = (struct ix_softc *) sc;

	if (isc->use_pio) {
		/* Reset write pointer to the specified offset */
		bus_space_write_2(sc->bt, sc->bh, IX_WRITEPTR, offset);
		bus_space_barrier(sc->bt, sc->bh, IX_WRITEPTR, 2,
						  BUS_SPACE_BARRIER_WRITE);
	}

	if (offset % 2) {
		if (isc->use_pio)
			bus_space_write_1(sc->bt, sc->bh, IX_DATAPORT, *bptr);
		else
		bus_space_write_1(sc->bt, sc->bh, offset, *bptr);
		offset++; bptr++; size--;
	}

	dribble = size % 2;
	wptr = (const u_int16_t*) bptr;

	if (isc->use_pio) {
		for(i = 0; i < size / 2; i++) {
			bus_space_write_2(sc->bt, sc->bh, IX_DATAPORT, *wptr);
			wptr++;
		}
	} else {
		bus_space_write_region_2(sc->bt, sc->bh, offset,
		    (const u_int16_t *)bptr, size / 2);
	}

	if (dribble) {
		bptr += size - 1;
		offset += size - 1;

		if (isc->use_pio)
			bus_space_write_1(sc->bt, sc->bh, IX_DATAPORT, *bptr);
		else
		bus_space_write_1(sc->bt, sc->bh, offset, *bptr);
	}

	if (isc->use_pio)
		bus_space_barrier(sc->bt, sc->bh, IX_DATAPORT, 2,
						  BUS_SPACE_BARRIER_WRITE);
	else
	bus_space_barrier(sc->bt, sc->bh, ooffset, osize,
			  BUS_SPACE_BARRIER_WRITE);
}

static void
ix_bus_barrier(struct ie_softc *sc, int offset, int length, int flags)
{
	struct ix_softc* isc = (struct ix_softc *) sc;

	if (isc->use_pio)
		bus_space_barrier(sc->bt, sc->bh, IX_DATAPORT, 2, flags);
	else
		bus_space_barrier(sc->bt, sc->bh, offset, length, flags);
}

static u_int16_t
ix_read_16 (struct ie_softc *sc, int offset)
{
	struct ix_softc* isc = (struct ix_softc *) sc;

	if (isc->use_pio) {
		bus_space_barrier(sc->bt, sc->bh, IX_DATAPORT, 2,
						  BUS_SPACE_BARRIER_READ);

		/* Reset read pointer to the specified offset */
		bus_space_write_2(sc->bt, sc->bh, IX_READPTR, offset);
		bus_space_barrier(sc->bt, sc->bh, IX_READPTR, 2,
						  BUS_SPACE_BARRIER_WRITE);

		return bus_space_read_2(sc->bt, sc->bh, IX_DATAPORT);
	} else {
		bus_space_barrier(sc->bt, sc->bh, offset, 2,
						  BUS_SPACE_BARRIER_READ);
        return bus_space_read_2(sc->bt, sc->bh, offset);
	}
}

static void
ix_write_16 (struct ie_softc *sc, int offset, u_int16_t value)
{
	struct ix_softc* isc = (struct ix_softc *) sc;

	if (isc->use_pio) {
		/* Reset write pointer to the specified offset */
		bus_space_write_2(sc->bt, sc->bh, IX_WRITEPTR, offset);
		bus_space_barrier(sc->bt, sc->bh, IX_WRITEPTR, 2,
						  BUS_SPACE_BARRIER_WRITE);

		bus_space_write_2(sc->bt, sc->bh, IX_DATAPORT, value);
		bus_space_barrier(sc->bt, sc->bh, IX_DATAPORT, 2,
						  BUS_SPACE_BARRIER_WRITE);
	} else {
        bus_space_write_2(sc->bt, sc->bh, offset, value);
		bus_space_barrier(sc->bt, sc->bh, offset, 2,
						  BUS_SPACE_BARRIER_WRITE);
	}
}

static void
ix_write_24 (struct ie_softc *sc, int offset, int addr)
{
	char* ptr;
	struct ix_softc* isc = (struct ix_softc *) sc;
	int val = addr + (u_long) sc->sc_maddr - (u_long) sc->sc_iobase;

	if (isc->use_pio) {
		/* Reset write pointer to the specified offset */
		bus_space_write_2(sc->bt, sc->bh, IX_WRITEPTR, offset);
		bus_space_barrier(sc->bt, sc->bh, IX_WRITEPTR, 2,
						  BUS_SPACE_BARRIER_WRITE);

		ptr = (char*) &val;
		bus_space_write_2(sc->bt, sc->bh, IX_DATAPORT,
						  *((u_int16_t *)ptr));
		bus_space_write_2(sc->bt, sc->bh, IX_DATAPORT,
						  *((u_int16_t *)(ptr + 2)));
		bus_space_barrier(sc->bt, sc->bh, IX_DATAPORT, 2,
						  BUS_SPACE_BARRIER_WRITE);
	} else {
        	bus_space_write_4(sc->bt, sc->bh, offset, val);
		bus_space_barrier(sc->bt, sc->bh, offset, 4,
						  BUS_SPACE_BARRIER_WRITE);
	}
}

static void
ix_zeromem(struct ie_softc *sc, int offset, int count)
{
	int i;
	int dribble;
	struct ix_softc* isc = (struct ix_softc *) sc;

	if (isc->use_pio) {
		/* Reset write pointer to the specified offset */
		bus_space_write_2(sc->bt, sc->bh, IX_WRITEPTR, offset);
		bus_space_barrier(sc->bt, sc->bh, IX_WRITEPTR, 2,
						  BUS_SPACE_BARRIER_WRITE);

		if (offset % 2) {
			bus_space_write_1(sc->bt, sc->bh, IX_DATAPORT, 0);
			count--;
		}

	        dribble = count % 2;
		for(i = 0; i < count / 2; i++)
			bus_space_write_2(sc->bt, sc->bh, IX_DATAPORT, 0);

		if (dribble)
			bus_space_write_1(sc->bt, sc->bh, IX_DATAPORT, 0);

		bus_space_barrier(sc->bt, sc->bh, IX_DATAPORT, 2,
						  BUS_SPACE_BARRIER_WRITE);
	} else {
		bus_space_set_region_1(sc->bt, sc->bh, offset, 0, count);
		bus_space_barrier(sc->bt, sc->bh, offset, count,
						  BUS_SPACE_BARRIER_WRITE);
	}
}

static void
ix_mediastatus(struct ie_softc *sc, struct ifmediareq *ifmr)
{
        struct ifmedia *ifm = &sc->sc_media;

        /*
         * The currently selected media is always the active media.
         */
        ifmr->ifm_active = ifm->ifm_cur->ifm_media;
}

int
ix_match(device_t parent, cfdata_t cf, void *aux)
{
	int i;
	int rv = 0;
	bus_addr_t maddr;
	bus_size_t msiz;
	u_short checksum = 0;
	bus_space_handle_t ioh;
	bus_space_tag_t iot;
	u_int8_t val, bart_config;
	u_short pg, adjust, decode, edecode;
	u_short board_id, id_var1, id_var2, irq, irq_encoded;
	struct isa_attach_args * const ia = aux;
	short irq_translate[] = {0, 0x09, 0x03, 0x04, 0x05, 0x0a, 0x0b, 0};

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_niomem < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	iot = ia->ia_iot;

	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);

	if (bus_space_map(iot, ia->ia_io[0].ir_addr,
			  IX_IOSIZE, 0, &ioh) != 0) {
		DPRINTF(("Can't map io space at 0x%x\n", ia->ia_iobase));
		return (0);
	}

	/* XXX: reset any ee16 at the current iobase */
	bus_space_write_1(iot, ioh, IX_ECTRL, IX_RESET_ASIC);
	bus_space_write_1(iot, ioh, IX_ECTRL, 0);
	delay(240);

	/* now look for ee16. */
	board_id = id_var1 = id_var2 = 0;
	for (i = 0; i < 4 ; i++) {
		id_var1 = bus_space_read_1(iot, ioh, IX_ID_PORT);
		id_var2 = ((id_var1 & 0x03) << 2);
		board_id |= (( id_var1 >> 4)  << id_var2);
	}

	if (board_id != IX_ID) {
		DPRINTF(("BART ID mismatch (got 0x%04x, expected 0x%04x)\n",
			board_id, IX_ID));
		goto out;
	}

	/*
	 * The shared RAM size and location of the EE16 is encoded into
	 * EEPROM location 6.  The location of the first set bit tells us
	 * the memory address (0xc0000 + (0x4000 * FSB)), where FSB is the
	 * number of the first set bit.  The zeroes are then shifted out,
	 * and the results is the memory size (1 = 16k, 3 = 32k, 7 = 48k,
	 * 0x0f = 64k).
	 *
	 * Examples:
	 *   0x3c -> 64k@0xc8000, 0x70 -> 48k@0xd0000, 0xc0 -> 32k@0xd8000
	 *   0x80 -> 16k@0xdc000.
	 *
	 * Side note: this comes from reading the old driver rather than
	 * from a more definitive source, so it could be out-of-whack
	 * with what the card can do...
	 */

	val = ix_read_eeprom(iot, ioh, 6) & 0xff;
	for (pg = 0; pg < 8; pg++) {
		if (val & 1)
			break;
		val >>= 1;
	}

	maddr = 0xc0000 + (pg * 0x4000);

	switch (val) {
	case 0x00:
		maddr = 0;
		msiz = 0;
		break;

	case 0x01:
		msiz = 16 * 1024;
		break;

	case 0x03:
		msiz = 32 * 1024;
		break;

	case 0x07:
		msiz = 48 * 1024;
		break;

	case 0x0f:
		msiz = 64 * 1024;
		break;

	default:
		DPRINTF(("invalid memory size %02x\n", val));
		goto out;
	}

	if (ia->ia_iomem[0].ir_addr != ISA_UNKNOWN_IOMEM &&
	    ia->ia_iomem[0].ir_addr != maddr) {
		DPRINTF((
		  "ix_match: memaddr of board @ 0x%x doesn't match config\n",
		  ia->ia_iobase));
		goto out;
	}

	if (ia->ia_iomem[0].ir_size != ISA_UNKNOWN_IOSIZ &&
	    ia->ia_iomem[0].ir_size != msiz) {
		DPRINTF((
		   "ix_match: memsize of board @ 0x%x doesn't match config\n",
		   ia->ia_iobase));
		goto out;
	}

	/* need to put the 586 in RESET, and leave it */
	bus_space_write_1(iot, ioh, IX_ECTRL, IX_RESET_586);

	/* read the eeprom and checksum it, should == IX_ID */
	for(i = 0; i < 0x40; i++)
		checksum += ix_read_eeprom(iot, ioh, i);

	if (checksum != IX_ID) {
		DPRINTF(("checksum mismatch (got 0x%04x, expected 0x%04x\n",
			checksum, IX_ID));
		goto out;
	}

	/*
	 * Only do the following bit if using memory-mapped access.  For
	 * boards with no mapped memory, we use PIO.  We also use PIO for
	 * boards with 16K of mapped memory, as those setups don't seem
	 * to work otherwise.
	 */
	if (msiz != 0 && msiz != 16384) {
		/* Set board up with memory-mapping info */
	adjust = IX_MCTRL_FMCS16 | (pg & 0x3) << 2;
	decode = ((1 << (ia->ia_iomem[0].ir_size / 16384)) - 1) << pg;
	edecode = ((~decode >> 4) & 0xF0) | (decode >> 8);

	bus_space_write_1(iot, ioh, IX_MEMDEC, decode & 0xFF);
	bus_space_write_1(iot, ioh, IX_MCTRL, adjust);
	bus_space_write_1(iot, ioh, IX_MPCTRL, (~decode & 0xFF));

		/* XXX disable Exxx */
		bus_space_write_1(iot, ioh, IX_MECTRL, edecode);
	}

	/*
	 * Get the encoded interrupt number from the EEPROM, check it
	 * against the passed in IRQ.  Issue a warning if they do not
	 * match, and fail the probe.  If irq is 'ISA_UNKNOWN_IRQ' then we
	 * use the EEPROM irq, and continue.
	 */
	irq_encoded = ix_read_eeprom(iot, ioh, IX_EEPROM_CONFIG1);
	irq_encoded = (irq_encoded & IX_EEPROM_IRQ) >> IX_EEPROM_IRQ_SHIFT;
	irq = irq_translate[irq_encoded];
	if (ia->ia_irq[0].ir_irq != ISA_UNKNOWN_IRQ &&
	    irq != ia->ia_irq[0].ir_irq) {
		DPRINTF(("board IRQ %d does not match config\n", irq));
		goto out;
	}

	/* disable the board interrupts */
	bus_space_write_1(iot, ioh, IX_IRQ, irq_encoded);

	bart_config = bus_space_read_1(iot, ioh, IX_CONFIG);
	bart_config |= IX_BART_LOOPBACK;
	bart_config |= IX_BART_MCS16_TEST; /* inb doesn't get bit! */
	bus_space_write_1(iot, ioh, IX_CONFIG, bart_config);
	bart_config = bus_space_read_1(iot, ioh, IX_CONFIG);

	bus_space_write_1(iot, ioh, IX_ECTRL, 0);
	delay(100);

	rv = 1;

	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = IX_IOSIZE;

	ia->ia_niomem = 1;
	ia->ia_iomem[0].ir_addr = maddr;
	ia->ia_iomem[0].ir_size = msiz;

	ia->ia_nirq = 1;
	ia->ia_irq[0].ir_irq = irq;

	DPRINTF(("ix_match: found board @ 0x%x\n", ia->ia_iobase));

out:
	bus_space_unmap(iot, ioh, IX_IOSIZE);
	return (rv);
}

void
ix_attach(device_t parent, device_t self, void *aux)
{
	struct ix_softc *isc = device_private(self);
	struct ie_softc *sc = &isc->sc_ie;
	struct isa_attach_args *ia = aux;

	int media;
	int i, memsize;
	u_int8_t bart_config;
	bus_space_tag_t iot;
	u_int8_t bpat, bval;
	u_int16_t wpat, wval;
	bus_space_handle_t ioh, memh;
	u_short irq_encoded;
	u_int8_t ethaddr[ETHER_ADDR_LEN];

	sc->sc_dev = self;
	iot = ia->ia_iot;

	/*
	 * Shared memory access seems to fail on 16K mapped boards, so
	 * disable shared memory access if the board is in 16K mode.  If
	 * no memory is mapped, we have no choice but to use PIO
	 */
	isc->use_pio = (ia->ia_iomem[0].ir_size <= (16 * 1024));

	if (bus_space_map(iot, ia->ia_io[0].ir_addr,
			  ia->ia_io[0].ir_size, 0, &ioh) != 0) {

		DPRINTF(("\n%s: can't map i/o space 0x%x-0x%x\n",
			  device_xname(self), ia->ia_[0].ir_addr,
			  ia->ia_io[0].ir_addr + ia->ia_io[0].ir_size - 1));
		return;
	}

	/* We map memory even if using PIO so something else doesn't grab it */
	if (ia->ia_iomem[0].ir_size) {
	if (bus_space_map(ia->ia_memt, ia->ia_iomem[0].ir_addr,
			  ia->ia_iomem[0].ir_size, 0, &memh) != 0) {
		DPRINTF(("\n%s: can't map iomem space 0x%x-0x%x\n",
			device_xname(self), ia->ia_iomem[0].ir_addr,
			ia->ia_iomem[0].ir_addr + ia->ia_iomem[0].ir_size - 1));
		bus_space_unmap(iot, ioh, ia->ia_io[0].ir_size);
		return;
	}
	}

	isc->sc_regt = iot;
	isc->sc_regh = ioh;

	/*
	 * Get the hardware ethernet address from the EEPROM and
	 * save it in the softc for use by the 586 setup code.
	 */
	wval = ix_read_eeprom(iot, ioh, IX_EEPROM_ENET_HIGH);
	ethaddr[1] = wval & 0xFF;
	ethaddr[0] = wval >> 8;
	wval = ix_read_eeprom(iot, ioh, IX_EEPROM_ENET_MID);
	ethaddr[3] = wval & 0xFF;
	ethaddr[2] = wval >> 8;
	wval = ix_read_eeprom(iot, ioh, IX_EEPROM_ENET_LOW);
	ethaddr[5] = wval & 0xFF;
	ethaddr[4] = wval >> 8;

	sc->hwinit = NULL;
	sc->hwreset = ix_reset;
	sc->chan_attn = ix_atten;
	sc->intrhook = ix_intrhook;

	sc->memcopyin = ix_copyin;
	sc->memcopyout = ix_copyout;

	/* If using PIO, make sure to setup single-byte read/write functions */
	if (isc->use_pio) {
		sc->ie_bus_barrier = ix_bus_barrier;
	} else {
		sc->ie_bus_barrier = NULL;
	}

	sc->ie_bus_read16 = ix_read_16;
	sc->ie_bus_write16 = ix_write_16;
	sc->ie_bus_write24 = ix_write_24;

	sc->do_xmitnopchain = 0;

	sc->sc_mediachange = NULL;
	sc->sc_mediastatus = ix_mediastatus;

	if (isc->use_pio) {
		sc->bt = iot;
		sc->bh = ioh;

		/*
		 * If using PIO, the memory size is bounded by on-card memory,
		 * not by how much is mapped into the memory-mapped region, so
		 * determine how much total memory we have to play with here.
		 */
		for(memsize = 64 * 1024; memsize; memsize -= 16 * 1024) {
			/* warm up shared memory, the zero it all out */
			ix_zeromem(sc, 0, 32);
			ix_zeromem(sc, 0, memsize);

			/* Reset write pointer to the start of RAM */
			bus_space_write_2(iot, ioh, IX_WRITEPTR, 0);
			bus_space_barrier(iot, ioh, IX_WRITEPTR, 2,
						    BUS_SPACE_BARRIER_WRITE);

			/* write test pattern */
			for(i = 0, wpat = 1; i < memsize; i += 2) {
				bus_space_write_2(iot, ioh, IX_DATAPORT, wpat);
				wpat += 3;
			}

			/* Flush all reads & writes to data port */
			bus_space_barrier(iot, ioh, IX_DATAPORT, 2,
						    BUS_SPACE_BARRIER_READ |
						    BUS_SPACE_BARRIER_WRITE);

			/* Reset read pointer to beginning of card RAM */
			bus_space_write_2(iot, ioh, IX_READPTR, 0);
			bus_space_barrier(iot, ioh, IX_READPTR, 2,
						    BUS_SPACE_BARRIER_WRITE);

			/* read and verify test pattern */
			for(i = 0, wpat = 1; i < memsize; i += 2) {
				wval = bus_space_read_2(iot, ioh, IX_DATAPORT);

				if (wval != wpat)
					break;

				wpat += 3;
			}

			/* If we failed, try next size down */
			if (i != memsize)
				continue;

			/* Now try it all with byte reads/writes */
			ix_zeromem(sc, 0, 32);
			ix_zeromem(sc, 0, memsize);

			/* Reset write pointer to start of card RAM */
			bus_space_write_2(iot, ioh, IX_WRITEPTR, 0);
			bus_space_barrier(iot, ioh, IX_WRITEPTR, 2,
						    BUS_SPACE_BARRIER_WRITE);

			/* write out test pattern */
			for(i = 0, bpat = 1; i < memsize; i++) {
				bus_space_write_1(iot, ioh, IX_DATAPORT, bpat);
				bpat += 3;
			}

			/* Flush all reads & writes to data port */
			bus_space_barrier(iot, ioh, IX_DATAPORT, 2,
						    BUS_SPACE_BARRIER_READ |
						    BUS_SPACE_BARRIER_WRITE);

			/* Reset read pointer to beginning of card RAM */
			bus_space_write_2(iot, ioh, IX_READPTR, 0);
			bus_space_barrier(iot, ioh, IX_READPTR, 2,
						    BUS_SPACE_BARRIER_WRITE);

			/* read and verify test pattern */
			for(i = 0, bpat = 1; i < memsize; i++) {
				bval = bus_space_read_1(iot, ioh, IX_DATAPORT);

				if (bval != bpat)
				bpat += 3;
			}

			/* If we got through all of memory, we're done! */
			if (i == memsize)
				break;
		}

		/* Memory tests failed, punt... */
		if (memsize == 0)  {
			DPRINTF(("\n%s: can't determine size of on-card RAM\n",
				device_xname(self)));
			bus_space_unmap(iot, ioh, ia->ia_io[0].ir_size);
			return;
		}

		sc->bt = iot;
		sc->bh = ioh;

		sc->sc_msize = memsize;
		sc->sc_maddr = (void*) 0;
	} else {
	sc->bt = ia->ia_memt;
	sc->bh = memh;

	sc->sc_msize = ia->ia_iomem[0].ir_size;
	sc->sc_maddr = (void *)memh;
	}

	/* Map i/o space. */
	sc->sc_iobase = (char *)sc->sc_maddr + sc->sc_msize - (1 << 24);

	/* set up pointers to important on-card control structures */
	sc->iscp = 0;
	sc->scb = IE_ISCP_SZ;
	sc->scp = sc->sc_msize + IE_SCP_ADDR - (1 << 24);

	sc->buf_area = sc->scb + IE_SCB_SZ;
	sc->buf_area_sz = sc->sc_msize - IE_ISCP_SZ - IE_SCB_SZ - IE_SCP_SZ;

	/* zero card memory */
	ix_zeromem(sc, 0, 32);
	ix_zeromem(sc, 0, sc->sc_msize);

	/* set card to 16-bit bus mode */
	if (isc->use_pio) {
		bus_space_write_2(sc->bt, sc->bh, IX_WRITEPTR,
				  	    IE_SCP_BUS_USE((u_long)sc->scp));
		bus_space_barrier(sc->bt, sc->bh, IX_WRITEPTR, 2,
					          BUS_SPACE_BARRIER_WRITE);

		bus_space_write_1(sc->bt, sc->bh, IX_DATAPORT,
				  IE_SYSBUS_16BIT);
	} else {
		bus_space_write_1(sc->bt, sc->bh,
				  IE_SCP_BUS_USE((u_long)sc->scp),
				  IE_SYSBUS_16BIT);
	}

	/* set up pointers to key structures */
	ix_write_24(sc, IE_SCP_ISCP((u_long)sc->scp), (u_long) sc->iscp);
	ix_write_16(sc, IE_ISCP_SCB((u_long)sc->iscp), (u_long) sc->scb);
	ix_write_24(sc, IE_ISCP_BASE((u_long)sc->iscp), (u_long) sc->iscp);

	/* flush setup of pointers, check if chip answers */
	if (isc->use_pio) {
		bus_space_barrier(sc->bt, sc->bh, 0, IX_IOSIZE,
				  BUS_SPACE_BARRIER_WRITE);
	} else {
	bus_space_barrier(sc->bt, sc->bh, 0, sc->sc_msize,
			  BUS_SPACE_BARRIER_WRITE);
	}

	if (!i82586_proberam(sc)) {
		DPRINTF(("\n%s: Can't talk to i82586!\n",
			device_xname(self)));
		bus_space_unmap(iot, ioh, ia->ia_io[0].ir_size);

		if (ia->ia_iomem[0].ir_size)
		bus_space_unmap(ia->ia_memt, memh, ia->ia_iomem[0].ir_size);
		return;
	}

	/* Figure out which media is being used... */
	if (ix_read_eeprom(iot, ioh, IX_EEPROM_CONFIG1) &
				IX_EEPROM_MEDIA_EXT) {
		if (ix_read_eeprom(iot, ioh, IX_EEPROM_MEDIA) &
				IX_EEPROM_MEDIA_TP)
			media = IFM_ETHER | IFM_10_T;
		else
			media = IFM_ETHER | IFM_10_2;
	} else
		media = IFM_ETHER | IFM_10_5;

	/* Take the card out of lookback */
	bart_config = bus_space_read_1(iot, ioh, IX_CONFIG);
	bart_config &= ~IX_BART_LOOPBACK;
	bart_config |= IX_BART_MCS16_TEST; /* inb doesn't get bit! */
	bus_space_write_1(iot, ioh, IX_CONFIG, bart_config);
	bart_config = bus_space_read_1(iot, ioh, IX_CONFIG);

	irq_encoded = ix_read_eeprom(iot, ioh,
				     IX_EEPROM_CONFIG1);
	irq_encoded = (irq_encoded & IX_EEPROM_IRQ) >> IX_EEPROM_IRQ_SHIFT;

	/* Enable interrupts */
	bus_space_write_1(iot, ioh, IX_IRQ,
			  irq_encoded | IX_IRQ_ENABLE);

	/* Flush all writes to registers */
	bus_space_barrier(iot, ioh, 0, ia->ia_io[0].ir_size,
	    BUS_SPACE_BARRIER_WRITE);

	isc->irq_encoded = irq_encoded;

	i82586_attach(sc, "EtherExpress/16", ethaddr,
		      ix_media, NIX_MEDIA, media);

	if (isc->use_pio)
		aprint_error_dev(self, "unsupported memory config, using PIO to access %d bytes of memory\n", sc->sc_msize);

	isc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_NET, i82586_intr, sc);
	if (isc->sc_ih == NULL) {
		DPRINTF(("\n%s: can't establish interrupt\n",
			device_xname(self)));
	}
}

CFATTACH_DECL_NEW(ix, sizeof(struct ix_softc),
    ix_match, ix_attach, NULL, NULL);
