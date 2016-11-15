/*	$OpenBSD: fins.c,v 1.1 2008/03/19 19:33:09 deraadt Exp $	*/
/*	$NetBSD: finsio_isa.c,v 1.8 2015/04/23 23:23:00 pgoyette Exp $	*/

/*
 * Copyright (c) 2008 Juan Romero Pardines
 * Copyright (c) 2007, 2008 Geoff Steckel
 * Copyright (c) 2005, 2006 Mark Kettenis
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
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: finsio_isa.c,v 1.8 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/sysmon/sysmonvar.h>

/* Derived from LM78 code. Only handles chips attached to ISA bus */

/*
 * Fintek F71805/F71883 Super I/O datasheets:
 * http://www.fintek.com.tw/files/productfiles/F71805F_V025.pdf
 * http://www.fintek.com.tw/files/productfiles/F71883_V026P.pdf
 *
 * This chip is a multi-io chip with many functions.
 * Each function may be relocated in I/O space by the BIOS.
 * The base address (2E or 4E) accesses a configuration space which
 * has pointers to the individual functions. The config space must be
 * unlocked with a cookie and relocked afterwards. The chip ID is stored
 * in config space so it is not normally visible.
 *
 * The voltage dividers specified are from reading the chips on one board.
 * There is no way to determine what they are in the general case.
 */

#define FINSIO_UNLOCK	0x87	/* magic constant - write 2x to select chip */
#define FINSIO_LOCK	0xaa	/* magic constant - write 1x to deselect reg */

#define FINSIO_FUNC_SEL	0x07	/* select which subchip to access */
#  define FINSIO_FUNC_HWMON 0x4

/* ISA registers index to an internal register space on chip */
#define FINSIO_DECODE_SIZE (8)
#define FINSIO_DECODE_MASK (FINSIO_DECODE_SIZE - 1)
#define FINSIO_ADDR	5	/* global configuration index */
#define FINSIO_DATA	6	/* and data registers */

/* Global configuration registers */
#define FINSIO_MANUF	0x23	/* manufacturer ID */
# define FINTEK_ID	0x1934
#define FINSIO_CHIP	0x20	/* chip ID */
# define FINSIO_IDF71805	0x0406
# define FINSIO_IDF71806 	0x0341	/* F71872 and F1806 F/FG */
# define FINSIO_IDF71883	0x0541	/* F71882 and F1883 */
# define FINSIO_IDF71862 	0x0601	/* F71862FG */
# define FINSIO_IDF8000 	0x0581	/* F8000 */

/* in bank sensors of config space */
#define FINSIO_SENSADDR	0x60	/* sensors assigned I/O address (2 bytes) */

#define FINSIO_HWMON_CONF	0x01	/* Hardware Monitor Config. Register */

/* in sensors space */
#define FINSIO_TMODE	0x01	/* temperature mode reg */

#define FINSIO_MAX_SENSORS	20
/*
 * Fintek chips typically measure voltages using 8mv steps.
 * To measure higher voltages the input is attenuated with (external)
 * resistors.  Negative voltages are measured using inverting op amps
 * and resistors.  So we have to convert the sensor values back to
 * real voltages by applying the appropriate resistor factor.
 */
#define FRFACT_NONE	8000
#define FRFACT(x, y)	(FRFACT_NONE * ((x) + (y)) / (y))
#define FNRFACT(x, y)	(-FRFACT_NONE * (x) / (y))

#if defined(FINSIODEBUG)
#define DPRINTF(x)		do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

struct finsio_softc {
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	struct sysmon_envsys *sc_sme;
	envsys_data_t sc_sensor[FINSIO_MAX_SENSORS];
	struct finsio_sensor *sc_finsio_sensors;

	u_int sc_tempsel;
};

struct finsio_sensor {
	const char *fs_desc;
	u_int fs_type;
	uint8_t fs_aux;
	uint8_t fs_reg;
	void (*fs_refresh)(struct finsio_softc *, envsys_data_t *);
	int fs_rfact;
};

static int 	finsio_isa_match(device_t, cfdata_t, void *);
static void 	finsio_isa_attach(device_t, device_t, void *);
static int 	finsio_isa_detach(device_t, int);

static void	finsio_enter(bus_space_tag_t, bus_space_handle_t);
static void 	finsio_exit(bus_space_tag_t, bus_space_handle_t);
static uint8_t 	finsio_readreg(bus_space_tag_t, bus_space_handle_t, int);
static void 	finsio_writereg(bus_space_tag_t, bus_space_handle_t, int, int);

static void 	finsio_refresh(struct sysmon_envsys *, envsys_data_t *);
static void 	finsio_refresh_volt(struct finsio_softc *, envsys_data_t *);
static void 	finsio_refresh_temp(struct finsio_softc *, envsys_data_t *);
static void 	finsio_refresh_fanrpm(struct finsio_softc *, envsys_data_t *);

CFATTACH_DECL_NEW(finsio, sizeof(struct finsio_softc),
    finsio_isa_match, finsio_isa_attach, finsio_isa_detach, NULL);

/* Sensors available in F71805/F71806 */
static struct finsio_sensor f71805_sensors[] = {
	/* Voltage */
	{ 
		.fs_desc = "+3.3V",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x10,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT(100, 100)
	},
	{
		.fs_desc = "Vtt",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x11,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT_NONE
	},
	{
		.fs_desc = "Vram",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x12,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT(100, 100)
	},
	{
		.fs_desc = "Vchips",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x13,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT(47, 100)
	},
	{
		.fs_desc = "+5V",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x14,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT(200, 47)
	},
	{
		.fs_desc = "+12V",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x15,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT(200, 20)
	},
	{
		.fs_desc = "Vcc 1.5V",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x16,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT_NONE
	},
	{
		.fs_desc = "VCore",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x17,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT_NONE
	},
	{
		.fs_desc = "Vsb",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x18,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT(200, 47)
	},
	{
		.fs_desc = "Vsbint",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x19,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT(200, 47)
	},
	{
		.fs_desc = "Vbat",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x1a,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT(200, 47)
	},
	/* Temperature */
	{
		.fs_desc = "Temp1",
		.fs_type = ENVSYS_STEMP,
		.fs_aux = 0x01,
		.fs_reg = 0x1b,
		.fs_refresh = finsio_refresh_temp,
		.fs_rfact = 0
	},
	{
		.fs_desc = "Temp2",
		.fs_type = ENVSYS_STEMP,
		.fs_aux = 0x02,
		.fs_reg = 0x1c,
		.fs_refresh = finsio_refresh_temp,
		.fs_rfact = 0
	},
	{
		.fs_desc = "Temp3",
		.fs_type = ENVSYS_STEMP,
		.fs_aux = 0x04,
		.fs_reg = 0x1d,
		.fs_refresh = finsio_refresh_temp,
		.fs_rfact = 0
	},
	/* Fans */
	{
		.fs_desc = "Fan1",
		.fs_type = ENVSYS_SFANRPM,
		.fs_aux = 0,
		.fs_reg = 0x20,
		.fs_refresh = finsio_refresh_fanrpm,
		.fs_rfact = 0
	},
	{
		.fs_desc = "Fan2",
		.fs_type = ENVSYS_SFANRPM,
		.fs_aux = 0,
		.fs_reg = 0x22,
		.fs_refresh = finsio_refresh_fanrpm,
		.fs_rfact = 0
	},
	{
		.fs_desc = "Fan3",
		.fs_type = ENVSYS_SFANRPM,
		.fs_aux = 0,
		.fs_reg = 0x24,
		.fs_refresh = finsio_refresh_fanrpm,
		.fs_rfact = 0
	},

	{	.fs_desc = NULL }
};

/* Sensors available in F71862/F71882/F71883 */
static struct finsio_sensor f71883_sensors[] = {
	/* Voltage */
	{
		.fs_desc = "+3.3V",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x20,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT(100, 100)
	},
	{
		.fs_desc = "Vcore",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x21,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT_NONE
	},
	{
		.fs_desc = "VIN2",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x22,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT(100, 100)
	},
	{
		.fs_desc = "VIN3",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x23,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT(47, 100)
	},
	{
		.fs_desc = "VIN4",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x24,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT(200, 47)
	},
	{
		.fs_desc = "VIN5",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x25,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT(200, 20)
	},
	{
		.fs_desc = "VIN6",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x26,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT(100, 100)
	},
	{
		.fs_desc = "VSB +3.3V",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x27,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT(200, 47)
	},
	{
		.fs_desc = "VBAT",
		.fs_type = ENVSYS_SVOLTS_DC,
		.fs_aux = 0,
		.fs_reg = 0x28,
		.fs_refresh = finsio_refresh_volt,
		.fs_rfact = FRFACT(200, 47)
	},
	/* Temperature */
	{
		.fs_desc = "Temp1",
		.fs_type = ENVSYS_STEMP,
		.fs_aux = 0x1,
		.fs_reg = 0x72,
		.fs_refresh = finsio_refresh_temp,
		.fs_rfact = 0
	},
	{
		.fs_desc = "Temp2",
		.fs_type = ENVSYS_STEMP,
		.fs_aux = 0x2,
		.fs_reg = 0x74,
		.fs_refresh = finsio_refresh_temp,
		.fs_rfact = 0
	},
	{
		.fs_desc = "Temp3",
		.fs_type = ENVSYS_STEMP,
		.fs_aux = 0x4,
		.fs_reg = 0x76,
		.fs_refresh = finsio_refresh_temp,
		.fs_rfact = 0
	},
	/* Fan */
	{
		.fs_desc = "Fan1",
		.fs_type = ENVSYS_SFANRPM,
		.fs_aux = 0,
		.fs_reg = 0xa0,
		.fs_refresh = finsio_refresh_fanrpm,
		.fs_rfact = 0
	},
	{
		.fs_desc = "Fan2",
		.fs_type = ENVSYS_SFANRPM,
		.fs_aux = 0,
		.fs_reg = 0xb0,
		.fs_refresh = finsio_refresh_fanrpm,
		.fs_rfact = 0
	},
	{
		.fs_desc = "Fan3",
		.fs_type = ENVSYS_SFANRPM,
		.fs_aux = 0,
		.fs_reg = 0xc0,
		.fs_refresh = finsio_refresh_fanrpm,
		.fs_rfact = 0
	},
	{
		.fs_desc = "Fan4",
		.fs_type = ENVSYS_SFANRPM,
		.fs_aux = 0,
		.fs_reg = 0xd0,
		.fs_refresh = finsio_refresh_fanrpm,
		.fs_rfact = 0
	},

	{	.fs_desc = NULL }
};

static int
finsio_isa_match(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh;
	uint16_t val;

        /* Must supply an address */
	if (ia->ia_nio < 1)
		return 0;

	if (ISA_DIRECT_CONFIG(ia))
		return 0;

	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return 0;

	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr, 2, 0, &ioh))
		return 0;

	finsio_enter(ia->ia_iot, ioh);
	/* Find out Manufacturer ID */
	val = finsio_readreg(ia->ia_iot, ioh, FINSIO_MANUF) << 8;
	val |= finsio_readreg(ia->ia_iot, ioh, FINSIO_MANUF + 1);
	finsio_exit(ia->ia_iot, ioh);
	bus_space_unmap(ia->ia_iot, ioh, 2);

	if (val != FINTEK_ID)
		return 0;

	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = 2;
	ia->ia_niomem = 0;
	ia->ia_nirq = 0;
	ia->ia_ndrq = 0;

	return 1;
}

static void
finsio_isa_attach(device_t parent, device_t self, void *aux)
{
	struct finsio_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh;
	uint16_t hwmon_baddr, chipid, cr;
	int i, rv = 0;

	aprint_naive("\n");

	sc->sc_iot = ia->ia_iot;

	/* Map Super I/O configuration space */
	if (bus_space_map(sc->sc_iot, ia->ia_io[0].ir_addr, 2, 0, &ioh)) {
		aprint_error(": can't map configuration I/O space\n");
		return;
	}

	finsio_enter(sc->sc_iot, ioh);
	/* Get the Chip ID */
	chipid = finsio_readreg(sc->sc_iot, ioh, FINSIO_CHIP) << 8;
	chipid |= finsio_readreg(sc->sc_iot, ioh, FINSIO_CHIP + 1);
	/* 
	 * Select the Hardware Monitor LDN to find out the I/O
	 * address space.
	 */
	finsio_writereg(sc->sc_iot, ioh, FINSIO_FUNC_SEL, FINSIO_FUNC_HWMON);
	hwmon_baddr = finsio_readreg(sc->sc_iot, ioh, FINSIO_SENSADDR) << 8;
	hwmon_baddr |= finsio_readreg(sc->sc_iot, ioh, FINSIO_SENSADDR + 1);
	finsio_exit(sc->sc_iot, ioh);
	bus_space_unmap(sc->sc_iot, ioh, 2);

	/*
	 * The address decoder ignores the bottom 3 bits, so do we.
	 */
	hwmon_baddr &= ~FINSIO_DECODE_MASK;

	switch (chipid) {
	case FINSIO_IDF71805:
		sc->sc_finsio_sensors = f71805_sensors;
		aprint_normal(": Fintek F71805 Super I/O\n");
		break;
	case FINSIO_IDF71806:
		sc->sc_finsio_sensors = f71805_sensors;
		aprint_normal(": Fintek F71806/F71872 Super I/O\n");
		break;
	case FINSIO_IDF71862:
		sc->sc_finsio_sensors = f71883_sensors;
		aprint_normal(": Fintek F71862 Super I/O\n");
		break;
	case FINSIO_IDF71883:
		sc->sc_finsio_sensors = f71883_sensors;
		aprint_normal(": Fintek F71882/F71883 Super I/O\n");
		break;
	case FINSIO_IDF8000:
		sc->sc_finsio_sensors = f71883_sensors;
		aprint_normal(": ASUS F8000 Super I/O\n");
		break;
	default:
		/* 
		 * Unknown Chip ID, assume the same register layout
		 * than F71805 for now.
		 */
		sc->sc_finsio_sensors = f71805_sensors;
		aprint_normal(": Fintek Super I/O (unknown chip ID %x)\n",
		    chipid);
		break;
	}

	/* Map Hardware Monitor I/O space */
	if (bus_space_map(sc->sc_iot, hwmon_baddr, FINSIO_DECODE_SIZE,
	    0, &sc->sc_ioh)) {
		aprint_error(": can't map hwmon I/O space\n");
		return;
	}

	/* 
	 * Enable Hardware monitoring for fan/temperature and
	 * voltage sensors.
	 */
	cr = finsio_readreg(sc->sc_iot, sc->sc_ioh, FINSIO_HWMON_CONF);
	finsio_writereg(sc->sc_iot, sc->sc_ioh, FINSIO_HWMON_CONF, cr | 0x3);

	/* Find out the temperature mode */
	sc->sc_tempsel = finsio_readreg(sc->sc_iot, sc->sc_ioh, FINSIO_TMODE);

	/* 
	 * Initialize and attach sensors with sysmon_envsys(9).
	 */
	sc->sc_sme = sysmon_envsys_create();
	for (i = 0; sc->sc_finsio_sensors[i].fs_desc; i++) {
		sc->sc_sensor[i].state = ENVSYS_SINVALID;
		sc->sc_sensor[i].units = sc->sc_finsio_sensors[i].fs_type;
		if (sc->sc_sensor[i].units == ENVSYS_SVOLTS_DC)
			sc->sc_sensor[i].flags = ENVSYS_FCHANGERFACT;
		strlcpy(sc->sc_sensor[i].desc, sc->sc_finsio_sensors[i].fs_desc,
			sizeof(sc->sc_sensor[i].desc));
		if (sysmon_envsys_sensor_attach(sc->sc_sme,
						&sc->sc_sensor[i]))
			goto fail;
	}

	sc->sc_sme->sme_name = device_xname(self);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = finsio_refresh;
	if ((rv = sysmon_envsys_register(sc->sc_sme))) {
		aprint_error(": unable to register with sysmon (%d)\n", rv);
		goto fail;
	}

	aprint_normal_dev(self,
	    "Hardware Monitor registers at 0x%x\n", hwmon_baddr);
	return;

fail:
	sysmon_envsys_destroy(sc->sc_sme);
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, FINSIO_DECODE_SIZE);
}

static int
finsio_isa_detach(device_t self, int flags)
{
	struct finsio_softc *sc = device_private(self);

	sysmon_envsys_unregister(sc->sc_sme);
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, FINSIO_DECODE_SIZE);
	return 0;
}

/* Enter Super I/O configuration mode */
static void
finsio_enter(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	bus_space_write_1(iot, ioh, FINSIO_ADDR, FINSIO_UNLOCK);
	bus_space_write_1(iot, ioh, FINSIO_ADDR, FINSIO_UNLOCK);
}

/* Exit Super I/O configuration mode */
static void
finsio_exit(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	bus_space_write_1(iot, ioh, FINSIO_ADDR, FINSIO_LOCK);
}

static uint8_t
finsio_readreg(bus_space_tag_t iot, bus_space_handle_t ioh, int reg)
{
	bus_space_write_1(iot, ioh, FINSIO_ADDR, reg);
	return bus_space_read_1(iot, ioh, FINSIO_DATA);
}

static void
finsio_writereg(bus_space_tag_t iot, bus_space_handle_t ioh, int reg, int val)
{
	bus_space_write_1(iot, ioh, FINSIO_ADDR, reg);
	bus_space_write_1(iot, ioh, FINSIO_DATA, val);
}

static void
finsio_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct finsio_softc *sc = sme->sme_cookie;
	int i = edata->sensor;

	sc->sc_finsio_sensors[i].fs_refresh(sc, edata);
}

static void
finsio_refresh_volt(struct finsio_softc *sc, envsys_data_t *edata)
{
	struct finsio_sensor *fs = &sc->sc_finsio_sensors[edata->sensor];
	int data;

	data = finsio_readreg(sc->sc_iot, sc->sc_ioh, fs->fs_reg);
	DPRINTF(("%s: data 0x%x\n", __func__, data));

	if (data == 0xff || data == 0)
		edata->state = ENVSYS_SINVALID;
	else {
		edata->state = ENVSYS_SVALID;
		if (edata->rfact)
			edata->value_cur = data * edata->rfact;
		else
			edata->value_cur = data * fs->fs_rfact;
	}
}

/* The BIOS seems to add a fudge factor to the CPU temp of +5C */
static void
finsio_refresh_temp(struct finsio_softc *sc, envsys_data_t *edata)
{
	struct finsio_sensor *fs = &sc->sc_finsio_sensors[edata->sensor];
	u_int data;
	u_int llmax;

	/*
	 * The data sheet says that the range of the temperature
	 * sensor is between 0 and 127 or 140 degrees C depending on
	 * what kind of sensor is used.
	 * A disconnected sensor seems to read over 110 or so.
	 */
	data = finsio_readreg(sc->sc_iot, sc->sc_ioh, fs->fs_reg) & 0xFF;
	DPRINTF(("%s: data 0x%x\n", __func__, data));

	llmax = (sc->sc_tempsel & fs->fs_aux) ? 111 : 128;
	if (data == 0 || data >= llmax) 	/* disconnected? */
		edata->state = ENVSYS_SINVALID;
	else {
		edata->state = ENVSYS_SVALID;
		edata->value_cur = data * 1000000 + 273150000;
	}
}

/* fan speed appears to be a 12-bit number */
static void
finsio_refresh_fanrpm(struct finsio_softc *sc, envsys_data_t *edata)
{
	struct finsio_sensor *fs = &sc->sc_finsio_sensors[edata->sensor];
	int data;

	data = finsio_readreg(sc->sc_iot, sc->sc_ioh, fs->fs_reg) << 8;
	data |= finsio_readreg(sc->sc_iot, sc->sc_ioh, fs->fs_reg + 1);
	DPRINTF(("%s: data 0x%x\n", __func__, data));

	if (data >= 0xfff)
		edata->state = ENVSYS_SINVALID;
	else {
		edata->value_cur = 1500000 / data;
		edata->state = ENVSYS_SVALID;
	}
}

MODULE(MODULE_CLASS_DRIVER, finsio, "sysmon_envsys");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
finsio_modcmd(modcmd_t cmd, void *opaque)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_finsio,
		    cfattach_ioconf_finsio, cfdata_ioconf_finsio);
#endif
		return error;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_finsio,
		    cfattach_ioconf_finsio, cfdata_ioconf_finsio);
#endif
		return error;
	default:
		return ENOTTY;
	}
}
