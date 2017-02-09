/* $NetBSD: if_ei.c,v 1.17 2011/06/03 16:28:40 tsutsui Exp $ */

/*-
 * Copyright (c) 2000, 2001 Ben Harris
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * if_ei.c -- driver for Acorn AKA25 Ether1 card
 */
/*
 * This is not directly based on the arm32 ie driver, as that doesn't
 * use the MI 82586 driver, and generally looks a mess (not to mention
 * having dodgy copyright status).  Let's try again.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ei.c,v 1.17 2011/06/03 16:28:40 tsutsui Exp $");

#include <sys/param.h>

#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/reboot.h>	/* For bootverbose */
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <sys/bus.h>
#include <dev/ic/i82586reg.h>
#include <dev/ic/i82586var.h>
#include <dev/podulebus/podulebus.h>
#include <dev/podulebus/podules.h>
#include <dev/podulebus/if_eireg.h>

/* Callbacks from the MI 82586 driver */
static void ei_hwreset(struct ie_softc *, int);
/* static void ei_hwinit(struct ie_softc *); */
static void ei_attn(struct ie_softc *, int);
static int ei_intrhook(struct ie_softc *, int);

static void ei_copyin(struct ie_softc *, void *, int, size_t);
static void ei_copyout(struct ie_softc *, const void *, int, size_t);
static u_int16_t ei_read16(struct ie_softc *, int);
static void ei_write16(struct ie_softc *, int, u_int16_t);
static void ei_write24(struct ie_softc *, int, int);

/* autoconfiguration glue */
static int ei_match(device_t, cfdata_t, void *);
static void ei_attach(device_t, device_t, void *);

struct ei_softc {
	struct	ie_softc sc_ie;
	bus_space_tag_t		sc_ctl_t;
	bus_space_handle_t	sc_ctl_h;
	bus_space_tag_t		sc_mem_t;
	bus_space_handle_t	sc_mem_h;
	bus_space_tag_t		sc_rom_t;
	bus_space_handle_t	sc_rom_h;
	u_int8_t	sc_idrom[EI_ROMSIZE];
	void			*sc_ih;
	struct		evcnt	sc_intrcnt;
};

CFATTACH_DECL_NEW(ei, sizeof(struct ei_softc),
    ei_match, ei_attach, NULL, NULL);

static inline void
ei_setpage(struct ei_softc *sc, int page)
{

	bus_space_write_1(sc->sc_ctl_t, sc->sc_ctl_h, EI_PAGE, page);
}

static inline void
ei_cli(struct ei_softc *sc)
{

	bus_space_write_1(sc->sc_ctl_t, sc->sc_ctl_h, EI_CONTROL, EI_CTL_CLI);
}

static int
ei_match(device_t parent, cfdata_t cf, void *aux)
{
	struct podulebus_attach_args *pa = aux;

	return pa->pa_product == PODULE_ETHER1;
}

static void
ei_attach(device_t parent, device_t self, void *aux)
{
	struct podulebus_attach_args *pa = aux;
	struct ei_softc *sc = device_private(self);
	int i;
	char descr[16];

	sc->sc_ie.sc_dev = self;

	/* Set up bus spaces */
	sc->sc_ctl_t = sc->sc_mem_t = pa->pa_fast_t;
	bus_space_map(pa->pa_fast_t, pa->pa_fast_base, EI_MEMOFF, 0,
	    &sc->sc_ctl_h);
	bus_space_map(pa->pa_fast_t, pa->pa_fast_base + EI_MEMOFF,
	    EI_PAGESIZE * 2, 0, &sc->sc_mem_h);
	sc->sc_rom_t = pa->pa_sync_t;
	bus_space_map(pa->pa_sync_t, pa->pa_sync_base, EI_ROMSIZE, 0,
	    &sc->sc_rom_h);

	/* Set up callbacks */
	sc->sc_ie.hwinit = NULL;
	sc->sc_ie.intrhook = ei_intrhook;
	sc->sc_ie.hwreset = ei_hwreset;
	sc->sc_ie.chan_attn = ei_attn;

	sc->sc_ie.memcopyin = ei_copyin;
	sc->sc_ie.memcopyout = ei_copyout;

	sc->sc_ie.ie_bus_barrier = NULL;
	sc->sc_ie.ie_bus_read16 = ei_read16;
	sc->sc_ie.ie_bus_write16 = ei_write16;
	sc->sc_ie.ie_bus_write24 = ei_write24;

	sc->sc_ie.do_xmitnopchain = 0; /* Does it listen? */

	sc->sc_ie.sc_mediachange = NULL;
	sc->sc_ie.sc_mediastatus = NULL;

	sc->sc_ie.bt = sc->sc_mem_t; /* XXX hope it doesn't use them */
	sc->sc_ie.bh = sc->sc_mem_h;

	printf(":");

	/* All Ether1s are 64k (I believe) */
	sc->sc_ie.sc_msize = EI_MEMSIZE;

	/* set up pointers to important on-card control structures */
	sc->sc_ie.iscp = 0;
	sc->sc_ie.scb = IE_ISCP_SZ;
	sc->sc_ie.scp = sc->sc_ie.sc_msize + IE_SCP_ADDR - (1 << 24);

	sc->sc_ie.buf_area = sc->sc_ie.scb + IE_SCB_SZ;
	sc->sc_ie.buf_area_sz =
	    sc->sc_ie.sc_msize - IE_ISCP_SZ - IE_SCB_SZ - IE_SCP_SZ;

	/* Wipe the whole of the card's memory */
	for (i = 0; i < EI_NPAGES; i++) {
		ei_setpage(sc, i);
		bus_space_set_region_2(sc->sc_mem_t, sc->sc_mem_h,
				       0, 0,EI_PAGESIZE / 2);
	}

	/* set up pointers to key structures */
	ei_write24(&sc->sc_ie, IE_SCP_ISCP((u_long)sc->sc_ie.scp),
		    (u_long) sc->sc_ie.iscp);
	ei_write16(&sc->sc_ie, IE_ISCP_SCB((u_long)sc->sc_ie.iscp),
		    (u_long) sc->sc_ie.scb);
	ei_write24(&sc->sc_ie, IE_ISCP_BASE((u_long)sc->sc_ie.iscp),
		    (u_long) sc->sc_ie.iscp);

	/* check if chip answers */
	if (i82586_proberam(&sc->sc_ie) == 0) {
		printf(" memory probe failed\n");
		return;
	}

	/* Read ROM */
	bus_space_read_region_1(sc->sc_rom_t, sc->sc_rom_h, 0,
				sc->sc_idrom, EI_ROMSIZE);

	snprintf(descr, 15, "AKA25 iss. %d", sc->sc_idrom[EI_ROM_HWREV]);
	i82586_attach(&sc->sc_ie, descr, sc->sc_idrom + EI_ROM_EADDR,
		      NULL, 0, 0);

	evcnt_attach_dynamic(&sc->sc_intrcnt, EVCNT_TYPE_INTR, NULL,
	    device_xname(self), "intr");
	sc->sc_ih = podulebus_irq_establish(pa->pa_ih, IPL_NET, i82586_intr,
	    self, &sc->sc_intrcnt);
	ei_cli(sc);
}

static void
ei_hwreset(struct ie_softc *sc_ie, int why)
{
	struct ei_softc *sc = (struct ei_softc *)sc_ie;

	bus_space_write_1(sc->sc_ctl_t, sc->sc_ctl_h, EI_CONTROL, EI_CTL_RST);
	DELAY(1000);
	bus_space_write_1(sc->sc_ctl_t, sc->sc_ctl_h, EI_CONTROL, 0);
	DELAY(1000);
	ei_cli(sc);
}

static int
ei_intrhook(struct ie_softc *sc_ie, int why)
{
	struct ei_softc *sc = (struct ei_softc *)sc_ie;

	switch (why) {
	case INTR_EXIT:
	case INTR_ACK:
		ei_cli(sc);
		break;
	}
	return 0;
}

static void
ei_attn(struct ie_softc *sc_ie, int why)
{
	struct ei_softc *sc = (void *)sc_ie;

	bus_space_write_1(sc->sc_ctl_t, sc->sc_ctl_h, EI_CONTROL, EI_CTL_CA);
}

/*
 * Various access functions to allow the MI 82586 driver to get at the
 * board's memory.  All memory accesses must go through these, as
 * nothing else knows how to manipulate the page register.  Note that
 * all addresses and lengths passed in are in bytes of actual data.
 * The bus_space functions deal in words, though, so we have to
 * convert on the way through.
 */

static void
ei_copyin(struct ie_softc *sc_ie, void *dest, int src, size_t size)
{
	struct ei_softc *sc = (struct ei_softc *)sc_ie;
	int cnt, s, extra_byte;
	u_int16_t *wptr;

#ifdef DIAGNOSTIC
	if (src % 2 != 0 || !ALIGNED_POINTER(dest, u_int16_t))
		panic("%s: unaligned copyin", device_xname(sc_ie->sc_dev));
#endif
	wptr = dest;
	extra_byte = size % 2;
	size -= extra_byte;
	while (size > 0) {
		cnt = EI_PAGESIZE - src % EI_PAGESIZE;
		if (cnt > size)
			cnt = size;
		s = splnet();
		ei_setpage(sc, ei_atop(src));
		/* bus ops are in words */
		bus_space_read_region_2(sc->sc_mem_t, sc->sc_mem_h,
					ei_atopo(src) / 2, wptr, cnt / 2);
		splx(s);
		src += cnt;
		wptr += cnt / 2;
		size -= cnt;
	}
	if (extra_byte) {
		/* Do we need to be this careful? */
		s = splnet();
		ei_setpage(sc, ei_atop(src));
		*(u_int8_t *)wptr =
		    bus_space_read_2(sc->sc_mem_t, sc->sc_mem_h,
				     ei_atopo(src) / 2) & 0xff;
		splx(s);
	}
}

static void
ei_copyout(struct ie_softc *sc_ie, const void *src, int dest, size_t size)
{
	struct ei_softc *sc = (struct ei_softc *)sc_ie;
	int cnt, s;
	const u_int16_t *wptr;
	u_int16_t *bounce = NULL;

#ifdef DIAGNOSTIC
	if (dest % 2 != 0)
		panic("%s: unaligned copyout", device_xname(sc_ie->sc_dev));
#endif
	if (!ALIGNED_POINTER(src, u_int16_t)) {
		bounce = (u_int16_t *) malloc(size, M_DEVBUF, M_NOWAIT);
		if (bounce == NULL)
			panic("%s: no memory to align copyout",
			      device_xname(sc_ie->sc_dev));
		memcpy(bounce, src, size);
		src = bounce;
	}
	wptr = src;
	if (size % 2 != 0)
		size++; /* This is safe, since the buffer is 16bit aligned */
	while (size > 0) {
		cnt = EI_PAGESIZE - dest % EI_PAGESIZE;
		if (cnt > size)
			cnt = size;
		s = splnet();
		ei_setpage(sc, ei_atop(dest));
		/* bus ops are in words */
		bus_space_write_region_2(sc->sc_mem_t, sc->sc_mem_h,
					 ei_atopo(dest) / 2, wptr, cnt / 2);
		splx(s);
		wptr += cnt / 2;
		dest += cnt;
		size -= cnt;
	}
	if (bounce != NULL)
		free(bounce, M_DEVBUF);
}

static u_int16_t
ei_read16(struct ie_softc *sc_ie, int addr)
{
	struct ei_softc *sc = (struct ei_softc *)sc_ie;
	int result, s;

#ifdef DIAGNOSTIC
	if (addr % 2 != 0)
		panic("%s: unaligned read16", device_xname(sc_ie->sc_dev));
#endif
	s = splnet();
	ei_setpage(sc, ei_atop(addr));
	result = bus_space_read_2(sc->sc_mem_t, sc->sc_mem_h,
				  ei_atopo(addr) / 2);
	splx(s);
	return result;
}

static void
ei_write16(struct ie_softc *sc_ie, int addr, u_int16_t value)
{
	struct ei_softc *sc = (struct ei_softc *)sc_ie;
	int s;

#ifdef DIAGNOSTIC
	if (addr % 2 != 0)
		panic("%s: unaligned write16", device_xname(sc_ie->sc_dev));
#endif
	s = splnet();
	ei_setpage(sc, ei_atop(addr));
	bus_space_write_2(sc->sc_mem_t, sc->sc_mem_h, ei_atopo(addr) / 2,
			  value);
	splx(s);
}

static void
ei_write24(struct ie_softc *sc_ie, int addr, int value)
{
	int s;

#ifdef DIAGNOSTIC
	if (addr % 2 != 0)
		panic("%s: unaligned write24", device_xname(sc_ie->sc_dev));
#endif
	s = splnet();
	ei_write16(sc_ie, addr, value & 0xffff);
	ei_write16(sc_ie, addr + 2, (value >> 16) & 0xff);
	splx(s);
}

