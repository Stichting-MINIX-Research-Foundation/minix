/*	$NetBSD: if_fmv_isapnp.c,v 1.12 2008/04/28 20:23:52 martin Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and Matt Thomas of the 3am Software Foundry.
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
__KERNEL_RCSID(0, "$NetBSD: if_fmv_isapnp.c,v 1.12 2008/04/28 20:23:52 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/mb86960reg.h>
#include <dev/ic/mb86960var.h>
#include <dev/ic/fmvreg.h>
#include <dev/ic/fmvvar.h>

#include <dev/isa/isavar.h>

#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>
#include <dev/isapnp/isapnpdevs.h>

int	fmv_isapnp_match(device_t, cfdata_t, void *);
void	fmv_isapnp_attach(device_t, device_t, void *);

struct fmv_isapnp_softc {
	struct	mb86960_softc sc_mb86960;	/* real "mb86960" softc */

	/* ISAPnP-specific goo. */
	void	*sc_ih;				/* interrupt cookie */
};

CFATTACH_DECL_NEW(fmv_isapnp, sizeof(struct fmv_isapnp_softc),
    fmv_isapnp_match, fmv_isapnp_attach, NULL, NULL);

int
fmv_isapnp_match(device_t parent, cfdata_t cf, void *aux)
{
	int pri, variant;

	pri = isapnp_devmatch(aux, &isapnp_fmv_devinfo, &variant);
	if (pri && variant > 0)
		pri = 0;
	return pri;
}

void
fmv_isapnp_attach(device_t parent, device_t self, void *aux)
{
	struct fmv_isapnp_softc *isc = device_private(self);
	struct mb86960_softc *sc = &isc->sc_mb86960;
	struct isapnp_attach_args * const ipa = aux;

	sc->sc_dev = self;

	if (isapnp_config(ipa->ipa_iot, ipa->ipa_memt, ipa)) {
		aprint_error(": can't configure isapnp resources\n");
		return;
	}

	sc->sc_bst = ipa->ipa_iot;
	sc->sc_bsh = ipa->ipa_io[0].h;

	fmv_attach(sc);

	/* Establish the interrupt handler. */
	isc->sc_ih = isa_intr_establish(ipa->ipa_ic, ipa->ipa_irq[0].num,
	    ipa->ipa_irq[0].type, IPL_NET, mb86960_intr, sc);
	if (isc->sc_ih == NULL)
		aprint_error_dev(sc->sc_dev,
		    "couldn't establish interrupt handler\n");
}
