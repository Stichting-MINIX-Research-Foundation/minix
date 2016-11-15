/*	$NetBSD: aps.c,v 1.17 2015/04/23 23:23:00 pgoyette Exp $	*/
/*	$OpenBSD: aps.c,v 1.15 2007/05/19 19:14:11 tedu Exp $	*/
/*	$OpenBSD: aps.c,v 1.17 2008/06/27 06:08:43 canacar Exp $	*/
/*
 * Copyright (c) 2005 Jonathan Gray <jsg@openbsd.org>
 * Copyright (c) 2008 Can Erkin Acar <canacar@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * A driver for the ThinkPad Active Protection System based on notes from
 * http://www.almaden.ibm.com/cs/people/marksmith/tpaps.html
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: aps.c,v 1.17 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/callout.h>
#include <sys/module.h>

#include <sys/bus.h>

#include <dev/sysmon/sysmonvar.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#if defined(APSDEBUG)
#define DPRINTF(x)		do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif


/*
 * EC interface on Thinkpad Laptops, from Linux HDAPS driver notes.
 * From Renesans H8S/2140B Group Hardware Manual
 * http://documentation.renesas.com/eng/products/mpumcu/rej09b0300_2140bhm.pdf
 *
 * EC uses LPC Channel 3 registers TWR0..15
 */

/* STR3 status register */
#define APS_STR3		0x04

#define APS_STR3_IBF3B	0x80	/* Input buffer full (host->slave) */
#define APS_STR3_OBF3B	0x40	/* Output buffer full (slave->host)*/
#define APS_STR3_MWMF	0x20	/* Master write mode */
#define APS_STR3_SWMF	0x10	/* Slave write mode */


/* Base address of TWR registers */
#define APS_TWR_BASE		0x10
#define APS_TWR_RET		0x1f

/* TWR registers */
#define APS_CMD			0x00
#define APS_ARG1		0x01
#define APS_ARG2		0x02
#define APS_ARG3		0x03
#define APS_RET			0x0f

/* Sensor values */
#define APS_STATE		0x01
#define	APS_XACCEL		0x02
#define APS_YACCEL		0x04
#define APS_TEMP		0x06
#define	APS_XVAR		0x07
#define APS_YVAR		0x09
#define APS_TEMP2		0x0b
#define APS_UNKNOWN		0x0c
#define APS_INPUT		0x0d

/* write masks for I/O, send command + 0-3 arguments*/
#define APS_WRITE_0		0x0001
#define APS_WRITE_1		0x0003
#define APS_WRITE_2		0x0007
#define APS_WRITE_3		0x000f

/* read masks for I/O, read 0-3 values (skip command byte) */
#define APS_READ_0		0x0000
#define APS_READ_1		0x0002
#define APS_READ_2		0x0006
#define APS_READ_3		0x000e

#define APS_READ_RET		0x8000
#define APS_READ_ALL		0xffff

/* Bit definitions for APS_INPUT value */
#define APS_INPUT_KB		(1 << 5)
#define APS_INPUT_MS		(1 << 6)
#define APS_INPUT_LIDOPEN	(1 << 7)

#define APS_ADDR_SIZE		0x1f

struct sensor_rec {
	uint8_t 	state;
	uint16_t	x_accel;
	uint16_t	y_accel;
	uint8_t 	temp1;
	uint16_t	x_var;
	uint16_t	y_var;
	uint8_t 	temp2;
	uint8_t 	unk;
	uint8_t 	input;
};

enum aps_sensors {
        APS_SENSOR_XACCEL = 0,
        APS_SENSOR_YACCEL,
        APS_SENSOR_XVAR,
        APS_SENSOR_YVAR,
        APS_SENSOR_TEMP1,
        APS_SENSOR_TEMP2,
        APS_SENSOR_KBACT,
        APS_SENSOR_MSACT,
        APS_SENSOR_LIDOPEN,
        APS_NUM_SENSORS
};

struct aps_softc {
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bool sc_bus_space_valid;

	struct sysmon_envsys *sc_sme;
	envsys_data_t sc_sensor[APS_NUM_SENSORS];
	struct callout sc_callout;

	struct sensor_rec aps_data;
};

static int 	aps_match(device_t, cfdata_t, void *);
static void 	aps_attach(device_t, device_t, void *);
static int	aps_detach(device_t, int);

static int 	aps_init(struct aps_softc *);
static int	aps_read_data(struct aps_softc *);
static void 	aps_refresh_sensor_data(struct aps_softc *);
static void 	aps_refresh(void *);
static int	aps_do_io(bus_space_tag_t, bus_space_handle_t,
			  unsigned char *, int, int);
static bool 	aps_suspend(device_t, const pmf_qual_t *);
static bool 	aps_resume(device_t, const pmf_qual_t *);

CFATTACH_DECL_NEW(aps, sizeof(struct aps_softc),
	      aps_match, aps_attach, aps_detach, NULL);

/* properly communicate with the controller, writing a set of memory
 * locations and reading back another set  */
static int
aps_do_io(bus_space_tag_t iot, bus_space_handle_t ioh,
	  unsigned char *buf, int wmask, int rmask)
{
	int bp, stat, n;

	DPRINTF(("aps_do_io: CMD: 0x%02x, wmask: 0x%04x, rmask: 0x%04x\n",
	    buf[0], wmask, rmask));

	/* write init byte using arbitration */
	for (n = 0; n < 100; n++) {
		stat = bus_space_read_1(iot, ioh, APS_STR3);
		if (stat & (APS_STR3_OBF3B | APS_STR3_SWMF)) {
			bus_space_read_1(iot, ioh, APS_TWR_RET);
			continue;
		}
		bus_space_write_1(iot, ioh, APS_TWR_BASE, buf[0]);
		stat = bus_space_read_1(iot, ioh, APS_STR3);
		if (stat & (APS_STR3_MWMF))
			break;
		delay(1);
	}

	if (n == 100) {
		DPRINTF(("aps_do_io: Failed to get bus\n"));
		return 1;
	}

	/* write data bytes, init already sent */
	/* make sure last bye is always written as this will trigger slave */
	wmask |= APS_READ_RET;
	buf[APS_RET] = 0x01;

	for (n = 1, bp = 2; n < 16; bp <<= 1, n++) {
		if (wmask & bp) {
			bus_space_write_1(iot, ioh, APS_TWR_BASE + n, buf[n]);
			DPRINTF(("aps_do_io:  write %2d 0x%02x\n", n, buf[n]));
		}
	}

	for (n = 0; n < 100; n++) {
		stat = bus_space_read_1(iot, ioh, APS_STR3);
		if (stat & (APS_STR3_OBF3B))
			break;
		delay(5 * 100);
	}

	if (n == 100) {
		DPRINTF(("aps_do_io: timeout waiting response\n"));
		return 1;
	}
	/* wait for data available */
	/* make sure to read the final byte to clear status */
	rmask |= APS_READ_RET;

	/* read cmd and data bytes */
	for (n = 0, bp = 1; n < 16; bp <<= 1, n++) {
		if (rmask & bp) {
			buf[n] = bus_space_read_1(iot, ioh, APS_TWR_BASE + n);
			DPRINTF(("aps_do_io:  read %2d 0x%02x\n", n, buf[n]));
		}
	}

	return 0;
}

static int
aps_match(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	unsigned char iobuf[16];
	int iobase;
	uint8_t cr;

	/* Must supply an address */
	if (ia->ia_nio < 1)
		return 0;

	if (ISA_DIRECT_CONFIG(ia))
		return 0;

	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return 0;

	iobase = ia->ia_io[0].ir_addr;

	if (bus_space_map(iot, iobase, APS_ADDR_SIZE, 0, &ioh)) {
		aprint_error("aps: can't map i/o space\n");
		return 0;
	}


	/* See if this machine has APS */

	/* get APS mode */
	iobuf[APS_CMD] = 0x13;
	if (aps_do_io(iot, ioh, iobuf, APS_WRITE_0, APS_READ_1)) {
		bus_space_unmap(iot, ioh, APS_ADDR_SIZE);
		return 0;
	}

	/*
	 * Observed values from Linux driver:
	 * 0x01: T42
	 * 0x02: chip already initialised
	 * 0x03: T41
	 * 0x05: T61
	 */

	cr = iobuf[APS_ARG1];

	bus_space_unmap(iot, ioh, APS_ADDR_SIZE);
	DPRINTF(("aps: state register 0x%x\n", cr));

	if (iobuf[APS_RET] != 0 || cr < 1 || cr > 5) {
		DPRINTF(("aps0: unsupported state %d\n", cr));
		return 0;
	}

	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = APS_ADDR_SIZE;
	ia->ia_niomem = 0;
	ia->ia_nirq = 0;
	ia->ia_ndrq = 0;

	return 1;
}

static void
aps_attach(device_t parent, device_t self, void *aux)
{
	struct aps_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;
	int iobase, i;

	sc->sc_iot = ia->ia_iot;
	iobase = ia->ia_io[0].ir_addr;

	callout_init(&sc->sc_callout, 0);
	callout_setfunc(&sc->sc_callout, aps_refresh, sc);

	if (bus_space_map(sc->sc_iot, iobase, APS_ADDR_SIZE, 0, &sc->sc_ioh)) {
		aprint_error(": can't map i/o space\n");
		return;
	}
	sc->sc_bus_space_valid = true;

	aprint_naive("\n");
	aprint_normal(": Thinkpad Active Protection System\n");

	if (aps_init(sc)) {
		aprint_error_dev(self, "failed to initialize\n");
		goto out;
	}

	/* Initialize sensors */
#define INITDATA(idx, unit, string)					\
	sc->sc_sensor[idx].units = unit;				\
	strlcpy(sc->sc_sensor[idx].desc, string,			\
	    sizeof(sc->sc_sensor[idx].desc));

	INITDATA(APS_SENSOR_XACCEL, ENVSYS_INTEGER, "x-acceleration");
	INITDATA(APS_SENSOR_YACCEL, ENVSYS_INTEGER, "y-acceleration");
	INITDATA(APS_SENSOR_TEMP1, ENVSYS_STEMP, "temperature 1");
	INITDATA(APS_SENSOR_TEMP2, ENVSYS_STEMP, "temperature 2");
	INITDATA(APS_SENSOR_XVAR, ENVSYS_INTEGER, "x-variable");
	INITDATA(APS_SENSOR_YVAR, ENVSYS_INTEGER, "y-variable");
	INITDATA(APS_SENSOR_KBACT, ENVSYS_INDICATOR, "keyboard active");
	INITDATA(APS_SENSOR_MSACT, ENVSYS_INDICATOR, "mouse active");
	INITDATA(APS_SENSOR_LIDOPEN, ENVSYS_INDICATOR, "lid open");

	sc->sc_sme = sysmon_envsys_create();

	for (i = 0; i < APS_NUM_SENSORS; i++) {

		sc->sc_sensor[i].state = ENVSYS_SVALID;

		if (sc->sc_sensor[i].units == ENVSYS_INTEGER)
			sc->sc_sensor[i].flags = ENVSYS_FHAS_ENTROPY;

		if (sysmon_envsys_sensor_attach(sc->sc_sme,
			&sc->sc_sensor[i])) {
			sysmon_envsys_destroy(sc->sc_sme);
			goto out;
		}
	}
        /*
         * Register with the sysmon_envsys(9) framework.
         */
	sc->sc_sme->sme_name = device_xname(self);
	sc->sc_sme->sme_flags = SME_DISABLE_REFRESH;

	if ((i = sysmon_envsys_register(sc->sc_sme))) {
		aprint_error_dev(self,
		    "unable to register with sysmon (%d)\n", i);
		sysmon_envsys_destroy(sc->sc_sme);
		goto out;
	}

	if (!pmf_device_register(self, aps_suspend, aps_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	/* Refresh sensor data every 0.5 seconds */
	callout_schedule(&sc->sc_callout, (hz) / 2);

	return;

out:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, APS_ADDR_SIZE);
}

static int
aps_init(struct aps_softc *sc)
{
	unsigned char iobuf[16];

	/* command 0x17/0x81: check EC */
	iobuf[APS_CMD] = 0x17;
	iobuf[APS_ARG1] = 0x81;

	if (aps_do_io(sc->sc_iot, sc->sc_ioh, iobuf, APS_WRITE_1, APS_READ_3))
		return 1;
	if (iobuf[APS_RET] != 0 ||iobuf[APS_ARG3] != 0)
		return 1;

	/* Test values from the Linux driver */
	if ((iobuf[APS_ARG1] != 0 || iobuf[APS_ARG2] != 0x60) &&
	    (iobuf[APS_ARG1] != 1 || iobuf[APS_ARG2] != 0))
		return 1;

	/* command 0x14: set power */
	iobuf[APS_CMD] = 0x14;
	iobuf[APS_ARG1] = 0x01;

	if (aps_do_io(sc->sc_iot, sc->sc_ioh, iobuf, APS_WRITE_1, APS_READ_0))
		return 1;

	if (iobuf[APS_RET] != 0)
		return 1;

	/* command 0x10: set config (sample rate and order) */
	iobuf[APS_CMD] = 0x10;
	iobuf[APS_ARG1] = 0xc8;
	iobuf[APS_ARG2] = 0x00;
	iobuf[APS_ARG3] = 0x02;

	if (aps_do_io(sc->sc_iot, sc->sc_ioh, iobuf, APS_WRITE_3, APS_READ_0))
		return 1;

	/* command 0x11: refresh data */
	iobuf[APS_CMD] = 0x11;
	if (aps_do_io(sc->sc_iot, sc->sc_ioh, iobuf, APS_WRITE_0, APS_READ_1))
		return 1;
	if (iobuf[APS_ARG1] != 0)
		return 1;

	return 0;
}

static int
aps_detach(device_t self, int flags)
{
	struct aps_softc *sc = device_private(self);

        callout_halt(&sc->sc_callout, NULL);
        callout_destroy(&sc->sc_callout);

	if (sc->sc_sme)
		sysmon_envsys_unregister(sc->sc_sme);
	if (sc->sc_bus_space_valid == true)
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, APS_ADDR_SIZE);

	return 0;
}

static int
aps_read_data(struct aps_softc *sc)
{
	unsigned char iobuf[16];

	iobuf[APS_CMD] = 0x11;
	if (aps_do_io(sc->sc_iot, sc->sc_ioh, iobuf, APS_WRITE_0, APS_READ_ALL))
		return 1;

	sc->aps_data.state = iobuf[APS_STATE];
	sc->aps_data.x_accel = iobuf[APS_XACCEL] + 256 * iobuf[APS_XACCEL + 1];
	sc->aps_data.y_accel = iobuf[APS_YACCEL] + 256 * iobuf[APS_YACCEL + 1];
	sc->aps_data.temp1 = iobuf[APS_TEMP];
	sc->aps_data.x_var = iobuf[APS_XVAR] + 256 * iobuf[APS_XVAR + 1];
	sc->aps_data.y_var = iobuf[APS_YVAR] + 256 * iobuf[APS_YVAR + 1];
	sc->aps_data.temp2 = iobuf[APS_TEMP2];
	sc->aps_data.input = iobuf[APS_INPUT];

	return 0;
}

static void
aps_refresh_sensor_data(struct aps_softc *sc)
{
	int64_t temp;

	if (aps_read_data(sc)) {
		printf("aps0: read data failed\n");
		return;
	}

	sc->sc_sensor[APS_SENSOR_XACCEL].value_cur = sc->aps_data.x_accel;
	sc->sc_sensor[APS_SENSOR_YACCEL].value_cur = sc->aps_data.y_accel;

	if (sc->aps_data.temp1 == 0xff)
		sc->sc_sensor[APS_SENSOR_TEMP1].state = ENVSYS_SINVALID;
	else {
		/* convert to micro (mu) degrees */
		temp = sc->aps_data.temp1 * 1000000;	
		/* convert to kelvin */
		temp += 273150000; 
		sc->sc_sensor[APS_SENSOR_TEMP1].value_cur = temp;
		sc->sc_sensor[APS_SENSOR_TEMP1].state = ENVSYS_SVALID;
	}

	if (sc->aps_data.temp2 == 0xff)
		sc->sc_sensor[APS_SENSOR_TEMP2].state = ENVSYS_SINVALID;
	else {
		/* convert to micro (mu) degrees */
		temp = sc->aps_data.temp2 * 1000000;	
		/* convert to kelvin */
		temp += 273150000; 
		sc->sc_sensor[APS_SENSOR_TEMP2].value_cur = temp;
		sc->sc_sensor[APS_SENSOR_TEMP2].state = ENVSYS_SVALID;
	}

	sc->sc_sensor[APS_SENSOR_XVAR].value_cur = sc->aps_data.x_var;
	sc->sc_sensor[APS_SENSOR_YVAR].value_cur = sc->aps_data.y_var;
	sc->sc_sensor[APS_SENSOR_KBACT].value_cur =
	    (sc->aps_data.input &  APS_INPUT_KB) ? 1 : 0;
	sc->sc_sensor[APS_SENSOR_MSACT].value_cur =
	    (sc->aps_data.input & APS_INPUT_MS) ? 1 : 0;
	sc->sc_sensor[APS_SENSOR_LIDOPEN].value_cur =
	    (sc->aps_data.input & APS_INPUT_LIDOPEN) ? 1 : 0;
}

static void
aps_refresh(void *arg)
{
	struct aps_softc *sc = arg;

	aps_refresh_sensor_data(sc);
	callout_schedule(&sc->sc_callout, (hz) / 2);
}

static bool
aps_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct aps_softc *sc = device_private(dv);

	callout_stop(&sc->sc_callout);

	return true;
}

static bool
aps_resume(device_t dv, const pmf_qual_t *qual)
{
	struct aps_softc *sc = device_private(dv);
	unsigned char iobuf[16];

	/*
	 * Redo the init sequence on resume, because APS is 
	 * as forgetful as it is deaf.
	 */

	/* get APS mode */
	iobuf[APS_CMD] = 0x13;
	if (aps_do_io(sc->sc_iot, sc->sc_ioh, iobuf, APS_WRITE_0, APS_READ_1)
	    || aps_init(sc))
		aprint_error_dev(dv, "failed to wake up\n");
	else
		callout_schedule(&sc->sc_callout, (hz) / 2);

	return true;
}

MODULE(MODULE_CLASS_DRIVER, aps, "sysmon_envsys");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
aps_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		return config_init_component(cfdriver_ioconf_aps,
		    cfattach_ioconf_aps, cfdata_ioconf_aps);
#else
		return 0;
#endif
	case MODULE_CMD_FINI:
#ifdef _MODULE
		return config_fini_component(cfdriver_ioconf_aps,
		    cfattach_ioconf_aps, cfdata_ioconf_aps);
#else
		return 0;
#endif
	default:
		return ENOTTY;
	}
}
