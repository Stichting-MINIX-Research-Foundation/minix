/*	$NetBSD: m41st84.c,v 1.22 2014/11/20 16:34:26 christos Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford and Jason R. Thorpe for Wasabi Systems, Inc.
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
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: m41st84.c,v 1.22 2014/11/20 16:34:26 christos Exp $");

#include "opt_strtc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/event.h>

#include <dev/clock_subr.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/m41st84reg.h>
#include <dev/i2c/m41st84var.h>

struct strtc_softc {
	device_t sc_dev;
	i2c_tag_t sc_tag;
	int sc_address;
	int sc_open;
	struct todr_chip_handle sc_todr;
};

static void	strtc_attach(device_t, device_t, void *);
static int	strtc_match(device_t, cfdata_t, void *);

CFATTACH_DECL_NEW(strtc, sizeof(struct strtc_softc),
    strtc_match, strtc_attach, NULL, NULL);

#ifndef STRTC_NO_USERRAM
extern struct cfdriver strtc_cd;

dev_type_open(strtc_open);
dev_type_close(strtc_close);
dev_type_read(strtc_read);
dev_type_write(strtc_write);

const struct cdevsw strtc_cdevsw = {
	.d_open = strtc_open,
	.d_close = strtc_close,
	.d_read = strtc_read,
	.d_write = strtc_write,
	.d_ioctl = noioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};
#endif

static int strtc_clock_read(struct strtc_softc *, struct clock_ymdhms *);
static int strtc_clock_write(struct strtc_softc *, struct clock_ymdhms *);
static int strtc_gettime(struct todr_chip_handle *, struct timeval *);
static int strtc_settime(struct todr_chip_handle *, struct timeval *);

static int
strtc_match(device_t parent, cfdata_t cf, void *arg)
{
	struct i2c_attach_args *ia = arg;

	if (ia->ia_name) {
		/* direct config - check name */
		if (strcmp(ia->ia_name, "strtc") == 0)
			return 1;
	} else {
		/* indirect config - check typical address */
		if (ia->ia_addr == M41ST84_ADDR)
			return 1;
	}
	return 0;
}

static void
strtc_attach(device_t parent, device_t self, void *arg)
{
	struct strtc_softc *sc = device_private(self);
	struct i2c_attach_args *ia = arg;

#ifndef STRTC_NO_USERRAM
	aprint_naive(": Real-time Clock/NVRAM\n");
	aprint_normal(": M41ST84 Real-time Clock/NVRAM\n");
#else
	aprint_naive(": Real-time Clock\n");
	aprint_normal(": M41T8x Real-time Clock\n");
#endif
	sc->sc_tag = ia->ia_tag;
	sc->sc_address = ia->ia_addr;
	sc->sc_dev = self;
	sc->sc_open = 0;
	sc->sc_todr.cookie = sc;
	sc->sc_todr.todr_gettime = strtc_gettime;
	sc->sc_todr.todr_settime = strtc_settime;
	sc->sc_todr.todr_setwen = NULL;

	todr_attach(&sc->sc_todr);
}

#ifndef STRTC_NO_USERRAM
/*ARGSUSED*/
int
strtc_open(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct strtc_softc *sc;

	if ((sc = device_lookup_private(&strtc_cd, minor(dev))) == NULL)
		return (ENXIO);

	/* XXX: Locking */

	if (sc->sc_open)
		return (EBUSY);

	sc->sc_open = 1;
	return (0);
}

/*ARGSUSED*/
int
strtc_close(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct strtc_softc *sc;

	if ((sc = device_lookup_private(&strtc_cd, minor(dev))) == NULL)
		return (ENXIO);

	sc->sc_open = 0;
	return (0);
}

/*ARGSUSED*/
int
strtc_read(dev_t dev, struct uio *uio, int flags)
{
	struct strtc_softc *sc;
	u_int8_t ch, cmdbuf[1];
	int a, error;

	if ((sc = device_lookup_private(&strtc_cd, minor(dev))) == NULL)
		return (ENXIO);

	if (uio->uio_offset >= M41ST84_USER_RAM_SIZE)
		return (EINVAL);

	if ((error = iic_acquire_bus(sc->sc_tag, 0)) != 0)
		return (error);

	while (uio->uio_resid && uio->uio_offset < M41ST84_USER_RAM_SIZE) {
		a = (int)uio->uio_offset;
		cmdbuf[0] = a + M41ST84_USER_RAM;
		if ((error = iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
				      sc->sc_address, cmdbuf, 1,
				      &ch, 1, 0)) != 0) {
			iic_release_bus(sc->sc_tag, 0);
			aprint_error_dev(sc->sc_dev,
			    "strtc_read: read failed at 0x%x\n", a);
			return (error);
		}
		if ((error = uiomove(&ch, 1, uio)) != 0) {
			iic_release_bus(sc->sc_tag, 0);
			return (error);
		}
	}

	iic_release_bus(sc->sc_tag, 0);

	return (0);
}

/*ARGSUSED*/
int
strtc_write(dev_t dev, struct uio *uio, int flags)
{
	struct strtc_softc *sc;
	u_int8_t cmdbuf[2];
	int a, error;

	if ((sc = device_lookup_private(&strtc_cd, minor(dev))) == NULL)
		return (ENXIO);

	if (uio->uio_offset >= M41ST84_USER_RAM_SIZE)
		return (EINVAL);

	if ((error = iic_acquire_bus(sc->sc_tag, 0)) != 0)
		return (error);

	while (uio->uio_resid && uio->uio_offset < M41ST84_USER_RAM_SIZE) {
		a = (int)uio->uio_offset;
		cmdbuf[0] = a + M41ST84_USER_RAM;
		if ((error = uiomove(&cmdbuf[1], 1, uio)) != 0)
			break;

		if ((error = iic_exec(sc->sc_tag,
		    uio->uio_resid ? I2C_OP_WRITE : I2C_OP_WRITE_WITH_STOP,
		    sc->sc_address, cmdbuf, 1, &cmdbuf[1], 1, 0)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "strtc_write: write failed at 0x%x\n", a);
			break;
		}
	}

	iic_release_bus(sc->sc_tag, 0);

	return (error);
}
#endif	/* STRTC_NO_USERRAM */

static int
strtc_gettime(struct todr_chip_handle *ch, struct timeval *tv)
{
	struct strtc_softc *sc = ch->cookie;
	struct clock_ymdhms dt, check;
	int retries;

	memset(&dt, 0, sizeof(dt));
	memset(&check, 0, sizeof(check));

	/*
	 * Since we don't support Burst Read, we have to read the clock twice
	 * until we get two consecutive identical results.
	 */
	retries = 5;
	do {
		strtc_clock_read(sc, &dt);
		strtc_clock_read(sc, &check);
	} while (memcmp(&dt, &check, sizeof(check)) != 0 && --retries);

	tv->tv_sec = clock_ymdhms_to_secs(&dt);
	tv->tv_usec = 0;

	return (0);
}

static int
strtc_settime(struct todr_chip_handle *ch, struct timeval *tv)
{
	struct strtc_softc *sc = ch->cookie;
	struct clock_ymdhms dt;

	clock_secs_to_ymdhms(tv->tv_sec, &dt);

	if (strtc_clock_write(sc, &dt) == 0)
		return (-1);

	return (0);
}

static int
strtc_clock_read(struct strtc_softc *sc, struct clock_ymdhms *dt)
{
	u_int8_t bcd[M41ST84_REG_DATE_BYTES], cmdbuf[2];
	int i;

	if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL)) {
		aprint_error_dev(sc->sc_dev,
		    "strtc_clock_read: failed to acquire I2C bus\n");
		return (0);
	}

	/*
	 * Check for the HT bit -- if set, then clock lost power & stopped
	 * If that happened, then clear the bit so that the clock will have
	 * a chance to run again.
	 */
	cmdbuf[0] = M41ST84_REG_AL_HOUR;
	if (iic_exec(sc->sc_tag, I2C_OP_READ, sc->sc_address,
		     cmdbuf, 1, &cmdbuf[1], 1, I2C_F_POLL)) {
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		aprint_error_dev(sc->sc_dev,
		    "strtc_clock_read: failed to read HT\n");
		return (0);
	}
	if (cmdbuf[1] & M41ST84_AL_HOUR_HT) {
		cmdbuf[1] &= ~M41ST84_AL_HOUR_HT;
		if (iic_exec(sc->sc_tag, I2C_OP_WRITE, sc->sc_address,
			     cmdbuf, 1, &cmdbuf[1], 1, I2C_F_POLL)) {
			iic_release_bus(sc->sc_tag, I2C_F_POLL);
			aprint_error_dev(sc->sc_dev,
			    "strtc_clock_read: failed to reset HT\n");
			return (0);
		}
	}

	/* Read each RTC register in order. */
	for (i = M41ST84_REG_CSEC; i < M41ST84_REG_DATE_BYTES; i++) {
		cmdbuf[0] = i;

		if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
			     sc->sc_address, cmdbuf, 1,
			     &bcd[i], 1, I2C_F_POLL)) {
			iic_release_bus(sc->sc_tag, I2C_F_POLL);
			aprint_error_dev(sc->sc_dev,
			    "strtc_clock_read: failed to read rtc "
			    "at 0x%x\n", i);
			return (0);
		}
	}

	/* Done with I2C */
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	/*
	 * Convert the M41ST84's register values into something useable
	 */
	dt->dt_sec = bcdtobin(bcd[M41ST84_REG_SEC] & M41ST84_SEC_MASK);
	dt->dt_min = bcdtobin(bcd[M41ST84_REG_MIN] & M41ST84_MIN_MASK);
	dt->dt_hour = bcdtobin(bcd[M41ST84_REG_CENHR] & M41ST84_HOUR_MASK);
	dt->dt_day = bcdtobin(bcd[M41ST84_REG_DATE] & M41ST84_DATE_MASK);
	dt->dt_mon = bcdtobin(bcd[M41ST84_REG_MONTH] & M41ST84_MONTH_MASK);

	/* XXX: Should be an MD way to specify EPOCH used by BIOS/Firmware */
	dt->dt_year = bcdtobin(bcd[M41ST84_REG_YEAR]) + POSIX_BASE_YEAR;

	return (1);
}

static int
strtc_clock_write(struct strtc_softc *sc, struct clock_ymdhms *dt)
{
	uint8_t bcd[M41ST84_REG_DATE_BYTES], cmdbuf[2];
	int i;

	/*
	 * Convert our time representation into something the M41ST84
	 * can understand.
	 */
	bcd[M41ST84_REG_CSEC] = bintobcd(0);	/* must always write as 0 */
	bcd[M41ST84_REG_SEC] = bintobcd(dt->dt_sec);
	bcd[M41ST84_REG_MIN] = bintobcd(dt->dt_min);
	bcd[M41ST84_REG_CENHR] = bintobcd(dt->dt_hour);
	bcd[M41ST84_REG_DATE] = bintobcd(dt->dt_day);
	bcd[M41ST84_REG_DAY] = bintobcd(dt->dt_wday);
	bcd[M41ST84_REG_MONTH] = bintobcd(dt->dt_mon);
	bcd[M41ST84_REG_YEAR] = bintobcd((dt->dt_year - POSIX_BASE_YEAR) % 100);

	if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL)) {
		aprint_error_dev(sc->sc_dev,
		    "strtc_clock_write: failed to acquire I2C bus\n");
		return (0);
	}

	/* Stop the clock */
	cmdbuf[0] = M41ST84_REG_SEC;
	cmdbuf[1] = M41ST84_SEC_ST;

	if (iic_exec(sc->sc_tag, I2C_OP_WRITE, sc->sc_address,
		     cmdbuf, 1, &cmdbuf[1], 1, I2C_F_POLL)) {
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		aprint_error_dev(sc->sc_dev,
		    "strtc_clock_write: failed to Hold Clock\n");
		return (0);
	}

	/*
	 * Check for the HT bit -- if set, then clock lost power & stopped
	 * If that happened, then clear the bit so that the clock will have
	 * a chance to run again.
	 */
	cmdbuf[0] = M41ST84_REG_AL_HOUR;
	if (iic_exec(sc->sc_tag, I2C_OP_READ, sc->sc_address,
		     cmdbuf, 1, &cmdbuf[1], 1, I2C_F_POLL)) {
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		aprint_error_dev(sc->sc_dev,
		    "strtc_clock_write: failed to read HT\n");
		return (0);
	}
	if (cmdbuf[1] & M41ST84_AL_HOUR_HT) {
		cmdbuf[1] &= ~M41ST84_AL_HOUR_HT;
		if (iic_exec(sc->sc_tag, I2C_OP_WRITE, sc->sc_address,
			     cmdbuf, 1, &cmdbuf[1], 1, I2C_F_POLL)) {
			iic_release_bus(sc->sc_tag, I2C_F_POLL);
			aprint_error_dev(sc->sc_dev,
			    "strtc_clock_write: failed to reset HT\n");
			return (0);
		}
	}

	/*
	 * Write registers in reverse order. The last write (to the Seconds
	 * register) will undo the Clock Hold, above.
	 */
	for (i = M41ST84_REG_DATE_BYTES - 1; i >= 0; i--) {
		cmdbuf[0] = i;
		if (iic_exec(sc->sc_tag,
			     i ? I2C_OP_WRITE : I2C_OP_WRITE_WITH_STOP,
			     sc->sc_address, cmdbuf, 1, &bcd[i], 1,
			     I2C_F_POLL)) {
			iic_release_bus(sc->sc_tag, I2C_F_POLL);
			aprint_error_dev(sc->sc_dev,
			    "strtc_clock_write: failed to write rtc "
			    " at 0x%x\n", i);
			/* XXX: Clock Hold is likely still asserted! */
			return (0);
		}
	}

	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	return (1);
}

#ifndef STRTC_NO_WATCHDOG
void
strtc_wdog_config(void *arg, uint8_t wd)
{
	struct strtc_softc *sc = arg;
	uint8_t	cmdbuf[2];

	if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL)) {
		aprint_error_dev(sc->sc_dev,
		    "strtc_wdog_config: failed to acquire I2C bus\n");
		return;
	}

	cmdbuf[0] = M41ST84_REG_WATCHDOG;
	cmdbuf[1] = wd;

	if (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP, sc->sc_address,
		     cmdbuf, 1, &cmdbuf[1], 1, I2C_F_POLL)) {
		aprint_error_dev(sc->sc_dev,
		    "strtc_wdog_config: failed to write watchdog\n");
		return;
	}

	iic_release_bus(sc->sc_tag, I2C_F_POLL);
}
#endif	/* STRTC_NO_WATCHDOG */
