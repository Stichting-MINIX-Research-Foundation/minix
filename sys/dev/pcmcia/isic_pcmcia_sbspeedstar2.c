/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: isic_pcmcia_sbspeedstar2.c,v 1.13 2014/03/23 02:50:08 christos Exp $");

#include "opt_isicpcmcia.h"
#ifdef ISICPCMCIA_SBSPEEDSTAR2

/*
 * Card format:
 *
 * iobase + 0 : reset on (0x03), off (0x0)
 * iobase + 1 : isac read/write
 * iobase + 2 : hscx read/write ( offset 0-0x3f    hscx0 ,
 *                                offset 0x40-0x7f hscx1 )
 * iobase + 4 : address register
 *
 */

#define SBSS_RESET  0 /* reset on / off           */
#define SBSS_ISAC   1 /* ISAC                     */
#define SBSS_HSCX   2 /* HSCX0                    */
#define SBSS_RW     4 /* indirect access register */

#define SBSS_REGS   8 /* we use an area of 8 bytes for io */

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/device.h>
#include <sys/bus.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_ioctl.h>
#include <netisdn/i4b_l2.h>
#include <netisdn/i4b_l1l2.h>

#include <dev/ic/isic_l1.h>
#include <dev/ic/isac.h>
#include <dev/ic/hscx.h>

#include <netisdn/i4b_global.h>
#include <netisdn/i4b_l1l2.h>
#include <netisdn/i4b_mbuf.h>
#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>

#include <dev/pcmcia/isic_pcmcia.h>

/*---------------------------------------------------------------------------*
 *      Sedlbauer SpeedStar ISAC get fifo routine
 *---------------------------------------------------------------------------*/

static void
sws_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, SBSS_RW, 0);
			bus_space_read_multi_1(t, h, SBSS_ISAC, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, SBSS_RW, 0);
			bus_space_read_multi_1(t, h, SBSS_HSCX, buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, SBSS_RW, 0x40);
			bus_space_read_multi_1(t, h, SBSS_HSCX, buf, size);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *      Sedlbauer SpeedStar ISAC put fifo routine
 *---------------------------------------------------------------------------*/

static void
sws_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, SBSS_RW, 0);
			bus_space_write_multi_1(t, h, SBSS_ISAC, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, SBSS_RW, 0);
			bus_space_write_multi_1(t, h, SBSS_HSCX, buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, SBSS_RW, 0x40);
			bus_space_write_multi_1(t, h, SBSS_HSCX, buf, size);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *      Sedlbauer SpeedStar ISAC put register routine
 *---------------------------------------------------------------------------*/

static void
sws_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, SBSS_RW, offs);
			bus_space_write_1(t, h, SBSS_ISAC, data);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, SBSS_RW, offs);
			bus_space_write_1(t, h, SBSS_HSCX, data);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, SBSS_RW, 0x40+offs);
			bus_space_write_1(t, h, SBSS_HSCX, data);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	Sedlbauer SpeedStar ISAC get register routine
 *---------------------------------------------------------------------------*/

static u_int8_t
sws_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, SBSS_RW, offs);
			return bus_space_read_1(t, h, SBSS_ISAC);
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, SBSS_RW, offs);
			return bus_space_read_1(t, h, SBSS_HSCX);
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, SBSS_RW, 0x40+offs);
			return bus_space_read_1(t, h, SBSS_HSCX);
	}
	return 0;
}

/*
 * XXX - one time only! Some of this has to go into an enable
 * function, with apropriate counterpart in disable, so a card
 * could be removed an inserted again.
 */
int
isic_attach_sbspeedstar2(struct pcmcia_isic_softc *psc, struct pcmcia_config_entry *cfe, struct pcmcia_attach_args *pa)
{
	struct isic_softc * sc = &psc->sc_isic;

	/* Validate config info */
	if (cfe->num_memspace != 0)
		printf(": unexpected number of memory spaces %d should be 0\n",
			cfe->num_memspace);
	if (cfe->num_iospace != 1)
		printf(": unexpected number of memory spaces %d should be 1\n",
			cfe->num_iospace);

	/* Allocate pcmcia space */
	if (pcmcia_io_alloc(pa->pf, 0, cfe->iospace[0].length,
			    cfe->iospace[0].length, &psc->sc_pcioh))
		printf(": can't allocate i/o space\n");

	/* map them */
	if (pcmcia_io_map(pa->pf, ((cfe->flags & PCMCIA_CFE_IO16) ?
	    PCMCIA_WIDTH_IO16 : PCMCIA_WIDTH_IO8), &psc->sc_pcioh,
	    &psc->sc_io_window)) {
		printf(": can't map i/o space\n");
		return 0;
	}

	/* Setup bus space maps */
	sc->sc_num_mappings = 1;
	MALLOC_MAPS(sc);

	/* Copy our handles/tags to the MI maps */
	sc->sc_maps[0].t = psc->sc_pcioh.iot;
	sc->sc_maps[0].h = psc->sc_pcioh.ioh;
	sc->sc_maps[0].offset = 0;
	sc->sc_maps[0].size = 0;	/* not our mapping */

	/* setup access routines */

	sc->readreg   = sws_read_reg;
	sc->writereg  = sws_write_reg;

	sc->readfifo  = sws_read_fifo;
	sc->writefifo = sws_write_fifo;

	/* setup IOM bus type */

	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* reset card */
        {
        	bus_space_tag_t t1 = sc->sc_maps[0].t;
        	bus_space_handle_t h1 = sc->sc_maps[0].h;
        	bus_space_write_1(t1, h1, SBSS_RESET, 0x3);
		DELAY(SEC_DELAY / 5);
		bus_space_write_1(t1, h1, SBSS_RESET, 0);
		DELAY(SEC_DELAY / 5);
	}

	return 1;
}

#endif /* ISICPCMCIA_SBSPEEDSTAR2 */
