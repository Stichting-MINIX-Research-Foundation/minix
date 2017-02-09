/*	$NetBSD: x1226.c,v 1.19 2014/11/20 16:34:26 christos Exp $	*/

/*
 * Copyright (c) 2003 Shigeyuki Fukushima.
 * All rights reserved.
 *
 * Written by Shigeyuki Fukushima for the NetBSD Project.
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
 *      Shigeyuki Fukushima.
 * 4. The name of Shigeyuki Fukushima may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SHIGEYUKI FUKUSHIMA ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL SHIGEYUKI FUKUSHIMA
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: x1226.c,v 1.19 2014/11/20 16:34:26 christos Exp $");

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
#include <dev/i2c/x1226reg.h>

struct xrtc_softc {
	device_t		sc_dev;
	i2c_tag_t		sc_tag;
	int			sc_address;
	int			sc_open;
	struct todr_chip_handle	sc_todr;
};

static void	xrtc_attach(device_t, device_t, void *);
static int	xrtc_match(device_t, cfdata_t, void *);

CFATTACH_DECL_NEW(xrtc, sizeof(struct xrtc_softc),
    xrtc_match, xrtc_attach, NULL, NULL);
extern struct cfdriver xrtc_cd;

dev_type_open(xrtc_open);
dev_type_close(xrtc_close);
dev_type_read(xrtc_read);
dev_type_write(xrtc_write);

const struct cdevsw xrtc_cdevsw = {
	.d_open = xrtc_open,
	.d_close = xrtc_close,
	.d_read = xrtc_read,
	.d_write = xrtc_write,
	.d_ioctl = noioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

static int xrtc_clock_read(struct xrtc_softc *, struct clock_ymdhms *);
static int xrtc_clock_write(struct xrtc_softc *, struct clock_ymdhms *);
static int xrtc_gettime(struct todr_chip_handle *, struct timeval *);
static int xrtc_settime(struct todr_chip_handle *, struct timeval *);

/*
 * xrtc_match()
 */
static int
xrtc_match(device_t parent, cfdata_t cf, void *arg)
{
	struct i2c_attach_args *ia = arg;

	/* match only this RTC devices */
	if (ia->ia_addr == X1226_ADDR)
		return (1);

	return (0);
}

/*
 * xrtc_attach()
 */
static void
xrtc_attach(device_t parent, device_t self, void *arg)
{
	struct xrtc_softc *sc = device_private(self);
	struct i2c_attach_args *ia = arg;

	aprint_naive(": Real-time Clock/NVRAM\n");
	aprint_normal(": Xicor X1226 Real-time Clock/NVRAM\n");

	sc->sc_tag = ia->ia_tag;
	sc->sc_address = ia->ia_addr;
	sc->sc_dev = self;
	sc->sc_open = 0;
	sc->sc_todr.cookie = sc;
	sc->sc_todr.todr_gettime = xrtc_gettime;
	sc->sc_todr.todr_settime = xrtc_settime;
	sc->sc_todr.todr_setwen = NULL;

	todr_attach(&sc->sc_todr);
}


/*ARGSUSED*/
int
xrtc_open(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct xrtc_softc *sc;

	if ((sc = device_lookup_private(&xrtc_cd, minor(dev))) == NULL)
		return (ENXIO);

	/* XXX: Locking */

	if (sc->sc_open)
		return (EBUSY);

	sc->sc_open = 1;
	return (0);
}

/*ARGSUSED*/
int
xrtc_close(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct xrtc_softc *sc;

	if ((sc = device_lookup_private(&xrtc_cd, minor(dev))) == NULL)
		return (ENXIO);

	sc->sc_open = 0;
	return (0);
}

/*ARGSUSED*/
int
xrtc_read(dev_t dev, struct uio *uio, int flags)
{
	struct xrtc_softc *sc;
	u_int8_t ch, cmdbuf[2];
	int addr, error;

	if ((sc = device_lookup_private(&xrtc_cd, minor(dev))) == NULL)
		return (ENXIO);

	if (uio->uio_offset >= X1226_NVRAM_SIZE)
		return (EINVAL);

	if ((error = iic_acquire_bus(sc->sc_tag, 0)) != 0)
		return (error);

	while (uio->uio_resid && uio->uio_offset < X1226_NVRAM_SIZE) {
		addr = (int)uio->uio_offset + X1226_NVRAM_START;
		cmdbuf[0] = (addr >> 8) & 0xff;
		cmdbuf[1] = addr & 0xff;
		if ((error = iic_exec(sc->sc_tag,
			I2C_OP_READ_WITH_STOP,
			sc->sc_address, cmdbuf, 2, &ch, 1, 0)) != 0) {
			iic_release_bus(sc->sc_tag, 0);
			aprint_error_dev(sc->sc_dev,
			    "xrtc_read: read failed at 0x%x\n",
				(int)uio->uio_offset);
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
xrtc_write(dev_t dev, struct uio *uio, int flags)
{
	struct xrtc_softc *sc;
	u_int8_t cmdbuf[3];
	int addr, error;

	if ((sc = device_lookup_private(&xrtc_cd, minor(dev))) == NULL)
		return (ENXIO);

	if (uio->uio_offset >= X1226_NVRAM_SIZE)
		return (EINVAL);

	if ((error = iic_acquire_bus(sc->sc_tag, 0)) != 0)
		return (error);

	while (uio->uio_resid && uio->uio_offset < X1226_NVRAM_SIZE) {
		addr = (int)uio->uio_offset + X1226_NVRAM_START;
		cmdbuf[0] = (addr >> 8) & 0xff;
		cmdbuf[1] = addr & 0xff;
		if ((error = uiomove(&cmdbuf[2], 1, uio)) != 0) {
			break;
		}
		if ((error = iic_exec(sc->sc_tag,
			uio->uio_resid ? I2C_OP_WRITE : I2C_OP_WRITE_WITH_STOP,
			sc->sc_address, cmdbuf, 2, &cmdbuf[2], 1, 0)) != 0) {
			iic_release_bus(sc->sc_tag, 0);
			aprint_error_dev(sc->sc_dev,
			    "xrtc_write: write failed at 0x%x\n",
				(int)uio->uio_offset);
			return (error);
		}
	}

	iic_release_bus(sc->sc_tag, 0);

	return (0);
}


static int
xrtc_gettime(struct todr_chip_handle *ch, struct timeval *tv)
{
	struct xrtc_softc *sc = ch->cookie;
	struct clock_ymdhms dt, check;
	int retries;

	memset(&dt, 0, sizeof(dt));
	memset(&check, 0, sizeof(check));

	retries = 5;
	do {
		xrtc_clock_read(sc, &dt);
		xrtc_clock_read(sc, &check);
	} while (memcmp(&dt, &check, sizeof(check)) != 0 && --retries);

	tv->tv_sec = clock_ymdhms_to_secs(&dt);
	tv->tv_usec = 0;

	return (0);
}

static int
xrtc_settime(struct todr_chip_handle *ch, struct timeval *tv)
{
	struct xrtc_softc *sc = ch->cookie;
	struct clock_ymdhms dt;

	clock_secs_to_ymdhms(tv->tv_sec, &dt);

	if (xrtc_clock_write(sc, &dt) == 0)
		return (-1);

	return (0);
}

static int
xrtc_clock_read(struct xrtc_softc *sc, struct clock_ymdhms *dt)
{
	int i = 0;
	u_int8_t bcd[X1226_REG_RTC_SIZE], cmdbuf[2];

	if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL)) {
		aprint_error_dev(sc->sc_dev,
		    "xrtc_clock_read: failed to acquire I2C bus\n");
		return (0);
	}

	/* Read each RTC register in order */
	for (i = 0 ; i < X1226_REG_RTC_SIZE ; i++) {
		int addr = i + X1226_REG_RTC_BASE;
		cmdbuf[0] = (addr >> 8) & 0xff;
		cmdbuf[1] = addr & 0xff;

		if (iic_exec(sc->sc_tag,
			I2C_OP_READ_WITH_STOP,
			sc->sc_address, cmdbuf, 2,
			&bcd[i], 1, I2C_F_POLL)) {
			iic_release_bus(sc->sc_tag, I2C_F_POLL);
			aprint_error_dev(sc->sc_dev,
			    "xrtc_clock_read: failed to read rtc "
				"at 0x%x\n", i);
			return (0);
		}
	}

	/* Done with I2C */
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	/*
	 * Convert the X1226's register bcd values
	 */
	dt->dt_sec = bcdtobin(bcd[X1226_REG_SC - X1226_REG_RTC_BASE]
			& X1226_REG_SC_MASK);
	dt->dt_min = bcdtobin(bcd[X1226_REG_MN - X1226_REG_RTC_BASE]
			& X1226_REG_MN_MASK);
	if (!(bcd[X1226_REG_HR - X1226_REG_RTC_BASE] & X1226_FLAG_HR_24H)) {
		dt->dt_hour = bcdtobin(bcd[X1226_REG_HR - X1226_REG_RTC_BASE]
				& X1226_REG_HR12_MASK);
		if (bcd[X1226_REG_HR - X1226_REG_RTC_BASE] & X1226_FLAG_HR_12HPM) {
			dt->dt_hour += 12;
		}
	} else {
		dt->dt_hour = bcdtobin(bcd[X1226_REG_HR - X1226_REG_RTC_BASE]
			& X1226_REG_HR24_MASK);
	}
	dt->dt_wday = bcdtobin(bcd[X1226_REG_DW - X1226_REG_RTC_BASE]
			& X1226_REG_DT_MASK);
	dt->dt_day = bcdtobin(bcd[X1226_REG_DT - X1226_REG_RTC_BASE]
			& X1226_REG_DT_MASK);
	dt->dt_mon = bcdtobin(bcd[X1226_REG_MO - X1226_REG_RTC_BASE]
			& X1226_REG_MO_MASK);
	dt->dt_year = bcdtobin(bcd[X1226_REG_YR - X1226_REG_RTC_BASE]
			& X1226_REG_YR_MASK);
	dt->dt_year += bcdtobin(bcd[X1226_REG_Y2K - X1226_REG_RTC_BASE]
			& X1226_REG_Y2K_MASK) * 100;

	return (1);
}

static int
xrtc_clock_write(struct xrtc_softc *sc, struct clock_ymdhms *dt)
{
	int i = 0, addr;
	u_int8_t bcd[X1226_REG_RTC_SIZE], cmdbuf[3];

	/*
	 * Convert our time to bcd values
	 */
	bcd[X1226_REG_SC - X1226_REG_RTC_BASE] = bintobcd(dt->dt_sec);
	bcd[X1226_REG_MN - X1226_REG_RTC_BASE] = bintobcd(dt->dt_min);
	bcd[X1226_REG_HR - X1226_REG_RTC_BASE] = bintobcd(dt->dt_hour)
						| X1226_FLAG_HR_24H;
	bcd[X1226_REG_DW - X1226_REG_RTC_BASE] = bintobcd(dt->dt_wday);
	bcd[X1226_REG_DT - X1226_REG_RTC_BASE] = bintobcd(dt->dt_day);
	bcd[X1226_REG_MO - X1226_REG_RTC_BASE] = bintobcd(dt->dt_mon);
	bcd[X1226_REG_YR - X1226_REG_RTC_BASE] = bintobcd(dt->dt_year % 100);
	bcd[X1226_REG_Y2K - X1226_REG_RTC_BASE] = bintobcd(dt->dt_year / 100);

	if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL)) {
		aprint_error_dev(sc->sc_dev,
		    "xrtc_clock_write: failed to acquire I2C bus\n");
		return (0);
	}

	/* Unlock register: Write Enable Latch */
	addr = X1226_REG_SR;
	cmdbuf[0] = ((addr >> 8) & 0xff);
	cmdbuf[1] = (addr & 0xff);
	cmdbuf[2] = X1226_FLAG_SR_WEL;
	if (iic_exec(sc->sc_tag,
		I2C_OP_WRITE_WITH_STOP,
		sc->sc_address, cmdbuf, 2, &cmdbuf[2], 1, 0) != 0) {
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		aprint_error_dev(sc->sc_dev, "xrtc_clock_write: "
			"failed to write-unlock status register(WEL=1)\n");
		return (0);
	}

	/* Unlock register: Register Write Enable Latch */
	addr = X1226_REG_SR;
	cmdbuf[0] = ((addr >> 8) & 0xff);
	cmdbuf[1] = (addr & 0xff);
	cmdbuf[2] = X1226_FLAG_SR_WEL | X1226_FLAG_SR_RWEL;
	if (iic_exec(sc->sc_tag,
		I2C_OP_WRITE_WITH_STOP,
		sc->sc_address, cmdbuf, 2, &cmdbuf[2], 1, 0) != 0) {
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		aprint_error_dev(sc->sc_dev, "xrtc_clock_write: "
			"failed to write-unlock status register(RWEL=1)\n");
		return (0);
	}

	/* Write each RTC register in reverse order */
	for (i = (X1226_REG_RTC_SIZE - 1) ; i >= 0; i--) {
		addr = i + X1226_REG_RTC_BASE;
		cmdbuf[0] = ((addr >> 8) & 0xff);
		cmdbuf[1] = (addr & 0xff);
		if (iic_exec(sc->sc_tag,
			I2C_OP_WRITE_WITH_STOP,
			sc->sc_address, cmdbuf, 2,
			&bcd[i], 1, I2C_F_POLL)) {

			/* Lock register: WEL/RWEL off */
			addr = X1226_REG_SR;
			cmdbuf[0] = ((addr >> 8) & 0xff);
			cmdbuf[1] = (addr & 0xff);
			cmdbuf[2] = 0;
			iic_exec(sc->sc_tag,
				I2C_OP_WRITE_WITH_STOP,
				sc->sc_address, cmdbuf, 2,
				&cmdbuf[2], 1, 0);

			iic_release_bus(sc->sc_tag, I2C_F_POLL);
			aprint_error_dev(sc->sc_dev, "xrtc_clock_write: failed to write rtc "
				"at 0x%x\n", i);
			return (0);
		}
	}

	/* Lock register: WEL/RWEL off */
	addr = X1226_REG_SR;
	cmdbuf[0] = ((addr >> 8) & 0xff);
	cmdbuf[1] = (addr & 0xff);
	cmdbuf[2] = 0;
	if (iic_exec(sc->sc_tag,
		I2C_OP_WRITE_WITH_STOP,
		sc->sc_address, cmdbuf, 2, &cmdbuf[2], 1, 0) != 0) {
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		aprint_error_dev(sc->sc_dev, "xrtc_clock_write: "
			"failed to write-lock status register\n");
		return (0);
	}

	iic_release_bus(sc->sc_tag, I2C_F_POLL);
	return (1);
}
