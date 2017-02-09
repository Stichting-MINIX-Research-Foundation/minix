/*	$NetBSD: if_le_mca.c,v 1.19 2010/11/13 13:52:04 uebayasi Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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

/*
 * Driver for SKNET Personal and MC2+ cards, which are AMD Lance 7990 based
 * cards made by Syskonnect, former Schneider & Koch Datensysteme GmbH.
 *
 * Syskonnect was very helpful and provided docs for these cards promptly.
 * I wish all vendors would be like that!
 * I'd like to thank to Alfred Arnold, author of the Linux driver, for
 * giving me contact to The Right Syskonnect person, too :-)
 *
 * Sources:
 * SKNET MC+ Technical Manual, version 1.1, July 21 1993
 * SKNET personal Technisches Manual, version 1.2, April 14 1988
 * SKNET junior Technisches Manual, version 1.0, July 14 1987
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_le_mca.c,v 1.19 2010/11/13 13:52:04 uebayasi Exp $");

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

#include <dev/ic/lancereg.h>
#include <dev/ic/lancevar.h>
#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#include <dev/mca/mcareg.h>
#include <dev/mca/mcavar.h>
#include <dev/mca/mcadevs.h>

#include <dev/mca/if_lereg.h>

int 	le_mca_match(device_t, cfdata_t, void *);
void	le_mca_attach(device_t, device_t, void *);

struct le_mca_softc {
	struct	am7990_softc sc_am7990;	/* glue to MI code */

	void	*sc_ih;
	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_memh;
};

static void le_mca_wrcsr(struct lance_softc *, uint16_t, uint16_t);
static uint16_t le_mca_rdcsr(struct lance_softc *, uint16_t);
static void le_mca_hwreset(struct lance_softc *);
static int le_mca_intredge(void *);

static void	le_mca_copytobuf(struct lance_softc *, void *, int, int);
static void	le_mca_copyfrombuf(struct lance_softc *, void *, int, int);
static void	le_mca_zerobuf(struct lance_softc *, int, int);

static inline void le_mca_wrreg(struct le_mca_softc *, int, int);
#define le_mca_set_RAP(sc, reg_number) \
		le_mca_wrreg(sc, reg_number, RAP | REGWRITE)

CFATTACH_DECL_NEW(le_mca, sizeof(struct le_mca_softc),
    le_mca_match, le_mca_attach, NULL, NULL);

/* SKNET MC+ POS mapping */
static const u_int8_t sknet_mcp_irq[] = {
	3, 5, 10, 11
};
static const u_int8_t sknet_mcp_media[] = {
	IFM_ETHER|IFM_10_2,
	IFM_ETHER|IFM_10_T,
	IFM_ETHER|IFM_10_5,
	0
};

int
le_mca_match(device_t parent, cfdata_t cf, void *aux)
{
	struct mca_attach_args *ma = aux;

	switch(ma->ma_id) {
	case MCA_PRODUCT_SKNETPER:
	case MCA_PRODUCT_SKNETG:
		return (1);
	}

	return (0);
}

void
le_mca_attach(device_t parent, device_t self, void *aux)
{
	struct le_mca_softc *lesc = device_private(self);
	struct lance_softc *sc = &lesc->sc_am7990.lsc;
	struct mca_attach_args *ma = aux;
	int i, pos2, pos3, pos4, irq, membase, supmedia=0;
	const char *typestr;

	sc->sc_dev = self;

	/*
	 * SKNET Personal:
	 *
	 * POS register 2: (adf pos0)
	 *
	 * 7 6 5 4 3 2 1 0
	 *       | \___/ \__ enable: 0=adapter disabled, 1=adapter enabled
	 *        \    \____ Memory: 0xC0000-0xC3FFF + XX*0x4000
	 *         \________ IRQ: 0=10 1=11
	 *
	 *
	 * SKNET MC+:
	 * POS register 2: (adf pos0)
	 *
	 * 7 6 5 4 3 2 1 0
	 *       \___/ \ \__ enable: 0=adapter disabled, 1=adapter enabled
	 *           \  \___ BootEPROM disable
	 *            \_____ BootEPROM start address: 0xC0000 + XX*0x4000
	 *
	 * POS register 3: (adf pos1)
	 *
	 * 7 6 5 4 3 2 1 0
	 * 0 0 1 1 \_____/
	 *               \__ RAM: 0xC0000 + XX*0x4000
	 *
	 * POS register 4: (adf pos2)
	 *
	 * 7 6 5 4 3 2 1 0
	 * \_/     \_/ \_/
	 *   \       \   \__ Need to be reset to 0 0 after boot
	 *    \       \_____ IRQ: 00=3 01=5 10=10 11=11
	 *     \____________ Medium: 00=BNC 01=UTP 10=AUI 11=not allowed
	 */

	switch (ma->ma_id) {
	case MCA_PRODUCT_SKNETPER:
		typestr = "Personal MC2";

		pos2 = mca_conf_read(ma->ma_mc, ma->ma_slot, 2);
		irq = (pos2 & (1<<4)) ? 11 : 10;
		membase = 0xc0000 + ((pos2 & 0x0e) >> 1) * 0x4000;
		break;
	case MCA_PRODUCT_SKNETG:
		typestr = "MC2+";

		/*
		 * SKNET MC+ needs the driver to clear 0, 1 bits of pos4
		 * and explicitly set the enable bit. Somebody at Syskonnect
		 * was obviously misguided when the card was designed ...
		 */
		pos3 = mca_conf_read(ma->ma_mc, ma->ma_slot, 3);
		pos4 = mca_conf_read(ma->ma_mc, ma->ma_slot, 4);
		if ((pos4 & 0x03) != 0) {
			/* clear the bits 0, 1 */
			mca_conf_write(ma->ma_mc, ma->ma_slot, 4,
				pos4 & ~0x03);
		}

		pos2 = mca_conf_read(ma->ma_mc, ma->ma_slot, 2);
		if ((pos2 & 0x01) == 0) {
			/* enable the card */
			mca_conf_write(ma->ma_mc, ma->ma_slot, 2, pos2 | 0x01);
		}

		/* get irq and memory base */
		irq = sknet_mcp_irq[(pos4 & 0x0c) >> 2];
		membase = 0xc0000 + ((pos3 & 0x0f) * 0x4000);

		/* Get configured media type */
		supmedia = sknet_mcp_media[(pos4 & 0xc0) >> 6];
		break;
	default:
		aprint_error(": unknown product %d\n", ma->ma_id);
		return;
	}

	lesc->sc_memt = ma->ma_memt;

	if (bus_space_map(lesc->sc_memt, membase, LE_MCA_MEMSIZE,
		0, &lesc->sc_memh)) {
		aprint_error(": can't map memory\n");
		return;
	}

	aprint_normal(" slot %d irq %d: SKNET %s Ethernet\n",
		ma->ma_slot + 1, irq, typestr);

	/*
	 * Extract the physical MAC address from the ROM.
	 */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc->sc_enaddr[i] = bus_space_read_1(lesc->sc_memt,
				lesc->sc_memh, LE_PROMOFF + i * 2);

	sc->sc_conf3 = LE_C3_ACON;
	sc->sc_addr = 0;
	sc->sc_memsize = LE_MCA_RAMSIZE;

	sc->sc_copytodesc = le_mca_copytobuf;
	sc->sc_copyfromdesc = le_mca_copyfrombuf;
	sc->sc_copytobuf = le_mca_copytobuf;
	sc->sc_copyfrombuf = le_mca_copyfrombuf;
	sc->sc_zerobuf = le_mca_zerobuf;

	sc->sc_rdcsr = le_mca_rdcsr;
	sc->sc_wrcsr = le_mca_wrcsr;
	sc->sc_hwinit = NULL;

	sc->sc_hwreset = le_mca_hwreset;

	/*
	 * This is merely cosmetic since it's not possible to switch
	 * the media anyway, even for MC2+.
	 */
	if (supmedia != 0) {
		sc->sc_supmedia = &supmedia;
		sc->sc_nsupmedia = 1;
		sc->sc_defaultmedia = supmedia;
	}

	lesc->sc_ih = mca_intr_establish(ma->ma_mc, irq, IPL_NET,
			le_mca_intredge, sc);
	if (lesc->sc_ih == NULL) {
		aprint_error_dev(self,
		    "couldn't establish interrupt handler\n");
		return;
	}

	aprint_normal("%s", device_xname(self));
	am7990_config(&lesc->sc_am7990);
}

/*
 * Controller interrupt.
 */
int
le_mca_intredge(void *arg)
{

	/*
	 * We could check the IRQ bit of LE_PORT, but it seems to be unset
	 * at this time anyway.
	 */

	if (am7990_intr(arg) == 0)
		return (0);
	for(;;)
		if (am7990_intr(arg) == 0)
			return (1);
}

/*
 * Push a value to LANCE controller.
 */
static inline void
le_mca_wrreg(struct le_mca_softc *sc, int val, int type)
{

	/*
	 * This follows steps in SKNET Personal/MC2+ docs:
	 * 1. write reg. number to LANCE register
	 * 2. write value RESET | type to port
	 * 3. flag REGDO
	 * 4. wait until REGREQ is cleared
	 */
	if ((type & REGREAD) == 0)
		bus_space_write_2(sc->sc_memt, sc->sc_memh, LE_LANCEREG, val);
	bus_space_write_1(sc->sc_memt, sc->sc_memh, LE_PORT,
		RESET | type);
	bus_space_write_1(sc->sc_memt, sc->sc_memh, LE_REGIO,
		REGDO);
	/* Delay here doesn't seem to be necessary */
	/* delay(1);  */
	while (bus_space_read_1(sc->sc_memt, sc->sc_memh, LE_PORT) & REGREQ)
		;
}

static void
le_mca_wrcsr(struct lance_softc *sc, uint16_t port, uint16_t val)
{
	struct le_mca_softc *lsc = (struct le_mca_softc *)sc;

	le_mca_set_RAP(lsc, port);
	le_mca_wrreg(lsc, val, RDATA | REGWRITE);
}

static uint16_t
le_mca_rdcsr(struct lance_softc *sc, uint16_t port)
{
	struct le_mca_softc *lsc = (struct le_mca_softc *)sc;

	le_mca_set_RAP(lsc, port);
	le_mca_wrreg(lsc, 0, RDATA | REGREAD);

	return (bus_space_read_2(lsc->sc_memt, lsc->sc_memh, LE_LANCEREG));
}

static void
le_mca_hwreset(struct lance_softc *sc)
{
	struct le_mca_softc *lsc = (struct le_mca_softc *)sc;

	bus_space_write_1(lsc->sc_memt, lsc->sc_memh, LE_PORT, 0);
	delay(5);	/* Delay >= 5 microseconds */
	bus_space_write_1(lsc->sc_memt, lsc->sc_memh, LE_PORT, RESET);
}

static void
le_mca_copytobuf(struct lance_softc *sc, void *from, int boff, int len)
{
	struct le_mca_softc *lsc = (struct le_mca_softc *)sc;

	bus_space_write_region_1(lsc->sc_memt, lsc->sc_memh, boff,
	    from, len);
}

static void
le_mca_copyfrombuf(struct lance_softc *sc, void *to, int boff, int len)
{
	struct le_mca_softc *lsc = (struct le_mca_softc *)sc;

	bus_space_read_region_1(lsc->sc_memt, lsc->sc_memh, boff,
	    to, len);
}

static void
le_mca_zerobuf(struct lance_softc *sc, int boff, int len)
{
	struct le_mca_softc *lsc = (struct le_mca_softc *)sc;

	bus_space_set_region_1(lsc->sc_memt, lsc->sc_memh, boff,
	    0x00, len);
}
