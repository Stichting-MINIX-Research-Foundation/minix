/*	$NetBSD: lm_i2c.c,v 1.2 2008/10/13 11:16:00 pgoyette Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: lm_i2c.c,v 1.2 2008/10/13 11:16:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>

#include <dev/i2c/i2cvar.h>

#include <dev/sysmon/sysmonvar.h>

#include <dev/ic/nslm7xvar.h>

int 	lm_i2c_match(device_t, cfdata_t, void *);
void 	lm_i2c_attach(device_t, device_t, void *);
int 	lm_i2c_detach(device_t, int);

uint8_t lm_i2c_readreg(struct lm_softc *, int);
void 	lm_i2c_writereg(struct lm_softc *, int, int);

struct lm_i2c_softc {
	struct lm_softc sc_lmsc;
	i2c_tag_t sc_tag;
	i2c_addr_t sc_addr;
};

CFATTACH_DECL_NEW(lm_iic, sizeof(struct lm_i2c_softc),
    lm_i2c_match, lm_i2c_attach, lm_i2c_detach, NULL);

int
lm_i2c_match(device_t parent, cfdata_t match, void *aux)
{
	struct i2c_attach_args *ia = aux;
	int rv = 0;
	struct lm_i2c_softc sc;

	/* Must supply an address */
	if (ia->ia_addr < 1)
		return 0;

	/* Bus independent probe */
	sc.sc_lmsc.lm_writereg = lm_i2c_writereg;
	sc.sc_lmsc.lm_readreg = lm_i2c_readreg;
	sc.sc_tag = ia->ia_tag;
	sc.sc_addr = ia->ia_addr;
	rv = lm_probe(&sc.sc_lmsc);

	return rv;
}


void
lm_i2c_attach(device_t parent, device_t self, void *aux)
{
	struct lm_i2c_softc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	/* Bus-independent attachment */
	sc->sc_lmsc.sc_dev = self;
	sc->sc_lmsc.lm_writereg = lm_i2c_writereg;
	sc->sc_lmsc.lm_readreg = lm_i2c_readreg;

	lm_attach(&sc->sc_lmsc);
}

int
lm_i2c_detach(device_t self, int flags)
{
	struct lm_i2c_softc *sc = device_private(self);

	lm_detach(&sc->sc_lmsc);
	return 0;
}

uint8_t
lm_i2c_readreg(struct lm_softc *lmsc, int reg)
{
	struct lm_i2c_softc *sc = (struct lm_i2c_softc *)lmsc;
	uint8_t cmd, data;

	iic_acquire_bus(sc->sc_tag, 0);

	cmd = reg;
	iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		 sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0);

	iic_release_bus(sc->sc_tag, 0);

	return data;
}


void
lm_i2c_writereg(struct lm_softc *lmsc, int reg, int val)
{
	struct lm_i2c_softc *sc = (struct lm_i2c_softc *)lmsc;
	uint8_t cmd, data;

	iic_acquire_bus(sc->sc_tag, 0);

	cmd = reg;
	data = val;
	iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP,
		 sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0);

	iic_release_bus(sc->sc_tag, 0);
}
