/* $NetBSD: ac100.c,v 1.1 2014/12/07 14:24:11 jmcneill Exp $ */

/*-
 * Copyright (c) 2014 Jared D. McNeill <jmcneill@invisible.ca>
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
__KERNEL_RCSID(0, "$NetBSD: ac100.c,v 1.1 2014/12/07 14:24:11 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kmem.h>

#include <dev/clock_subr.h>

#include <dev/i2c/i2cvar.h>

#define AC100_CHIP_AUDIO_RST_REG	0x00
#define AC100_PLL_CTRL1_REG		0x02
#define AC100_PLL_CTRL2_REG		0x03
#define AC100_SYSCLK_CTRL_REG		0x04
#define AC100_MOD_RST_CTRL_REG		0x05
#define AC100_ADDA_SR_CTRL_REG		0x06
#define AC100_I2S1_LCK_CTRL_REG		0x10
#define AC100_I2S1_SDIN_CTRL_REG	0x11
#define AC100_I2S1_SDOUT_CTRL_REG	0x12
#define AC100_I2S1_DIG_MIXER_REG	0x13
#define AC100_I2S1_VOL_CTRL1_REG	0x14
#define AC100_I2S1_VOL_CTRL2_REG	0x15
#define AC100_I2S1_VOL_CTRL3_REG	0x16
#define AC100_I2S1_VOL_CTRL4_REG	0x17
#define AC100_I2S1_MXR_GAIN_REG		0x18
#define AC100_I2S2_CLK_CTRL_REG		0x20
#define AC100_I2S2_SDIN_CTRL_REG	0x21
#define AC100_I2S2_SDOUT_CTRL_REG	0x22
#define AC100_I2S2_DIG_MIXER_REG	0x23
#define AC100_I2S2_VOL_CTRL1_REG	0x24
#define AC100_I2S2_VOL_CTRL2_REG	0x26
#define AC100_I2S2_MXR_GAIN_REG		0x28
#define AC100_I2S3_CLK_CTRL_REG		0x30
#define AC100_I2S3_SDIN_CTRL_REG	0x31
#define AC100_I2S3_SDOUT_CTRL_REG	0x32
#define AC100_I2S3_SGP_CTRL_REG		0x33
#define AC100_ADC_DIG_CTRL_REG		0x40

#define AC100_RTC_RESET_REG		0xc6
#define AC100_RTC_CTRL_REG		0xc7
#define AC100_RTC_SEC_REG		0xc8
#define AC100_RTC_MIN_REG		0xc9
#define AC100_RTC_HOU_REG		0xca
#define AC100_RTC_WEE_REG		0xcb
#define AC100_RTC_DAY_REG		0xcc
#define AC100_RTC_MON_REG		0xcd
#define AC100_RTC_YEA_REG		0xce
#define AC100_RTC_UPD_TRIG_REG		0xcf

#define AC100_RTC_GP_REG(n)		(0xe0 + (n))

#define AC100_RTC_CTRL_12H_24H_MODE	__BIT(0)

#define AC100_RTC_UPD_TRIG_WRITE	__BIT(15)

struct ac100_softc {
	device_t	sc_dev;
	i2c_tag_t	sc_i2c;
	i2c_addr_t	sc_addr;

	struct todr_chip_handle sc_todr;
};

static int	ac100_match(device_t, cfdata_t, void *);
static void	ac100_attach(device_t, device_t, void *);

static int	ac100_rtc_gettime(todr_chip_handle_t, struct clock_ymdhms *);
static int	ac100_rtc_settime(todr_chip_handle_t, struct clock_ymdhms *);

static int	ac100_read(struct ac100_softc *, uint8_t, uint16_t *);
static int	ac100_write(struct ac100_softc *, uint8_t, uint16_t);

CFATTACH_DECL_NEW(ac100ic, sizeof(struct ac100_softc),
    ac100_match, ac100_attach, NULL, NULL);

static int
ac100_match(device_t parent, cfdata_t match, void *aux)
{
	return 1;
}

static void
ac100_attach(device_t parent, device_t self, void *aux)
{
	struct ac100_softc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;

	sc->sc_dev = self;
	sc->sc_i2c = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	aprint_naive("\n");
	aprint_normal(": CODEC/RTC\n");

	iic_acquire_bus(sc->sc_i2c, 0);
	ac100_write(sc, AC100_RTC_CTRL_REG, AC100_RTC_CTRL_12H_24H_MODE);
	iic_release_bus(sc->sc_i2c, 0);

	sc->sc_todr.todr_gettime_ymdhms = ac100_rtc_gettime;
	sc->sc_todr.todr_settime_ymdhms = ac100_rtc_settime;
	sc->sc_todr.cookie = sc;
	todr_attach(&sc->sc_todr);
}

static int
ac100_read(struct ac100_softc *sc, uint8_t reg, uint16_t *val)
{
	return iic_smbus_read_word(sc->sc_i2c, sc->sc_addr, reg, val,
	    cold ? I2C_F_POLL : 0);
}

static int
ac100_write(struct ac100_softc *sc, uint8_t reg, uint16_t val)
{
	return iic_smbus_write_word(sc->sc_i2c, sc->sc_addr, reg, val,
	    cold ? I2C_F_POLL : 0);
}

static int
ac100_rtc_gettime(todr_chip_handle_t tch, struct clock_ymdhms *dt)
{
	struct ac100_softc *sc = tch->cookie;
	uint16_t sec, min, hou, wee, day, mon, yea;

	iic_acquire_bus(sc->sc_i2c, 0);
	ac100_read(sc, AC100_RTC_SEC_REG, &sec);
	ac100_read(sc, AC100_RTC_MIN_REG, &min);
	ac100_read(sc, AC100_RTC_HOU_REG, &hou);
	ac100_read(sc, AC100_RTC_WEE_REG, &wee);
	ac100_read(sc, AC100_RTC_DAY_REG, &day);
	ac100_read(sc, AC100_RTC_MON_REG, &mon);
	ac100_read(sc, AC100_RTC_YEA_REG, &yea);
	iic_release_bus(sc->sc_i2c, 0);

	dt->dt_year = POSIX_BASE_YEAR + bcdtobin(yea & 0xff);
	dt->dt_mon = bcdtobin(mon & 0x1f);
	dt->dt_day = bcdtobin(day & 0x3f);
	dt->dt_wday = bcdtobin(wee & 0x7);
	dt->dt_hour = bcdtobin(hou & 0x3f);
	dt->dt_min = bcdtobin(min & 0x7f);
	dt->dt_sec = bcdtobin(sec & 0x7f);

	return 0;
}

static int
ac100_rtc_settime(todr_chip_handle_t tch, struct clock_ymdhms *dt)
{
	struct ac100_softc *sc = tch->cookie;

	iic_acquire_bus(sc->sc_i2c, 0);
	ac100_write(sc, AC100_RTC_SEC_REG, bintobcd(dt->dt_sec) & 0x7f);
	ac100_write(sc, AC100_RTC_MIN_REG, bintobcd(dt->dt_min) & 0x7f);
	ac100_write(sc, AC100_RTC_HOU_REG, bintobcd(dt->dt_hour) & 0x3f);
	ac100_write(sc, AC100_RTC_WEE_REG, bintobcd(dt->dt_wday) & 0x7);
	ac100_write(sc, AC100_RTC_DAY_REG, bintobcd(dt->dt_day) & 0x3f);
	ac100_write(sc, AC100_RTC_MON_REG, bintobcd(dt->dt_mon) & 0x1f);
	ac100_write(sc, AC100_RTC_YEA_REG,
	    bintobcd(dt->dt_year - POSIX_BASE_YEAR) & 0xff);
	ac100_write(sc, AC100_RTC_UPD_TRIG_REG, AC100_RTC_UPD_TRIG_WRITE);
	iic_release_bus(sc->sc_i2c, 0);

	return 0;
}
