/*	$NetBSD: sb_ofisa.c,v 1.17 2008/04/28 20:23:54 martin Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
__KERNEL_RCSID(0, "$NetBSD: sb_ofisa.c,v 1.17 2008/04/28 20:23:54 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/mulaw.h>

#include <dev/ofw/openfirm.h>
#include <dev/isa/isavar.h>
#include <dev/ofisa/ofisavar.h>

#include <dev/isa/sbreg.h>
#include <dev/isa/sbvar.h>
#include <dev/isa/sbdspvar.h>

int	sb_ofisa_match(device_t, cfdata_t, void *);
void	sb_ofisa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(sb_ofisa, sizeof(struct sbdsp_softc),
    sb_ofisa_match, sb_ofisa_attach, NULL, NULL);

int
sb_ofisa_match(device_t parent, cfdata_t cf, void *aux)
{
	struct ofisa_attach_args *aa = aux;
	static const char *const compatible_strings[] = {
		"pnpPNP,b000",			/* generic SB 1.5 */
		"pnpPNP,b001",			/* generic SB 2.0 */
		"pnpPNP,b002",			/* generic SB Pro */
		"pnpPNP,b003",			/* generic SB 16 */
		NULL,
	};
	int rv = 0;

	if (of_compatible(aa->oba.oba_phandle, compatible_strings) != -1) {
		/*
		 * Use a low match priority so that a more specific driver
		 * can match, e.g. a native ESS driver.
		 */
		rv = 1;
	}

	return (rv);
}

void
sb_ofisa_attach(device_t parent, device_t self, void *aux)
{
	struct sbdsp_softc *sc = device_private(self);
	struct ofisa_attach_args *aa = aux;
	struct ofisa_reg_desc reg;
	struct ofisa_intr_desc intr;
	struct ofisa_dma_desc dma[2];
	int n, ndrq;
	char *model;

	sc->sc_dev = self;

	/*
	 * We're living on an OFW.  We have to ask the OFW what our
	 * registers and interrupts properties look like.
	 *
	 * We expect:
	 *
	 *	1 i/o register region
	 *	1 interrupt
	 *	1 or 2 DMA channels
	 */

	n = ofisa_reg_get(aa->oba.oba_phandle, &reg, 1);
	if (n != 1) {
		printf(": error getting register data\n");
		return;
	}
	if (reg.type != OFISA_REG_TYPE_IO) {
		printf(": register type not i/o\n");
		return;
	}
	if (reg.len != SB_NPORT && reg.len != SBP_NPORT) {
		printf(": weird register size (%lu, expected %d or %d)\n",
		    (unsigned long)reg.len, SB_NPORT, SBP_NPORT);
		return;
	}

	n = ofisa_intr_get(aa->oba.oba_phandle, &intr, 1);
	if (n != 1) {
		printf(": error getting interrupt data\n");
		return;
	}

	ndrq = ofisa_dma_get(aa->oba.oba_phandle, dma, 2);
	if (ndrq != 1 && ndrq != 2) {
		printf(": error getting DMA data\n");
		return;
	}

	sc->sc_ic = aa->ic;

	sc->sc_iot = aa->iot;
	if (bus_space_map(sc->sc_iot, reg.addr, reg.len, 0, &sc->sc_ioh)) {
		aprint_error(": unable to map register space\n");
		return;
	}

	/* XXX These are only for setting chip configuration registers. */
	sc->sc_iobase = reg.addr;
	sc->sc_irq = intr.irq;

	sc->sc_drq8 = DRQUNK;
	sc->sc_drq16 = DRQUNK;

	for (n = 0; n < ndrq; n++) {
		/* XXX check mode? */
		switch (dma[n].width) {
		case 8:
			if (sc->sc_drq8 == DRQUNK)
				sc->sc_drq8 = dma[n].drq;
			break;
		case 16:
			if (sc->sc_drq16 == DRQUNK)
				sc->sc_drq16 = dma[n].drq;
			break;
		default:
			aprint_error(": weird DMA width %d\n", dma[n].width);
			return;
		}
	}

	if (sc->sc_drq8 == DRQUNK) {
		aprint_error(": no 8-bit DMA channel\n");
		return;
	}

	if (sbmatch(sc) == 0) {
		aprint_error(": sbmatch failed\n");
		return;
	}

	sc->sc_ih = isa_intr_establish(aa->ic, intr.irq, IST_EDGE, IPL_AUDIO,
	    sbdsp_intr, sc);

	n = OF_getproplen(aa->oba.oba_phandle, "model");
	if (n > 0) {
		model = alloca(n);
		if (OF_getprop(aa->oba.oba_phandle, "model", model, n) == n)
			aprint_normal(": %s\n%s", model, device_xname(self));
	}

	sbattach(sc);
}
