/*	$NetBSD: joy_isa.c,v 1.14 2011/12/05 19:20:54 christos Exp $	*/

/*-
 * Copyright (c) 1995 Jean-Marc Zucconi
 * All rights reserved.
 *
 * Ported to NetBSD by Matthieu Herrb <matthieu@laas.fr>.  Additional
 * modification by Jason R. Thorpe <thorpej@NetBSD.org>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: joy_isa.c,v 1.14 2011/12/05 19:20:54 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <sys/bus.h>

#include <dev/isa/isavar.h>

#include <dev/ic/joyvar.h>

#define JOY_NPORTS    1

struct joy_isa_softc {
	struct joy_softc	sc_joy;
	kmutex_t		sc_lock;
};

static int	joy_isa_probe(device_t, cfdata_t, void *);
static void	joy_isa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(joy_isa, sizeof(struct joy_isa_softc),
    joy_isa_probe, joy_isa_attach, NULL, NULL);

static int
joy_isa_probe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int rval = 0;

	if (ia->ia_nio < 1)
		return 0;

	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return 0;

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, JOY_NPORTS, 0, &ioh))
		return 0;

#ifdef WANT_JOYSTICK_CONNECTED
	bus_space_write_1(iot, ioh, 0, 0xff);
	DELAY(10000);		/* 10 ms delay */
	if ((bus_space_read_1(iot, ioh, 0) & 0x0f) != 0x0f)
		rval = 1;
#else
	rval = 1;
#endif

	bus_space_unmap(iot, ioh, JOY_NPORTS);

	if (rval) {
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = JOY_NPORTS;

		ia->ia_niomem = 0;
		ia->ia_nirq = 0;
		ia->ia_ndrq = 0;
	}
	return rval;
}

static void
joy_isa_attach(device_t parent, device_t self, void *aux)
{
	struct joy_isa_softc *isc = device_private(self);
	struct joy_softc *sc = &isc->sc_joy;
	struct isa_attach_args *ia = aux;

	aprint_normal("\n");

	sc->sc_iot = ia->ia_iot;
	sc->sc_dev = self;

	if (bus_space_map(sc->sc_iot, ia->ia_io[0].ir_addr, JOY_NPORTS, 0,
	    &sc->sc_ioh)) {
		aprint_error_dev(self, "can't map i/o space\n");
		return;
	}

	mutex_init(&isc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	sc->sc_lock = &isc->sc_lock;

	joyattach(sc);
}
