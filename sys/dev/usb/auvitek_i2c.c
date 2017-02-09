/* $NetBSD: auvitek_i2c.c,v 1.3 2011/10/02 16:30:58 jmcneill Exp $ */

/*-
 * Copyright (c) 2010 Jared D. McNeill <jmcneill@invisible.ca>
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

/*
 * Auvitek AU0828 USB controller - I2C access ops
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: auvitek_i2c.c,v 1.3 2011/10/02 16:30:58 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/i2c/i2cvar.h>

#include <dev/usb/auvitekreg.h>
#include <dev/usb/auvitekvar.h>

/* #define AUVITEK_I2C_DEBUG */

static int	auvitek_i2c_acquire_bus(void *, int);
static void	auvitek_i2c_release_bus(void *, int);
static int	auvitek_i2c_exec(void *, i2c_op_t, i2c_addr_t,
				 const void *, size_t, void *, size_t, int);

static int	auvitek_i2c_read(struct auvitek_softc *, i2c_addr_t,
				 uint8_t *, size_t);
static int	auvitek_i2c_write(struct auvitek_softc *, i2c_addr_t,
				  const uint8_t *, size_t);
static bool	auvitek_i2c_wait(struct auvitek_softc *);
static bool	auvitek_i2c_wait_rdack(struct auvitek_softc *);
static bool	auvitek_i2c_wait_rddone(struct auvitek_softc *);
static bool	auvitek_i2c_wait_wrdone(struct auvitek_softc *);

int
auvitek_i2c_attach(struct auvitek_softc *sc)
{
	mutex_init(&sc->sc_i2c_lock, MUTEX_DEFAULT, IPL_NONE);
	sc->sc_i2c.ic_cookie = sc;
	sc->sc_i2c.ic_acquire_bus = auvitek_i2c_acquire_bus;
	sc->sc_i2c.ic_release_bus = auvitek_i2c_release_bus;
	sc->sc_i2c.ic_exec = auvitek_i2c_exec;

	auvitek_i2c_rescan(sc, NULL, NULL);

	return 0;
}

int
auvitek_i2c_detach(struct auvitek_softc *sc, int flags)
{
	mutex_destroy(&sc->sc_i2c_lock);

	if (sc->sc_i2cdev)
		config_detach(sc->sc_i2cdev, flags);

	return 0;
}

void
auvitek_i2c_rescan(struct auvitek_softc *sc, const char *ifattr,
    const int *locs)
{
#ifdef AUVITEK_I2C_DEBUG
	struct i2cbus_attach_args iba;

	if (ifattr_match(ifattr, "i2cbus") && sc->sc_i2cdev == NULL) {
		memset(&iba, 0, sizeof(iba));
		iba.iba_type = I2C_TYPE_SMBUS;
		iba.iba_tag = &sc->sc_i2c;
		sc->sc_i2cdev = config_found_ia(sc->sc_dev, "i2cbus",
		    &iba, iicbus_print);
	}
#endif
}

void
auvitek_i2c_childdet(struct auvitek_softc *sc, device_t child)
{
	if (sc->sc_i2cdev == child)
		sc->sc_i2cdev = NULL;
}

static int
auvitek_i2c_acquire_bus(void *opaque, int flags)
{
	struct auvitek_softc *sc = opaque;

	if (flags & I2C_F_POLL) {
		if (!mutex_tryenter(&sc->sc_i2c_lock))
			return EBUSY;
	} else {
		mutex_enter(&sc->sc_i2c_lock);
	}

	return 0;
}

static void
auvitek_i2c_release_bus(void *opaque, int flags)
{
	struct auvitek_softc *sc = opaque;

	mutex_exit(&sc->sc_i2c_lock);
}

static int
auvitek_i2c_exec(void *opaque, i2c_op_t op, i2c_addr_t addr,
    const void *cmd, size_t cmdlen, void *vbuf, size_t buflen, int flags)
{
	struct auvitek_softc *sc = opaque;

	if (I2C_OP_READ_P(op))
		return auvitek_i2c_read(sc, addr, vbuf, buflen);
	else
		return auvitek_i2c_write(sc, addr, cmd, cmdlen);
}

static int
auvitek_i2c_read(struct auvitek_softc *sc, i2c_addr_t addr,
    uint8_t *buf, size_t buflen)
{
	uint8_t v;
	unsigned int i;

	//KASSERT(mutex_owned(&sc->sc_i2c_lock));

	auvitek_write_1(sc, AU0828_REG_I2C_MBMODE, 1);
	auvitek_write_1(sc, AU0828_REG_I2C_CLKDIV, sc->sc_i2c_clkdiv);
	auvitek_write_1(sc, AU0828_REG_I2C_DSTADDR, addr << 1);

	if (buflen == 0) {
		auvitek_write_1(sc, AU0828_REG_I2C_TRIGGER,
		    AU0828_I2C_TRIGGER_RD);
		if (auvitek_i2c_wait_rdack(sc) == false)
			return EBUSY;
		return 0;
	}

	for (i = 0; i < buflen; i++) {
		v = AU0828_I2C_TRIGGER_RD;
		if (i < (buflen - 1))
			v |= AU0828_I2C_TRIGGER_HOLD;
		auvitek_write_1(sc, AU0828_REG_I2C_TRIGGER, v);

		if (auvitek_i2c_wait_rddone(sc) == false)
			return EBUSY;

		buf[i] = auvitek_read_1(sc, AU0828_REG_I2C_FIFORD);
	}

	if (auvitek_i2c_wait(sc) == false)
		return EBUSY;

	return 0;
}

static int
auvitek_i2c_write(struct auvitek_softc *sc, i2c_addr_t addr,
    const uint8_t *buf, size_t buflen)
{
	uint8_t v;
	unsigned int i, fifolen;

	//KASSERT(mutex_owned(&sc->sc_i2c_lock));

	auvitek_write_1(sc, AU0828_REG_I2C_MBMODE, 1);
	auvitek_write_1(sc, AU0828_REG_I2C_CLKDIV, sc->sc_i2c_clkdiv);
	auvitek_write_1(sc, AU0828_REG_I2C_DSTADDR, addr << 1);

	if (buflen == 0) {
		auvitek_write_1(sc, AU0828_REG_I2C_TRIGGER,
		    AU0828_I2C_TRIGGER_RD);
		if (auvitek_i2c_wait(sc) == false)
			return EBUSY;
		if (auvitek_i2c_wait_rdack(sc) == false)
			return EBUSY;
		return 0;
	}

	fifolen = 0;
	for (i = 0; i < buflen; i++) {
		v = AU0828_I2C_TRIGGER_WR;
		if (i < (buflen - 1))
			v |= AU0828_I2C_TRIGGER_HOLD;

		auvitek_write_1(sc, AU0828_REG_I2C_FIFOWR, buf[i]);
		++fifolen;

		if (fifolen == 4 || i == (buflen - 1)) {
			auvitek_write_1(sc, AU0828_REG_I2C_TRIGGER, v);
			fifolen = 0;

			if (auvitek_i2c_wait_wrdone(sc) == false)
				return EBUSY;
		}
	}

	if (auvitek_i2c_wait(sc) == false)
		return EBUSY;

	return 0;
}

static bool
auvitek_i2c_wait(struct auvitek_softc *sc)
{
	uint8_t status;
	int retry = 1000;

	while (--retry > 0) {
		status = auvitek_read_1(sc, AU0828_REG_I2C_STATUS);
		if (!(status & AU0828_I2C_STATUS_BUSY))
			break;
		delay(10);
	}
	if (retry == 0)
		return false;

	return true;
}

static bool
auvitek_i2c_wait_rdack(struct auvitek_softc *sc)
{
	uint8_t status;
	int retry = 1000;

	while (--retry > 0) {
		status = auvitek_read_1(sc, AU0828_REG_I2C_STATUS);
		if (!(status & AU0828_I2C_STATUS_NO_RD_ACK))
			break;
		delay(10);
	}
	if (retry == 0)
		return false;

	return true;
}

static bool
auvitek_i2c_wait_rddone(struct auvitek_softc *sc)
{
	uint8_t status;
	int retry = 1000;

	while (--retry > 0) {
		status = auvitek_read_1(sc, AU0828_REG_I2C_STATUS);
		if (status & AU0828_I2C_STATUS_RD_DONE)
			break;
		delay(10);
	}
	if (retry == 0)
		return false;

	return true;
}

static bool
auvitek_i2c_wait_wrdone(struct auvitek_softc *sc)
{
	uint8_t status;
	int retry = 1000;

	while (--retry > 0) {
		status = auvitek_read_1(sc, AU0828_REG_I2C_STATUS);
		if (status & AU0828_I2C_STATUS_WR_DONE)
			break;
		delay(10);
	}
	if (retry == 0)
		return false;

	return true;
}
