/*	$NetBSD: itesio_isa.c,v 1.25 2015/04/23 23:23:00 pgoyette Exp $ */
/*	Derived from $OpenBSD: it.c,v 1.19 2006/04/10 00:57:54 deraadt Exp $	*/

/*
 * Copyright (c) 2006-2007 Juan Romero Pardines <xtraeme@netbsd.org>
 * Copyright (c) 2003 Julien Bordet <zejames@greyhats.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITD TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITD TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for the iTE IT87xxF Super I/O. Currently supporting
 * the Environmental Controller to monitor the sensors and the
 * Watchdog Timer.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: itesio_isa.c,v 1.25 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/sysmon/sysmonvar.h>

#include <dev/isa/itesio_isavar.h>

#define IT_VOLTSTART_IDX 	3 	/* voltage start index */
#define IT_FANSTART_IDX 	12 	/* fan start index */

#if defined(ITESIO_DEBUG)
#define DPRINTF(x)		do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

/*
 * IT87-compatible chips can typically measure voltages up to 4.096 V.
 * To measure higher voltages the input is attenuated with (external)
 * resistors.  Negative voltages are measured using a reference
 * voltage.  So we have to convert the sensor values back to real
 * voltages by applying the appropriate resistor factor.
 */
#define RFACT_NONE	10000
#define RFACT(x, y)	(RFACT_NONE * ((x) + (y)) / (y))

/* autoconf(9) functions */
static int	itesio_isa_match(device_t, cfdata_t, void *);
static void	itesio_isa_attach(device_t, device_t, void *);
static int	itesio_isa_detach(device_t, int);

CFATTACH_DECL_NEW(itesio, sizeof(struct itesio_softc),
    itesio_isa_match, itesio_isa_attach, itesio_isa_detach, NULL);

/* driver functions */
static uint8_t	itesio_ecreadreg(struct itesio_softc *, int);
static void	itesio_ecwritereg(struct itesio_softc *, int, int);
static uint8_t	itesio_readreg(bus_space_tag_t, bus_space_handle_t, int);
static void	itesio_writereg(bus_space_tag_t, bus_space_handle_t, int, int);
static void	itesio_enter(bus_space_tag_t, bus_space_handle_t);
static void	itesio_exit(bus_space_tag_t, bus_space_handle_t);

/* sysmon_envsys(9) glue */
static void	itesio_setup_sensors(struct itesio_softc *);
static void	itesio_refresh_temp(struct itesio_softc *, envsys_data_t *);
static void	itesio_refresh_volts(struct itesio_softc *, envsys_data_t *);
static void	itesio_refresh_fans(struct itesio_softc *, envsys_data_t *);
static void	itesio_refresh(struct sysmon_envsys *, envsys_data_t *);

/* sysmon_wdog glue */
static bool	itesio_wdt_suspend(device_t, const pmf_qual_t *);
static int	itesio_wdt_setmode(struct sysmon_wdog *);
static int 	itesio_wdt_tickle(struct sysmon_wdog *);

/* rfact values for voltage sensors */
static const int itesio_vrfact[] = {
	RFACT_NONE,	/* VCORE_A	*/
	RFACT_NONE,	/* VCORE_B	*/
	RFACT_NONE,	/* +3.3V	*/
	RFACT(68, 100),	/* +5V 		*/
	RFACT(30, 10),	/* +12V 	*/
	RFACT(21, 10),	/* -5V 		*/
	RFACT(83, 20),	/* -12V 	*/
	RFACT(68, 100),	/* STANDBY	*/
	RFACT_NONE	/* VBAT		*/
};

static int
itesio_isa_match(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh;
	uint16_t cr;

	/* Must supply an address */
	if (ia->ia_nio < 1)
		return 0;

	if (ISA_DIRECT_CONFIG(ia))
		return 0;

	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return 0;

	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr, 2, 0, &ioh))
		return 0;

	itesio_enter(ia->ia_iot, ioh);
	cr = (itesio_readreg(ia->ia_iot, ioh, ITESIO_CHIPID1) << 8);
	cr |= itesio_readreg(ia->ia_iot, ioh, ITESIO_CHIPID2);
	itesio_exit(ia->ia_iot, ioh);
	bus_space_unmap(ia->ia_iot, ioh, 2);

	switch (cr) {
	case ITESIO_ID8705:
	case ITESIO_ID8712:
	case ITESIO_ID8716:
	case ITESIO_ID8718:
	case ITESIO_ID8720:
	case ITESIO_ID8721:
	case ITESIO_ID8726:
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = 2;
		ia->ia_niomem = 0;
		ia->ia_nirq = 0;
		ia->ia_ndrq = 0;
		return 1;
	default:
		return 0;
	}
}

static void
itesio_isa_attach(device_t parent, device_t self, void *aux)
{
	struct itesio_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;
	int i;
	uint8_t cr;

	sc->sc_iot = ia->ia_iot;

	if (bus_space_map(sc->sc_iot, ia->ia_io[0].ir_addr, 2, 0,
			  &sc->sc_pnp_ioh)) {
		aprint_error(": can't map pnp i/o space\n");
		return;
	}

	aprint_naive("\n");

	/*
	 * Enter to the Super I/O MB PNP mode.
	 */
	itesio_enter(sc->sc_iot, sc->sc_pnp_ioh);
	/*
	 * Get info from the Super I/O Global Configuration Registers:
	 * Chip IDs and Device Revision.
	 */
	sc->sc_chipid = (itesio_readreg(sc->sc_iot, sc->sc_pnp_ioh,
	    ITESIO_CHIPID1) << 8);
	sc->sc_chipid |= itesio_readreg(sc->sc_iot, sc->sc_pnp_ioh,
	    ITESIO_CHIPID2);
	sc->sc_devrev = (itesio_readreg(sc->sc_iot, sc->sc_pnp_ioh,
	    ITESIO_DEVREV) & 0x0f);
	/*
	 * Select the EC LDN to get the Base Address.
	 */
	itesio_writereg(sc->sc_iot, sc->sc_pnp_ioh, ITESIO_LDNSEL,
	    ITESIO_EC_LDN);
	sc->sc_hwmon_baseaddr =
	    (itesio_readreg(sc->sc_iot, sc->sc_pnp_ioh, ITESIO_EC_MSB) << 8);
	sc->sc_hwmon_baseaddr |= itesio_readreg(sc->sc_iot, sc->sc_pnp_ioh,
	    ITESIO_EC_LSB);
	/*
	 * We are done, exit MB PNP mode.
	 */
	itesio_exit(sc->sc_iot, sc->sc_pnp_ioh);

	aprint_normal(": iTE IT%4xF Super I/O (rev %d)\n",
	    sc->sc_chipid, sc->sc_devrev);
	aprint_normal_dev(self, "Hardware Monitor registers at 0x%x\n",
	    sc->sc_hwmon_baseaddr);

	if (bus_space_map(sc->sc_iot, sc->sc_hwmon_baseaddr, 8, 0,
	    &sc->sc_ec_ioh)) {
		aprint_error_dev(self, "cannot map hwmon i/o space\n");
		goto out2;
	}

	sc->sc_hwmon_mapped = true;

	/* Activate monitoring */
	cr = itesio_ecreadreg(sc, ITESIO_EC_CONFIG);
	SET(cr, 0x01);
	itesio_ecwritereg(sc, ITESIO_EC_CONFIG, cr);

#ifdef notyet
	/* Enable beep alarms */
	cr = itesio_ecreadreg(sc, ITESIO_EC_BEEPEER);
	SET(cr, 0x02);	/* Voltage exceeds limit */
	SET(cr, 0x04);	/* Temperature exceeds limit */
	itesio_ecwritereg(sc, ITESIO_EC_BEEPEER, cr);
#endif

	/*
	 * Initialize and attach sensors.
	 */
	itesio_setup_sensors(sc);
	sc->sc_sme = sysmon_envsys_create();
	for (i = 0; i < IT_NUM_SENSORS; i++) {
		if (sysmon_envsys_sensor_attach(sc->sc_sme,
						&sc->sc_sensor[i])) {
			sysmon_envsys_destroy(sc->sc_sme);
			goto out;
		}
	}
	/*
	 * Hook into the system monitor.
	 */
	sc->sc_sme->sme_name = device_xname(self);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = itesio_refresh;
	
	if ((i = sysmon_envsys_register(sc->sc_sme))) {
		aprint_error_dev(self,
		    "unable to register with sysmon (%d)\n", i);
		sysmon_envsys_destroy(sc->sc_sme);
		goto out;
	}
	sc->sc_hwmon_enabled = true;

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	/* The IT8705 doesn't support the WDT */
	if (sc->sc_chipid == ITESIO_ID8705)
		goto out2;

	/*
	 * Initialize the watchdog timer.
	 */
	sc->sc_smw.smw_name = device_xname(self);
	sc->sc_smw.smw_cookie = sc;
	sc->sc_smw.smw_setmode = itesio_wdt_setmode;
	sc->sc_smw.smw_tickle = itesio_wdt_tickle;
	sc->sc_smw.smw_period = 60;

	if (sysmon_wdog_register(&sc->sc_smw)) {
		aprint_error_dev(self, "unable to register watchdog timer\n");
		goto out2;
	}
	sc->sc_wdt_enabled = true;
	aprint_normal_dev(self, "Watchdog Timer present\n");

	pmf_device_deregister(self);
	if (!pmf_device_register(self, itesio_wdt_suspend, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;

out:
	bus_space_unmap(sc->sc_iot, sc->sc_ec_ioh, 8);
out2:
	bus_space_unmap(sc->sc_iot, sc->sc_pnp_ioh, 2);
}

static int
itesio_isa_detach(device_t self, int flags)
{
	struct itesio_softc *sc = device_private(self);

	if (sc->sc_hwmon_enabled)
		sysmon_envsys_unregister(sc->sc_sme);
	if (sc->sc_hwmon_mapped)
		bus_space_unmap(sc->sc_iot, sc->sc_ec_ioh, 8);
	if (sc->sc_wdt_enabled) {
		sysmon_wdog_unregister(&sc->sc_smw);
		bus_space_unmap(sc->sc_iot, sc->sc_pnp_ioh, 2);
	}

	return 0;
}

static bool
itesio_wdt_suspend(device_t dev, const pmf_qual_t *qual)
{
	struct itesio_softc *sc = device_private(dev);

	/* Don't allow suspend if watchdog is armed */
	if ((sc->sc_smw.smw_mode & WDOG_MODE_MASK) != WDOG_MODE_DISARMED)
		return false;
	return true;
}

/*
 * Functions to read/write to the Environmental Controller.
 */
static uint8_t
itesio_ecreadreg(struct itesio_softc *sc, int reg)
{
	bus_space_write_1(sc->sc_iot, sc->sc_ec_ioh, ITESIO_EC_ADDR, reg);
	return bus_space_read_1(sc->sc_iot, sc->sc_ec_ioh, ITESIO_EC_DATA);
}

static void
itesio_ecwritereg(struct itesio_softc *sc, int reg, int val)
{
	bus_space_write_1(sc->sc_iot, sc->sc_ec_ioh, ITESIO_EC_ADDR, reg);
	bus_space_write_1(sc->sc_iot, sc->sc_ec_ioh, ITESIO_EC_DATA, val);
}

/*
 * Functions to enter/exit/read/write to the Super I/O.
 */
static uint8_t
itesio_readreg(bus_space_tag_t iot, bus_space_handle_t ioh, int reg)
{
	bus_space_write_1(iot, ioh, ITESIO_ADDR, reg);
	return bus_space_read_1(iot, ioh, ITESIO_DATA);
}

static void
itesio_writereg(bus_space_tag_t iot, bus_space_handle_t ioh, int reg, int val)
{
	bus_space_write_1(iot, ioh, ITESIO_ADDR, reg);
	bus_space_write_1(iot, ioh, ITESIO_DATA, val);
}

static void
itesio_enter(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	bus_space_write_1(iot, ioh, ITESIO_ADDR, 0x87);
	bus_space_write_1(iot, ioh, ITESIO_ADDR, 0x01);
	bus_space_write_1(iot, ioh, ITESIO_ADDR, 0x55);
	bus_space_write_1(iot, ioh, ITESIO_ADDR, 0x55);
}

static void
itesio_exit(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	bus_space_write_1(iot, ioh, ITESIO_ADDR, 0x02);
	bus_space_write_1(iot, ioh, ITESIO_DATA, 0x02);
}


#define COPYDESCR(x, y)				\
	do {					\
		strlcpy((x), (y), sizeof(x));	\
	} while (0)
/*
 * sysmon_envsys(9) glue.
 */
static void
itesio_setup_sensors(struct itesio_softc *sc)
{
	int i;

	/* temperatures */
	for (i = 0; i < IT_VOLTSTART_IDX; i++)
		sc->sc_sensor[i].units = ENVSYS_STEMP;

	COPYDESCR(sc->sc_sensor[0].desc, "CPU Temp");
	COPYDESCR(sc->sc_sensor[1].desc, "System Temp");
	COPYDESCR(sc->sc_sensor[2].desc, "Aux Temp");

	/* voltages */
	for (i = IT_VOLTSTART_IDX; i < IT_FANSTART_IDX; i++) {
		sc->sc_sensor[i].units = ENVSYS_SVOLTS_DC;
		sc->sc_sensor[i].flags = ENVSYS_FCHANGERFACT;
	}

	COPYDESCR(sc->sc_sensor[3].desc, "VCORE_A");
	COPYDESCR(sc->sc_sensor[4].desc, "VCORE_B");
	COPYDESCR(sc->sc_sensor[5].desc, "+3.3V");
	COPYDESCR(sc->sc_sensor[6].desc, "+5V");
	COPYDESCR(sc->sc_sensor[7].desc, "+12V");
	COPYDESCR(sc->sc_sensor[8].desc, "-5V");
	COPYDESCR(sc->sc_sensor[9].desc, "-12V");
	COPYDESCR(sc->sc_sensor[10].desc, "STANDBY");
	COPYDESCR(sc->sc_sensor[11].desc, "VBAT");

	/* fans */
	for (i = IT_FANSTART_IDX; i < IT_NUM_SENSORS; i++)
		sc->sc_sensor[i].units = ENVSYS_SFANRPM;

	COPYDESCR(sc->sc_sensor[12].desc, "CPU Fan");
	COPYDESCR(sc->sc_sensor[13].desc, "System Fan");
	COPYDESCR(sc->sc_sensor[14].desc, "Aux Fan");

	/* all */
	for (i = 0; i < IT_NUM_SENSORS; i++)
		sc->sc_sensor[i].state = ENVSYS_SINVALID;
}
#undef COPYDESCR

static void
itesio_refresh_temp(struct itesio_softc *sc, envsys_data_t *edata)
{
	int sdata;

	sdata = itesio_ecreadreg(sc, ITESIO_EC_SENSORTEMPBASE + edata->sensor);
	/* sensor is not connected or reporting invalid data */
	if (sdata == 0 || sdata >= 0xfa) {
		edata->state = ENVSYS_SINVALID;
		return;
	}

	DPRINTF(("%s: sdata[temp%d] 0x%x\n", __func__, edata->sensor, sdata));
	/* Convert temperature to uK */
	edata->value_cur = sdata * 1000000 + 273150000;
	edata->state = ENVSYS_SVALID;
}

static void
itesio_refresh_volts(struct itesio_softc *sc, envsys_data_t *edata)
{
	uint8_t vbatcr = 0;
	int i, sdata;

	i = edata->sensor - IT_VOLTSTART_IDX;

	sdata = itesio_ecreadreg(sc, ITESIO_EC_SENSORVOLTBASE + i);
	/* not connected */
	if (sdata == 0 || sdata == 0xff) {
		edata->state = ENVSYS_SINVALID;
		return;
	}

	/* 
	 * update VBAT voltage reading every time we read it, to get
	 * latest value.
	 */
	if (i == 8) {
		vbatcr = itesio_ecreadreg(sc, ITESIO_EC_CONFIG);
		SET(vbatcr, ITESIO_EC_UPDATEVBAT);
		itesio_ecwritereg(sc, ITESIO_EC_CONFIG, vbatcr);
	}

	DPRINTF(("%s: sdata[volt%d] 0x%x\n", __func__, i, sdata));

	/* voltage returned as (mV << 4) */
	edata->value_cur = (sdata << 4);
	/* negative values */
	if (i == 5 || i == 6)
		edata->value_cur -= ITESIO_EC_VREF;
	/* rfact is (factor * 10^4) */
	if (edata->rfact)
		edata->value_cur *= edata->rfact;
	else
		edata->value_cur *= itesio_vrfact[i];
	/* division by 10 gets us back to uVDC */
	edata->value_cur /= 10;
	if (i == 5 || i == 6)
		edata->value_cur += ITESIO_EC_VREF * 1000;

	edata->state = ENVSYS_SVALID;
}

static void
itesio_refresh_fans(struct itesio_softc *sc, envsys_data_t *edata)
{
	uint8_t mode = 0;
	uint16_t sdata = 0;
	int i, divisor, odivisor, ndivisor;

	i = edata->sensor - IT_FANSTART_IDX;
	divisor = odivisor = ndivisor = 0;

	if (sc->sc_chipid == ITESIO_ID8705 || sc->sc_chipid == ITESIO_ID8712) {
		/* 
		 * Use the Fan Tachometer Divisor Register for
		 * IT8705F and IT8712F.
		 */
		divisor = odivisor = ndivisor =
		    itesio_ecreadreg(sc, ITESIO_EC_FAN_TDR);
		sdata = itesio_ecreadreg(sc, ITESIO_EC_SENSORFANBASE + i);
		if (sdata == 0xff) {
			edata->state = ENVSYS_SINVALID;
			if (i == 2)
				ndivisor |= 0x40;
			else {
				ndivisor &= ~(7 << (i * 3));
				ndivisor |= ((divisor + 1) & 7) << (i * 3);
			}
		} else {
			if (i == 2)
				divisor = divisor & 1 ? 3 : 1;

			if ((sdata << (divisor & 7)) == 0)
				edata->state = ENVSYS_SINVALID;
			else {
				edata->value_cur =
				    1350000 / (sdata << (divisor & 7));
				edata->state = ENVSYS_SVALID;
			}
		}
		DPRINTF(("%s: 8bit sdata[fan%d] 0x%x div: 0x%x\n", __func__,
		    i, sdata, divisor));
		if (ndivisor != odivisor)
			itesio_ecwritereg(sc, ITESIO_EC_FAN_TDR, ndivisor);
	} else {
		mode = itesio_ecreadreg(sc, ITESIO_EC_FAN16_CER);
		sdata = itesio_ecreadreg(sc, ITESIO_EC_SENSORFANBASE + i);
		if (mode & (1 << i))
			sdata += (itesio_ecreadreg(sc,
			    ITESIO_EC_SENSORFANEXTBASE + i) << 8);
		edata->state = ENVSYS_SVALID;
		if (sdata == 0 ||
		    sdata == ((mode & (1 << i)) ? 0xffff : 0xff))
			edata->state = ENVSYS_SINVALID;
		else {
			edata->value_cur = 1350000 / 2 / sdata;
			edata->state = ENVSYS_SVALID;
		}
		DPRINTF(("%s: 16bit sdata[fan%d] 0x%x\n", __func__, i, sdata));
	}
}

static void
itesio_refresh(struct sysmon_envsys *sme, struct envsys_data *edata)
{
	struct itesio_softc *sc = sme->sme_cookie;

	if (edata->sensor < IT_VOLTSTART_IDX)
		itesio_refresh_temp(sc, edata);
	else if (edata->sensor >= IT_VOLTSTART_IDX &&
	         edata->sensor < IT_FANSTART_IDX)
		itesio_refresh_volts(sc, edata);
	else
		itesio_refresh_fans(sc, edata);
}

static int
itesio_wdt_setmode(struct sysmon_wdog *smw)
{
	struct itesio_softc *sc = smw->smw_cookie;
	int period = smw->smw_period;

	/* Enter MB PNP mode and select the WDT LDN */
	itesio_enter(sc->sc_iot, sc->sc_pnp_ioh);
	itesio_writereg(sc->sc_iot, sc->sc_pnp_ioh, ITESIO_LDNSEL,
	    ITESIO_WDT_LDN);

	if ((smw->smw_mode & WDOG_MODE_MASK) == WDOG_MODE_DISARMED) {
		/* Disable the watchdog */
		itesio_writereg(sc->sc_iot, sc->sc_pnp_ioh, ITESIO_WDT_CTL, 0);
		itesio_writereg(sc->sc_iot, sc->sc_pnp_ioh, ITESIO_WDT_CNF, 0);
		itesio_writereg(sc->sc_iot, sc->sc_pnp_ioh, ITESIO_WDT_TMO_MSB, 0);
		itesio_writereg(sc->sc_iot, sc->sc_pnp_ioh, ITESIO_WDT_TMO_LSB, 0);
	} else {
		/* Enable the watchdog */
		if (period > ITESIO_WDT_MAXTIMO || period < 1)
			period = smw->smw_period = ITESIO_WDT_MAXTIMO;

		period *= 2;

		/* set the timeout and start the watchdog */
		itesio_writereg(sc->sc_iot, sc->sc_pnp_ioh, ITESIO_WDT_TMO_MSB,
		    period >> 8);
		itesio_writereg(sc->sc_iot, sc->sc_pnp_ioh, ITESIO_WDT_TMO_LSB,
		    period & 0xff);
		itesio_writereg(sc->sc_iot, sc->sc_pnp_ioh, ITESIO_WDT_CNF,
		    ITESIO_WDT_CNF_SECS | ITESIO_WDT_CNF_KRST |
		    ITESIO_WDT_CNF_PWROK);
	}
	/* we are done, exit MB PNP mode */
	itesio_exit(sc->sc_iot, sc->sc_pnp_ioh);

	return 0;
}

static int
itesio_wdt_tickle(struct sysmon_wdog *smw)
{
	struct itesio_softc *sc = smw->smw_cookie;
	int period = smw->smw_period * 2;

	/* refresh timeout value and exit */
	itesio_enter(sc->sc_iot, sc->sc_pnp_ioh);
	itesio_writereg(sc->sc_iot, sc->sc_pnp_ioh, ITESIO_LDNSEL,
	    ITESIO_WDT_LDN);
	itesio_writereg(sc->sc_iot, sc->sc_pnp_ioh, ITESIO_WDT_TMO_MSB,
	    period >> 8);
	itesio_writereg(sc->sc_iot, sc->sc_pnp_ioh, ITESIO_WDT_TMO_LSB,
	    period & 0xff);
	itesio_exit(sc->sc_iot, sc->sc_pnp_ioh);

	return 0;
}

MODULE(MODULE_CLASS_DRIVER, itesio, "sysmon_envsys,sysmon_wdog");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
itesio_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		return config_init_component(cfdriver_ioconf_itesio,
		    cfattach_ioconf_itesio, cfdata_ioconf_itesio);
#else
		return 0;
#endif
	case MODULE_CMD_FINI:
#ifdef _MODULE
		return config_fini_component(cfdriver_ioconf_itesio,
		    cfattach_ioconf_itesio, cfdata_ioconf_itesio);
#else
		return 0;
#endif
	default:
		return ENOTTY;
	}
}
