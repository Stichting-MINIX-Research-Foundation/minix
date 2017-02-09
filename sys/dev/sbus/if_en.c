/*	$NetBSD: if_en.c,v 1.29 2011/07/18 00:58:52 mrg Exp $	*/

/*
 * Copyright (c) 1996 Charles D. Cranor and Washington University.
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *
 * i f _ e n _ s b u s . c
 *
 * author: Chuck Cranor <chuck@ccrc.wustl.edu>
 * started: spring, 1996.
 *
 * SBUS glue for the eni155s card.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_en.c,v 1.29 2011/07/18 00:58:52 mrg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>

#include <sys/bus.h>
#include <sys/intr.h>
#include <sys/cpu.h>

#include <dev/sbus/sbusvar.h>

#include <dev/ic/midwayreg.h>
#include <dev/ic/midwayvar.h>


/*
 * prototypes
 */
static	int en_sbus_match(device_t, cfdata_t, void *);
static	void en_sbus_attach(device_t, device_t, void *);

/*
 * SBus autoconfig attachments
 */

CFATTACH_DECL_NEW(en_sbus, sizeof(struct en_softc),
    en_sbus_match, en_sbus_attach, NULL, NULL);

/***********************************************************************/

/*
 * autoconfig stuff
 */

static int
en_sbus_match(device_t parent, cfdata_t cf, void *aux)
{
	struct sbus_attach_args *sa = aux;

	if (strcmp("ENI-155s", sa->sa_name) == 0)  {
		if (CPU_ISSUN4M) {
#ifdef DEBUG
			printf("%s: sun4m DMA not supported yet\n",
			    sa->sa_name);
#endif
			return (0);
		}
		return (1);
	} else {
		return (0);
	}
}


static void
en_sbus_attach(device_t parent, device_t self, void *aux)
{
	struct sbus_attach_args *sa = aux;
	struct en_softc *sc = device_private(self);

	sc->sc_dev = self;

	printf("\n");

	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 sa->sa_offset,
			 4*1024*1024,
			 0, &sc->en_base) != 0) {
		aprint_error_dev(self, "cannot map registers\n");
		return;
	}

	/* Establish interrupt handler */
	if (sa->sa_nintr != 0)
		(void)bus_intr_establish(sa->sa_bustag, sa->sa_pri,
					 IPL_NET, en_intr, sc);

	sc->ipl = sa->sa_pri;	/* appropriate? */

	/*
	 * done SBUS specific stuff
	 */
	en_attach(sc);
}
