/* $NetBSD: nsclpcsio_isa.c,v 1.31 2015/04/23 23:23:00 pgoyette Exp $ */

/*
 * Copyright (c) 2002
 * 	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
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

/*
 * National Semiconductor PC87366 LPC Super I/O driver.
 * Supported logical devices: GPIO, TMS, VLM.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nsclpcsio_isa.c,v 1.31 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/mutex.h>
#include <sys/gpio.h>
#include <sys/bus.h>
#include <sys/module.h>

/* Don't use gpio for now in the module */
#ifdef _MODULE
#undef NGPIO
#endif

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#ifndef _MODULE
#include "gpio.h"
#endif
#if NGPIO > 0
#include <dev/gpio/gpiovar.h>
#endif
#include <dev/sysmon/sysmonvar.h>

#define SIO_REG_SID	0x20	/* Super I/O ID */
#define SIO_SID_PC87366	0xE9	/* PC87366 is identified by 0xE9.*/

#define SIO_REG_SRID	0x27	/* Super I/O Revision */

#define SIO_REG_LDN	0x07	/* Logical Device Number */
#define SIO_LDN_FDC	0x00	/* Floppy Disk Controller (FDC) */
#define SIO_LDN_PP	0x01	/* Parallel Port (PP) */
#define SIO_LDN_SP2	0x02	/* Serial Port 2 with IR (SP2) */
#define SIO_LDN_SP1	0x03	/* Serial Port 1 (SP1) */
#define SIO_LDN_SWC	0x04	/* System Wake-Up Control (SWC) */
#define SIO_LDN_KBCM	0x05	/* Mouse Controller (KBC) */
#define SIO_LDN_KBCK	0x06	/* Keyboard Controller (KBC) */
#define SIO_LDN_GPIO	0x07	/* General-Purpose I/O (GPIO) Ports */
#define SIO_LDN_ACB	0x08	/* ACCESS.bus Interface (ACB) */
#define SIO_LDN_FSCM	0x09	/* Fan Speed Control and Monitor (FSCM) */
#define SIO_LDN_WDT	0x0A	/* WATCHDOG Timer (WDT) */
#define SIO_LDN_GMP	0x0B	/* Game Port (GMP) */
#define SIO_LDN_MIDI	0x0C	/* Musical Instrument Digital Interface */
#define SIO_LDN_VLM	0x0D	/* Voltage Level Monitor (VLM) */
#define SIO_LDN_TMS	0x0E	/* Temperature Sensor (TMS) */

#define SIO_REG_ACTIVE	0x30	/* Logical Device Activate Register */
#define SIO_ACTIVE_EN		0x01	/* enabled */

#define SIO_REG_IO_MSB	0x60	/* I/O Port Base, bits 15-8 */
#define SIO_REG_IO_LSB	0x61	/* I/O Port Base, bits 7-0 */

#define SIO_LDNUM	15	/* total number of logical devices */

/* Supported logical devices description */
static const struct {
	const char *ld_name;
	int ld_num;
	int ld_iosize;
} sio_ld[] = {
	{ "GPIO",	SIO_LDN_GPIO,	16 },
	{ "VLM",	SIO_LDN_VLM,	16 },
	{ "TMS",	SIO_LDN_TMS,	16 }
};

/* GPIO */
#define SIO_GPIO_PINSEL	0xf0
#define SIO_GPIO_PINCFG	0xf1
#define SIO_GPIO_PINEV	0xf2

#define	SIO_GPIO_CONF_OUTPUTEN	(1 << 0)
#define	SIO_GPIO_CONF_PUSHPULL	(1 << 1)
#define	SIO_GPIO_CONF_PULLUP	(1 << 2)

#define SIO_GPDO0	0x00
#define SIO_GPDI0	0x01
#define SIO_GPEVEN0	0x02
#define SIO_GPEVST0	0x03
#define SIO_GPDO1	0x04
#define SIO_GPDI1	0x05
#define SIO_GPEVEN1	0x06
#define SIO_GPEVST1	0x07
#define SIO_GPDO2	0x08
#define SIO_GPDI2	0x09
#define SIO_GPDO3	0x0a
#define SIO_GPDI3	0x0b

#define SIO_GPIO_NPINS	29

/* TMS */
#define SIO_TEVSTS	0x00	/* Temperature Event Status */
#define SIO_TEVSMI	0x02	/* Temperature Event to SMI */
#define SIO_TEVIRQ	0x04	/* Temperature Event to IRQ */
#define SIO_TMSCFG	0x08	/* TMS Configuration */
#define SIO_TMSBS	0x09	/* TMS Bank Select */
#define SIO_TCHCFST	0x0a	/* Temperature Channel Config and Status */
#define SIO_RDCHT	0x0b	/* Read Channel Temperature */
#define SIO_CHTH	0x0c	/* Channel Temperature High Limit */
#define SIO_CHTL	0x0d	/* Channel Temperature Low Limit */
#define SIO_CHOTL	0x0e	/* Channel Overtemperature Limit */

/* VLM */
#define SIO_VEVSTS0	0x00	/* Voltage Event Status 0 */
#define SIO_VEVSTS1	0x01	/* Voltage Event Status 1 */
#define SIO_VEVSMI0	0x02	/* Voltage Event to SMI 0 */
#define SIO_VEVSMI1	0x03	/* Voltage Event to SMI 1 */
#define SIO_VEVIRQ0	0x04	/* Voltage Event to IRQ 0 */
#define SIO_VEVIRQ1	0x05	/* Voltage Event to IRQ 1 */
#define SIO_VID 	0x06	/* Voltage ID */
#define SIO_VCNVR	0x07	/* Voltage Conversion Rate */
#define SIO_VLMCFG	0x08	/* VLM Configuration */
#define SIO_VLMBS	0x09	/* VLM Bank Select */
#define SIO_VCHCFST	0x0a	/* Voltage Channel Config and Status */
#define SIO_RDCHV	0x0b	/* Read Channel Voltage */
#define SIO_CHVH	0x0c	/* Channel Voltage High Limit */
#define SIO_CHVL	0x0d	/* Channel Voltage Low Limit */
#define SIO_OTSL	0x0e	/* Overtemperature Shutdown Limit */

#define SIO_REG_SIOCF1	0x21
#define SIO_REG_SIOCF2	0x22
#define SIO_REG_SIOCF3	0x23
#define SIO_REG_SIOCF4	0x24
#define SIO_REG_SIOCF5	0x25
#define SIO_REG_SIOCF8	0x28
#define SIO_REG_SIOCFA	0x2a
#define SIO_REG_SIOCFB	0x2b
#define SIO_REG_SIOCFC	0x2c
#define SIO_REG_SIOCFD	0x2d

#define SIO_VLM_OFF	3
#define SIO_NUM_SENSORS	(SIO_VLM_OFF + 14)
#define SIO_VREF	1235	/* 1000.0 * VREF */

struct nsclpcsio_softc {
	device_t sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	bus_space_handle_t sc_ld_ioh[SIO_LDNUM];
	int sc_ld_en[SIO_LDNUM];

	/* TMS and VLM */
	struct sysmon_envsys *sc_sme;
	envsys_data_t sc_sensor[SIO_NUM_SENSORS];

	kmutex_t sc_lock;
#if NGPIO > 0
	/* GPIO */
	struct gpio_chipset_tag sc_gpio_gc;
	struct gpio_pin sc_gpio_pins[SIO_GPIO_NPINS];
#endif
};

#define GPIO_READ(sc, reg)			\
	bus_space_read_1((sc)->sc_iot,			\
	    (sc)->sc_ld_ioh[SIO_LDN_GPIO], (reg))
#define GPIO_WRITE(sc, reg, val)		\
	bus_space_write_1((sc)->sc_iot,			\
	    (sc)->sc_ld_ioh[SIO_LDN_GPIO], (reg), (val))
#define TMS_WRITE(sc, reg, val)				\
	bus_space_write_1((sc)->sc_iot,			\
	    (sc)->sc_ld_ioh[SIO_LDN_TMS], (reg), (val))
#define TMS_READ(sc, reg)				\
	bus_space_read_1((sc)->sc_iot,			\
	    (sc)->sc_ld_ioh[SIO_LDN_TMS], (reg))
#define VLM_WRITE(sc, reg, val)				\
	bus_space_write_1((sc)->sc_iot,			\
	    (sc)->sc_ld_ioh[SIO_LDN_VLM], (reg), (val))
#define VLM_READ(sc, reg)				\
	bus_space_read_1((sc)->sc_iot,			\
	    (sc)->sc_ld_ioh[SIO_LDN_VLM], (reg))

static int	nsclpcsio_isa_match(device_t, cfdata_t, void *);
static void	nsclpcsio_isa_attach(device_t, device_t, void *);
static int	nsclpcsio_isa_detach(device_t, int);

CFATTACH_DECL_NEW(nsclpcsio_isa, sizeof(struct nsclpcsio_softc),
    nsclpcsio_isa_match, nsclpcsio_isa_attach, nsclpcsio_isa_detach, NULL);

static uint8_t	nsread(bus_space_tag_t, bus_space_handle_t, int);
static void	nswrite(bus_space_tag_t, bus_space_handle_t, int, uint8_t);
static int	nscheck(bus_space_tag_t, int);

static void	nsclpcsio_tms_init(struct nsclpcsio_softc *);
static void	nsclpcsio_vlm_init(struct nsclpcsio_softc *);
static void	nsclpcsio_refresh(struct sysmon_envsys *, envsys_data_t *);

#if NGPIO > 0
static void nsclpcsio_gpio_init(struct nsclpcsio_softc *);
static void nsclpcsio_gpio_pin_select(struct nsclpcsio_softc *, int);
static void nsclpcsio_gpio_pin_write(void *, int, int);
static int nsclpcsio_gpio_pin_read(void *, int);
static void nsclpcsio_gpio_pin_ctl(void *, int, int);
#endif

static uint8_t
nsread(bus_space_tag_t iot, bus_space_handle_t ioh, int idx)
{
	bus_space_write_1(iot, ioh, 0, idx);
	return bus_space_read_1(iot, ioh, 1);
}

static void
nswrite(bus_space_tag_t iot, bus_space_handle_t ioh, int idx, uint8_t data)
{
	bus_space_write_1(iot, ioh, 0, idx);
	bus_space_write_1(iot, ioh, 1, data);
}

static int
nscheck(bus_space_tag_t iot, int base)
{
	bus_space_handle_t ioh;
	int rv = 0;

	if (bus_space_map(iot, base, 2, 0, &ioh))
		return 0;

	/* XXX this is for PC87366 only for now */
	if (nsread(iot, ioh, SIO_REG_SID) == SIO_SID_PC87366)
		rv = 1;

	bus_space_unmap(iot, ioh, 2);
	return rv;
}

static int
nsclpcsio_isa_match(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	int iobase;

	if (ISA_DIRECT_CONFIG(ia))
		return 0;

	if (ia->ia_nio > 0 && ia->ia_io[0].ir_addr != ISA_UNKNOWN_PORT) {
		/* XXX check for legal iobase ??? */
		if (nscheck(ia->ia_iot, ia->ia_io[0].ir_addr)) {
			iobase = ia->ia_io[0].ir_addr;
			goto found;
		}
		return 0;
	}

	/* PC87366 has two possible locations depending on wiring */
	if (nscheck(ia->ia_iot, 0x2e)) {
		iobase = 0x2e;
		goto found;
	}
	if (nscheck(ia->ia_iot, 0x4e)) {
		iobase = 0x4e;
		goto found;
	}

	return 0;

found:
	ia->ia_nio = 1;
	ia->ia_io[0].ir_addr = iobase;
	ia->ia_io[0].ir_size = 2;
	ia->ia_niomem = 0;
	ia->ia_nirq = 0;
	ia->ia_ndrq = 0;

	return 1;
}

static struct sysmon_envsys *
nsclpcsio_envsys_init(struct nsclpcsio_softc *sc)
{
	int i;
	struct sysmon_envsys *sme;

	sme = sysmon_envsys_create();
	for (i = 0; i < SIO_NUM_SENSORS; i++) {
		sc->sc_sensor[i].state = ENVSYS_SINVALID;
		if (sysmon_envsys_sensor_attach(sme, &sc->sc_sensor[i]) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not attach sensor %d", i);
			goto err;
		}
	}

	/*
	 * Hook into the System Monitor.
	 */
	sme->sme_name = device_xname(sc->sc_dev);
	sme->sme_cookie = sc;
	sme->sme_refresh = nsclpcsio_refresh;

	if ((i = sysmon_envsys_register(sme)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to register with sysmon (%d)\n", i);
		goto err;
	}
	return sme;
err:
	sysmon_envsys_destroy(sme);
	return NULL;
}

static void
nsclpcsio_isa_attach(device_t parent, device_t self, void *aux)
{
	struct nsclpcsio_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;
#if NGPIO > 0
	struct gpiobus_attach_args gba;
#endif
	int i, iobase;

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);

	sc->sc_dev = self;
	sc->sc_iot = ia->ia_iot;
	iobase = ia->ia_io[0].ir_addr;

	if (bus_space_map(ia->ia_iot, iobase, 2, 0, &sc->sc_ioh)) {
		aprint_error(": can't map i/o space\n");
		return;
	}

	aprint_normal(": NSC PC87366 rev. 0x%d ",
	    nsread(sc->sc_iot, sc->sc_ioh, SIO_REG_SRID));

	/* Configure all supported logical devices */
	for (i = 0; i < __arraycount(sio_ld); i++) {
		sc->sc_ld_en[sio_ld[i].ld_num] = 0;

		/* Select the device and check if it's activated */
		nswrite(sc->sc_iot, sc->sc_ioh, SIO_REG_LDN, sio_ld[i].ld_num);
		if ((nsread(sc->sc_iot, sc->sc_ioh,
		    SIO_REG_ACTIVE) & SIO_ACTIVE_EN) == 0)
			continue;

		/* Map I/O space if necessary */
		if (sio_ld[i].ld_iosize != 0) {
			iobase = (nsread(sc->sc_iot, sc->sc_ioh,
			    SIO_REG_IO_MSB) << 8);
			iobase |= nsread(sc->sc_iot, sc->sc_ioh,
			    SIO_REG_IO_LSB);
			if (bus_space_map(sc->sc_iot, iobase,
			    sio_ld[i].ld_iosize, 0,
			    &sc->sc_ld_ioh[sio_ld[i].ld_num]))
				continue;
		}

		sc->sc_ld_en[sio_ld[i].ld_num] = 1;
		aprint_normal("%s ", sio_ld[i].ld_name);
	}

	aprint_normal("\n");

#if NGPIO > 0
	nsclpcsio_gpio_init(sc);
#endif
	nsclpcsio_tms_init(sc);
	nsclpcsio_vlm_init(sc);
	sc->sc_sme = nsclpcsio_envsys_init(sc);

#if NGPIO > 0
	/* attach GPIO framework */
	if (sc->sc_ld_en[SIO_LDN_GPIO]) {
		gba.gba_gc = &sc->sc_gpio_gc;
		gba.gba_pins = sc->sc_gpio_pins;
		gba.gba_npins = SIO_GPIO_NPINS;
		config_found_ia(self, "gpiobus", &gba, NULL);
	}
#endif
}

static int
nsclpcsio_isa_detach(device_t self, int flags)
{
	int i, rc;
	struct nsclpcsio_softc *sc = device_private(self);

	if ((rc = config_detach_children(self, flags)) != 0)
		return rc;

	if (sc->sc_sme != NULL)
		sysmon_envsys_unregister(sc->sc_sme);
	mutex_destroy(&sc->sc_lock);

	for (i = 0; i < __arraycount(sio_ld); i++) {
		if (sc->sc_ld_en[sio_ld[i].ld_num] &&
		    sio_ld[i].ld_iosize != 0) {
			bus_space_unmap(sc->sc_iot,
			    sc->sc_ld_ioh[sio_ld[i].ld_num],
			    sio_ld[i].ld_iosize);
		}
	}

	bus_space_unmap(sc->sc_iot, sc->sc_ioh, 2);

	return 0;
}

static void
nsclpcsio_tms_init(struct nsclpcsio_softc *sc)
{
	int i;

	/* Initialisation, PC87366.pdf, page 208 */
	TMS_WRITE(sc, 0x08, 0x00);
	TMS_WRITE(sc, 0x09, 0x0f);
	TMS_WRITE(sc, 0x0a, 0x08);
	TMS_WRITE(sc, 0x0b, 0x04);
	TMS_WRITE(sc, 0x0c, 0x35);
	TMS_WRITE(sc, 0x0d, 0x05);
	TMS_WRITE(sc, 0x0e, 0x05);

	TMS_WRITE(sc, SIO_TMSCFG, 0x00);

	for (i = 0; i < SIO_VLM_OFF; i++) {
		TMS_WRITE(sc, SIO_TMSBS, i);
		TMS_WRITE(sc, SIO_TCHCFST, 0x01);
		sc->sc_sensor[i].units = ENVSYS_STEMP;
	}

#define COPYDESCR(x, y)					\
	do {						\
		(void)strlcpy((x), (y), sizeof(x));	\
	} while (/* CONSTCOND */ 0)

	COPYDESCR(sc->sc_sensor[0].desc, "TSENS1");
	COPYDESCR(sc->sc_sensor[1].desc, "TSENS2");
	COPYDESCR(sc->sc_sensor[2].desc, "TNSC");
}

static void
nsclpcsio_vlm_init(struct nsclpcsio_softc *sc)
{
	int i;
	char tmp[16];
	envsys_data_t *sensor = &sc->sc_sensor[SIO_VLM_OFF];

	for (i = 0; i < SIO_NUM_SENSORS - SIO_VLM_OFF; i++) {
		VLM_WRITE(sc, SIO_VLMBS, i);
		VLM_WRITE(sc, SIO_VCHCFST, 0x01);
		sensor[i].units = ENVSYS_SVOLTS_DC;
	}

	for (i = 0; i < 7; i++) {
		(void)snprintf(tmp, sizeof(tmp), "VSENS%d", i);
		COPYDESCR(sensor[i].desc, tmp);
	}

	COPYDESCR(sensor[7 ].desc, "VSB");
	COPYDESCR(sensor[8 ].desc, "VDD");
	COPYDESCR(sensor[9 ].desc, "VBAT");
	COPYDESCR(sensor[10].desc, "AVDD");
	COPYDESCR(sensor[11].desc, "TS1");
	COPYDESCR(sensor[12].desc, "TS2");
	COPYDESCR(sensor[13].desc, "TS3");
}


static void
nsclpcsio_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct nsclpcsio_softc *sc = sme->sme_cookie;
	uint8_t status, data;
	int8_t sdata = 0;
	int scale, rfact;

	scale = rfact = 0;
	status = data = 0;

	mutex_enter(&sc->sc_lock);
	/* TMS */
	if (edata->sensor < SIO_VLM_OFF && sc->sc_ld_en[SIO_LDN_TMS]) {
		TMS_WRITE(sc, SIO_TMSBS, edata->sensor);
		status = TMS_READ(sc, SIO_TCHCFST);
		if (!(status & 0x01))
			edata->state = ENVSYS_SINVALID;

		sdata = TMS_READ(sc, SIO_RDCHT);
		edata->value_cur = sdata * 1000000 + 273150000;
		edata->state = ENVSYS_SVALID;
	/* VLM */
	} else if (edata->sensor >= SIO_VLM_OFF &&
		   edata->sensor < SIO_NUM_SENSORS &&
		   sc->sc_ld_en[SIO_LDN_VLM]) {
		VLM_WRITE(sc, SIO_VLMBS, edata->sensor - SIO_VLM_OFF);
		status = VLM_READ(sc, SIO_VCHCFST);
		if (!(status & 0x01)) {
			edata->state = ENVSYS_SINVALID;
		} else {
			data = VLM_READ(sc, SIO_RDCHV);
			scale = 1;
			switch (edata->sensor - SIO_VLM_OFF) {
			case 7:
			case 8:
			case 10:
				scale = 2;
				break;
			}
			/* Vi = (2.45±0.05)*VREF *RDCHVi / 256 */
			rfact = 10 * scale * ((245 * SIO_VREF) >> 8);
			edata->value_cur = data * rfact;
			edata->state = ENVSYS_SVALID;
		}
	}
	mutex_exit(&sc->sc_lock);
}

#if NGPIO > 0
static void
nsclpcsio_gpio_pin_select(struct nsclpcsio_softc *sc, int pin)
{
	uint8_t v;

	v = ((pin / 8) << 4) | (pin % 8);

	nswrite(sc->sc_iot, sc->sc_ioh, SIO_REG_LDN, SIO_LDN_GPIO);
	nswrite(sc->sc_iot, sc->sc_ioh, SIO_GPIO_PINSEL, v);
}

static void
nsclpcsio_gpio_init(struct nsclpcsio_softc *sc)
{
	int i;

	for (i = 0; i < SIO_GPIO_NPINS; i++) {
		sc->sc_gpio_pins[i].pin_num = i;
		sc->sc_gpio_pins[i].pin_caps = GPIO_PIN_INPUT |
		    GPIO_PIN_OUTPUT | GPIO_PIN_OPENDRAIN |
		    GPIO_PIN_PUSHPULL | GPIO_PIN_TRISTATE |
		    GPIO_PIN_PULLUP;
		/* safe defaults */
		sc->sc_gpio_pins[i].pin_flags = GPIO_PIN_TRISTATE;
		sc->sc_gpio_pins[i].pin_state = GPIO_PIN_LOW;
		nsclpcsio_gpio_pin_ctl(sc, i, sc->sc_gpio_pins[i].pin_flags);
		nsclpcsio_gpio_pin_write(sc, i, sc->sc_gpio_pins[i].pin_state);
	}

	/* create controller tag */
	sc->sc_gpio_gc.gp_cookie = sc;
	sc->sc_gpio_gc.gp_pin_read = nsclpcsio_gpio_pin_read;
	sc->sc_gpio_gc.gp_pin_write = nsclpcsio_gpio_pin_write;
	sc->sc_gpio_gc.gp_pin_ctl = nsclpcsio_gpio_pin_ctl;
}

static int
nsclpcsio_gpio_pin_read(void *aux, int pin)
{
	struct nsclpcsio_softc *sc = (struct nsclpcsio_softc *)aux;
	int port, shift, reg;
	uint8_t v;

	port = pin / 8;
	shift = pin % 8;

	switch (port) {
	case 0:
		reg = SIO_GPDI0;
		break;
	case 1:
		reg = SIO_GPDI1;
		break;
	case 2:
		reg = SIO_GPDI2;
		break;
	case 3:
		reg = SIO_GPDI3;
		break;
	default:
		reg = SIO_GPDI0;
		break;
	}

	v = GPIO_READ(sc, reg);

	return ((v >> shift) & 0x1);
}

static void
nsclpcsio_gpio_pin_write(void *aux, int pin, int v)
{
	struct nsclpcsio_softc *sc = (struct nsclpcsio_softc *)aux;
	int port, shift, reg;
	uint8_t d;

	port = pin / 8;
	shift = pin % 8;

	switch (port) {
	case 0:
		reg = SIO_GPDO0;
		break;
	case 1:
		reg = SIO_GPDO1;
		break;
	case 2:
		reg = SIO_GPDO2;
		break;
	case 3:
		reg = SIO_GPDO3;
		break;
	default:
		reg = SIO_GPDO0;
		break; /* shouldn't happen */
	}

	d = GPIO_READ(sc, reg);
	if (v == 0)
		d &= ~(1 << shift);
	else if (v == 1)
		d |= (1 << shift);
	GPIO_WRITE(sc, reg, d);
}

void
nsclpcsio_gpio_pin_ctl(void *aux, int pin, int flags)
{
	struct nsclpcsio_softc *sc = (struct nsclpcsio_softc *)aux;
	uint8_t conf;

	mutex_enter(&sc->sc_lock);

	nswrite(sc->sc_iot, sc->sc_ioh, SIO_REG_LDN, SIO_LDN_GPIO);
	nsclpcsio_gpio_pin_select(sc, pin);
	conf = nsread(sc->sc_iot, sc->sc_ioh, SIO_GPIO_PINCFG);

	conf &= ~(SIO_GPIO_CONF_OUTPUTEN | SIO_GPIO_CONF_PUSHPULL |
	    SIO_GPIO_CONF_PULLUP);
	if ((flags & GPIO_PIN_TRISTATE) == 0)
		conf |= SIO_GPIO_CONF_OUTPUTEN;
	if (flags & GPIO_PIN_PUSHPULL)
		conf |= SIO_GPIO_CONF_PUSHPULL;
	if (flags & GPIO_PIN_PULLUP)
		conf |= SIO_GPIO_CONF_PULLUP;

	nswrite(sc->sc_iot, sc->sc_ioh, SIO_GPIO_PINCFG, conf);

	mutex_exit(&sc->sc_lock);
}
#endif /* NGPIO */

MODULE(MODULE_CLASS_DRIVER, nsclpcsio, "sysmon_envsys");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
nsclpcsio_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		return config_init_component(cfdriver_ioconf_nsclpcsio,
		    cfattach_ioconf_nsclpcsio, cfdata_ioconf_nsclpcsio);
#else
		return 0;
#endif
	case MODULE_CMD_FINI:
#ifdef _MODULE
		return config_fini_component(cfdriver_ioconf_nsclpcsio,
		    cfattach_ioconf_nsclpcsio, cfdata_ioconf_nsclpcsio);
#else
		return 0;
#endif
	default:
		return ENOTTY;
	}
}
