/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Husemann <martin@NetBSD.org>.
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
__KERNEL_RCSID(0, "$NetBSD: daic_isa.c,v 1.20 2012/10/27 17:18:24 chs Exp $");

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/device.h>

#if defined(__NetBSD__) && __NetBSD_Version__ >= 104230000
#include <sys/callout.h>
#endif

#include <sys/cpu.h>
#include <sys/intr.h>
#include <sys/bus.h>

#include <sys/socket.h>
#include <net/if.h>
#include <dev/isa/isavar.h>
#include <netisdn/i4b_ioctl.h>
#include <netisdn/i4b_l3l4.h>
#include <dev/ic/daicvar.h>

/* driver state */
struct daic_isa_softc {
	struct daic_softc sc_daic;	/* MI driver state */
	void *sc_ih;			/* interrupt handler */
};

/* local functions */
#ifdef __BROKEN_INDIRECT_CONFIG
static int daic_isa_probe(device_t, void *, void *);
#else
static int daic_isa_probe(device_t, cfdata_t, void *);
#endif
static void daic_isa_attach(device_t, device_t, void *);
static int daic_isa_intr(void *);

CFATTACH_DECL_NEW(daic_isa, sizeof(struct daic_isa_softc),
    daic_isa_probe, daic_isa_attach, NULL, NULL);

static int
daic_isa_probe(device_t parent, cfdata_t cf, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t memt = ia->ia_memt;
	bus_space_handle_t memh;
	int card, need_unmap = 0;

	/* We need some controller memory to comunicate! */
	if (ia->ia_iomem[0].ir_addr == 0 || ia->ia_iomem[0].ir_size == -1)
		goto bad;

	/* Map card RAM. */
	ia->ia_iomem[0].ir_size = DAIC_ISA_MEMSIZE;
	ia->ia_nio = 0;
	ia->ia_ndrq = 0;
	ia->ia_nirq = 1;
	ia->ia_niomem = 1;
	if (bus_space_map(memt, ia->ia_iomem[0].ir_addr, ia->ia_iomem[0].ir_size,
	    0, &memh))
		goto bad;
	need_unmap = 1;

	/* MI check for card at this location */
	card = daic_probe(memt, memh);
	if (card < 0)
		goto bad;
	if (card == DAIC_TYPE_QUAD)
		ia->ia_iomem[0].ir_size = DAIC_ISA_QUADSIZE;

	bus_space_unmap(memt, memh, DAIC_ISA_MEMSIZE);
	return 1;

bad:
	/* unmap card RAM if already mapped */
	if (need_unmap)
		bus_space_unmap(memt, memh, DAIC_ISA_MEMSIZE);
	return 0;
}

static void
daic_isa_attach(device_t parent, device_t self, void *aux)
{
	struct daic_isa_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;
	bus_space_tag_t memt = ia->ia_memt;
	bus_space_handle_t memh;

	/* Map card RAM. */
	if (bus_space_map(memt, ia->ia_iomem[0].ir_addr, ia->ia_iomem[0].ir_size,
	    0, &memh))
		return;

	sc->sc_daic.sc_iot = memt;
	sc->sc_daic.sc_ioh = memh;

	/* MI initialization of card */
	daic_attach(self, &sc->sc_daic);

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq, IST_EDGE,
	    IPL_NET, daic_isa_intr, sc);
}

/*
 * Controller interrupt.
 */
static int
daic_isa_intr(void *arg)
{
	struct daic_isa_softc *sc = arg;
	return daic_intr(&sc->sc_daic);
}
