/* $NetBSD: spdmem_i2c.c,v 1.10 2015/03/07 14:16:51 jmcneill Exp $ */

/*
 * Copyright (c) 2007 Nicolas Joly
 * Copyright (c) 2007 Paul Goyette
 * Copyright (c) 2007 Tobias Nygren
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS
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
 * Serial Presence Detect (SPD) memory identification
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: spdmem_i2c.c,v 1.10 2015/03/07 14:16:51 jmcneill Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/endian.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <machine/bswap.h>

#include <dev/i2c/i2cvar.h>
#include <dev/ic/spdmemreg.h>
#include <dev/ic/spdmemvar.h>

/* Constants for matching i2c bus address */
#define SPDMEM_I2C_ADDRMASK 0x3f8
#define SPDMEM_I2C_ADDR     0x50

struct spdmem_i2c_softc {
	struct spdmem_softc sc_base;
	i2c_tag_t sc_tag;
	i2c_addr_t sc_addr;
};

static int  spdmem_i2c_match(device_t, cfdata_t, void *);
static void spdmem_i2c_attach(device_t, device_t, void *);
static int  spdmem_i2c_detach(device_t, int);

CFATTACH_DECL_NEW(spdmem_iic, sizeof(struct spdmem_i2c_softc),
    spdmem_i2c_match, spdmem_i2c_attach, spdmem_i2c_detach, NULL);

static uint8_t spdmem_i2c_read(struct spdmem_softc *, uint8_t);

static int
spdmem_i2c_match(device_t parent, cfdata_t match, void *aux)
{
	struct i2c_attach_args *ia = aux;
	struct spdmem_i2c_softc sc;

	if (ia->ia_name) {
		/* add other names as we find more firmware variations */
		if (strcmp(ia->ia_name, "dimm-spd") &&
		    strcmp(ia->ia_name, "dimm"))
			return 0;
	}

	/* only do this lame test when not using direct config */
	if (ia->ia_name == NULL) {
		if ((ia->ia_addr & SPDMEM_I2C_ADDRMASK) != SPDMEM_I2C_ADDR)
			return 0;
	}

	sc.sc_tag = ia->ia_tag;
	sc.sc_addr = ia->ia_addr;
	sc.sc_base.sc_read = spdmem_i2c_read;

	return spdmem_common_probe(&sc.sc_base);
}

static void
spdmem_i2c_attach(device_t parent, device_t self, void *aux)
{
	struct spdmem_i2c_softc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;
	sc->sc_base.sc_read = spdmem_i2c_read;

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	spdmem_common_attach(&sc->sc_base, self);
}

static int
spdmem_i2c_detach(device_t self, int flags)
{
	struct spdmem_i2c_softc *sc = device_private(self);

	pmf_device_deregister(self);

	return spdmem_common_detach(&sc->sc_base, self);
}

static uint8_t
spdmem_i2c_read(struct spdmem_softc *softc, uint8_t reg)
{
	uint8_t val;
	struct spdmem_i2c_softc *sc = (struct spdmem_i2c_softc *)softc;

	iic_acquire_bus(sc->sc_tag, 0);
	iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_addr, &reg, 1,
		 &val, 1, I2C_F_POLL);
	iic_release_bus(sc->sc_tag, 0);

	return val;
}

MODULE(MODULE_CLASS_DRIVER, spdmem, "i2cexec");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
spdmem_modcmd(modcmd_t cmd, void *opaque)
{
	int error = 0;
#ifdef _MODULE
	static struct sysctllog *spdmem_sysctl_clog;
#endif

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_spdmem,
		    cfattach_ioconf_spdmem, cfdata_ioconf_spdmem);
#endif
		return error;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_spdmem,
		    cfattach_ioconf_spdmem, cfdata_ioconf_spdmem);
		sysctl_teardown(&spdmem_sysctl_clog);
#endif
		return error;
	default:
		return ENOTTY;
	}
}
