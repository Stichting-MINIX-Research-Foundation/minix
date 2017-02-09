/*	$NetBSD: hilid.c,v 1.2 2011/02/15 11:05:51 tsutsui Exp $	*/
/*	$OpenBSD: hilid.c,v 1.4 2005/01/09 23:49:36 miod Exp $	*/
/*
 * Copyright (c) 2003, Miodrag Vallat.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/bus.h>
#include <sys/cpu.h>

#include <machine/autoconf.h>

#include <dev/hil/hilreg.h>
#include <dev/hil/hilvar.h>
#include <dev/hil/hildevs.h>

struct hilid_softc {
	struct hildev_softc sc_hildev;

	uint8_t sc_id[16];
};

static int	hilidprobe(device_t, cfdata_t, void *);
static void	hilidattach(device_t, device_t, void *);
static int	hiliddetach(device_t, int);

CFATTACH_DECL_NEW(hilid, sizeof(struct hilid_softc),
    hilidprobe, hilidattach, hiliddetach, NULL);

int
hilidprobe(device_t parent, cfdata_t cf, void *aux)
{
	struct hil_attach_args *ha = aux;

	if (ha->ha_type != HIL_DEVICE_IDMODULE)
		return 0;

	return 1;
}

void
hilidattach(device_t parent, device_t self, void *aux)
{
	struct hilid_softc *sc = device_private(self);
	struct hildev_softc *hdsc = &sc->sc_hildev;
	struct hil_attach_args *ha = aux;
	u_int i, len;

	sc->sc_hildev.sc_dev = self;
	sc->hd_code = ha->ha_code;
	sc->hd_type = ha->ha_type;
	sc->hd_infolen = ha->ha_infolen;
	memcpy(sc->hd_info, ha->ha_info, ha->ha_infolen);
	sc->hd_fn = NULL;

	aprint_normal("\n");

	memset(sc->sc_id, 0, sizeof(sc->sc_id));
	len = sizeof(sc->sc_id);
	aprint_normal("%s: security code", device_xname(self));

	if (send_hildev_cmd(hdsc,
	    HIL_SECURITY, sc->sc_id, &len) == 0) {
		for (i = 0; i < sizeof(sc->sc_id); i++)
			printf(" %02x", sc->sc_id[i]);
		aprint_normal("\n");
	} else
		aprint_normal(" unavailable\n");
}

int
hiliddetach(device_t self, int flags)
{

	return 0;
}
