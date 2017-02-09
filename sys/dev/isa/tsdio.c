/*	$NetBSD: tsdio.c,v 1.11 2012/10/27 17:18:25 chs Exp $	*/

/*-
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jesse Off
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
__KERNEL_RCSID(0, "$NetBSD: tsdio.c,v 1.11 2012/10/27 17:18:25 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>
#include <dev/isa/tsdiovar.h>

int	tsdio_probe(device_t, cfdata_t, void *);
void	tsdio_attach(device_t, device_t, void *);
int	tsdio_search(device_t, cfdata_t, const int *, void *);
int	tsdio_print(void *, const char *);

CFATTACH_DECL_NEW(tsdio, sizeof(struct tsdio_softc),
    tsdio_probe, tsdio_attach, NULL, NULL);

int
tsdio_probe(device_t parent, cfdata_t cf, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t dioh;
	int rv = 0, have_io = 0;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/*
	 * Disallow wildcarded I/O base.
	 */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);

	/*
	 * Map the I/O space.
	 */
	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr, 8,
	    0, &dioh))
		goto out;
	have_io = 1;

	if (bus_space_read_1(ia->ia_iot, dioh, 0) != 0x54) {
		goto out;
	}

	rv = 1;
	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = 8;
	ia->ia_niomem = 0;
	ia->ia_nirq = 0;
	ia->ia_ndrq = 0;

 out:
	if (have_io)
		bus_space_unmap(iot, dioh, 8);

	return (rv);
}

void
tsdio_attach(device_t parent, device_t self, void *aux)
{
	struct tsdio_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;

	sc->sc_iot = ia->ia_iot;

	aprint_normal(": Technologic Systems TS-DIO24\n");

	/*
	 * Map the device.
	 */
	if (bus_space_map(sc->sc_iot, ia->ia_io[0].ir_addr, 8,
	    0, &sc->sc_ioh)) {
		aprint_error_dev(self, "unable to map i/o space\n");
		return;
	}

	/*
	 *  Attach sub-devices
	 */
	config_search_ia(tsdio_search, self, "tsdio", NULL);
}

int
tsdio_search(device_t parent, cfdata_t cf, const int *l, void *aux)
{
	struct tsdio_softc *sc = device_private(parent);
	struct tsdio_attach_args sa;

	sa.ta_iot = sc->sc_iot;
	sa.ta_ioh = sc->sc_ioh;

	if (config_match(parent, cf, &sa) > 0)
		config_attach(parent, cf, &sa, tsdio_print);

	return (0);
}

int
tsdio_print(void *aux, const char *name)
{

	return (UNCONF);
}
