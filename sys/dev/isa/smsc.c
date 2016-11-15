/*	$NetBSD: smsc.c,v 1.12 2015/04/23 23:23:00 pgoyette Exp $ */

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Brett Lymn.
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
 * This is a driver for the Standard Microsystems Corp (SMSC)
 * LPC47B397 "super i/o" chip.  This driver only handles the environment
 * monitoring capabilities of the chip, the other functions will be
 * probed/matched as "normal" PC hardware devices (serial ports, fdc, so on).
 * SMSC has not deigned to release a datasheet for this particular chip
 * (though they do for others they make) so this driver was written from
 * information contained in the comment block for the Linux driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: smsc.c,v 1.12 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/sysmon/sysmonvar.h>
#include <dev/isa/smscvar.h>

#if defined(LMDEBUG)
#define DPRINTF(x)	do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

static int	smsc_match(device_t, cfdata_t, void *);
static void	smsc_attach(device_t, device_t, void *);
static int	smsc_detach(device_t, int);

static uint8_t	smsc_readreg(bus_space_tag_t, bus_space_handle_t, int);
static void 	smsc_writereg(bus_space_tag_t, bus_space_handle_t, int, int);

static void 	smsc_refresh(struct sysmon_envsys *, envsys_data_t *);

CFATTACH_DECL_NEW(smsc, sizeof(struct smsc_softc),
    smsc_match, smsc_attach, smsc_detach, NULL);

/*
 * Probe for the SMSC Super I/O chip
 */
static int
smsc_match(device_t parent, cfdata_t match, void *aux)
{
	bus_space_handle_t ioh;
	struct isa_attach_args *ia = aux;
	int rv;
	uint8_t cr;

	/* Must supply an address */
	if (ia->ia_nio < 1)
		return 0;

	if (ISA_DIRECT_CONFIG(ia))
		return 0;

	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return 0;

	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr, 2, 0, &ioh))
		return 0;

	/* To get the device ID we must enter config mode... */
	bus_space_write_1(ia->ia_iot, ioh, SMSC_ADDR, SMSC_CONFIG_START);

	/* Then select the device id register */
	cr = smsc_readreg(ia->ia_iot, ioh, SMSC_DEVICE_ID);

	/* Exit config mode, apparently this is important to do */
	bus_space_write_1(ia->ia_iot, ioh, SMSC_ADDR, SMSC_CONFIG_END);

	switch (cr) {
	case SMSC_ID_47B397:
	case SMSC_ID_SCH5307NS:
	case SMSC_ID_SCH5317:
		rv = 1;
		break;
	default:
		rv = 0;
		break;
	}

	DPRINTF(("smsc: rv = %d, cr = %x\n", rv, cr));

	bus_space_unmap(ia->ia_iot, ioh, 2);

	if (rv) {
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = 2;

		ia->ia_niomem = 0;
		ia->ia_nirq = 0;
		ia->ia_ndrq = 0;
	}

	return rv;
}

/*
 * Get the base address for the monitoring registers and set up the
 * sysmon_envsys(9) framework.
 */
static void
smsc_attach(device_t parent, device_t self, void *aux)
{
	struct smsc_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh;
	uint8_t rev, msb, lsb, chipid;
	unsigned address;
	int i;

	sc->sc_iot = ia->ia_iot;

	aprint_naive("\n");

	/* 
	 * To attach we need to find the actual Hardware Monitor
	 * I/O address space.
	 */
	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr, 2, 0,
	    &ioh)) {
		aprint_error(": can't map base i/o space\n");
		return;
	}

	/* Enter config mode */
	bus_space_write_1(ia->ia_iot, ioh, SMSC_ADDR, SMSC_CONFIG_START);

	/* 
	 * While we have the base registers mapped, grab the chip
	 * revision and device ID.
	 */
	rev = smsc_readreg(ia->ia_iot, ioh, SMSC_DEVICE_REVISION);
	chipid = smsc_readreg(ia->ia_iot, ioh, SMSC_DEVICE_ID);

	/* Select the Hardware Monitor LDN */
	smsc_writereg(ia->ia_iot, ioh, SMSC_LOGICAL_DEV_SEL,
	    SMSC_LOGICAL_DEVICE);

	/* Read the base address for the registers. */
	msb = smsc_readreg(ia->ia_iot, ioh, SMSC_IO_BASE_MSB);
	lsb = smsc_readreg(ia->ia_iot, ioh, SMSC_IO_BASE_LSB);
	address = (msb << 8) | lsb;

	/* Exit config mode */
	bus_space_write_1(ia->ia_iot, ioh, SMSC_ADDR, SMSC_CONFIG_END);
	bus_space_unmap(ia->ia_iot, ioh, 2);

	/* Map the Hardware Monitor I/O space. */
	if (bus_space_map(ia->ia_iot, address, 2, 0, &sc->sc_ioh)) {
		aprint_error(": can't map register i/o space\n");
		return;
	}

	sc->sc_sme = sysmon_envsys_create();

#define INITSENSOR(index, string, reg, type) 			\
	do {							\
		strlcpy(sc->sc_sensor[index].desc, string,	\
		    sizeof(sc->sc_sensor[index].desc));		\
		sc->sc_sensor[index].units = type;		\
		sc->sc_regs[index] = reg;			\
		sc->sc_sensor[index].state = ENVSYS_SVALID;	\
	} while (/* CONSTCOND */ 0)

	/* Temperature sensors */
	INITSENSOR(0, "Temp0", SMSC_TEMP1, ENVSYS_STEMP);
	INITSENSOR(1, "Temp1", SMSC_TEMP2, ENVSYS_STEMP);
	INITSENSOR(2, "Temp2", SMSC_TEMP3, ENVSYS_STEMP);
	INITSENSOR(3, "Temp3", SMSC_TEMP4, ENVSYS_STEMP);
	
	/* Fan sensors */
	INITSENSOR(4, "Fan0", SMSC_FAN1_LSB, ENVSYS_SFANRPM);
	INITSENSOR(5, "Fan1", SMSC_FAN2_LSB, ENVSYS_SFANRPM);
	INITSENSOR(6, "Fan2", SMSC_FAN3_LSB, ENVSYS_SFANRPM);
	INITSENSOR(7, "Fan3", SMSC_FAN4_LSB, ENVSYS_SFANRPM);

	for (i = 0; i < SMSC_MAX_SENSORS; i++) {
		sc->sc_sensor[i].state = ENVSYS_SINVALID;
		if (sysmon_envsys_sensor_attach(sc->sc_sme,
						&sc->sc_sensor[i])) {
			sysmon_envsys_destroy(sc->sc_sme);
			bus_space_unmap(sc->sc_iot, sc->sc_ioh, 2);
			return;
		}
	}

	sc->sc_sme->sme_name = device_xname(self);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = smsc_refresh;

	if ((i = sysmon_envsys_register(sc->sc_sme)) != 0) {
		aprint_error(": unable to register with sysmon (%d)\n", i);
		sysmon_envsys_destroy(sc->sc_sme);
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, 2);
		return;
	}

	switch (chipid) {
	case SMSC_ID_47B397:
		aprint_normal(": SMSC LPC47B397 Super I/O");
		break;
	case SMSC_ID_SCH5307NS:
		aprint_normal(": SMSC SCH5307-NS Super I/O");
		break;
	case SMSC_ID_SCH5317:
		aprint_normal(": SMSC SCH5317 Super I/O");
		break;
	}

	aprint_normal(" (rev %u)\n", rev);
	aprint_normal_dev(self, "Hardware Monitor registers at 0x%04x\n",
	    address);
}

static int
smsc_detach(device_t self, int flags)
{
	struct smsc_softc *sc = device_private(self);

	sysmon_envsys_unregister(sc->sc_sme);
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, 2);
	return 0;
}

/*
 * Read the value of the given register
 */
static uint8_t
smsc_readreg(bus_space_tag_t iot, bus_space_handle_t ioh, int reg)
{
	bus_space_write_1(iot, ioh, SMSC_ADDR, reg);
	return bus_space_read_1(iot, ioh, SMSC_DATA);
}

/*
 * Write the given value to the given register.
 */
static void
smsc_writereg(bus_space_tag_t iot, bus_space_handle_t ioh, int reg, int val)
{
	bus_space_write_1(iot, ioh, SMSC_ADDR, reg);
	bus_space_write_1(iot, ioh, SMSC_DATA, val);
}

/* convert temperature read from the chip to micro kelvin */
static inline int
smsc_temp2muk(uint8_t t)
{
        int temp=t;

        return temp * 1000000 + 273150000U;
}

/*
 * convert register value read from chip into rpm using:
 *
 * RPM = 60/(Count * 11.111us)
 *
 * 1/1.1111us = 90kHz
 *
 */
static inline int
smsc_reg2rpm(unsigned int r)
{
	unsigned long rpm;

        if (r == 0x0)
                return 0;

	rpm = (90000 * 60) / ((unsigned long) r);
        return (int) rpm;
}

/* min and max temperatures in uK */
#define SMSC_MIN_TEMP_UK ((-127 * 1000000) + 273150000)
#define SMSC_MAX_TEMP_UK ((127 * 1000000) + 273150000)

/*
 * Get the data for the requested sensor, update the sysmon structure
 * with the retrieved value.
 */
static void
smsc_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct smsc_softc *sc = sme->sme_cookie;
	int reg;
	unsigned int rpm;
	uint8_t msb, lsb;

	reg = sc->sc_regs[edata->sensor];

	switch (edata->units) {
	case ENVSYS_STEMP:
		edata->value_cur = 
		    smsc_temp2muk(smsc_readreg(sc->sc_iot, sc->sc_ioh, reg));
		break;

	case ENVSYS_SFANRPM:
		/* reading lsb first locks msb... */
		lsb = smsc_readreg(sc->sc_iot, sc->sc_ioh, reg);
		msb = smsc_readreg(sc->sc_iot, sc->sc_ioh, reg + 1);
		rpm = (msb << 8) | lsb;
		edata->value_cur = smsc_reg2rpm(rpm);
		break;
	}
}

MODULE(MODULE_CLASS_DRIVER, smsc, "sysmon_envsys");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
smsc_modcmd(modcmd_t cmd, void *opaque)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_smsc,
		    cfattach_ioconf_smsc, cfdata_ioconf_smsc);
#endif
		return error;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_smsc,
		    cfattach_ioconf_smsc, cfdata_ioconf_smsc);
#endif
		return error;
	default:
		return ENOTTY;
	}
}
