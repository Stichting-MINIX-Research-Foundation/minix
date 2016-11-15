/*	$NetBSD: joy_isapnp.c,v 1.14 2011/12/05 19:20:54 christos Exp $	*/

/*-
 * Copyright (c) 1996, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
__KERNEL_RCSID(0, "$NetBSD: joy_isapnp.c,v 1.14 2011/12/05 19:20:54 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <sys/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>
#include <dev/isapnp/isapnpdevs.h>

#include <dev/ic/joyvar.h>

struct joy_isapnp_softc {
	struct joy_softc sc_joy;
	kmutex_t sc_lock;
};

static int	joy_isapnp_match(device_t, cfdata_t, void *);
static void	joy_isapnp_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(joy_isapnp, sizeof(struct joy_isapnp_softc),
    joy_isapnp_match, joy_isapnp_attach, NULL, NULL);

static int
joy_isapnp_match(device_t parent, cfdata_t match, void *aux)
{
	int pri, variant;

	pri = isapnp_devmatch(aux, &isapnp_joy_devinfo, &variant);
	if (pri && variant > 0)
		pri = 0;
	return pri;
}

static void
joy_isapnp_attach(device_t parent, device_t self, void *aux)
{
	struct joy_isapnp_softc *isc = device_private(self);
	struct joy_softc *sc = &isc->sc_joy;
	struct isapnp_attach_args *ipa = aux;
	bus_space_handle_t ioh;

	aprint_normal("\n");

	if (isapnp_config(ipa->ipa_iot, ipa->ipa_memt, ipa)) {
		aprint_error_dev(self, "error in region allocation\n");
		return;
	}

	if (ipa->ipa_io[0].length == 8) {
		if (bus_space_subregion(ipa->ipa_iot, ipa->ipa_io[0].h, 1, 1,
		    &ioh) < 0) {
			aprint_error_dev(self, "error in region allocation\n");
			return;
		}
	} else
		ioh = ipa->ipa_io[0].h;

	mutex_init(&isc->sc_lock, MUTEX_DEFAULT, IPL_NONE);

	sc->sc_iot = ipa->ipa_iot;
	sc->sc_ioh = ioh;
	sc->sc_dev = self;
	sc->sc_lock = &isc->sc_lock;

	aprint_normal_dev(self, "%s %s\n", ipa->ipa_devident,
	    ipa->ipa_devclass);

	joyattach(sc);
}
