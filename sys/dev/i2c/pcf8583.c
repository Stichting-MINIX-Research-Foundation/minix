/*	$NetBSD: pcf8583.c,v 1.16 2014/11/20 16:34:26 christos Exp $	*/

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

/*
 * Driver for the Philips PCF8583 Real Time Clock.
 *
 * This driver is partially derived from Ben Harris's PCF8583 driver
 * for NetBSD/acorn26.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pcf8583.c,v 1.16 2014/11/20 16:34:26 christos Exp $");

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
#include <dev/i2c/pcf8583reg.h>
#include <dev/i2c/pcf8583var.h>

struct pcfrtc_softc {
	device_t sc_dev;
	i2c_tag_t sc_tag;
	int sc_address;
	int sc_open;
	struct todr_chip_handle sc_todr;
};

static int  pcfrtc_match(device_t, cfdata_t, void *);
static void pcfrtc_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(pcfrtc, sizeof(struct pcfrtc_softc),
	pcfrtc_match, pcfrtc_attach, NULL, NULL);
extern struct cfdriver pcfrtc_cd;

dev_type_open(pcfrtc_open);
dev_type_close(pcfrtc_close);
dev_type_read(pcfrtc_read);
dev_type_write(pcfrtc_write);

const struct cdevsw pcfrtc_cdevsw = {
	.d_open = pcfrtc_open,
	.d_close = pcfrtc_close,
	.d_read = pcfrtc_read,
	.d_write = pcfrtc_write,
	.d_ioctl = noioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

static int pcfrtc_clock_read(struct pcfrtc_softc *, struct clock_ymdhms *,
			     uint8_t *);
static int pcfrtc_clock_write(struct pcfrtc_softc *, struct clock_ymdhms *,
			      uint8_t);
static int pcfrtc_gettime(struct todr_chip_handle *, struct timeval *);
static int pcfrtc_settime(struct todr_chip_handle *, struct timeval *);

int
pcfrtc_match(device_t parent, cfdata_t cf, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if ((ia->ia_addr & PCF8583_ADDRMASK) == PCF8583_ADDR)
		return (1);

	return (0);
}

void
pcfrtc_attach(device_t parent, device_t self, void *aux)
{
	struct pcfrtc_softc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;
	uint8_t cmdbuf[1], csr;

	sc->sc_tag = ia->ia_tag;
	sc->sc_address = ia->ia_addr;
	sc->sc_dev = self;

	aprint_naive(": Real-time Clock/NVRAM\n");
	aprint_normal(": PCF8583 Real-time Clock/NVRAM\n");

	cmdbuf[0] = PCF8583_REG_CSR;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_address,
	    cmdbuf, 1, &csr, 1, 0) != 0) {
		aprint_error_dev(self, "unable to read CSR\n");
		return;
	}
	aprint_normal_dev(sc->sc_dev, "");
	switch (csr & PCF8583_CSR_FN_MASK) {
	case PCF8583_CSR_FN_32768HZ:
		aprint_normal(" 32.768 kHz clock");
		break;

	case PCF8583_CSR_FN_50HZ:
		aprint_normal(" 50 Hz clock");
		break;

	case PCF8583_CSR_FN_EVENT:
		aprint_normal(" event counter");
		break;

	case PCF8583_CSR_FN_TEST:
		aprint_normal(" test mode");
		break;
	}
	if (csr & PCF8583_CSR_STOP)
		aprint_normal(", stopped");
	if (csr & PCF8583_CSR_ALARMENABLE)
		aprint_normal(", alarm enabled");
	aprint_normal("\n");

	sc->sc_open = 0;

	sc->sc_todr.cookie = sc;
	sc->sc_todr.todr_gettime = pcfrtc_gettime;
	sc->sc_todr.todr_settime = pcfrtc_settime;
	sc->sc_todr.todr_setwen = NULL;

	todr_attach(&sc->sc_todr);
}

/*ARGSUSED*/
int
pcfrtc_open(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct pcfrtc_softc *sc;

	if ((sc = device_lookup_private(&pcfrtc_cd, minor(dev))) == NULL)
		return (ENXIO);

	/* XXX: Locking */

	if (sc->sc_open)
		return (EBUSY);

	sc->sc_open = 1;
	return (0);
}

/*ARGSUSED*/
int
pcfrtc_close(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct pcfrtc_softc *sc;

	if ((sc = device_lookup_private(&pcfrtc_cd, minor(dev))) == NULL)
		return (ENXIO);

	sc->sc_open = 0;
	return (0);
}

/*ARGSUSED*/
int
pcfrtc_read(dev_t dev, struct uio *uio, int flags)
{
	struct pcfrtc_softc *sc;
	u_int8_t ch, cmdbuf[1];
	int a, error;

	if ((sc = device_lookup_private(&pcfrtc_cd, minor(dev))) == NULL)
		return (ENXIO);

	if (uio->uio_offset >= PCF8583_NVRAM_SIZE)
		return (EINVAL);

	if ((error = iic_acquire_bus(sc->sc_tag, 0)) != 0)
		return (error);

	while (uio->uio_resid && uio->uio_offset < PCF8583_NVRAM_SIZE) {
		a = (int)uio->uio_offset;
		cmdbuf[0] = a + PCF8583_NVRAM_START;
		if ((error = iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
				      sc->sc_address, cmdbuf, 1,
				      &ch, 1, 0)) != 0) {
			iic_release_bus(sc->sc_tag, 0);
			aprint_error_dev(sc->sc_dev,
			    "pcfrtc_read: read failed at 0x%x\n", a);
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
pcfrtc_write(dev_t dev, struct uio *uio, int flags)
{
	struct pcfrtc_softc *sc;
	u_int8_t cmdbuf[2];
	int a, error;

	if ((sc = device_lookup_private(&pcfrtc_cd, minor(dev))) == NULL)
		return (ENXIO);

	if (uio->uio_offset >= PCF8583_NVRAM_SIZE)
		return (EINVAL);

	if ((error = iic_acquire_bus(sc->sc_tag, 0)) != 0)
		return (error);

	while (uio->uio_resid && uio->uio_offset < PCF8583_NVRAM_SIZE) {
		a = (int)uio->uio_offset;
		cmdbuf[0] = a + PCF8583_NVRAM_START;
		if ((error = uiomove(&cmdbuf[1], 1, uio)) != 0)
			break;

		if ((error = iic_exec(sc->sc_tag,
		    uio->uio_resid ? I2C_OP_WRITE : I2C_OP_WRITE_WITH_STOP,
		    sc->sc_address, cmdbuf, 1, &cmdbuf[1], 1, 0)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "pcfrtc_write: write failed at 0x%x\n", a);
			return (error);
		}
	}

	iic_release_bus(sc->sc_tag, 0);

	return (error);
}

static int
pcfrtc_gettime(struct todr_chip_handle *ch, struct timeval *tv)
{
	struct pcfrtc_softc *sc = ch->cookie;
	struct clock_ymdhms dt;
	int err;
	uint8_t centi;

	if ((err = pcfrtc_clock_read(sc, &dt, &centi)))
		return err;

	tv->tv_sec = clock_ymdhms_to_secs(&dt);
	tv->tv_usec = centi * 10000;

	return (0);
}

static int
pcfrtc_settime(struct todr_chip_handle *ch, struct timeval *tv)
{
	struct pcfrtc_softc *sc = ch->cookie;
	struct clock_ymdhms dt;
	int err;

	clock_secs_to_ymdhms(tv->tv_sec, &dt);

	if ((err = pcfrtc_clock_write(sc, &dt, tv->tv_usec / 10000)) != 0)
		return err;

	return (0);
}

static const int pcf8583_rtc_offset[] = {
	PCF8583_REG_CSR,
	PCF8583_REG_CENTI,
	PCF8583_REG_SEC,
	PCF8583_REG_MIN,
	PCF8583_REG_HOUR,
	PCF8583_REG_YEARDATE,
	PCF8583_REG_WKDYMON,
	PCF8583_REG_TIMER,
	0xc0,			/* NVRAM -- year stored here */
	0xc1,			/* NVRAM -- century stored here */
};

static int
pcfrtc_clock_read(struct pcfrtc_softc *sc, struct clock_ymdhms *dt,
    uint8_t *centi)
{
	u_int8_t bcd[10], cmdbuf[1];
	int i, err;

	if ((err = iic_acquire_bus(sc->sc_tag, I2C_F_POLL))) {
		aprint_error_dev(sc->sc_dev,
		    "pcfrtc_clock_read: failed to acquire I2C bus\n");
		return err;
	}

	/* Read each timekeeping register in order. */
	for (i = 0; i < 10; i++) {
		cmdbuf[0] = pcf8583_rtc_offset[i];

		if ((err = iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
			     sc->sc_address, cmdbuf, 1,
			     &bcd[i], 1, I2C_F_POLL))) {
			iic_release_bus(sc->sc_tag, I2C_F_POLL);
			aprint_error_dev(sc->sc_dev,
			    "pcfrtc_clock_read: failed to read rtc "
			    "at 0x%x\n",
			    pcf8583_rtc_offset[i]);
			return err;
		}
	}

	/* Done with I2C */
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	/*
	 * Convert the PCF8583's register values into something useable
	 */
	*centi      = bcdtobin(bcd[PCF8583_REG_CENTI]);
	dt->dt_sec  = bcdtobin(bcd[PCF8583_REG_SEC]);
	dt->dt_min  = bcdtobin(bcd[PCF8583_REG_MIN]);
	dt->dt_hour = bcdtobin(bcd[PCF8583_REG_HOUR] & PCF8583_HOUR_MASK);
	if (bcd[PCF8583_REG_HOUR] & PCF8583_HOUR_12H) {
		dt->dt_hour %= 12;	/* 12AM -> 0, 12PM -> 12 */
		if (bcd[PCF8583_REG_HOUR] & PCF8583_HOUR_PM)
			dt->dt_hour += 12;
	}

	dt->dt_day = bcdtobin(bcd[PCF8583_REG_YEARDATE] & PCF8583_DATE_MASK);
	dt->dt_mon = bcdtobin(bcd[PCF8583_REG_WKDYMON] & PCF8583_MON_MASK);

	dt->dt_year = bcd[8] + (bcd[9] * 100);
	/* Try to notice if the year's rolled over. */
	if (bcd[PCF8583_REG_CSR] & PCF8583_CSR_MASK)
		aprint_error_dev(sc->sc_dev,
		    "cannot check year in mask mode\n");
	else {
		while (dt->dt_year % 4 !=
		       (bcd[PCF8583_REG_YEARDATE] &
			PCF8583_YEAR_MASK) >> PCF8583_YEAR_SHIFT)
			dt->dt_year++;
	}

	return 0;
}

static int
pcfrtc_clock_write(struct pcfrtc_softc *sc, struct clock_ymdhms *dt,
    uint8_t centi)
{
	uint8_t bcd[10], cmdbuf[2];
	int i, err;

	/*
	 * Convert our time representation into something the PCF8583
	 * can understand.
	 */
	bcd[PCF8583_REG_CENTI]    = centi;
	bcd[PCF8583_REG_SEC]      = bintobcd(dt->dt_sec);
	bcd[PCF8583_REG_MIN]      = bintobcd(dt->dt_min);
	bcd[PCF8583_REG_HOUR]     = bintobcd(dt->dt_hour) & PCF8583_HOUR_MASK;
	bcd[PCF8583_REG_YEARDATE] = bintobcd(dt->dt_day) |
	    ((dt->dt_year % 4) << PCF8583_YEAR_SHIFT);
	bcd[PCF8583_REG_WKDYMON]  = bintobcd(dt->dt_mon) |
	    ((dt->dt_wday % 4) << PCF8583_WKDY_SHIFT);
	bcd[8]                    = dt->dt_year % 100;
	bcd[9]                    = dt->dt_year / 100;

	if ((err = iic_acquire_bus(sc->sc_tag, I2C_F_POLL))) {
		aprint_error_dev(sc->sc_dev,
		    "pcfrtc_clock_write: failed to acquire I2C bus\n");
		return err;
	}

	for (i = 1; i < 10; i++) {
		cmdbuf[0] = pcf8583_rtc_offset[i];
		if ((err = iic_exec(sc->sc_tag,
			     i != 9 ? I2C_OP_WRITE : I2C_OP_WRITE_WITH_STOP,
			     sc->sc_address, cmdbuf, 1,
			     &bcd[i], 1, I2C_F_POLL))) {
			iic_release_bus(sc->sc_tag, I2C_F_POLL);
			aprint_error_dev(sc->sc_dev,
			    "pcfrtc_clock_write: failed to write rtc "
			    " at 0x%x\n",
			    pcf8583_rtc_offset[i]);
			return err;
		}
	}

	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	return 0;
}

int
pcfrtc_bootstrap_read(i2c_tag_t tag, int i2caddr, int offset,
    u_int8_t *rvp, size_t len)
{
	u_int8_t cmdbuf[1];

	/*
	 * NOTE: "offset" is an absolute offset into the PCF8583
	 * address space, not relative to the NVRAM.
	 */

	if (len == 0)
		return (0);

	if (iic_acquire_bus(tag, I2C_F_POLL) != 0)
		return (-1);

	while (len) {
		/* Read a single byte. */
		cmdbuf[0] = offset;
		if (iic_exec(tag, I2C_OP_READ_WITH_STOP, i2caddr,
			     cmdbuf, 1, rvp, 1, I2C_F_POLL)) {
			iic_release_bus(tag, I2C_F_POLL);
			return (-1);
		}

		len--;
		rvp++;
		offset++;
	}

	iic_release_bus(tag, I2C_F_POLL);
	return (0);
}

int
pcfrtc_bootstrap_write(i2c_tag_t tag, int i2caddr, int offset,
    u_int8_t *rvp, size_t len)
{
	u_int8_t cmdbuf[1];

	/*
	 * NOTE: "offset" is an absolute offset into the PCF8583
	 * address space, not relative to the NVRAM.
	 */

	if (len == 0)
		return (0);

	if (iic_acquire_bus(tag, I2C_F_POLL) != 0)
		return (-1);

	while (len) {
		/* Write a single byte. */
		cmdbuf[0] = offset;
		if (iic_exec(tag, I2C_OP_WRITE_WITH_STOP, i2caddr,
			     cmdbuf, 1, rvp, 1, I2C_F_POLL)) {
			iic_release_bus(tag, I2C_F_POLL);
			return (-1);
		}

		len--;
		rvp++;
		offset++;
	}

	iic_release_bus(tag, I2C_F_POLL);
	return (0);
}
