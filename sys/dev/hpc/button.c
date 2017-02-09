/*	$NetBSD: button.c,v 1.16 2012/10/27 17:18:17 chs Exp $	*/

/*-
 * Copyright (c) 1999-2001
 *         TAKEMURA Shin1 and PocketBSD Project. All rights reserved.
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
__KERNEL_RCSID(0, "$NetBSD: button.c,v 1.16 2012/10/27 17:18:17 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/bus.h>

#include <machine/config_hook.h>
#include <machine/platid.h>
#include <machine/platid_mask.h>

#include <dev/hpc/hpciovar.h>

#include "locators.h"

struct button_softc {
	hpcio_chip_t sc_hc;
	hpcio_intr_handle_t sc_intr_handle;
	int sc_port;
	long sc_id;
	int sc_active;
	config_hook_tag sc_hook_tag;
	config_hook_tag sc_ghook_tag;
};

static int	button_match(device_t, cfdata_t, void *);
static void	button_attach(device_t, device_t, void *);
static int	button_intr(void *);
static int	button_state(void *, int, long, void *);

CFATTACH_DECL_NEW(button, sizeof(struct button_softc),
    button_match, button_attach, NULL, NULL);

int
button_match(device_t parent, cfdata_t match, void *aux)
{
	struct hpcio_attach_args *haa = aux;
	platid_mask_t mask;

	if (strcmp(haa->haa_busname, HPCIO_BUSNAME))
		return (0);
	if (match->cf_loc[HPCIOIFCF_PLATFORM] == 0)
		return (0);
	mask = PLATID_DEREF(match->cf_loc[HPCIOIFCF_PLATFORM]);
	return (platid_match(&platid, &mask));
}

void
button_attach(device_t parent, device_t self, void *aux)
{
	struct hpcio_attach_args *haa = aux;
	int *loc;
	struct button_softc *sc = device_private(self);
	int mode;

	loc = device_cfdata(self)->cf_loc;
	sc->sc_hc = (*haa->haa_getchip)(haa->haa_sc, loc[HPCIOIFCF_IOCHIP]);
	sc->sc_port = loc[HPCIOIFCF_PORT];
	sc->sc_id = loc[HPCIOIFCF_ID];
	sc->sc_active = loc[HPCIOIFCF_ACTIVE];
	printf(" port=%d id=%ld active=%s",
	    sc->sc_port, sc->sc_id, sc->sc_active ? "high" : "low");

	mode = HPCIO_INTR_HOLD;
	if (loc[HPCIOIFCF_LEVEL] != HPCIOIFCF_LEVEL_DEFAULT) {
		mode |= HPCIO_INTR_LEVEL;
		if (loc[HPCIOIFCF_LEVEL] == 0)
			mode |= HPCIO_INTR_LOW;
		else
			mode |= HPCIO_INTR_HIGH;
		printf(" sense=level");
	} else {
		mode |= HPCIO_INTR_EDGE;
		switch (loc[HPCIOIFCF_EDGE]) {
		case 1:
			mode |= HPCIO_INTR_POSEDGE;
			break;
		case 2:
			mode |= HPCIO_INTR_NEGEDGE;
			break;
		case 0:
		case 3:
		case HPCIOMANCF_EDGE_DEFAULT:
			mode |= HPCIO_INTR_POSEDGE;
			mode |= HPCIO_INTR_NEGEDGE;
			break;
		default:
			printf("%s(%d): invalid configuration, edge=%d",
			    __FILE__, __LINE__, loc[HPCIOIFCF_EDGE]);
			break;
		}
		printf(" sense=edge");
	}

	if (sc->sc_port == HPCIOIFCF_PORT_DEFAULT ||
	    sc->sc_id == HPCIOIFCF_ID_DEFAULT)
		printf(" (ignored)");
	else
		sc->sc_intr_handle =
		    hpcio_intr_establish(sc->sc_hc, sc->sc_port,
			mode, button_intr, sc);
	sc->sc_ghook_tag = config_hook(CONFIG_HOOK_GET, sc->sc_id,
	    CONFIG_HOOK_SHARE, button_state, sc);
	printf("\n");
}

int
button_state(void *ctx, int type, long id, void *msg)
{
	struct button_softc *sc = ctx;

	if (type != CONFIG_HOOK_GET || id != sc->sc_id)
		return (1);

	if (CONFIG_HOOK_VALUEP(msg))
		return (1);

	*(int*)msg = (hpcio_portread(sc->sc_hc, sc->sc_port) == sc->sc_active);
	return (0);
}

int
button_intr(void *ctx)
{
	struct button_softc *sc = ctx;
	int on;

	on = (hpcio_portread(sc->sc_hc, sc->sc_port) == sc->sc_active);

	/* Clear interrupt */
	hpcio_intr_clear(sc->sc_hc, sc->sc_intr_handle);

	config_hook_call(CONFIG_HOOK_BUTTONEVENT, sc->sc_id, (void*)on);

	return (0);
}
