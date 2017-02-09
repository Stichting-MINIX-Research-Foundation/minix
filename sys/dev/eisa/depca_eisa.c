/*	$NetBSD: depca_eisa.c,v 1.15 2014/03/29 19:28:24 christos Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

/*
 * EISA bus front-end for the Digital DEPCA Ethernet controller.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: depca_eisa.c,v 1.15 2014/03/29 19:28:24 christos Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/eisa/eisareg.h>
#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>

#include <dev/ic/lancereg.h>
#include <dev/ic/lancevar.h>
#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>
#include <dev/ic/depcareg.h>
#include <dev/ic/depcavar.h>

static int	depca_eisa_match(device_t, cfdata_t, void *);
static void	depca_eisa_attach(device_t, device_t, void *);

struct depca_eisa_softc {
	struct depca_softc sc_depca;

	eisa_chipset_tag_t sc_ec;
	int sc_irq;
	int sc_ist;
};

CFATTACH_DECL_NEW(depca_eisa, sizeof(struct depca_eisa_softc),
    depca_eisa_match, depca_eisa_attach, NULL, NULL);

static void	*depca_eisa_intr_establish(struct depca_softc *,
					   struct lance_softc *);

static int
depca_eisa_match(device_t parent, cfdata_t cf, void *aux)
{
	struct eisa_attach_args *ea = aux;

	return (strcmp(ea->ea_idstring, "DEC4220") == 0);
}

#define	DEPCA_ECU_FUNC_NETINTR	0
#define	DEPCA_ECU_FUNC_NETBUF	1

static void
depca_eisa_attach(device_t parent, device_t self, void *aux)
{
	struct depca_eisa_softc *esc = device_private(self);
	struct depca_softc *sc = &esc->sc_depca;
	struct eisa_attach_args *ea = aux;
	struct eisa_cfg_mem ecm;
	struct eisa_cfg_irq eci;

	sc->sc_dev = self;
	aprint_error(": DEC DE422 Ethernet\n");

	sc->sc_iot = ea->ea_iot;
	sc->sc_memt = ea->ea_memt;

	esc->sc_ec = ea->ea_ec;

	sc->sc_intr_establish = depca_eisa_intr_establish;

	if (eisa_conf_read_mem(ea->ea_ec, ea->ea_slot,
	    DEPCA_ECU_FUNC_NETBUF, 0, &ecm) != 0) {
		aprint_error_dev(self, "unable to find network buffer\n");
		return;
	}

	aprint_normal_dev(self, "shared memory at 0x%lx-0x%lx\n",
	    ecm.ecm_addr, ecm.ecm_addr + ecm.ecm_size - 1);

	sc->sc_memsize = ecm.ecm_size;

	if (bus_space_map(sc->sc_iot, EISA_SLOT_ADDR(ea->ea_slot) + 0xc00, 16,
	    0, &sc->sc_ioh) != 0) {
		aprint_error_dev(self, "unable to map i/o space\n");
		return;
	}
	if (bus_space_map(sc->sc_memt, ecm.ecm_addr, sc->sc_memsize,
	    0, &sc->sc_memh) != 0) {
		aprint_error_dev(self, "unable to map memory space\n");
		return;
	}

	if (eisa_conf_read_irq(ea->ea_ec, ea->ea_slot,
	    DEPCA_ECU_FUNC_NETINTR, 0, &eci) != 0) {
		aprint_error_dev(self, "unable to determine IRQ\n");
		return;
	}

	esc->sc_irq = eci.eci_irq;
	esc->sc_ist = eci.eci_ist;

	depca_attach(sc);
}

static void *
depca_eisa_intr_establish(struct depca_softc *sc, struct lance_softc *child)
{
	struct depca_eisa_softc *esc = (struct depca_eisa_softc *)sc;
	eisa_intr_handle_t ih;
	const char *intrstr;
	void *rv;
	char intrbuf[EISA_INTRSTR_LEN];

	if (eisa_intr_map(esc->sc_ec, esc->sc_irq, &ih)) {
		aprint_error_dev(sc->sc_dev,
		    "unable to map interrupt (%d)\n", esc->sc_irq);
		return (NULL);
	}
	intrstr = eisa_intr_string(esc->sc_ec, ih, intrbuf, sizeof(intrbuf));
	rv = eisa_intr_establish(esc->sc_ec, ih, esc->sc_ist, IPL_NET,
	    (esc->sc_ist == IST_LEVEL) ? am7990_intr : depca_intredge, child);
	if (rv == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "unable to establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return (NULL);
	}
	if (intrstr != NULL)
		aprint_normal_dev(sc->sc_dev, "interrupting at %s\n",
		    intrstr);

	return (rv);
}
