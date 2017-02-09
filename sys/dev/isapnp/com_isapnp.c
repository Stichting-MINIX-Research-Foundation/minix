/*	$NetBSD: com_isapnp.c,v 1.29 2008/04/28 20:23:52 martin Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Michael Graff <explorer@flame.org>.
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
__KERNEL_RCSID(0, "$NetBSD: com_isapnp.c,v 1.29 2008/04/28 20:23:52 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/termios.h>

#include <sys/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>
#include <dev/isapnp/isapnpdevs.h>

#include <dev/ic/comvar.h>
#include <dev/ic/comreg.h>

struct com_isapnp_softc {
	struct	com_softc sc_com;	/* real "com" softc */

	/* ISAPnP-specific goo. */
	void	*sc_ih;			/* interrupt handler */
};

int	com_isapnp_match(device_t, cfdata_t , void *);
void	com_isapnp_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(com_isapnp, sizeof(struct com_isapnp_softc),
    com_isapnp_match, com_isapnp_attach, NULL, NULL);

int
com_isapnp_match(device_t parent, cfdata_t match, void *aux)
{
	int pri, variant;

	pri = isapnp_devmatch(aux, &isapnp_com_devinfo, &variant);
	if (pri && variant > 0)
		pri = 0;
	return (pri);
}

void
com_isapnp_attach(device_t parent, device_t self, void *aux)
{
	struct com_isapnp_softc *isc = device_private(self);
	struct com_softc *sc = &isc->sc_com;
	struct isapnp_attach_args *ipa = aux;

	sc->sc_dev = self;

	if (isapnp_config(ipa->ipa_iot, ipa->ipa_memt, ipa)) {
		aprint_error_dev(self, "error in region allocation\n");
		return;
	}

	COM_INIT_REGS(sc->sc_regs, ipa->ipa_iot, ipa->ipa_io[0].h,
	    ipa->ipa_io[0].base);

	/*
	 * if the chip isn't something we recognise skip it.
	 */
	if (com_probe_subr(&sc->sc_regs) == 0)
		return;

	sc->sc_frequency = 115200 * 16; /* is that always right? */

	com_attach_subr(sc);

	isc->sc_ih = isa_intr_establish(ipa->ipa_ic, ipa->ipa_irq[0].num,
	    ipa->ipa_irq[0].type, IPL_SERIAL, comintr, sc);
}
