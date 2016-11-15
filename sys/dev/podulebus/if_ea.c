/* $NetBSD: if_ea.c,v 1.17 2012/10/10 22:17:44 skrll Exp $ */

/*
 * Copyright (c) 2000, 2001 Ben Harris
 * Copyright (c) 1995 Mark Brinicombe
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * if_ea.c - Ether3 device driver
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ea.c,v 1.17 2012/10/10 22:17:44 skrll Exp $");

#include <sys/param.h>

#include <sys/device.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <net/if.h>
#include <net/if_ether.h>

#include <dev/podulebus/podulebus.h>
#include <dev/podulebus/podules.h>

#include <dev/podulebus/if_eareg.h>
#include <dev/ic/seeq8005var.h>

/*
 * per-line info and status
 */

struct ea_softc {
	struct seeq8005_softc	sc_8005;
	void	*sc_ih;
	struct evcnt sc_intrcnt;
};

/*
 * prototypes
 */

int eaprobe(device_t, cfdata_t, void *);
void eaattach(device_t, device_t, void *);

/* driver structure for autoconf */

CFATTACH_DECL_NEW(ea, sizeof(struct ea_softc),
    eaprobe, eaattach, NULL, NULL);

/*
 * Probe routine.
 */

/*
 * Probe for the ether3 podule.
 */

int
eaprobe(device_t parent, cfdata_t cf, void *aux)
{
	struct podulebus_attach_args *pa = aux;

	return pa->pa_product == PODULE_ETHER3;
}


/*
 * Attach podule.
 */

void
eaattach(device_t parent, device_t self, void *aux)
{
	struct ea_softc *sc = device_private(self);
	struct podulebus_attach_args *pa = aux;
	u_int8_t myaddr[ETHER_ADDR_LEN];
	char *ptr;
	int i;

	sc->sc_8005.sc_dev = self;

/*	dprintf(("Attaching %s...\n", device_xname(self)));*/

	/* Set the address of the controller for easy access */
	podulebus_shift_tag(pa->pa_mod_t, EA_8005_SHIFT, &sc->sc_8005.sc_iot);
	bus_space_map(sc->sc_8005.sc_iot, pa->pa_mod_base, /* XXX */ 0, 0,
	    &sc->sc_8005.sc_ioh);

	/* Get the Ethernet address from the device description string. */
	if (pa->pa_descr == NULL) {
		printf(": No description for Ethernet address\n");
		return;
	}
	ptr = strchr(pa->pa_descr, '(');
	if (ptr == NULL) {
		printf(": Ethernet address not found in description\n");
		return;
	}
	ptr++;
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		myaddr[i] = strtoul(ptr, &ptr, 16);
		if (*ptr++ != (i == ETHER_ADDR_LEN - 1 ? ')' : ':')) {
			printf(": Bad Ethernet address found in "
			       "description\n");
			return;
		}
	}

	printf(":");
	seeq8005_attach(&sc->sc_8005, myaddr, NULL, 0, 0);

	/* Claim a podule interrupt */

	evcnt_attach_dynamic(&sc->sc_intrcnt, EVCNT_TYPE_INTR, NULL,
	    device_xname(self), "intr");
	sc->sc_ih = podulebus_irq_establish(pa->pa_ih, IPL_NET, seeq8005intr,
	    sc, &sc->sc_intrcnt);
}

/* End of if_ea.c */
