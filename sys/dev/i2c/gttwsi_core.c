/*	$NetBSD: gttwsi_core.c,v 1.2 2014/11/23 13:37:27 jmcneill Exp $	*/
/*
 * Copyright (c) 2008 Eiji Kawauchi.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Eiji Kawauchi.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
/*
 * Copyright (c) 2005 Brocade Communcations, inc.
 * All rights reserved.
 *
 * Written by Matt Thomas for Brocade Communcations, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Brocade Communications, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BROCADE COMMUNICATIONS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL EITHER BROCADE COMMUNICATIONS, INC. BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
//#define TWSI_DEBUG

/*
 * Marvell Two-Wire Serial Interface (aka I2C) master driver
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: gttwsi_core.c,v 1.2 2014/11/23 13:37:27 jmcneill Exp $");
#include "locators.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include <dev/i2c/i2cvar.h>

#include <dev/i2c/gttwsireg.h>
#include <dev/i2c/gttwsivar.h>

static int	gttwsi_acquire_bus(void *, int);
static void	gttwsi_release_bus(void *, int);
static int	gttwsi_send_start(void *v, int flags);
static int	gttwsi_send_stop(void *v, int flags);
static int	gttwsi_initiate_xfer(void *v, i2c_addr_t addr, int flags);
static int	gttwsi_read_byte(void *v, uint8_t *valp, int flags);
static int	gttwsi_write_byte(void *v, uint8_t val, int flags);

static int	gttwsi_wait(struct gttwsi_softc *, uint32_t, uint32_t, int);

static inline uint32_t
gttwsi_read_4(struct gttwsi_softc *sc, uint32_t reg)
{
	uint32_t val = bus_space_read_4(sc->sc_bust, sc->sc_bush, reg);
#ifdef TWSI_DEBUG
	printf("I2C:R:%02x:%02x\n", reg, val);
#else
	DELAY(TWSI_READ_DELAY);
#endif
	return val;
}

static inline void
gttwsi_write_4(struct gttwsi_softc *sc, uint32_t reg, uint32_t val)
{
	bus_space_write_4(sc->sc_bust, sc->sc_bush, reg, val);
#ifdef TWSI_DEBUG
	printf("I2C:W:%02x:%02x\n", reg, val);
#else
	DELAY(TWSI_WRITE_DELAY);
#endif
	return;
}



/* ARGSUSED */
void
gttwsi_attach_subr(device_t self, bus_space_tag_t iot, bus_space_handle_t ioh)
{
	struct gttwsi_softc * const sc = device_private(self);
	prop_dictionary_t cfg = device_properties(self);

	aprint_naive("\n");
	aprint_normal(": Marvell TWSI controller\n");

	sc->sc_dev = self;
	sc->sc_bust = iot;
	sc->sc_bush = ioh;

	mutex_init(&sc->sc_buslock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_BIO);
	cv_init(&sc->sc_cv, device_xname(self));

	prop_dictionary_get_bool(cfg, "iflg-rwc", &sc->sc_iflg_rwc);

	sc->sc_started = false;
	sc->sc_i2c.ic_cookie = sc;
	sc->sc_i2c.ic_acquire_bus = gttwsi_acquire_bus;
	sc->sc_i2c.ic_release_bus = gttwsi_release_bus;
	sc->sc_i2c.ic_exec = NULL;
	sc->sc_i2c.ic_send_start = gttwsi_send_start;
	sc->sc_i2c.ic_send_stop = gttwsi_send_stop;
	sc->sc_i2c.ic_initiate_xfer = gttwsi_initiate_xfer;
	sc->sc_i2c.ic_read_byte = gttwsi_read_byte;
	sc->sc_i2c.ic_write_byte = gttwsi_write_byte;

	/*
	 * Put the controller into Soft Reset.
	 */
	/* reset */
	gttwsi_write_4(sc, TWSI_SOFTRESET, SOFTRESET_VAL);

}

void
gttwsi_config_children(device_t self)
{
	struct gttwsi_softc * const sc = device_private(self);
	struct i2cbus_attach_args iba;

	memset(&iba, 0, sizeof(iba));
	iba.iba_tag = &sc->sc_i2c;

	(void) config_found_ia(sc->sc_dev, "i2cbus", &iba, iicbus_print);
}

int
gttwsi_intr(void *arg)
{
	struct gttwsi_softc *sc = arg;
	uint32_t val;

	val = gttwsi_read_4(sc, TWSI_CONTROL);
	if (val & CONTROL_IFLG) {
		gttwsi_write_4(sc, TWSI_CONTROL, val & ~CONTROL_INTEN);
		mutex_enter(&sc->sc_mtx);
		cv_signal(&sc->sc_cv);
		mutex_exit(&sc->sc_mtx);

		return 1;	/* handled */
	}
	return 0;
}

/* ARGSUSED */
static int
gttwsi_acquire_bus(void *arg, int flags)
{
	struct gttwsi_softc *sc = arg;

	mutex_enter(&sc->sc_buslock);
	return 0;
}

/* ARGSUSED */
static void
gttwsi_release_bus(void *arg, int flags)
{
	struct gttwsi_softc *sc = arg;

	mutex_exit(&sc->sc_buslock);
}

static int
gttwsi_send_start(void *v, int flags)
{
	struct gttwsi_softc *sc = v;
	int expect;

	if (sc->sc_started)
		expect = STAT_RSCT;
	else
		expect = STAT_SCT;
	sc->sc_started = true;
	return gttwsi_wait(sc, CONTROL_START, expect, flags);
}

static int
gttwsi_send_stop(void *v, int flags)
{
	struct gttwsi_softc *sc = v;
	int retry = TWSI_RETRY_COUNT;
	uint32_t control;

	sc->sc_started = false;

	/* Interrupt is not generated for STAT_NRS. */
	control = CONTROL_STOP | CONTROL_TWSIEN;
	if (sc->sc_iflg_rwc)
		control |= CONTROL_IFLG;
	gttwsi_write_4(sc, TWSI_CONTROL, control);
	while (retry > 0) {
		if (gttwsi_read_4(sc, TWSI_STATUS) == STAT_NRS)
			return 0;
		retry--;
		DELAY(TWSI_STAT_DELAY);
	}

	aprint_error_dev(sc->sc_dev, "send STOP failed\n");
	return -1;
}

static int
gttwsi_initiate_xfer(void *v, i2c_addr_t addr, int flags)
{
	struct gttwsi_softc *sc = v;
	uint32_t data, expect;
	int error, read;

	gttwsi_send_start(v, flags);

	read = (flags & I2C_F_READ) != 0;
	if (read)
		expect = STAT_ARBT_AR;
	else
		expect = STAT_AWBT_AR;

	/*
	 * First byte contains whether this xfer is a read or write.
	 */
	data = read;
	if (addr > 0x7f) {
		/*
		 * If this is a 10bit request, the first address byte is
		 * 0b11110<b9><b8><r/w>.
		 */
		data |= 0xf0 | ((addr & 0x300) >> 7);
		gttwsi_write_4(sc, TWSI_DATA, data);
		error = gttwsi_wait(sc, 0, expect, flags);
		if (error)
			return error;
		/*
		 * The first address byte has been sent, now to send
		 * the second one.
		 */
		if (read)
			expect = STAT_SARBT_AR;
		else
			expect = STAT_SAWBT_AR;
		data = (uint8_t)addr;
	} else
		data |= (addr << 1);

	gttwsi_write_4(sc, TWSI_DATA, data);
	return gttwsi_wait(sc, 0, expect, flags);
}

static int
gttwsi_read_byte(void *v, uint8_t *valp, int flags)
{
	struct gttwsi_softc *sc = v;
	int error;

	if (flags & I2C_F_LAST)
		error = gttwsi_wait(sc, 0, STAT_MRRD_ANT, flags);
	else
		error = gttwsi_wait(sc, CONTROL_ACK, STAT_MRRD_AT, flags);
	if (!error)
		*valp = gttwsi_read_4(sc, TWSI_DATA);
	if ((flags & (I2C_F_LAST | I2C_F_STOP)) == (I2C_F_LAST | I2C_F_STOP))
		error = gttwsi_send_stop(sc, flags);
	return error;
}

static int
gttwsi_write_byte(void *v, uint8_t val, int flags)
{
	struct gttwsi_softc *sc = v;
	int error;

	gttwsi_write_4(sc, TWSI_DATA, val);
	error = gttwsi_wait(sc, 0, STAT_MTDB_AR, flags);
	if (flags & I2C_F_STOP)
		gttwsi_send_stop(sc, flags);
	return error;
}

static int
gttwsi_wait(struct gttwsi_softc *sc, uint32_t control, uint32_t expect,
	    int flags)
{
	uint32_t status;
	int timo, error = 0;

	DELAY(5);
	if (!(flags & I2C_F_POLL))
		control |= CONTROL_INTEN;
	if (sc->sc_iflg_rwc)
		control |= CONTROL_IFLG;
	gttwsi_write_4(sc, TWSI_CONTROL, control | CONTROL_TWSIEN);

	timo = 0;
	for (;;) {
		control = gttwsi_read_4(sc, TWSI_CONTROL);
		if (control & CONTROL_IFLG)
			break;
		if (!(flags & I2C_F_POLL)) {
			mutex_enter(&sc->sc_mtx);
			error = cv_timedwait_sig(&sc->sc_cv, &sc->sc_mtx, hz);
			mutex_exit(&sc->sc_mtx);
			if (error)
				return error;
		}
		DELAY(TWSI_RETRY_DELAY);
		if (timo++ > 1000000)	/* 1sec */
			break;
	}

	status = gttwsi_read_4(sc, TWSI_STATUS);
	if (status != expect) {
		aprint_error_dev(sc->sc_dev,
		    "unexpected status 0x%x: expect 0x%x\n", status, expect);
		return EIO;
	}
	return error;
}
