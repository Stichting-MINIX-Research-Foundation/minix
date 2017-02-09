/*	$NetBSD: max6900.c,v 1.15 2014/11/20 16:34:26 christos Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: max6900.c,v 1.15 2014/11/20 16:34:26 christos Exp $");

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
#include <dev/i2c/max6900reg.h>

struct maxrtc_softc {
	device_t sc_dev;
	i2c_tag_t sc_tag;
	int sc_address;
	int sc_open;
	struct todr_chip_handle sc_todr;
};

static int  maxrtc_match(device_t, cfdata_t, void *);
static void maxrtc_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(maxrtc, sizeof(struct maxrtc_softc),
	maxrtc_match, maxrtc_attach, NULL, NULL);
extern struct cfdriver maxrtc_cd;

dev_type_open(maxrtc_open);
dev_type_close(maxrtc_close);
dev_type_read(maxrtc_read);
dev_type_write(maxrtc_write);

const struct cdevsw maxrtc_cdevsw = {
	.d_open = maxrtc_open,
	.d_close = maxrtc_close,
	.d_read = maxrtc_read,
	.d_write = maxrtc_write,
	.d_ioctl = noioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

static int maxrtc_clock_read(struct maxrtc_softc *, struct clock_ymdhms *);
static int maxrtc_clock_write(struct maxrtc_softc *, struct clock_ymdhms *);
static int maxrtc_gettime(struct todr_chip_handle *, struct timeval *);
static int maxrtc_settime(struct todr_chip_handle *, struct timeval *);

int
maxrtc_match(device_t parent, cfdata_t cf, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if ((ia->ia_addr & MAX6900_ADDRMASK) == MAX6900_ADDR)
		return (1);

	return (0);
}

void
maxrtc_attach(device_t parent, device_t self, void *aux)
{
	struct maxrtc_softc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;

	sc->sc_tag = ia->ia_tag;
	sc->sc_address = ia->ia_addr;
	sc->sc_dev = self;

	aprint_naive(": Real-time Clock/NVRAM\n");
	aprint_normal(": MAX6900 Real-time Clock/NVRAM\n");

	sc->sc_open = 0;

	sc->sc_todr.cookie = sc;
	sc->sc_todr.todr_gettime = maxrtc_gettime;
	sc->sc_todr.todr_settime = maxrtc_settime;
	sc->sc_todr.todr_setwen = NULL;

	todr_attach(&sc->sc_todr);
}

/*ARGSUSED*/
int
maxrtc_open(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct maxrtc_softc *sc;

	if ((sc = device_lookup_private(&maxrtc_cd, minor(dev))) == NULL)
		return (ENXIO);

	/* XXX: Locking */

	if (sc->sc_open)
		return (EBUSY);

	sc->sc_open = 1;
	return (0);
}

/*ARGSUSED*/
int
maxrtc_close(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct maxrtc_softc *sc;

	if ((sc = device_lookup_private(&maxrtc_cd, minor(dev))) == NULL)
		return (ENXIO);

	sc->sc_open = 0;
	return (0);
}

/*ARGSUSED*/
int
maxrtc_read(dev_t dev, struct uio *uio, int flags)
{
	struct maxrtc_softc *sc;
	u_int8_t ch, cmdbuf[1];
	int a, error;

	if ((sc = device_lookup_private(&maxrtc_cd, minor(dev))) == NULL)
		return (ENXIO);

	if (uio->uio_offset >= MAX6900_RAM_BYTES)
		return (EINVAL);

	if ((error = iic_acquire_bus(sc->sc_tag, 0)) != 0)
		return (error);

	while (uio->uio_resid && uio->uio_offset < MAX6900_RAM_BYTES) {
		a = (int)uio->uio_offset;
		cmdbuf[0] = MAX6900_REG_RAM(a) | MAX6900_CMD_READ;
		if ((error = iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
				      sc->sc_address, cmdbuf, 1,
				      &ch, 1, 0)) != 0) {
			iic_release_bus(sc->sc_tag, 0);
			aprint_error_dev(sc->sc_dev,
			    "maxrtc_read: read failed at 0x%x\n", a);
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
maxrtc_write(dev_t dev, struct uio *uio, int flags)
{
	struct maxrtc_softc *sc;
	u_int8_t cmdbuf[2];
	int a, error, sverror;

	if ((sc = device_lookup_private(&maxrtc_cd, minor(dev))) == NULL)
		return (ENXIO);

	if (uio->uio_offset >= MAX6900_RAM_BYTES)
		return (EINVAL);

	if ((error = iic_acquire_bus(sc->sc_tag, 0)) != 0)
		return (error);

	/* Start by clearing the control register's write-protect bit. */
	cmdbuf[0] = MAX6900_REG_CONTROL | MAX6900_CMD_WRITE;
	cmdbuf[1] = 0;

	if ((error = iic_exec(sc->sc_tag, I2C_OP_WRITE, sc->sc_address,
			      cmdbuf, 1, &cmdbuf[1], 1, 0)) != 0) {
		iic_release_bus(sc->sc_tag, 0);
		aprint_error_dev(sc->sc_dev,
		    "maxrtc_write: failed to clear WP bit\n");
		return (error);
	}

	while (uio->uio_resid && uio->uio_offset < MAX6900_RAM_BYTES) {
		a = (int)uio->uio_offset;

		cmdbuf[0] = MAX6900_REG_RAM(a) | MAX6900_CMD_WRITE;
		if ((error = uiomove(&cmdbuf[1], 1, uio)) != 0)
			break;

		if ((error = iic_exec(sc->sc_tag, I2C_OP_WRITE, sc->sc_address,
				      cmdbuf, 1, &cmdbuf[1], 1, 0)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "maxrtc_write: write failed at 0x%x\n", a);
			break;
		}
	}

	/* Set the write-protect bit again. */
	cmdbuf[0] = MAX6900_REG_CONTROL | MAX6900_CMD_WRITE;
	cmdbuf[1] = MAX6900_CONTROL_WP;

	sverror = error;
	if ((error = iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP,
			      sc->sc_address, cmdbuf, 1,
			      &cmdbuf[1], 1, 0)) != 0) {
		if (sverror != 0)
			error = sverror;
		aprint_error_dev(sc->sc_dev,
		    "maxrtc_write: failed to set WP bit\n");
	}

	iic_release_bus(sc->sc_tag, 0);

	return (error);
}

static int
maxrtc_gettime(struct todr_chip_handle *ch, struct timeval *tv)
{
	struct maxrtc_softc *sc = ch->cookie;
	struct clock_ymdhms dt;

	if (maxrtc_clock_read(sc, &dt) == 0)
		return (-1);

	tv->tv_sec = clock_ymdhms_to_secs(&dt);
	tv->tv_usec = 0;

	return (0);
}

static int
maxrtc_settime(struct todr_chip_handle *ch, struct timeval *tv)
{
	struct maxrtc_softc *sc = ch->cookie;
	struct clock_ymdhms dt;

	clock_secs_to_ymdhms(tv->tv_sec, &dt);

	if (maxrtc_clock_write(sc, &dt) == 0)
		return (-1);

	return (0);
}

/*
 * While the MAX6900 has a nice Clock Burst Read/Write command,
 * we can't use it, since some I2C controllers do not support
 * anything other than single-byte transfers.
 */
static int max6900_rtc_offset[] = {
	MAX6900_REG_SECOND,
	MAX6900_REG_MINUTE,
	MAX6900_REG_HOUR,
	MAX6900_REG_DATE,
	MAX6900_REG_MONTH,
	MAX6900_REG_DAY,
	MAX6900_REG_YEAR,
	MAX6900_REG_CENTURY,	/* control, if burst */
};

static int
maxrtc_clock_read(struct maxrtc_softc *sc, struct clock_ymdhms *dt)
{
	u_int8_t bcd[MAX6900_BURST_LEN], cmdbuf[1];
	int i;

	if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL)) {
		aprint_error_dev(sc->sc_dev,
		    "maxrtc_clock_read: failed to acquire I2C bus\n");
		return (0);
	}

	/* Read each timekeeping register in order. */
	for (i = 0; i < MAX6900_BURST_LEN; i++) {
		cmdbuf[0] = max6900_rtc_offset[i] | MAX6900_CMD_READ;

		if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
			     sc->sc_address, cmdbuf, 1,
			     &bcd[i], 1, I2C_F_POLL)) {
			iic_release_bus(sc->sc_tag, I2C_F_POLL);
			aprint_error_dev(sc->sc_dev,
			    "maxrtc_clock_read: failed to read rtc "
			    "at 0x%x\n",
			    max6900_rtc_offset[i]);
			return (0);
		}
	}

	/* Done with I2C */
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	/*
	 * Convert the MAX6900's register values into something useable
	 */
	dt->dt_sec = bcdtobin(bcd[MAX6900_BURST_SECOND] & MAX6900_SECOND_MASK);
	dt->dt_min = bcdtobin(bcd[MAX6900_BURST_MINUTE] & MAX6900_MINUTE_MASK);

	if (bcd[MAX6900_BURST_HOUR] & MAX6900_HOUR_12HRS) {
		dt->dt_hour = bcdtobin(bcd[MAX6900_BURST_HOUR] &
		    MAX6900_HOUR_12MASK);
		if (bcd[MAX6900_BURST_HOUR] & MAX6900_HOUR_12HRS_PM)
			dt->dt_hour += 12;
	} else {
		dt->dt_hour = bcdtobin(bcd[MAX6900_BURST_HOUR] &
		    MAX6900_HOUR_24MASK);
	}

	dt->dt_day = bcdtobin(bcd[MAX6900_BURST_DATE] & MAX6900_DATE_MASK);
	dt->dt_mon = bcdtobin(bcd[MAX6900_BURST_MONTH] & MAX6900_MONTH_MASK);
	dt->dt_year = bcdtobin(bcd[MAX6900_BURST_YEAR]);
		/* century in the burst control slot */
	dt->dt_year += (int)bcdtobin(bcd[MAX6900_BURST_CONTROL]) * 100;

	return (1);
}

static int
maxrtc_clock_write(struct maxrtc_softc *sc, struct clock_ymdhms *dt)
{
	uint8_t bcd[MAX6900_BURST_LEN], cmdbuf[2];
	uint8_t init_seconds, final_seconds;
	int i;

	/*
	 * Convert our time representation into something the MAX6900
	 * can understand.
	 */
	bcd[MAX6900_BURST_SECOND] = bintobcd(dt->dt_sec);
	bcd[MAX6900_BURST_MINUTE] = bintobcd(dt->dt_min);
	bcd[MAX6900_BURST_HOUR] = bintobcd(dt->dt_hour) & MAX6900_HOUR_24MASK;
	bcd[MAX6900_BURST_DATE] = bintobcd(dt->dt_day);
	bcd[MAX6900_BURST_WDAY] = bintobcd(dt->dt_wday);
	bcd[MAX6900_BURST_MONTH] = bintobcd(dt->dt_mon);
	bcd[MAX6900_BURST_YEAR] = bintobcd(dt->dt_year % 100);
		/* century in control slot */
	bcd[MAX6900_BURST_CONTROL] = bintobcd(dt->dt_year / 100);

	if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL)) {
		aprint_error_dev(sc->sc_dev,
		    "maxrtc_clock_write: failed to acquire I2C bus\n");
		return (0);
	}

	/* Start by clearing the control register's write-protect bit. */
	cmdbuf[0] = MAX6900_REG_CONTROL | MAX6900_CMD_WRITE;
	cmdbuf[1] = 0;

	if (iic_exec(sc->sc_tag, I2C_OP_WRITE, sc->sc_address,
		     cmdbuf, 1, &cmdbuf[1], 1, I2C_F_POLL)) {
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		aprint_error_dev(sc->sc_dev,
		    "maxrtc_clock_write: failed to clear WP bit\n");
		return (0);
	}

	/*
	 * The MAX6900 RTC manual recommends ensuring "atomicity" of
	 * a non-burst write by:
	 *
	 *	- writing SECONDS
	 *	- reading back SECONDS, remembering it as "initial seconds"
	 *	- write the remaing RTC registers
	 *	- read back SECONDS as "final seconds"
	 *	- if "initial seconds" == 59, ensure "final seconds" == 59
	 *	- else, ensure "final seconds" is no more than one second
	 *	  beyond "initial seconds".
	 */
 again:
	cmdbuf[0] = MAX6900_REG_SECOND | MAX6900_CMD_WRITE;
	if (iic_exec(sc->sc_tag, I2C_OP_WRITE, sc->sc_address,
		     cmdbuf, 1, &bcd[MAX6900_BURST_SECOND], 1, I2C_F_POLL)) {
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		aprint_error_dev(sc->sc_dev,
		    "maxrtc_clock_write: failed to write SECONDS\n");
		return (0);
	}

	cmdbuf[0] = MAX6900_REG_SECOND | MAX6900_CMD_READ;
	if (iic_exec(sc->sc_tag, I2C_OP_READ, sc->sc_address,
		     cmdbuf, 1, &init_seconds, 1, I2C_F_POLL)) {
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		aprint_error_dev(sc->sc_dev,
		    "maxrtc_clock_write: failed to read "
		    "INITIAL SECONDS\n");
		return (0);
	}

	for (i = 1; i < MAX6900_BURST_LEN; i++) {
		cmdbuf[0] = max6900_rtc_offset[i] | MAX6900_CMD_WRITE;
		if (iic_exec(sc->sc_tag,
			     i != MAX6900_BURST_LEN - 1 ? I2C_OP_WRITE :
			     I2C_OP_WRITE_WITH_STOP, sc->sc_address,
			     cmdbuf, 1, &bcd[i], 1, I2C_F_POLL)) {
			iic_release_bus(sc->sc_tag, I2C_F_POLL);
			aprint_error_dev(sc->sc_dev,
			    "maxrtc_clock_write: failed to write rtc "
			    " at 0x%x\n",
			    max6900_rtc_offset[i]);
			return (0);
		}
	}

	cmdbuf[0] = MAX6900_REG_SECOND | MAX6900_CMD_READ;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_address,
		     cmdbuf, 1, &final_seconds, 1, I2C_F_POLL)) {
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		aprint_error_dev(sc->sc_dev,
		    "maxrtc_clock_write: failed to read "
		    "FINAL SECONDS\n");
		return (0);
	}

	if ((init_seconds == 59 && final_seconds != 59) ||
	    (init_seconds != 59 && final_seconds != init_seconds + 1)) {
#if 1
		printf("%s: maxrtc_clock_write: init %d, final %d, try again\n",
		    device_xname(sc->sc_dev), init_seconds, final_seconds);
#endif
		goto again;
	}

	/* Finish by setting the control register's write-protect bit. */
	cmdbuf[0] = MAX6900_REG_CONTROL | MAX6900_CMD_WRITE;
	cmdbuf[1] = MAX6900_CONTROL_WP;

	if (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP, sc->sc_address,
		     cmdbuf, 1, &cmdbuf[1], 1, I2C_F_POLL)) {
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		aprint_error_dev(sc->sc_dev,
		    "maxrtc_clock_write: failed to set WP bit\n");
		return (0);
	}

	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	return (1);
}
