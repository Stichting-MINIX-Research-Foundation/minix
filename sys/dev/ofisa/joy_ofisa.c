/*	$NetBSD: joy_ofisa.c,v 1.15 2011/11/23 23:07:33 jmcneill Exp $	*/

/*-
 * Copyright (c) 1996, 1998, 2008 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: joy_ofisa.c,v 1.15 2011/11/23 23:07:33 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <sys/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/isa/isavar.h>
#include <dev/ofisa/ofisavar.h>

#include <dev/ic/joyvar.h>

#define	JOY_NPORTS	1	/* XXX should be in a header file */

struct joy_ofisa_softc {
	struct joy_softc sc_joy;
	kmutex_t sc_lock;
};

static int	joy_ofisa_match(device_t, cfdata_t, void *);
static void	joy_ofisa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(joy_ofisa, sizeof(struct joy_ofisa_softc),
    joy_ofisa_match, joy_ofisa_attach, NULL, NULL);

static int
joy_ofisa_match(device_t parent, cfdata_t match, void *aux)
{
	struct ofisa_attach_args *aa = aux;
	static const char *const compatible_strings[] = {
		"pnpPNP,b02f",			/* generic joystick */
		NULL,
	};
	int rv = 0;

	if (of_compatible(aa->oba.oba_phandle, compatible_strings) != -1)
		rv = 1;
	return rv;
}

static void
joy_ofisa_attach(device_t parent, device_t self, void *aux)
{
	struct joy_ofisa_softc *osc = device_private(self);
	struct joy_softc *sc = &osc->sc_joy;
	struct ofisa_attach_args *aa = aux;
	struct ofisa_reg_desc reg;
	char *model = NULL;
	int n;

	/*
	 * We're living on an OFW.  We have to ask the OFW what our
	 * register property looks like.
	 *
	 * We expect:
	 *
	 *	1 i/o register region
	 */

	n = ofisa_reg_get(aa->oba.oba_phandle, &reg, 1);
	if (n != 1) {
		aprint_error(": error getting register data\n");
		return;
	}
	if (reg.type != OFISA_REG_TYPE_IO) {
		aprint_error(": register type not i/o\n");
		return;
	}
	if (reg.len != JOY_NPORTS) {
		aprint_error(": weird register size (%lu, expected %d)\n",
		    (unsigned long)reg.len, JOY_NPORTS);
		return;
	}

	sc->sc_iot = aa->iot;
	sc->sc_dev = self;

	if (bus_space_map(sc->sc_iot, reg.addr, reg.len, 0, &sc->sc_ioh)) {
		aprint_error(": unable to map register space\n");
		return;
	}

	n = OF_getproplen(aa->oba.oba_phandle, "model");
	if (n > 0) {
		model = alloca(n);
		if (OF_getprop(aa->oba.oba_phandle, "model", model, n) != n)
			model = NULL;	/* safe; alloca */
	}
	if (model != NULL)
		aprint_normal(": %s", model);
	aprint_normal("\n");

	mutex_init(&osc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	sc->sc_lock = &osc->sc_lock;

	joyattach(sc);
}
