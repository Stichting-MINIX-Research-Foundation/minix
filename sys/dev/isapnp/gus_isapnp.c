/*	$NetBSD: gus_isapnp.c,v 1.37 2012/10/27 17:18:26 chs Exp $	*/

/*
 * Copyright (c) 1997, 1999, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Kari Mettinen
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
__KERNEL_RCSID(0, "$NetBSD: gus_isapnp.c,v 1.37 2012/10/27 17:18:26 chs Exp $");

#include "guspnp.h"
#if NGUSPNP > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/audioio.h>

#include <dev/audio_if.h>
#include <dev/audiovar.h>
#include <dev/mulaw.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>
#include <dev/isapnp/isapnpdevs.h>

#include <dev/ic/interwavevar.h>
#include <dev/ic/interwavereg.h>


int	gus_isapnp_match(device_t, cfdata_t, void *);
void	gus_isapnp_attach(device_t, device_t, void *);
static int     gus_isapnp_open(void *, int);

static const struct audio_hw_if guspnp_hw_if = {
	gus_isapnp_open,
	iwclose,
	NULL,			/* drain */
	iw_query_encoding,
	iw_set_params,
	iw_round_blocksize,
	iw_commit_settings,
	iw_init_output,
	iw_init_input,
	iw_start_output,
	iw_start_input,
	iw_halt_output,
	iw_halt_input,
	iw_speaker_ctl,
	iw_getdev,
	iw_setfd,
	iw_set_port,
	iw_get_port,
	iw_query_devinfo,
	iw_malloc,
	iw_free,
	iw_round_buffersize,
	iw_mappage,
	iw_get_props,
	NULL,			/* trigger_output */
	NULL,			/* trigger_input */
	NULL,			/* dev_ioctl */
	iw_get_locks,
};

CFATTACH_DECL_NEW(guspnp, sizeof(struct iw_softc),
    gus_isapnp_match, gus_isapnp_attach, NULL, NULL);

extern struct cfdriver guspnp_cd;

/*
 * Probe / attach routines.
 */

/*
 * Probe for the guspnp hardware.
 *
 * The thing has 5 separate devices on the card
 */

static int gus_0 = 1;		/* XXX what's this */

int
gus_isapnp_match(device_t parent, cfdata_t match, void *aux)
{
	int pri, variant;

	pri = isapnp_devmatch(aux, &isapnp_gus_devinfo, &variant);
	if (pri && variant > 0)
		pri = 0;
	return pri;
}

/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver.
 */
void
gus_isapnp_attach(device_t parent, device_t self, void *aux)
{
	struct iw_softc *sc;
	struct isapnp_attach_args *ipa;

	sc = device_private(self);
	ipa = aux;
	printf("\n");

	if (!gus_0)
		return;
	gus_0 = 0;

	if (isapnp_config(ipa->ipa_iot, ipa->ipa_memt, ipa)) {
		aprint_error_dev(self, "error in region allocation\n");
		return;
	}

	sc->sc_dev = self;
	sc->sc_iot = ipa->ipa_iot;

	/* handle is the region base */

	sc->dir_h = 0; /* XXXXX */
	sc->p2xr = 0;
	sc->p2xr_h = ipa->ipa_io[0].h;
	sc->sc_p2xr_ic = ipa->ipa_ic;
	sc->p3xr = 0;
	sc->p3xr_h = ipa->ipa_io[1].h;
	sc->sc_p3xr_ic = ipa->ipa_ic;
	sc->codec_index = 0;
	sc->codec_index_h = ipa->ipa_io[2].h;
	sc->sc_irq = ipa->ipa_irq[0].num;
	sc->sc_recdrq = ipa->ipa_drq[0].num;
	sc->sc_playdrq = ipa->ipa_drq[1].num;

	sc->sc_ic = ipa->ipa_ic;

	/*
         * Create our DMA maps.
         */
	if (sc->sc_playdrq != -1) {
		sc->sc_play_maxsize = isa_dmamaxsize(sc->sc_ic,
		    sc->sc_playdrq);
		if (isa_drq_alloc(sc->sc_ic, sc->sc_playdrq) != 0) {
			aprint_error_dev(self, "can't reserve drq %d\n",
			    sc->sc_playdrq);
			return;
		}
		if (isa_dmamap_create(sc->sc_ic, sc->sc_playdrq,
		    sc->sc_play_maxsize, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW)) {
			aprint_error_dev(self, "can't create map for drq %d\n",
			    sc->sc_playdrq);
			return;
		}
	}
	if (sc->sc_recdrq != -1) {
		sc->sc_rec_maxsize = isa_dmamaxsize(sc->sc_ic,
		    sc->sc_recdrq);
		if (isa_drq_alloc(sc->sc_ic, sc->sc_recdrq) != 0) {
			aprint_error_dev(self, "can't reserve drq %d\n",
			    sc->sc_recdrq);
			return;
		}
		if (isa_dmamap_create(sc->sc_ic, sc->sc_recdrq,
		    sc->sc_rec_maxsize, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW)) {
			aprint_error_dev(self, "can't create map for drq %d\n",
			    sc->sc_recdrq);
			return;
		}
	}

	/*
         * isapnp is a child if isa, and we need isa for the DMA
         * routines.
         */
	sc->iw_cd = &guspnp_cd;
	sc->iw_hw_if = &guspnp_hw_if;

	printf("%s: %s %s", device_xname(self), ipa->ipa_devident,
	       ipa->ipa_devclass);

	iwattach(sc);
}

static int
gus_isapnp_open(void *addr, int flags)
{
	/* open hardware */
	struct iw_softc *sc;

	sc = (struct iw_softc *)addr;
	if (!sc)
		return ENXIO;

	return iwopen(sc,flags);
}

#endif /* NGUSPNP */
