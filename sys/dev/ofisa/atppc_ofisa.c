/* $NetBSD: atppc_ofisa.c,v 1.10 2008/04/28 20:23:54 martin Exp $ */

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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
__KERNEL_RCSID(0, "$NetBSD: atppc_ofisa.c,v 1.10 2008/04/28 20:23:54 martin Exp $");

#include "opt_atppc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/termios.h>

#include <sys/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/isa/isavar.h>
#include <dev/ofisa/ofisavar.h>

#include <dev/ic/atppcvar.h>
#include <dev/isa/atppc_isadma.h>

static int	atppc_ofisa_match(device_t, cfdata_t, void *);
static void	atppc_ofisa_attach(device_t, device_t, void *);

struct atppc_ofisa_softc {
	struct atppc_softc sc_atppc;

	isa_chipset_tag_t sc_ic;
	int sc_drq;
};

CFATTACH_DECL_NEW(atppc_ofisa, sizeof(struct atppc_ofisa_softc), atppc_ofisa_match,
    atppc_ofisa_attach, NULL, NULL);

static int atppc_ofisa_dma_start(struct atppc_softc *, void *, u_int,
	u_int8_t);
static int atppc_ofisa_dma_finish(struct atppc_softc *);
static int atppc_ofisa_dma_abort(struct atppc_softc *);
static int atppc_ofisa_dma_malloc(device_t, void **, bus_addr_t *,
	bus_size_t);
static void atppc_ofisa_dma_free(device_t, void **, bus_addr_t *,
	bus_size_t);
/*
 * atppc_ofisa_match: autoconf(9) match routine
 */
static int
atppc_ofisa_match(device_t parent, cfdata_t match, void *aux)
{
	struct ofisa_attach_args *aa = aux;
	static const char *const compatible_strings[] = { "pnpPNP,401", NULL };
	int rv = 0;

	if (of_compatible(aa->oba.oba_phandle, compatible_strings) != -1)
		rv = 5;
#ifdef _LPT_OFISA_MD_MATCH
	if (!rv)
		rv = lpt_ofisa_md_match(parent, match, aux);
#endif
	return (rv);
}

static void
atppc_ofisa_attach(device_t parent, device_t self, void *aux)
{
	struct atppc_softc *sc = device_private(self);
	struct atppc_ofisa_softc *asc = device_private(self);
	struct ofisa_attach_args *aa = aux;
	struct ofisa_reg_desc reg;
	struct ofisa_intr_desc intr;
	struct ofisa_dma_desc dma;
	int n;

	sc->sc_dev_ok = ATPPC_NOATTACH;

	printf(": AT Parallel Port\n");

	/* find our I/O space */
	n = ofisa_reg_get(aa->oba.oba_phandle, &reg, 1);
#ifdef _LPT_OFISA_MD_REG_FIXUP
	n = lpt_ofisa_md_reg_fixup(parent, self, aux, &reg, 1, n);
#endif
	if (n != 1) {
		aprint_error_dev(sc->sc_dev, "unable to find i/o register resource\n");
		return;
	}

	/* find our IRQ */
	n = ofisa_intr_get(aa->oba.oba_phandle, &intr, 1);
#ifdef _LPT_OFISA_MD_INTR_FIXUP
	n = lpt_ofisa_md_intr_fixup(parent, self, aux, &intr, 1, n);
#endif
	if (n != 1) {
		aprint_error_dev(sc->sc_dev, "unable to find irq resource\n");
		return;
	}

	/* find our DRQ */
	if (ofisa_dma_get(aa->oba.oba_phandle, &dma, 1) != 1) {
		aprint_error_dev(sc->sc_dev, "unable to find DMA data\n");
		return;
	}
	asc->sc_drq = dma.drq;

	/* Attach */
	sc->sc_dev = self;
	sc->sc_iot = (reg.type == OFISA_REG_TYPE_IO) ? aa->iot : aa->memt;
	sc->sc_has = 0;
	asc->sc_ic = aa->ic;

	sc->sc_dev_ok = ATPPC_ATTACHED;

	if (bus_space_map(sc->sc_iot, reg.addr, reg.len, 0,
		&sc->sc_ioh) != 0) {
		aprint_error_dev(self, "attempt to map bus space failed, device not "
			"properly attached.\n");
		return;
	}

	sc->sc_ieh = isa_intr_establish(aa->ic, intr.irq, intr.share,
	    IPL_TTY, atppcintr, sc);
	sc->sc_has |= ATPPC_HAS_INTR;

	/* setup DMA hooks */
	if (atppc_isadma_setup(sc, asc->sc_ic, asc->sc_drq) == 0) {
		sc->sc_has |= ATPPC_HAS_DMA;
		sc->sc_dma_start = atppc_ofisa_dma_start;
		sc->sc_dma_finish = atppc_ofisa_dma_finish;
		sc->sc_dma_abort = atppc_ofisa_dma_abort;
		sc->sc_dma_malloc = atppc_ofisa_dma_malloc;
		sc->sc_dma_free = atppc_ofisa_dma_free;
	}

	/* Run soft configuration attach */
	atppc_sc_attach(sc);
}

/* Start DMA operation over ISA bus */
static int
atppc_ofisa_dma_start(struct atppc_softc *lsc, void *buf, u_int nbytes,
	u_int8_t mode)
{
	struct atppc_ofisa_softc * sc = (struct atppc_ofisa_softc *) lsc;

	return atppc_isadma_start(sc->sc_ic, sc->sc_drq, buf, nbytes, mode);
}

/* Stop DMA operation over ISA bus */
static int
atppc_ofisa_dma_finish(struct atppc_softc * lsc)
{
	struct atppc_ofisa_softc * sc = (struct atppc_ofisa_softc *) lsc;

	return atppc_isadma_finish(sc->sc_ic, sc->sc_drq);
}

/* Abort DMA operation over ISA bus */
int
atppc_ofisa_dma_abort(struct atppc_softc * lsc)
{
	struct atppc_ofisa_softc * sc = (struct atppc_ofisa_softc *) lsc;

	return atppc_isadma_abort(sc->sc_ic, sc->sc_drq);
}

/* Allocate memory for DMA over ISA bus */
int
atppc_ofisa_dma_malloc(device_t dev, void ** buf, bus_addr_t * bus_addr,
	bus_size_t size)
{
	struct atppc_ofisa_softc * sc = device_private(dev);

	return atppc_isadma_malloc(sc->sc_ic, sc->sc_drq, buf, bus_addr, size);
}

/* Free memory allocated by atppc_isa_dma_malloc() */
void
atppc_ofisa_dma_free(device_t dev, void ** buf, bus_addr_t * bus_addr,
	bus_size_t size)
{
	struct atppc_ofisa_softc * sc = device_private(dev);

	return atppc_isadma_free(sc->sc_ic, sc->sc_drq, buf, bus_addr, size);
}
