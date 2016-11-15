/*	$NetBSD: pca9564.c,v 1.1 2010/04/09 10:09:50 nonaka Exp $	*/

/*
 * Copyright (c) 2010 NONAKA Kimihiro <nonaka@netbsd.org>
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
__KERNEL_RCSID(0, "$NetBSD: pca9564.c,v 1.1 2010/04/09 10:09:50 nonaka Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/mutex.h>
#include <sys/bus.h>

#include <dev/i2c/i2cvar.h>

#include <dev/ic/pca9564reg.h>
#include <dev/ic/pca9564var.h>

#if defined(PCA9564_DEBUG)
int pca9564debug = 0;
#define	DPRINTF(s)	if (pca9564debug) printf s
#else
#define	DPRINTF(s)
#endif

static int pca9564_acquire_bus(void *, int);
static void pca9564_release_bus(void *, int);

static int pca9564_send_start(void *, int);
static int pca9564_send_stop(void *, int);
static int pca9564_initiate_xfer(void *, uint16_t, int);
static int pca9564_read_byte(void *, uint8_t *, int);
static int pca9564_write_byte(void *, uint8_t, int);

static int pca9564_ack(void *, bool, int);

#define	CSR_READ(sc, r)		(*sc->sc_ios.read_byte)(sc->sc_dev, r)
#define	CSR_WRITE(sc, r, v)	(*sc->sc_ios.write_byte)(sc->sc_dev, r, v)

void
pca9564_attach(struct pca9564_softc *sc)
{
	struct i2cbus_attach_args iba;

	aprint_naive("\n");
	aprint_normal(": PCA9564 I2C Controller\n");

	mutex_init(&sc->sc_buslock, MUTEX_DEFAULT, IPL_NONE);

	sc->sc_i2c.ic_cookie = sc;
	sc->sc_i2c.ic_acquire_bus = pca9564_acquire_bus;
	sc->sc_i2c.ic_release_bus = pca9564_release_bus;
	sc->sc_i2c.ic_send_start = pca9564_send_start;
	sc->sc_i2c.ic_send_stop = pca9564_send_stop;
	sc->sc_i2c.ic_initiate_xfer = pca9564_initiate_xfer;
	sc->sc_i2c.ic_read_byte = pca9564_read_byte;
	sc->sc_i2c.ic_write_byte = pca9564_write_byte;
	sc->sc_i2c.ic_exec = NULL;

	/* set serial clock rate */
	switch (sc->sc_i2c_clock) {
	case 330000:	/* 330kHz */
		sc->sc_i2c_clock = I2CCON_CR_330KHZ;
		break;
	case 288000:	/* 288kHz */
		sc->sc_i2c_clock = I2CCON_CR_288KHZ;
		break;
	case 217000:	/* 217kHz */
		sc->sc_i2c_clock = I2CCON_CR_217KHZ;
		break;
	case 146000:	/* 146kHz */
		sc->sc_i2c_clock = I2CCON_CR_146KHZ;
		break;
	case 88000:	/* 88kHz */
		sc->sc_i2c_clock = I2CCON_CR_88KHZ;
		break;
	case 0:		/* default */
	case 59000:	/* 59kHz */
		sc->sc_i2c_clock = I2CCON_CR_59KHZ;
		break;
	case 44000:	/* 44kHz */
		sc->sc_i2c_clock = I2CCON_CR_44KHZ;
		break;
	case 36000:	/* 36kHz */
		sc->sc_i2c_clock = I2CCON_CR_36KHZ;
		break;
	default:
		aprint_error_dev(sc->sc_dev, "unknown i2c clock %dHz\n",
		    sc->sc_i2c_clock);
		sc->sc_i2c_clock = I2CCON_CR_59KHZ;
		break;
	}

	iba.iba_tag = &sc->sc_i2c;
	(void) config_found_ia(sc->sc_dev, "i2cbus", &iba, iicbus_print);
}

static int
pca9564_acquire_bus(void *cookie, int flags)
{
	struct pca9564_softc *sc = cookie;
	uint8_t control;

	mutex_enter(&sc->sc_buslock);

	/* Enable SIO and set clock */
	control = CSR_READ(sc, PCA9564_I2CCON);
	control |= I2CCON_ENSIO;
	control &= ~(I2CCON_STA|I2CCON_STO|I2CCON_SI|I2CCON_AA);
	control &= ~I2CCON_CR_MASK;
	control |= sc->sc_i2c_clock;
	CSR_WRITE(sc, PCA9564_I2CCON, control);
	delay(500);

	return 0;
}

static void
pca9564_release_bus(void *cookie, int flags)
{
	struct pca9564_softc *sc = cookie;
	uint8_t control;

	/* Disable SIO */
	control = CSR_READ(sc, PCA9564_I2CCON);
	control &= ~I2CCON_ENSIO;
	CSR_WRITE(sc, PCA9564_I2CCON, control);

	mutex_exit(&sc->sc_buslock);
}

#define	PCA9564_TIMEOUT		100	/* protocol timeout, in uSecs */

static int
pca9564_wait(struct pca9564_softc *sc, int flags)
{
	int timeout;

	DPRINTF(("%s: status=%#x\n", __func__, CSR_READ(sc, PCA9564_I2CSTA)));
	for (timeout = PCA9564_TIMEOUT; timeout > 0; timeout--) {
		if (CSR_READ(sc, PCA9564_I2CCON) & I2CCON_SI)
			break;
		delay(1);
	}
	DPRINTF(("%s: status=%#x\n", __func__, CSR_READ(sc, PCA9564_I2CSTA)));
	if (timeout == 0) {
		aprint_error_dev(sc->sc_dev, "timeout\n");
		return ETIMEDOUT;
	}
	return 0;
}

static int
pca9564_send_start(void *cookie, int flags)
{
	struct pca9564_softc *sc = cookie;
	uint8_t control;

	DPRINTF(("%s: status=%#x\n", __func__, CSR_READ(sc, PCA9564_I2CSTA)));
	control = CSR_READ(sc, PCA9564_I2CCON);
	control |= I2CCON_STA;
	control &= ~(I2CCON_STO|I2CCON_SI);
	CSR_WRITE(sc, PCA9564_I2CCON, control);
	DPRINTF(("%s: status=%#x\n", __func__, CSR_READ(sc, PCA9564_I2CSTA)));

	return pca9564_wait(sc, flags);
}

static int
pca9564_send_stop(void *cookie, int flags)
{
	struct pca9564_softc *sc = cookie;
	uint8_t control;

	DPRINTF(("%s: status=%#x\n", __func__, CSR_READ(sc, PCA9564_I2CSTA)));
	control = CSR_READ(sc, PCA9564_I2CCON);
	control |= I2CCON_STO;
	control &= ~(I2CCON_STA|I2CCON_SI);
	CSR_WRITE(sc, PCA9564_I2CCON, control);
	DPRINTF(("%s: status=%#x\n", __func__, CSR_READ(sc, PCA9564_I2CSTA)));

	return 0;
}

static int
pca9564_initiate_xfer(void *cookie, uint16_t addr, int flags)
{
	struct pca9564_softc *sc = cookie;
	int error, rd_req = (flags & I2C_F_READ) != 0;
	uint8_t data, control;

	error = pca9564_send_start(sc, flags);
	if (error) {
		aprint_error_dev(sc->sc_dev, "failed to send start %s xfer\n",
		    rd_req ? "read" : "write");
		return error;
	}

	DPRINTF(("%s: status=%#x\n", __func__, CSR_READ(sc, PCA9564_I2CSTA)));
	control = CSR_READ(sc, PCA9564_I2CCON);

	data = (addr << 1) | (rd_req ? 1 : 0);
	CSR_WRITE(sc, PCA9564_I2CDAT, data);

	control &= ~(I2CCON_STO|I2CCON_STA|I2CCON_SI);
	CSR_WRITE(sc, PCA9564_I2CCON, control);
	DPRINTF(("%s: status=%#x\n", __func__, CSR_READ(sc, PCA9564_I2CSTA)));

	error = pca9564_wait(sc, flags);
	if (error)
		aprint_error_dev(sc->sc_dev, "failed to initiate %s xfer\n",
		    rd_req ? "read" : "write");
	return error;
}

static int
pca9564_read_byte(void *cookie, uint8_t *bytep, int flags)
{
	struct pca9564_softc *sc = cookie;
	int send_stop = (flags & I2C_F_STOP) != 0;
	int error;

	error = pca9564_ack(sc, !send_stop, flags);
	if (error) {
		aprint_error_dev(sc->sc_dev, "failed to ack\n");
		return error;
	}

	DPRINTF(("%s: status=%#x\n", __func__, CSR_READ(sc, PCA9564_I2CSTA)));
	*bytep = CSR_READ(sc, PCA9564_I2CDAT);
	DPRINTF(("%s: status=%#x, byte=%#x\n", __func__,
	    CSR_READ(sc, PCA9564_I2CSTA), *bytep));

	if (send_stop)
		pca9564_send_stop(sc, flags);

	return 0;
}

static int
pca9564_write_byte(void *cookie, uint8_t byte, int flags)
{
	struct pca9564_softc *sc = cookie;
	int send_stop = (flags & I2C_F_STOP) != 0;
	int error;
	uint8_t control;

	DPRINTF(("%s: status=%#x, byte=%#x\n", __func__,
	    CSR_READ(sc, PCA9564_I2CSTA), byte));
	control = CSR_READ(sc, PCA9564_I2CCON);

	CSR_WRITE(sc, PCA9564_I2CDAT, byte);

	control &= ~(I2CCON_STO|I2CCON_STA|I2CCON_SI);
	CSR_WRITE(sc, PCA9564_I2CCON, control);
	DPRINTF(("%s: status=%#x\n", __func__, CSR_READ(sc, PCA9564_I2CSTA)));

	error = pca9564_wait(sc, flags);
	if (error)
		aprint_error_dev(sc->sc_dev, "write byte failed\n");

	if (send_stop)
		pca9564_send_stop(sc, flags);

	return error;
}

static int
pca9564_ack(void *cookie, bool ack, int flags)
{
	struct pca9564_softc *sc = cookie;
	uint8_t control;

	DPRINTF(("%s: status=%#x\n", __func__, CSR_READ(sc, PCA9564_I2CSTA)));
	control = CSR_READ(sc, PCA9564_I2CCON);
	control &= ~(I2CCON_STO|I2CCON_STA|I2CCON_SI|I2CCON_AA);
	if (ack)
		control |= I2CCON_AA;
	CSR_WRITE(sc, PCA9564_I2CCON, control);
	DPRINTF(("%s: status=%#x\n", __func__, CSR_READ(sc, PCA9564_I2CSTA)));

	return pca9564_wait(sc, flags);
}
