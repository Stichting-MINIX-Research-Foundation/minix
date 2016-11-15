/*	$NetBSD: aic_isapnp.c,v 1.21 2009/09/22 13:20:36 tsutsui Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Enami Tsugutomo <enami@but-b.or.jp>
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
__KERNEL_RCSID(0, "$NetBSD: aic_isapnp.c,v 1.21 2009/09/22 13:20:36 tsutsui Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/bus.h>

#include <dev/isa/isavar.h>

#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>
#include <dev/isapnp/isapnpdevs.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsipiconf.h>
#include <dev/scsipi/scsi_all.h>

#include <dev/ic/aic6360var.h>

struct aic_isapnp_softc {
	struct	aic_softc sc_aic;	/* real "com" softc */

	/* ISAPnP-specific goo. */
	void	*sc_ih;			/* interrupt handler */
};

static int	aic_isapnp_match(device_t, cfdata_t, void *);
static void	aic_isapnp_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(aic_isapnp, sizeof(struct aic_isapnp_softc),
    aic_isapnp_match, aic_isapnp_attach, NULL, NULL);

int
aic_isapnp_match(device_t parent, cfdata_t match, void *aux)
{
	int pri, variant;

	pri = isapnp_devmatch(aux, &isapnp_aic_devinfo, &variant);
	if (pri && variant > 0)
		pri = 0;
	return (pri);
}

void
aic_isapnp_attach(device_t parent, device_t self, void *aux)
{
	struct aic_isapnp_softc *isc = device_private(self);
	struct aic_softc *sc = &isc->sc_aic;
	struct isapnp_attach_args *ipa = aux;

	sc->sc_dev = self;

	printf("\n");

	if (isapnp_config(ipa->ipa_iot, ipa->ipa_memt, ipa)) {
		aprint_error_dev(self, "error in region allocation\n");
		return;
	}

	sc->sc_iot = ipa->ipa_iot;
	sc->sc_ioh = ipa->ipa_io[0].h;

	if (!aic_find(sc->sc_iot, sc->sc_ioh)) {
		aprint_error_dev(self, "couldn't find device\n");
		return;
	}

	aicattach(sc);

	/* Establish the interrupt handler. */
	isc->sc_ih = isa_intr_establish(ipa->ipa_ic, ipa->ipa_irq[0].num,
	    ipa->ipa_irq[0].type, IPL_BIO, aicintr, sc);
	if (isc->sc_ih == NULL)
		aprint_error_dev(self, "couldn't establish interrupt\n");
}
