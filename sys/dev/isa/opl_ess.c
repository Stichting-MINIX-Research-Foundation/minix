/*	$NetBSD: opl_ess.c,v 1.18 2012/04/09 10:18:17 plunky Exp $	*/

/*-
 * Copyright (c) 1999, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson.
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
__KERNEL_RCSID(0, "$NetBSD: opl_ess.c,v 1.18 2012/04/09 10:18:17 plunky Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/select.h>
#include <sys/audioio.h>
#include <sys/midiio.h>

#include <sys/bus.h>

#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/ic/oplreg.h>
#include <dev/ic/oplvar.h>

#include <dev/isa/isavar.h>
#include <dev/isa/essvar.h>

extern int	ess_speaker_ctl(void *, int);

int	opl_ess_match(device_t, cfdata_t, void *);
void	opl_ess_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(opl_ess, sizeof(struct opl_softc),
    opl_ess_match, opl_ess_attach, NULL, NULL);

int
opl_ess_match(device_t parent, cfdata_t match, void *aux)
{
	struct audio_attach_args *aa = (struct audio_attach_args *)aux;
	struct ess_softc *ssc = device_private(parent);

	if (aa->type != AUDIODEV_TYPE_OPL)
		return (0);
	return opl_match(ssc->sc_iot, ssc->sc_ioh, 0);
}

void
opl_ess_attach(device_t parent, device_t self, void *aux)
{
	struct ess_softc *ssc = device_private(parent);
	struct opl_softc *sc = device_private(self);

	sc->dev = self;
	sc->ioh = ssc->sc_ioh;
	sc->iot = ssc->sc_iot;
	sc->offs = 0;
	sc->spkrctl = ess_speaker_ctl;
	sc->spkrarg = ssc;
	sc->lock = &ssc->sc_intr_lock;
	strcpy(sc->syn.name, "ESS ");

	opl_attach(sc);
}
