/* $NetBSD: emdtv_i2c.c,v 1.1 2011/07/11 18:02:04 jmcneill Exp $ */

/*-
 * Copyright (c) 2008, 2010 Jared D. McNeill <jmcneill@invisible.ca>
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
__KERNEL_RCSID(0, "$NetBSD: emdtv_i2c.c,v 1.1 2011/07/11 18:02:04 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/i2c/i2cvar.h>

#include <dev/usb/emdtvvar.h>
#include <dev/usb/emdtvreg.h>

static int	emdtv_i2c_acquire_bus(void *, int);
static void	emdtv_i2c_release_bus(void *, int);
static int	emdtv_i2c_exec(void *, i2c_op_t, i2c_addr_t,
			       const void *, size_t, void *, size_t, int);

static int	emdtv_i2c_check(struct emdtv_softc *, i2c_addr_t);
static int	emdtv_i2c_recv(struct emdtv_softc *, i2c_addr_t,
			       uint8_t *, size_t);
static int	emdtv_i2c_send(struct emdtv_softc *, i2c_addr_t,
				const uint8_t *, size_t, bool);

int
emdtv_i2c_attach(struct emdtv_softc *sc)
{
	mutex_init(&sc->sc_i2c_lock, MUTEX_DEFAULT, IPL_VM);
	sc->sc_i2c.ic_cookie = sc;
	sc->sc_i2c.ic_acquire_bus = emdtv_i2c_acquire_bus;
	sc->sc_i2c.ic_release_bus = emdtv_i2c_release_bus;
	sc->sc_i2c.ic_exec = emdtv_i2c_exec;

	return 0;
}

int
emdtv_i2c_detach(struct emdtv_softc *sc, int flags)
{
	mutex_destroy(&sc->sc_i2c_lock);

	return 0;
}

static int
emdtv_i2c_acquire_bus(void *opaque, int flags)
{
	struct emdtv_softc *sc = opaque;

	mutex_enter(&sc->sc_i2c_lock);

	return 0;
}

static int
emdtv_i2c_exec(void *opaque, i2c_op_t op, i2c_addr_t addr,
    const void *cmd, size_t cmdlen, void *vbuf, size_t buflen, int flags)
{
	struct emdtv_softc *sc = opaque;
	int error;

	if (I2C_OP_READ_P(op)) {
		if (buflen == 0)
			error = emdtv_i2c_check(sc, addr);
		else
			error = emdtv_i2c_recv(sc, addr, vbuf, buflen);
	} else {
		error = emdtv_i2c_send(sc, addr, cmd, cmdlen,
		    I2C_OP_STOP_P(op));
		error = 0;
	}

	return error;
}

static void
emdtv_i2c_release_bus(void *opaque, int flags)
{
	struct emdtv_softc *sc = opaque;

	mutex_exit(&sc->sc_i2c_lock);
}

static int
emdtv_i2c_check(struct emdtv_softc *sc, i2c_addr_t addr)
{
	emdtv_read_1(sc, EM28XX_UR_I2C, addr);
	if (emdtv_read_1(sc, UR_GET_STATUS, EM28XX_REG_I2C_STATUS) != 0) {
		device_printf(sc->sc_dev, "%s failed\n", __func__);
		return ENXIO;
	}
	return 0;
}

static int
emdtv_i2c_recv(struct emdtv_softc *sc, i2c_addr_t addr, uint8_t *datap,
    size_t len)
{
	emdtv_read_multi_1(sc, EM28XX_UR_I2C, addr, datap, len);
	if (emdtv_read_1(sc, UR_GET_STATUS, EM28XX_REG_I2C_STATUS) != 0) {
		device_printf(sc->sc_dev, "%s failed\n", __func__);
		return ENXIO;
	}
	return 0;
}

static int
emdtv_i2c_send(struct emdtv_softc *sc, i2c_addr_t addr, const uint8_t *datap,
    size_t len, bool stop)
{
	int off = (stop == false ? 1 : 0);
	emdtv_write_multi_1(sc, EM28XX_UR_I2C + off, addr, datap, len);
	if (emdtv_read_1(sc, UR_GET_STATUS, EM28XX_REG_I2C_STATUS) != 0) {
		device_printf(sc->sc_dev, "%s failed\n", __func__);
		return ENXIO;
	}
	return 0;
}
