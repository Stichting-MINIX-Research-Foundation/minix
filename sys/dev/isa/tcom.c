/*	$NetBSD: tcom.c,v 1.19 2009/05/12 09:10:16 cegger Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Eric S. Hvozda.
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
 * Copyright (c) 1998 Jukka Marin.  All rights reserved.
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1995 Charles Hannum.  All rights reserved.
 *
 * This code is derived from public-domain software written by
 * Roland McGrath.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
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
 * NOTE: This driver has been tested in TCOM "UNIX mode" with base address
 * set to 0x100 or 0x200 and baud rate jumpers to "normal".  This code
 * does NOT know about the 2x and 4x baud rates available on TCOM cards.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tcom.c,v 1.19 2009/05/12 09:10:16 cegger Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/termios.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include <dev/isa/isavar.h>
#include <dev/isa/com_multi.h>

#define	NSLAVES	8			/* TCOM cards have 4 or 8 UARTs */
#define STATUS_OFFSET	0x40		/* offset from board base address */
#define	STATUS_SIZE	8		/* 8 bytes reserved for irq status */

struct tcom_softc {
	device_t sc_dev;
	void *sc_ih;

	bus_space_tag_t sc_iot;
	int sc_iobase;

	int sc_alive;			/* mask of slave units attached */
	void *sc_slaves[NSLAVES];	/* com device softc pointers */
	bus_space_handle_t sc_slaveioh[NSLAVES];
	bus_space_handle_t sc_statusioh;
};

int tcomprobe(device_t, cfdata_t, void *);
void tcomattach(device_t, device_t, void *);
int tcomintr(void *);

CFATTACH_DECL_NEW(tcom, sizeof(struct tcom_softc),
    tcomprobe, tcomattach, NULL, NULL);

int
tcomprobe(device_t parent, cfdata_t self, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int i, iobase, rv = 1;

	/*
	 * Do the normal com probe for the first UART and assume
	 * its presence, and the ability to map the other UARTS,
	 * means there is a multiport board there.
	 * XXX Needs more robustness.
	 */

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (1);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* Disallow wildcarded i/o address. */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);
	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ)
		return (0);

	/* if the first port is in use as console, then it. */
	if (com_is_console(iot, ia->ia_io[0].ir_addr, 0))
		goto checkmappings;

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, COM_NPORTS, 0, &ioh)) {
		rv = 0;
		goto out;
	}
	rv = comprobe1(iot, ioh);
	bus_space_unmap(iot, ioh, COM_NPORTS);
	if (rv == 0)
		goto out;

checkmappings:
	for (i = 1, iobase = ia->ia_io[0].ir_addr; i < NSLAVES; i++) {
		iobase += COM_NPORTS;

		if (com_is_console(iot, iobase, 0))
			continue;

		if (bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh)) {
			rv = 0;
			goto out;
		}
		bus_space_unmap(iot, ioh, COM_NPORTS);
	}

out:
	if (rv) {
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = NSLAVES * COM_NPORTS;

		ia->ia_nirq = 1;

		ia->ia_niomem = 0;
		ia->ia_ndrq = 0;
	}
	return (rv);
}

void
tcomattach(device_t parent, device_t self, void *aux)
{
	struct tcom_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;
	struct commulti_attach_args ca;
	bus_space_tag_t iot = ia->ia_iot;
	int i, iobase;
	device_t slave;

	printf("\n");

	sc->sc_dev = self;
	sc->sc_iot = ia->ia_iot;
	sc->sc_iobase = ia->ia_io[0].ir_addr;

	for (i = 0; i < NSLAVES; i++) {
		iobase = sc->sc_iobase + i * COM_NPORTS;
		if (!com_is_console(iot, iobase, &sc->sc_slaveioh[i]) &&
		    bus_space_map(iot, iobase, COM_NPORTS, 0,
			&sc->sc_slaveioh[i])) {
			aprint_error_dev(sc->sc_dev, "can't map i/o space for slave %d\n", i);
			return;
		}
	}

	if (bus_space_map(iot, sc->sc_iobase + STATUS_OFFSET, STATUS_SIZE, 0,
	    &sc->sc_statusioh)) {
		aprint_error_dev(sc->sc_dev, "can't map status space\n");
		return;
	}

	for (i = 0; i < NSLAVES; i++) {
		ca.ca_slave = i;
		ca.ca_iot = sc->sc_iot;
		ca.ca_ioh = sc->sc_slaveioh[i];
		ca.ca_iobase = sc->sc_iobase + i * COM_NPORTS;
		ca.ca_noien = 0;

		slave = config_found(self, &ca, commultiprint);
		if (slave != NULL) {
			sc->sc_alive |= 1 << i;
			sc->sc_slaves[i] = device_private(slave);
		}
	}

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_SERIAL, tcomintr, sc);
}

int
tcomintr(void *arg)
{
	struct tcom_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	int alive = sc->sc_alive;
	int bits;

	bits = ~bus_space_read_1(iot, sc->sc_statusioh, 0) & alive;
	if (bits == 0)
		return (0);

	for (;;) {
#define	TRY(n) \
		if (bits & (1 << (n))) \
			comintr(sc->sc_slaves[n]);
		TRY(0);
		TRY(1);
		TRY(2);
		TRY(3);
		TRY(4);
		TRY(5);
		TRY(6);
		TRY(7);
#undef TRY
		bits = ~bus_space_read_1(iot, sc->sc_statusioh, 0) & alive;
		if (bits == 0)
			return (1);
 	}
}
