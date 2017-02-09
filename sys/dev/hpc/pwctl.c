/*	$NetBSD: pwctl.c,v 1.20 2012/10/27 17:18:17 chs Exp $	*/

/*-
 * Copyright (c) 1999-2001
 *         TAKEMURA Shin and PocketBSD Project. All rights reserved.
 * Copyright (c) 2000,2001
 *         SATO Kazumi. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the PocketBSD project
 *	and its contributors.
 * 4. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pwctl.c,v 1.20 2012/10/27 17:18:17 chs Exp $");

#ifdef _KERNEL_OPT
#include "opt_pwctl.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <sys/bus.h>
#include <machine/config_hook.h>
#include <machine/platid.h>
#include <machine/platid_mask.h>

#include <dev/hpc/hpciovar.h>

#include "locators.h"

#ifdef PWCTLDEBUG
#ifndef PWCTLDEBUG_CONF
#define PWCTLDEBUG_CONF	0
#endif
int	pwctl_debug = PWCTLDEBUG_CONF;
#define	DPRINTF(arg) if (pwctl_debug) printf arg;
#define	VPRINTF(arg) if (bootverbose) printf arg;
#else
#define	DPRINTF(arg)
#define	VPRINTF(arg) if (bootverbose) printf arg;
#endif

struct pwctl_softc {
	hpcio_chip_t sc_hc;
	int sc_port;
	long sc_id;
	int sc_on, sc_off;
	config_hook_tag sc_hook_tag;
	config_hook_tag sc_hook_hardpower;
	config_hook_tag sc_ghook_tag;
	int sc_save;
	int sc_initvalue;
};

static int	pwctl_match(device_t, cfdata_t, void *);
static void	pwctl_attach(device_t, device_t, void *);
static int	pwctl_hook(void *, int, long, void *);
static int	pwctl_ghook(void *, int, long, void *);
int	pwctl_hardpower(void *, int, long, void *);

CFATTACH_DECL_NEW(pwctl, sizeof(struct pwctl_softc),
    pwctl_match, pwctl_attach, NULL, NULL);

int
pwctl_match(device_t parent, cfdata_t match, void *aux)
{
	struct hpcio_attach_args *haa = aux;
	platid_mask_t mask;

	if (strcmp(haa->haa_busname, HPCIO_BUSNAME))
		return (0);
	if (match->cf_loc[HPCIOIFCF_PLATFORM] == 0)
		return (0);
	mask = PLATID_DEREF(match->cf_loc[HPCIOIFCF_PLATFORM]);
	if (!platid_match(&platid, &mask))
		return (0);
	return (1);
}

void
pwctl_attach(device_t parent, device_t self, void *aux)
{
	struct hpcio_attach_args *haa = aux;
	int *loc;
	struct pwctl_softc *sc = device_private(self);

	loc = device_cfdata(self)->cf_loc;
	sc->sc_hc = (*haa->haa_getchip)(haa->haa_sc, loc[HPCIOIFCF_IOCHIP]);
	sc->sc_port = loc[HPCIOIFCF_PORT];
	sc->sc_id = loc[HPCIOIFCF_ID];
	sc->sc_on = loc[HPCIOIFCF_ACTIVE] ? 1 : 0;
	sc->sc_off = loc[HPCIOIFCF_ACTIVE] ? 0 : 1;
	sc->sc_initvalue = loc[HPCIOIFCF_INITVALUE];

	printf(" port=%d id=%ld on=%d%s",
	    sc->sc_port, sc->sc_id, sc->sc_on,
	    sc->sc_initvalue == -1 ? "" :
	    sc->sc_initvalue ? " init=on" : " init=off");

	if (sc->sc_port == HPCIOIFCF_PORT_DEFAULT ||
	    sc->sc_id == HPCIOIFCF_ID_DEFAULT) {
		printf(" (ignored)");
	} else {
		sc->sc_hook_tag = config_hook(CONFIG_HOOK_POWERCONTROL,
		    sc->sc_id, CONFIG_HOOK_SHARE, pwctl_hook, sc);
		sc->sc_ghook_tag = config_hook(CONFIG_HOOK_GET,
		    sc->sc_id, CONFIG_HOOK_SHARE, pwctl_ghook, sc);
		sc->sc_hook_hardpower = config_hook(CONFIG_HOOK_PMEVENT,
		    CONFIG_HOOK_PMEVENT_HARDPOWER, CONFIG_HOOK_SHARE,
		    pwctl_hardpower, sc);
	}

	if (sc->sc_initvalue != -1)
		hpcio_portwrite(sc->sc_hc, sc->sc_port,
		    sc->sc_initvalue ? sc->sc_on : sc->sc_off);
	printf("\n");
}

int
pwctl_hook(void *ctx, int type, long id, void *msg)
{
	struct pwctl_softc *sc = ctx;

	DPRINTF(("pwctl hook: port %d %s(%d)", sc->sc_port,
	    msg ? "ON" : "OFF", msg ? sc->sc_on : sc->sc_off));
	hpcio_portwrite(sc->sc_hc, sc->sc_port,
	    msg ? sc->sc_on : sc->sc_off);

	return (0);
}

int
pwctl_ghook(void *ctx, int type, long id, void *msg)
{
	struct pwctl_softc *sc = ctx;

	if (CONFIG_HOOK_VALUEP(msg))
		return (1);

	*(int*)msg = hpcio_portread(sc->sc_hc, sc->sc_port) == sc->sc_on;
	DPRINTF(("pwctl ghook: port %d %s(%d)", sc->sc_port,
	    *(int*)msg? "ON" : "OFF", *(int*)msg ? sc->sc_on : sc->sc_off));

	return (0);
}

int
pwctl_hardpower(void *ctx, int type, long id, void *msg)
{
	struct pwctl_softc *sc = ctx;
	int why =(int)msg;

	VPRINTF(("pwctl hardpower: port %d %s: %s(%d)\n", sc->sc_port,
	    why == PWR_RESUME? "resume"
	    : why == PWR_SUSPEND? "suspend" : "standby",
	    sc->sc_save == sc->sc_on ? "on": "off", sc->sc_save));

	switch (why) {
	case PWR_STANDBY:
		break;
	case PWR_SUSPEND:
		sc->sc_save = hpcio_portread(sc->sc_hc, sc->sc_port);
		hpcio_portwrite(sc->sc_hc, sc->sc_port, sc->sc_off);
		break;
	case PWR_RESUME:
		hpcio_portwrite(sc->sc_hc, sc->sc_port, sc->sc_save);
		break;
	}

	return (0);
}
