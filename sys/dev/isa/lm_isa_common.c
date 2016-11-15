/*	$NetBSD: lm_isa_common.c,v 1.3 2012/01/18 00:11:43 jakllsch Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Squier.
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
__KERNEL_RCSID(0, "$NetBSD: lm_isa_common.c,v 1.3 2012/01/18 00:11:43 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/module.h>
#include <sys/conf.h>

#include <sys/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/ic/nslm7xvar.h>

int 	lm_isa_match(device_t, cfdata_t, void *);
void 	lm_isa_attach(device_t, device_t, void *);
int 	lm_isa_detach(device_t, int);

static uint8_t 	lm_isa_readreg(struct lm_softc *, int);
static void 	lm_isa_writereg(struct lm_softc *, int, int);

struct lm_isa_softc {
	struct lm_softc lmsc;
	bus_space_tag_t lm_iot;
	bus_space_handle_t lm_ioh;
};

int
lm_isa_match(device_t parent, cfdata_t match, void *aux)
{
	bus_space_handle_t ioh;
	struct isa_attach_args *ia = aux;
	struct lm_isa_softc sc;
	int rv;

	/* Must supply an address */
	if (ia->ia_nio < 1)
		return 0;

	if (ISA_DIRECT_CONFIG(ia))
		return 0;

	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return 0;

	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr, 8, 0, &ioh))
		return 0;


	/* Bus independent probe */
	sc.lm_iot = ia->ia_iot;
	sc.lm_ioh = ioh;
	sc.lmsc.lm_writereg = lm_isa_writereg;
	sc.lmsc.lm_readreg = lm_isa_readreg;
	rv = lm_probe(&sc.lmsc);

	bus_space_unmap(ia->ia_iot, ioh, 8);

	if (rv) {
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = 8;

		ia->ia_niomem = 0;
		ia->ia_nirq = 0;
		ia->ia_ndrq = 0;
	}

	return rv;
}


void
lm_isa_attach(device_t parent, device_t self, void *aux)
{
	struct lm_isa_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;

	sc->lm_iot = ia->ia_iot;

	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr, 8, 0,
	    &sc->lm_ioh)) {
		aprint_error(": can't map i/o space\n");
		return;
	}

	/* Bus-independent attachment */
	sc->lmsc.sc_dev = self;
	sc->lmsc.lm_writereg = lm_isa_writereg;
	sc->lmsc.lm_readreg = lm_isa_readreg;

	lm_attach(&sc->lmsc);
}

int
lm_isa_detach(device_t self, int flags)
{
	struct lm_isa_softc *sc = device_private(self);

	lm_detach(&sc->lmsc);
	bus_space_unmap(sc->lm_iot, sc->lm_ioh, 8);
	return 0;
}

static uint8_t
lm_isa_readreg(struct lm_softc *lmsc, int reg)
{
	struct lm_isa_softc *sc = (struct lm_isa_softc *)lmsc;

	bus_space_write_1(sc->lm_iot, sc->lm_ioh, LMC_ADDR, reg);
	return bus_space_read_1(sc->lm_iot, sc->lm_ioh, LMC_DATA);
}

static void
lm_isa_writereg(struct lm_softc *lmsc, int reg, int val)
{
	struct lm_isa_softc *sc = (struct lm_isa_softc *)lmsc;

	bus_space_write_1(sc->lm_iot, sc->lm_ioh, LMC_ADDR, reg);
	bus_space_write_1(sc->lm_iot, sc->lm_ioh, LMC_DATA, val);
}

MODULE(MODULE_CLASS_DRIVER, lm_isa_common, "lm");

static int
lm_isa_common_modcmd(modcmd_t cmd, void *priv)
{
	if ((cmd == MODULE_CMD_INIT) || (cmd == MODULE_CMD_FINI))
		return 0;
	return ENOTTY;
}
