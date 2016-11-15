/*	$NetBSD: opl_ym.c,v 1.18 2012/04/09 10:18:17 plunky Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Lennart Augustsson
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
__KERNEL_RCSID(0, "$NetBSD: opl_ym.c,v 1.18 2012/04/09 10:18:17 plunky Exp $");

#include "mpu_ym.h"

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
#include <dev/isa/ad1848var.h>
#include <dev/ic/opl3sa3reg.h>
#include <dev/isa/ymvar.h>

int	opl_ym_match(device_t, cfdata_t, void *);
void	opl_ym_attach(device_t, device_t, void *);
#ifndef AUDIO_NO_POWER_CTL
int	opl_ym_power_ctl(void *, int);
#endif

CFATTACH_DECL_NEW(opl_ym, sizeof(struct opl_softc),
    opl_ym_match, opl_ym_attach, NULL, NULL);

int
opl_ym_match(device_t parent, cfdata_t match, void *aux)
{
	struct audio_attach_args *aa = (struct audio_attach_args *)aux;
	struct ym_softc *ssc = device_private(parent);

	if (aa->type != AUDIODEV_TYPE_OPL || ssc->sc_opl_ioh == 0)
		return (0);
	return opl_match(ssc->sc_iot, ssc->sc_opl_ioh, 0);
}

void
opl_ym_attach(device_t parent, device_t self, void *aux)
{
	struct ym_softc *ssc = device_private(parent);
	struct opl_softc *sc = device_private(self);

	sc->dev = self;
	sc->ioh = ssc->sc_opl_ioh;
	sc->iot = ssc->sc_iot;
	sc->offs = 0;
#ifndef AUDIO_NO_POWER_CTL
	sc->powerctl = opl_ym_power_ctl;
	sc->powerarg = ssc;
#endif
	sc->lock = &ssc->sc_ad1848.sc_ad1848.sc_intr_lock;
	snprintf(sc->syn.name, sizeof(sc->syn.name), "%s ",
	    ssc->sc_ad1848.sc_ad1848.chip_name);

	opl_attach(sc);
}

#ifndef AUDIO_NO_POWER_CTL
int
opl_ym_power_ctl(void *arg, int onoff)
{
	struct ym_softc *ssc = arg;

	ym_power_ctl(ssc, YM_POWER_OPL3 | YM_POWER_OPL3_DA, onoff);
	return 0;
}
#endif
