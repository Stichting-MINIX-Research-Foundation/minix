/*	$NetBSD: if_an_isapnp.c,v 1.22 2009/05/12 10:16:35 cegger Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
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

/*
 * ISAPnP bus front-end for the Aironet ISA4500/ISA4800 Wireless LAN Adapter.
 * Unlike WaveLAN, this adapter is attached as an ISA device using an address
 * decoder hack.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_an_isapnp.c,v 1.22 2009/05/12 10:16:35 cegger Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/callout.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net80211/ieee80211_netbsd.h>
#include <net80211/ieee80211_var.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/anreg.h>
#include <dev/ic/anvar.h>

#include <dev/isa/isavar.h>

#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>
#include <dev/isapnp/isapnpdevs.h>

int	an_isapnp_match(device_t, cfdata_t, void *);
void	an_isapnp_attach(device_t, device_t, void *);

struct an_isapnp_softc {
	struct an_softc sc_an;			/* real "an" softc */

	/* ISA-specific goo. */
	void	*sc_ih;				/* interrupt cookie */
};

CFATTACH_DECL_NEW(an_isapnp, sizeof(struct an_isapnp_softc),
    an_isapnp_match, an_isapnp_attach, NULL, NULL);

int
an_isapnp_match(device_t parent, cfdata_t match, void *aux)
{
	int pri, variant;

	pri = isapnp_devmatch(aux, &isapnp_an_devinfo, &variant);
	if (pri && variant > 0)
		pri = 0;
	return (pri);
}

void
an_isapnp_attach(device_t parent, device_t self, void *aux)
{
	struct an_isapnp_softc *isc = device_private(self);
	struct an_softc *sc = &isc->sc_an;
	struct isapnp_attach_args *ipa = aux;

	printf("\n");

	if (isapnp_config(ipa->ipa_iot, ipa->ipa_memt, ipa)) {
		aprint_error_dev(self, "can't configure isapnp resources\n");
		return;
	}

	sc->sc_dev = self;
	sc->sc_iot = ipa->ipa_iot;
	sc->sc_ioh = ipa->ipa_io[0].h;

	printf("%s: %s %s\n", device_xname(self), ipa->ipa_devident,
	    ipa->ipa_devclass);

	/* This interface is always enabled. */
	sc->sc_enabled = 1;

	/* Establish the interrupt handler. */
	isc->sc_ih = isa_intr_establish(ipa->ipa_ic, ipa->ipa_irq[0].num,
	    ipa->ipa_irq[0].type, IPL_NET, an_intr, sc);
	if (isc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt handler\n");
		return;
	}

	if (an_attach(sc) != 0) {
		aprint_error_dev(self, "failed to attach controller\n");
		isa_intr_disestablish(ipa->ipa_ic, isc->sc_ih);
		isc->sc_ih = NULL;
	}
}
