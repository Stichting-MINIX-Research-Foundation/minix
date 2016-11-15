/*	$NetBSD: hpacel_acpi.c,v 1.5 2015/04/23 23:23:00 pgoyette Exp $ */

/*-
 * Copyright (c) 2009, 2011 Jukka Ruohonen <jruohonen@iki.fi>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hpacel_acpi.c,v 1.5 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/module.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_power.h>

#include <dev/sysmon/sysmonvar.h>

#define _COMPONENT	ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME	("hpacel_acpi")

/*
 * An ACPI driver for Hewlett-Packard 3D DriveGuard accelerometer.
 *
 * The supported chipset is LIS3LV02DL from STMicroelectronics:
 *
 *    http://www.st.com/stonline/products/literature/anp/12441.pdf
 *
 *    (Obtained on Sat Apr 25 00:32:04 EEST 2009.)
 *
 * The chip is a three axes digital output linear accelerometer
 * that is controllable through I2C / SPI serial interface. This
 * implementation however supports only indirect connection through
 * ACPI. Other chips from the same family, such as LIS3LV02DQ, may
 * also work with the driver, provided that there is a suitable DSDT.
 *
 * The chip can generate wake-up, direction detection and free-fall
 * interrupts. The latter could be used to evoke emergency action.
 * None of this is however supported. Only sysmon_envsys(9) is used.
 */
enum {
	HPACEL_SENSOR_X = 0,
	HPACEL_SENSOR_Y,
	HPACEL_SENSOR_Z,
	HPACEL_SENSOR_COUNT
};

#define LIS3LV02DL_ID     0x3A

enum lis3lv02dl_reg {
	WHO_AM_I        = 0x0F,	/* r  */
	OFFSET_X        = 0x16,	/* rw */
	OFFSET_Y        = 0x17,	/* rw */
	OFFSET_Z        = 0x18,	/* rw */
	GAIN_X          = 0x19,	/* rw */
	GAIN_Y          = 0x1A,	/* rw */
	GAIN_Z          = 0x1B,	/* rw */
	CTRL_REG1       = 0x20,	/* rw */
	CTRL_REG2       = 0x21,	/* rw */
	CTRL_REG3       = 0x22,	/* rw */
	HP_FILTER_RESET = 0x23,	/* r  */
	STATUS_REG      = 0x27,	/* rw */
	OUTX_L          = 0x28,	/* r  */
	OUTX_H          = 0x29,	/* r  */
	OUTY_L          = 0x2A,	/* r  */
	OUTY_H          = 0x2B,	/* r  */
	OUTZ_L          = 0x2C,	/* r  */
	OUTZ_H          = 0x2D,	/* r  */
	FF_WU_CFG       = 0x30,	/* r  */
	FF_WU_SRC       = 0x31,	/* rw */
	FF_WU_ACK       = 0x32,	/* r  */
	FF_WU_THS_L     = 0x34,	/* rw */
	FF_WU_THS_H     = 0x35,	/* rw */
	FF_WU_DURATION  = 0x36,	/* rw */
	DD_CFG          = 0x38,	/* rw */
	DD_SRC          = 0x39,	/* rw */
	DD_ACK          = 0x3A,	/* r  */
	DD_THSI_L       = 0x3C,	/* rw */
	DD_THSI_H       = 0x3D,	/* rw */
	DD_THSE_L       = 0x3E,	/* rw */
	DD_THSE_H       = 0x3F	/* rw */
};

enum lis3lv02dl_ctrl1 {
	CTRL1_Xen  = (1 << 0),	/* X-axis enable */
	CTRL1_Yen  = (1 << 1),	/* Y-axis enable */
	CTRL1_Zen  = (1 << 2),	/* Z-axis enable */
	CTRL1_ST   = (1 << 3),	/* Self test enable */
	CTRL1_DF0  = (1 << 4),	/* Decimation factor control */
	CTRL1_DF1  = (1 << 5),	/* Decimation factor control */
	CTRL1_PD0  = (1 << 6),	/* Power down control */
	CTRL1_PD1  = (1 << 7)	/* Power down control */
};

enum lis3lv02dl_ctrl2 {
	CTRL2_DAS  = (1 << 0),  /* Data alignment selection */
	CTRL2_SIM  = (1 << 1),  /* SPI serial interface mode */
	CTRL2_DRDY = (1 << 2),  /* Enable data-ready generation */
	CTRL2_IEN  = (1 << 3),  /* Enable interrupt mode */
	CTRL2_BOOT = (1 << 4),  /* Reboot memory contents */
	CTRL2_BLE  = (1 << 5),  /* Endian mode */
	CTRL2_BDU  = (1 << 6),  /* Block data update */
	CTRL2_FS   = (1 << 7)   /* Full scale selection */
};

enum lis3lv02dl_ctrl3 {
	CTRL3_CFS0 = (1 << 0),	/* High-pass filter cut-off frequency */
	CTRL3_CFS1 = (1 << 1),  /* High-pass filter cut-off frequency */
	CTRL3_FDS  = (1 << 4),  /* Filtered data selection */
	CTRL3_HPFF = (1 << 5),  /* High pass filter for free-fall */
	CTRL3_HPDD = (1 << 6),  /* High pass filter for DD */
	CTRL3_ECK  = (1 << 7)   /* External clock */
};

struct hpacel_softc {
	device_t		 sc_dev;
	struct acpi_devnode	*sc_node;
	struct sysmon_envsys	*sc_sme;
	bool			 sc_state;
	uint8_t			 sc_whoami;
	uint8_t			 sc_ctrl[3];
	envsys_data_t		 sc_sensor[HPACEL_SENSOR_COUNT];
};

const char * const hpacel_ids[] = {
	"HPQ0004",
	NULL
};

static int		hpacel_match(device_t, cfdata_t, void *);
static void		hpacel_attach(device_t, device_t, void *);
static int		hpacel_detach(device_t, int);
static bool		hpacel_reg_init(device_t);
static bool		hpacel_suspend(device_t, const pmf_qual_t *);
static bool		hpacel_resume(device_t, const pmf_qual_t *);
static ACPI_STATUS	hpacel_reg_info(device_t);
static ACPI_STATUS	hpacel_reg_read(ACPI_HANDLE, ACPI_INTEGER, uint8_t *);
static ACPI_STATUS	hpacel_reg_write(ACPI_HANDLE, ACPI_INTEGER, uint8_t);
static ACPI_STATUS	hpacel_reg_xyz(ACPI_HANDLE, const int, int16_t *);
static ACPI_STATUS	hpacel_power(device_t, bool);
static bool		hpacel_sensor_init(device_t);
static void		hpacel_sensor_refresh(struct sysmon_envsys *,
					      envsys_data_t *);

CFATTACH_DECL_NEW(hpacel, sizeof(struct hpacel_softc),
    hpacel_match, hpacel_attach, hpacel_detach, NULL);

static int
hpacel_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, hpacel_ids);
}

static void
hpacel_attach(device_t parent, device_t self, void *aux)
{
	struct hpacel_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;

	sc->sc_sme = NULL;
	sc->sc_dev = self;
	sc->sc_state = false;
	sc->sc_node = aa->aa_node;

	aprint_naive("\n");
	aprint_normal(": HP 3D DriveGuard accelerometer\n");

	if (hpacel_reg_init(self) != true)
		return;

	(void)pmf_device_register(self, hpacel_suspend, hpacel_resume);

	if (hpacel_sensor_init(self) != false)
		(void)hpacel_power(self, true);

	sc->sc_state = true;
}

static int
hpacel_detach(device_t self, int flags)
{
	struct hpacel_softc *sc = device_private(self);

	if (sc->sc_state != false)
		(void)hpacel_power(self, false);

	if (sc->sc_sme != NULL)
		sysmon_envsys_unregister(sc->sc_sme);

	return 0;
}

static bool
hpacel_suspend(device_t self, const pmf_qual_t *qual)
{
	struct hpacel_softc *sc = device_private(self);

	if (sc->sc_state != false)
		(void)hpacel_power(self, false);

	return true;
}

static bool
hpacel_resume(device_t self, const pmf_qual_t *qual)
{
	struct hpacel_softc *sc = device_private(self);

	if (sc->sc_state != false)
		(void)hpacel_power(self, true);

	return true;
}

static bool
hpacel_reg_init(device_t self)
{
	struct hpacel_softc *sc = device_private(self);
	ACPI_HANDLE hdl = sc->sc_node->ad_handle;
	ACPI_STATUS rv;
	uint8_t val;

	rv = AcpiEvaluateObject(hdl, "_INI", NULL, NULL);

	if (ACPI_FAILURE(rv))
		goto out;

        /*
	 * Since the "_INI" is practically
	 * a black box, it is better to verify
	 * the control registers manually.
	 */
	rv = hpacel_reg_info(self);

	if (ACPI_FAILURE(rv))
		goto out;

	val = sc->sc_ctrl[0];

	if ((sc->sc_ctrl[0] & CTRL1_Xen) == 0)
		val |= CTRL1_Xen;

	if ((sc->sc_ctrl[0] & CTRL1_Yen) == 0)
		val |= CTRL1_Yen;

	if ((sc->sc_ctrl[0] & CTRL1_Zen) == 0)
		val |= CTRL1_Zen;

	if (val != sc->sc_ctrl[0]) {

		rv = hpacel_reg_write(hdl, CTRL_REG1, val);

		if (ACPI_FAILURE(rv))
			return rv;
	}

        val = sc->sc_ctrl[1];

	if ((sc->sc_ctrl[1] & CTRL2_BDU) == 0)
		val |= CTRL2_BDU;

	if ((sc->sc_ctrl[1] & CTRL2_BLE) != 0)
		val &= ~CTRL2_BLE;

	if ((sc->sc_ctrl[1] & CTRL2_DAS) != 0)
		val &= ~CTRL2_DAS;

	/*
	 * Given the use of sysmon_envsys(9),
	 * there is no need for the data-ready pin.
	 */
	if ((sc->sc_ctrl[1] & CTRL2_DRDY) != 0)
		val &= ~CTRL2_DRDY;

	/*
	 * Disable interrupt mode.
	 */
	if ((sc->sc_ctrl[1] & CTRL2_IEN) != 0)
		val &= ~CTRL2_IEN;

	if (val != sc->sc_ctrl[1]) {

		rv = hpacel_reg_write(hdl, CTRL_REG2, val);

		if (ACPI_FAILURE(rv))
			return rv;
	}

	/*
	 * Clear possible interrupt setups from
	 * the direction-detection register and
	 * from the free-fall-wake-up register.
	 */
	(void)hpacel_reg_write(hdl, DD_CFG, 0x00);
	(void)hpacel_reg_write(hdl, FF_WU_CFG, 0x00);

	/*
	 * Update the register information.
	 */
	(void)hpacel_reg_info(self);

out:
	if (ACPI_FAILURE(rv))
		aprint_error_dev(self, "failed to initialize "
		    "device: %s\n", AcpiFormatException(rv));

	return (rv != AE_OK) ? false : true;
}

static ACPI_STATUS
hpacel_reg_info(device_t self)
{
	struct hpacel_softc *sc = device_private(self);
	ACPI_HANDLE hdl = sc->sc_node->ad_handle;
	ACPI_STATUS rv;
	size_t i;

	rv = hpacel_reg_read(hdl, WHO_AM_I, &sc->sc_whoami);

	if (ACPI_FAILURE(rv))
		return rv;

	for (i = 0; i < __arraycount(sc->sc_sensor); i++) {

		rv = hpacel_reg_read(hdl, CTRL_REG1 + i, &sc->sc_ctrl[i]);

		if (ACPI_FAILURE(rv))
			return rv;
	}

	return AE_OK;
}

static ACPI_STATUS
hpacel_reg_read(ACPI_HANDLE hdl, ACPI_INTEGER reg, uint8_t *valp)
{
	ACPI_OBJECT_LIST arg;
	ACPI_OBJECT obj, val;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;

	obj.Type = ACPI_TYPE_INTEGER;
	obj.Integer.Value = reg;

	buf.Pointer = &val;
	buf.Length = sizeof(val);

	arg.Count = 1;
	arg.Pointer = &obj;

	rv = AcpiEvaluateObjectTyped(hdl, "ALRD",
	    &arg, &buf, ACPI_TYPE_INTEGER);

	if (ACPI_FAILURE(rv))
		return rv;

	if (val.Integer.Value > UINT8_MAX)
		return AE_AML_NUMERIC_OVERFLOW;

	*valp = val.Integer.Value;

	return AE_OK;
}

static ACPI_STATUS
hpacel_reg_write(ACPI_HANDLE hdl, ACPI_INTEGER reg, uint8_t val)
{
	ACPI_OBJECT_LIST arg;
	ACPI_OBJECT obj[2];

	obj[0].Type = obj[1].Type = ACPI_TYPE_INTEGER;

	obj[0].Integer.Value = reg;
	obj[1].Integer.Value = val;

	arg.Count = 2;
	arg.Pointer = obj;

	return AcpiEvaluateObject(hdl, "ALWR", &arg, NULL);
}

static ACPI_STATUS
hpacel_reg_xyz(ACPI_HANDLE hdl, const int xyz, int16_t *out)
{
	ACPI_INTEGER reg[2];
	ACPI_STATUS rv[2];
	uint8_t hi, lo;

	switch (xyz) {

	case HPACEL_SENSOR_X:
		reg[0] = OUTX_L;
		reg[1] = OUTX_H;
		break;

	case HPACEL_SENSOR_Y:
		reg[0] = OUTY_L;
		reg[1] = OUTY_H;
		break;

	case HPACEL_SENSOR_Z:
		reg[0] = OUTZ_L;
		reg[1] = OUTZ_H;
		break;

	default:
		return AE_BAD_PARAMETER;
	}

	rv[0] = hpacel_reg_read(hdl, reg[0], &lo);
	rv[1] = hpacel_reg_read(hdl, reg[1], &hi);

	if (ACPI_FAILURE(rv[0]) || ACPI_FAILURE(rv[1]))
		return AE_ERROR;

	/*
	 * These registers are read in "12 bit right
	 * justified mode", meaning that the four
	 * most significant bits are replaced with
	 * the value of bit 12. Note the signed type.
	 */
	hi = (hi & 0x10) ? hi | 0xE0 : hi & ~0xE0;

	*out = (hi << 8) | lo;

	return AE_OK;
}

static ACPI_STATUS
hpacel_power(device_t self, bool enable)
{
	struct hpacel_softc *sc = device_private(self);
	ACPI_HANDLE hdl = sc->sc_node->ad_handle;
	ACPI_OBJECT_LIST arg;
	ACPI_OBJECT obj;
	ACPI_STATUS rv;
	uint8_t val;

	rv = hpacel_reg_info(self);

	if (ACPI_FAILURE(rv))
		return rv;

	val = sc->sc_ctrl[0];

	if (enable != false)
		val |= CTRL1_PD0 | CTRL1_PD1;
	else {
		val &= ~(CTRL1_PD0 | CTRL1_PD1);
	}

	if (val != sc->sc_ctrl[0]) {

		rv = hpacel_reg_write(hdl, CTRL_REG1, val);

		if (ACPI_FAILURE(rv))
			return rv;
	}

	obj.Type = ACPI_TYPE_INTEGER;
	obj.Integer.Value = enable;

	arg.Count = 1;
	arg.Pointer = &obj;

	/*
	 * This should turn on/off a led, if available.
	 */
	(void)AcpiEvaluateObject(hdl, "ALED", &arg, NULL);

	return rv;
}

static bool
hpacel_sensor_init(device_t self)
{
	const char zyx[HPACEL_SENSOR_COUNT] = { 'x', 'y', 'z' };
	struct hpacel_softc *sc = device_private(self);
	size_t i;
	int rv;

	CTASSERT(HPACEL_SENSOR_X == 0);
	CTASSERT(HPACEL_SENSOR_Y == 1);
	CTASSERT(HPACEL_SENSOR_Z == 2);

	sc->sc_sme = sysmon_envsys_create();

	for (i = 0; i < __arraycount(sc->sc_sensor); i++) {

		sc->sc_sensor[i].units = ENVSYS_INTEGER;
		sc->sc_sensor[i].state = ENVSYS_SINVALID;
		sc->sc_sensor[i].flags = ENVSYS_FHAS_ENTROPY;

		(void)snprintf(sc->sc_sensor[i].desc,
		    ENVSYS_DESCLEN, "%c-acceleration", zyx[i]);

		rv = sysmon_envsys_sensor_attach(sc->sc_sme,&sc->sc_sensor[i]);

		if (rv != 0)
			goto fail;
	}

	/*
	 * We only do polling, given the hopelessly
	 * slow way of reading registers with ACPI.
	 */
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_flags = SME_POLL_ONLY;
	sc->sc_sme->sme_name = device_xname(self);
	sc->sc_sme->sme_refresh = hpacel_sensor_refresh;

	rv = sysmon_envsys_register(sc->sc_sme);

	if (rv != 0)
		goto fail;

	return true;

fail:
	aprint_error_dev(self, "failed to initialize sensors\n");

	sysmon_envsys_destroy(sc->sc_sme);
	sc->sc_sme = NULL;

	return false;
}

static void
hpacel_sensor_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
        struct hpacel_softc *sc = sme->sme_cookie;
	ACPI_STATUS rv;
	int16_t val;
	size_t i;

	for (i = 0; i < __arraycount(sc->sc_sensor); i++) {

		rv = hpacel_reg_xyz(sc->sc_node->ad_handle, i, &val);

		if (ACPI_SUCCESS(rv)) {
			sc->sc_sensor[i].value_cur = val;
			sc->sc_sensor[i].state = ENVSYS_SVALID;
			continue;
		}

		sc->sc_sensor[i].state = ENVSYS_SINVALID;
	}
}

MODULE(MODULE_CLASS_DRIVER, hpacel, "sysmon_envsys");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
hpacel_modcmd(modcmd_t cmd, void *aux)
{
	int rv = 0;

	switch (cmd) {

	case MODULE_CMD_INIT:

#ifdef _MODULE
		rv = config_init_component(cfdriver_ioconf_hpacel,
		    cfattach_ioconf_hpacel, cfdata_ioconf_hpacel);
#endif
		break;

	case MODULE_CMD_FINI:

#ifdef _MODULE
		rv = config_fini_component(cfdriver_ioconf_hpacel,
		    cfattach_ioconf_hpacel, cfdata_ioconf_hpacel);
#endif
		break;

	default:
		rv = ENOTTY;
	}

	return rv;
}
