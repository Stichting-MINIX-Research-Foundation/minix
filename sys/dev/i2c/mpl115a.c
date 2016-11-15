/*	$NetBSD: mpl115a.c,v 1.1 2013/09/08 14:59:42 rkujawa Exp $ */

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Radoslaw Kujawa.
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
 * Freescale MPL115A2 miniature digital barometer driver.
 *
 * This driver could be split into bus-indepented driver and I2C-specific
 * attachment, as SPI variant of this chip also exist.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mpl115a.c,v 1.1 2013/09/08 14:59:42 rkujawa Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/endian.h>

#include <sys/bus.h>
#include <dev/i2c/i2cvar.h>

#include <dev/sysmon/sysmonvar.h>

#include <dev/i2c/mpl115areg.h>

#define MPL115A_DEBUG 1

struct mpl115a_softc {
	device_t		sc_dev;

	i2c_tag_t		sc_tag;
	i2c_addr_t		sc_addr;

	/* raw coefficients */
	int16_t			sc_a0;
	int16_t			sc_b1;
	int16_t			sc_b2;
	int16_t			sc_c12;

	/* envsys(4) stuff */
	struct sysmon_envsys	*sc_sme;
	envsys_data_t		sc_sensor;
	kmutex_t		sc_lock; 
};


static int	mpl115a_match(device_t, cfdata_t, void *);
static void	mpl115a_attach(device_t, device_t, void *);

static uint8_t	mpl115a_reg_read_1(struct mpl115a_softc *sc, uint8_t);
static void	mpl115a_reg_write_1(struct mpl115a_softc *sc, uint8_t, uint8_t);

static void	mpl115a_load_coeffs(struct mpl115a_softc *sc);
static uint16_t	mpl115a_make_coeff(uint8_t msb, uint8_t lsb);
static uint32_t	mpl115a_pressure(struct mpl115a_softc *sc);
static uint32_t mpl115a_calc(struct mpl115a_softc *sc, uint16_t padc, uint16_t tadc) ;

static void mpl115a_envsys_register(struct mpl115a_softc *);
static void mpl115a_envsys_refresh(struct sysmon_envsys *, envsys_data_t *);

CFATTACH_DECL_NEW(mpl115a, sizeof (struct mpl115a_softc),
    mpl115a_match, mpl115a_attach, NULL, NULL);

static int
mpl115a_match(device_t parent, cfdata_t cf, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (ia->ia_addr == MPL115A_ADDR) 
		return 1;
	return 0;
}

static void
mpl115a_attach(device_t parent, device_t self, void *aux)
{
	struct mpl115a_softc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;

	sc->sc_dev = self;
	sc->sc_addr = ia->ia_addr;
	sc->sc_tag = ia->ia_tag;

	aprint_normal(": Freescale MPL115A2 Pressure Sensor\n");

	/* Since coefficients do not change load them once. */
	mpl115a_load_coeffs(sc);

	mpl115a_pressure(sc);

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);

	mpl115a_envsys_register(sc);
}

/* Construct a coefficients from MSB and LSB bytes. */
static uint16_t
mpl115a_make_coeff(uint8_t msb, uint8_t lsb)
{
	uint16_t rv;

	rv = le16toh((msb << 8) | lsb);

#ifdef MPL115A_DEBUG
	aprint_normal("msb %x, lsb %x, ret val %x\n", msb, lsb, rv);
#endif /* MPL115A_DEBUG */

	return rv;
}

static void
mpl115a_load_coeffs(struct mpl115a_softc *sc)
{
	sc->sc_a0 = mpl115a_make_coeff(mpl115a_reg_read_1(sc, MPL115A_A0_MSB),
	    mpl115a_reg_read_1(sc, MPL115A_A0_LSB));
	sc->sc_b1 = mpl115a_make_coeff(mpl115a_reg_read_1(sc, MPL115A_B1_MSB),
	    mpl115a_reg_read_1(sc, MPL115A_B1_LSB));
	sc->sc_b2 = mpl115a_make_coeff(mpl115a_reg_read_1(sc, MPL115A_B2_MSB),
	    mpl115a_reg_read_1(sc, MPL115A_B2_LSB));
	sc->sc_c12 = mpl115a_make_coeff(mpl115a_reg_read_1(sc, MPL115A_C12_MSB),
	    mpl115a_reg_read_1(sc, MPL115A_C12_LSB));
}

static void 
mpl115a_reg_write_1(struct mpl115a_softc *sc, uint8_t reg, uint8_t val) 
{
	if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL) != 0) {
		aprint_error_dev(sc->sc_dev, "cannot acquire bus for write\n");
		return;
	}

	if (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP, sc->sc_addr, &reg, 1,
	    &val, 1, I2C_F_POLL)) {
		aprint_error_dev(sc->sc_dev, "cannot execute write\n");
	}
	iic_release_bus(sc->sc_tag, I2C_F_POLL);
}

static uint8_t
mpl115a_reg_read_1(struct mpl115a_softc *sc, uint8_t reg)
{
	uint8_t rv, wbuf[2];

	if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL) != 0) {
#ifdef MPL115A_DEBUG
		aprint_error_dev(sc->sc_dev, "cannot acquire bus for read\n");
#endif /* MPL115A_DEBUG */ 
		return 0;
	}

	wbuf[0] = reg;

	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_addr, wbuf,
	    1, &rv, 1, I2C_F_POLL)) {
		aprint_error_dev(sc->sc_dev, "cannot execute read\n");
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		return 0;
	}
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	return rv;
}



/* Get pressure in pascals. */
static uint32_t
mpl115a_pressure(struct mpl115a_softc *sc)
{
	uint32_t rv;

	uint16_t padc, tadc;

	rv = 0;

	mpl115a_reg_write_1(sc, MPL115A_CONVERT, 1);
	delay(20); /* even 3 should be enough */

	padc = mpl115a_make_coeff(mpl115a_reg_read_1(sc, MPL115A_PADC_MSB),
	    mpl115a_reg_read_1(sc, MPL115A_PADC_LSB)); /* XXX, this fails */
	padc = mpl115a_make_coeff(mpl115a_reg_read_1(sc, MPL115A_PADC_MSB),
	    mpl115a_reg_read_1(sc, MPL115A_PADC_LSB)); 

	tadc = mpl115a_make_coeff(mpl115a_reg_read_1(sc, MPL115A_TADC_MSB),
	    mpl115a_reg_read_1(sc, MPL115A_TADC_LSB)); 

#ifdef MPL115A_DEBUG
	aprint_normal_dev(sc->sc_dev, "padc %x, tadc %x\n", padc, tadc);
#endif /* MPL115A_DEBUG */

	rv = mpl115a_calc(sc, padc, tadc);

#ifdef MPL115A_DEBUG
	aprint_normal_dev(sc->sc_dev, "%d Pa\n", rv);
#endif /* MPL115A_DEBUG */

	return rv;	
}

/* The following calculation routine was suggested by Matt Thomas. */
static uint32_t
mpl115a_calc(struct mpl115a_softc *sc, uint16_t padc, uint16_t tadc) 
{
	int32_t fp_a0, fp_b1, fp_b2, fp_c12, fp_padc, fp_tadc;
	int32_t c12x2, a1, a1x2, y1, a2x2, pcomp;
	uint32_t pre_kpa1, pre_kpa2, pa;

	/* convert coefficients to common fixed point format */
	fp_a0 = sc->sc_a0 << 13;	/* 12.3 -> 15.16 */
	fp_b1 = sc->sc_b1 << 3;		/* 2.13 -> 15.16 */
	fp_b2 = sc->sc_b2 << 2;		/* 1.14 -> 15.16 */
	fp_c12 = sc->sc_c12 >> 2;	/* 0.22 */ 

	fp_padc = padc >> 6;
	fp_tadc = tadc >> 6;

	c12x2 = (fp_c12 * fp_tadc + 0x3f) >> 6;
	a1 = fp_b1 + c12x2;
	a1x2 = a1 * fp_padc;
	y1 = fp_a0 + a1x2;
	a2x2 = fp_b2 * fp_tadc;
	pcomp = y1 + a2x2;

	/* pre_kpa has 16 fractional digits so it's accurate to 1/65536 kPa */
	pre_kpa1 = pcomp * (115 - 50);
	pre_kpa2 = pre_kpa1 / 1023 + (50 << 16);

	/* but the real accuracy of the sensor is around +/- 1 kPa... */
	pa = ((pre_kpa2 + 32768) >> 16) * 1000;

	return pa;
}

static void
mpl115a_envsys_register(struct mpl115a_softc *sc)
{
	sc->sc_sme = sysmon_envsys_create();

	strlcpy(sc->sc_sensor.desc, "Absolute pressure",
	    sizeof(sc->sc_sensor.desc));
	/* sc->sc_sensor.units = ENVSYS_SPRESSURE; */ 
	sc->sc_sensor.units = ENVSYS_INTEGER;
	sc->sc_sensor.state = ENVSYS_SINVALID;

	if (sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensor)) {
		aprint_error_dev(sc->sc_dev,
		    "error attaching sensor\n");
		return;
	}

	sc->sc_sme->sme_name = device_xname(sc->sc_dev);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = mpl115a_envsys_refresh;

	if (sysmon_envsys_register(sc->sc_sme)) {
		aprint_error_dev(sc->sc_dev, "unable to register in sysmon\n");
		sysmon_envsys_destroy(sc->sc_sme);
	}
}

static void
mpl115a_envsys_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct mpl115a_softc *sc = sme->sme_cookie;

	mutex_enter(&sc->sc_lock);

	edata->value_cur = mpl115a_pressure(sc);
	edata->state = ENVSYS_SVALID;

	mutex_exit(&sc->sc_lock);
}

