/*	$NetBSD: dma_sbus.c,v 1.35 2009/09/17 16:28:12 tsutsui Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 * Copyright (c) 1994 Peter Galbavy.  All rights reserved.
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
 *	This product includes software developed by Peter Galbavy.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dma_sbus.c,v 1.35 2009/09/17 16:28:12 tsutsui Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <sys/bus.h>
#include <sys/intr.h>
#include <machine/autoconf.h>

#include <dev/sbus/sbusvar.h>

#include <dev/ic/lsi64854reg.h>
#include <dev/ic/lsi64854var.h>

struct dma_softc {
	struct lsi64854_softc	sc_lsi64854;	/* base device */
	/* possible sbus specific stuff here */
};

int	dmamatch_sbus(device_t, cfdata_t, void *);
void	dmaattach_sbus(device_t, device_t, void *);

int	dmaprint_sbus(void *, const char *);

void	*dmabus_intr_establish(
		bus_space_tag_t,
		int,			/*bus interrupt priority*/
		int,			/*`device class' level*/
		int (*)(void *),	/*handler*/
		void *,			/*handler arg*/
		void (*) (void));	/*optional fast trap handler*/

CFATTACH_DECL_NEW(dma_sbus, sizeof(struct dma_softc),
    dmamatch_sbus, dmaattach_sbus, NULL, NULL);

CFATTACH_DECL_NEW(ledma, sizeof(struct dma_softc),
    dmamatch_sbus, dmaattach_sbus, NULL, NULL);

int
dmaprint_sbus(void *aux, const char *busname)
{
	struct sbus_attach_args *sa = aux;
	bus_space_tag_t t = sa->sa_bustag;
	struct dma_softc *sc = t->cookie;

	sa->sa_bustag = sc->sc_lsi64854.sc_bustag;	/* XXX */
	sbus_print(aux, busname);	/* XXX */
	sa->sa_bustag = t;		/* XXX */
	return UNCONF;
}

int
dmamatch_sbus(device_t parent, cfdata_t cf, void *aux)
{
	struct sbus_attach_args *sa = aux;

	return strcmp(cf->cf_name, sa->sa_name) == 0 ||
	    strcmp("espdma", sa->sa_name) == 0;
}

void
dmaattach_sbus(device_t parent, device_t self, void *aux)
{
	struct dma_softc *dsc = device_private(self);
	struct lsi64854_softc *sc = &dsc->sc_lsi64854;
	struct sbus_attach_args *sa = aux;
	struct sbus_softc *sbsc = device_private(parent);
	bus_space_tag_t sbt;
	int sbusburst, burst;
	int node;

	node = sa->sa_node;

	sc->sc_dev = self;
	sc->sc_bustag = sa->sa_bustag;
	sc->sc_dmatag = sa->sa_dmatag;

	/* Map registers */
	if (sa->sa_npromvaddrs) {
		sbus_promaddr_to_handle(sa->sa_bustag,
		    sa->sa_promvaddrs[0], &sc->sc_regs);
	} else {
		if (sbus_bus_map(sa->sa_bustag,
		    sa->sa_slot, sa->sa_offset, sa->sa_size,
		    0, &sc->sc_regs) != 0) {
			aprint_error(": cannot map registers\n");
			return;
		}
	}

	/*
	 * Get transfer burst size from PROM and plug it into the
	 * controller registers. This is needed on the Sun4m; do
	 * others need it too?
	 */
	sbusburst = sbsc->sc_burst;
	if (sbusburst == 0)
		sbusburst = SBUS_BURST_32 - 1; /* 1->16 */

	burst = prom_getpropint(node,"burst-sizes", -1);
	if (burst == -1)
		/* take SBus burst sizes */
		burst = sbusburst;

	/* Clamp at parent's burst sizes */
	burst &= sbusburst;
	sc->sc_burst = (burst & SBUS_BURST_32) ? 32 :
		       (burst & SBUS_BURST_16) ? 16 : 0;

	if (device_is_a(self, "ledma")) {
		char *cabletype;
		uint32_t csr;
		/*
		 * Check to see which cable type is currently active and
		 * set the appropriate bit in the ledma csr so that it
		 * gets used. If we didn't netboot, the PROM won't have
		 * the "cable-selection" property; default to TP and then
		 * the user can change it via a "media" option to ifconfig.
		 */
		cabletype = prom_getpropstring(node, "cable-selection");
		csr = L64854_GCSR(sc);
		if (strcmp(cabletype, "tpe") == 0) {
			csr |= E_TP_AUI;
		} else if (strcmp(cabletype, "aui") == 0) {
			csr &= ~E_TP_AUI;
		} else {
			/* assume TP if nothing there */
			csr |= E_TP_AUI;
		}
		L64854_SCSR(sc, csr);
		delay(20000);	/* manual says we need a 20ms delay */
		sc->sc_channel = L64854_CHANNEL_ENET;
	} else {
		sc->sc_channel = L64854_CHANNEL_SCSI;
	}

	if ((sbt = bus_space_tag_alloc(sc->sc_bustag, dsc)) == NULL) {
		aprint_error(": out of memory\n");
		return;
	}
	sbt->sparc_intr_establish = dmabus_intr_establish;
	lsi64854_attach(sc);

	/* Attach children */
	for (node = firstchild(sa->sa_node); node; node = nextsibling(node)) {
		struct sbus_attach_args sax;
		sbus_setup_attach_args(sbsc, sbt, sc->sc_dmatag, node, &sax);
		(void)config_found(self, (void *)&sax, dmaprint_sbus);
		sbus_destroy_attach_args(&sax);
	}
}

void *
dmabus_intr_establish(bus_space_tag_t t, int pri, int level,
    int (*handler)(void *), void *arg, void (*fastvec)(void))
{
	struct lsi64854_softc *sc = t->cookie;

	/* XXX - for now only le; do esp later */
	if (sc->sc_channel == L64854_CHANNEL_ENET) {
		sc->sc_intrchain = handler;
		sc->sc_intrchainarg = arg;
		handler = lsi64854_enet_intr;
		arg = sc;
	}
	return bus_intr_establish(sc->sc_bustag, pri, level, handler, arg);
}
