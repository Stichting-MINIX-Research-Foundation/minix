/*	$NetBSD: opl_isa.c,v 1.21 2012/04/09 10:18:17 plunky Exp $	*/

/*-
 * Copyright (c) 1999, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson, and by Andrew Doran.
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
__KERNEL_RCSID(0, "$NetBSD: opl_isa.c,v 1.21 2012/04/09 10:18:17 plunky Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/select.h>
#include <sys/audioio.h>
#include <sys/midiio.h>

#include <sys/bus.h>

#include <dev/audio_if.h>
#include <dev/midi_if.h>

#include <dev/ic/oplreg.h>
#include <dev/ic/oplvar.h>

#include <dev/isa/isavar.h>

#define OPL_SIZE 4

int	opl_isa_match(device_t, cfdata_t, void *);
void	opl_isa_attach(device_t, device_t, void *);

struct opl_isa_softc {
	struct opl_softc	sc_opl;
	kmutex_t		sc_lock;
};

CFATTACH_DECL_NEW(opl_isa, sizeof(struct opl_isa_softc),
    opl_isa_match, opl_isa_attach, NULL, NULL);

int
opl_isa_match(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh;
	int r;

	if (ia->ia_nio < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);

	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr, OPL_SIZE, 0, &ioh))
		return (0);
	r = opl_match(ia->ia_iot, ioh, 0);
        bus_space_unmap(ia->ia_iot, ioh, OPL_SIZE);
	if (r != 0) {
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = OPL_SIZE;

		ia->ia_nirq = 0;
		ia->ia_niomem = 0;
		ia->ia_ndrq = 0;
	}
	return (r);
}

void
opl_isa_attach(device_t parent, device_t self, void *aux)
{
	struct opl_softc *sc = device_private(self);
	struct opl_isa_softc *isa = device_private(self);
	struct isa_attach_args *ia = aux;

	sc->dev = self;
	sc->iot = ia->ia_iot;

	if (bus_space_map(sc->iot, ia->ia_io[0].ir_addr, OPL_SIZE,
	    0, &sc->ioh)) {
		aprint_error("opl_isa_attach: bus_space_map failed\n");
		return;
	}
	sc->offs = 0;
	sc->lock = &isa->sc_lock;
	mutex_init(&isa->sc_lock, MUTEX_DEFAULT, IPL_NONE);

	opl_attach(sc);
}
