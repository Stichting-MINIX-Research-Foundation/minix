/*	$NetBSD: wss_isapnp.c,v 1.27 2011/11/22 19:33:38 jakllsch Exp $	*/

/*
 * Copyright (c) 1997, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@NetBSD.org).
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
__KERNEL_RCSID(0, "$NetBSD: wss_isapnp.c,v 1.27 2011/11/22 19:33:38 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/errno.h>

#include <sys/bus.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>
#include <dev/isapnp/isapnpdevs.h>

#include <dev/isa/ad1848var.h>
#include <dev/isa/madreg.h>
#include <dev/isa/wssreg.h>
#include <dev/isa/wssvar.h>
#include <dev/isa/sbreg.h>

int	wss_isapnp_match(device_t, cfdata_t, void *);
void	wss_isapnp_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(wss_isapnp, sizeof(struct wss_softc),
    wss_isapnp_match, wss_isapnp_attach, NULL, NULL);

/*
 * Probe / attach routines.
 */

/*
 * Probe for the WSS hardware.
 */
int
wss_isapnp_match(device_t parent, cfdata_t match, void *aux)
{
	int pri, variant;

	pri = isapnp_devmatch(aux, &isapnp_wss_devinfo, &variant);
	if (pri && variant > 1)
		pri = 0;
	return pri;
}

/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver.
 */
void
wss_isapnp_attach(device_t parent, device_t self, void *aux)
{
	struct wss_softc *sc;
	struct ad1848_softc *ac;
	struct isapnp_attach_args *ipa;
	int variant;

	sc = device_private(self);
	ac = &sc->sc_ad1848.sc_ad1848;
	ac->sc_dev = self;
	ipa = aux;
	printf("\n");

	if (!isapnp_devmatch(aux, &isapnp_wss_devinfo, &variant)) {
		aprint_error_dev(self, "match failed?\n");
		return;
	}

	if (isapnp_config(ipa->ipa_iot, ipa->ipa_memt, ipa)) {
		aprint_error_dev(self, "error in region allocation\n");
		return;
	}

	switch (variant) {
	case 1:
		/* We have to put the chip into `WSS mode'. */
		{
			bus_space_tag_t iot;
			bus_space_handle_t ioh;

			iot = ipa->ipa_iot;
			ioh = ipa->ipa_io[0].h;

			while (bus_space_read_1(iot, ioh,
			    SBP_DSP_WSTAT) & SB_DSP_BUSY);
			bus_space_write_1(iot, ioh, SBP_DSP_WRITE, 0x09);
			while (bus_space_read_1(iot, ioh,
			    SBP_DSP_WSTAT) & SB_DSP_BUSY);
			bus_space_write_1(iot, ioh, SBP_DSP_WRITE, 0x00);
			while (bus_space_read_1(iot, ioh,
			    SBP_DSP_WSTAT) & SB_DSP_BUSY);

			delay(1000);
		}

		sc->sc_iot = ipa->ipa_iot;
		sc->sc_ioh = ipa->ipa_io[2].h;
		break;

	default:
		sc->sc_iot = ipa->ipa_iot;
		sc->sc_ioh = ipa->ipa_io[0].h;
		break;
	}

	sc->mad_chip_type = MAD_NONE;

	/* Set up AD1848 I/O handle. */
	ac->sc_iot = sc->sc_iot;
	ac->sc_ioh = sc->sc_ioh;

	sc->sc_ad1848.sc_ic = ipa->ipa_ic;

	sc->wss_ic = ipa->ipa_ic;
	sc->wss_irq = ipa->ipa_irq[0].num;
	sc->wss_playdrq = ipa->ipa_drq[0].num;
	sc->wss_recdrq =
	    ipa->ipa_ndrq > 1 ? ipa->ipa_drq[1].num : ipa->ipa_drq[0].num;

	if (!ad1848_isa_probe(&sc->sc_ad1848)) {
		aprint_error_dev(self, "ad1848_probe failed\n");
		return;
	}

	aprint_error_dev(self, "%s %s", ipa->ipa_devident,
	    ipa->ipa_devclass);

	ac->mode = 2;

	wssattach(sc);

	/* set up OPL I/O handle for ISAPNP boards w/o MAD */
	if (ipa->ipa_nio > 1 && sc->mad_chip_type == MAD_NONE) {
		struct audio_attach_args arg;

		sc->sc_opl_ioh = ipa->ipa_io[1].h;

		arg.type = AUDIODEV_TYPE_OPL;
		arg.hwif = 0;
		arg.hdl = 0;
		(void)config_found(self, &arg, audioprint);
	}
}
