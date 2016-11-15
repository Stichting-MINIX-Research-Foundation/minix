/*	$NetBSD: if_agrtimer.c,v 1.6 2010/02/08 17:59:06 dyoung Exp $	*/

/*-
 * Copyright (c)2005 YAMAMOTO Takashi,
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_agrtimer.c,v 1.6 2010/02/08 17:59:06 dyoung Exp $");

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <net/if.h>

#include <net/agr/if_agrvar_impl.h>
#include <net/agr/if_agrsubr.h>

static void agrtimer_tick(void *);
static int agrtimer_port_tick(struct agr_port *, void *);

void
agrtimer_init(struct agr_softc *sc)
{

	callout_init(&sc->sc_callout, 0);
	callout_setfunc(&sc->sc_callout, agrtimer_tick, sc);
}

void
agrtimer_destroy(struct agr_softc *sc)
{

	callout_destroy(&sc->sc_callout);
}

void
agrtimer_start(struct agr_softc *sc)
{

	callout_schedule(&sc->sc_callout, 0);
}

void
agrtimer_stop(struct agr_softc *sc)
{

	callout_stop(&sc->sc_callout);
}

static void
agrtimer_tick(void *arg)
{
	struct agr_softc *sc = arg;
	const struct agr_iftype_ops *iftop = sc->sc_iftop;

	KASSERT(iftop);

	AGR_LOCK(sc);
	if (iftop->iftop_tick) {
		(*iftop->iftop_tick)(sc);
	}
	if (iftop->iftop_porttick) {
		agr_port_foreach(sc, agrtimer_port_tick, sc);
	}
	callout_schedule(&sc->sc_callout, hz);
	AGR_UNLOCK(sc);
}

static int
agrtimer_port_tick(struct agr_port *port, void *arg)
{
	struct agr_softc *sc = arg;

	agrport_monitor(port);
	(*sc->sc_iftop->iftop_porttick)(sc, port);

	return 0;
}
