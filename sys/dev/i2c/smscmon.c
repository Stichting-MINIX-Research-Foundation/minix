/* $NetBSD: smscmon.c,v 1.2 2011/06/20 20:16:19 pgoyette Exp $ */

/*
 * Copyright (c) 2009 Takahiro Hayashi
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
__KERNEL_RCSID(0, "$NetBSD: smscmon.c,v 1.2 2011/06/20 20:16:19 pgoyette Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/i2c/i2cvar.h>
#include <dev/sysmon/sysmonvar.h>
#include <dev/i2c/smscmonvar.h>

/*
 * A driver for SMSC LPC47M192 hardware monitor at SMBus.
 * This driver supports 8 Voltage and 3 Temperature sensors.
 * Fan RPM monitoring is not supported in this driver because
 * they are seen on ISA bus.
 *
 * Datasheet available (as of Feb. 20, 2010) at
 * http://pdf1.alldatasheet.com/datasheet-pdf/view/109752/SMSC/LPC47M192-NC.html
 */

static int smscmon_match(device_t, cfdata_t, void *);
static void smscmon_attach(device_t, device_t, void *);
static uint8_t smscmon_readreg(struct smscmon_sc *, int);
static void smscmon_writereg(struct smscmon_sc *, int, int);
static void smscmon_sensors_setup(struct smscmon_sc *, struct smscmon_sensor *);
static void smscmon_refresh_volt(struct smscmon_sc *, envsys_data_t *);
static void smscmon_refresh_temp(struct smscmon_sc *, envsys_data_t *);
static void smscmon_refresh(struct sysmon_envsys *, envsys_data_t *);

CFATTACH_DECL_NEW(smscmon, sizeof(struct smscmon_sc),
    smscmon_match, smscmon_attach, NULL, NULL);

static struct smscmon_sensor smscmon_lpc47m192[] = {
	{
		.desc = "+2.5V",
		.type = ENVSYS_SVOLTS_DC,
		.reg = 0x20,
		.refresh = smscmon_refresh_volt,
		.vmin =   13000,
		.vmax = 3320000
	},
	{
		.desc = "Vccp",
		.type = ENVSYS_SVOLTS_DC,
		.reg = 0x21,
		.refresh = smscmon_refresh_volt,
		.vmin =   12000,
		.vmax = 2988000
	},
	{
		.desc = "+3.3V",
		.type = ENVSYS_SVOLTS_DC,
		.reg = 0x22,
		.refresh = smscmon_refresh_volt,
		.vmin =   17000,
		.vmax = 4383000
	},
	{
		.desc = "+5V",
		.type = ENVSYS_SVOLTS_DC,
		.reg = 0x23,
		.refresh = smscmon_refresh_volt,
		.vmin =   26000,
		.vmax = 6640000
	},
	{
		.desc = "+12V",
		.type = ENVSYS_SVOLTS_DC,
		.reg = 0x24,
		.refresh = smscmon_refresh_volt,
		.vmin =    62000,
		.vmax = 15938000
	},
	{
		.desc = "Vcc",
		.type = ENVSYS_SVOLTS_DC,
		.reg = 0x25,
		.refresh = smscmon_refresh_volt,
		.vmin =   17000,
		.vmax = 4383000
	},
	{
		.desc = "+1.5V",
		.type = ENVSYS_SVOLTS_DC,
		.reg = 0x50,
		.refresh = smscmon_refresh_volt,
		.vmin =    8000,
		.vmax = 1992000
	},
	{
		.desc = "+1.8V",
		.type = ENVSYS_SVOLTS_DC,
		.reg = 0x51,
		.refresh = smscmon_refresh_volt,
		.vmin =    9000,
		.vmax = 2391000
	},
	{
		.desc = "Remote Temp1",
		.type = ENVSYS_STEMP,
		.reg = 0x26,
		.refresh = smscmon_refresh_temp,
		.vmin = 0,
		.vmax = 0
	},
	{
		.desc = "Ambient Temp",
		.type = ENVSYS_STEMP,
		.reg = 0x27,
		.refresh = smscmon_refresh_temp,
		.vmin = 0,
		.vmax = 0
	},
	{
		.desc = "Remote Temp2",
		.type = ENVSYS_STEMP,
		.reg = 0x52,
		.refresh = smscmon_refresh_temp,
		.vmax = 0,
		.vmin = 0
	},

	{ .desc = NULL }
};

static int
smscmon_match(device_t parent, cfdata_t match, void *aux)
{
	struct i2c_attach_args *ia = aux;
	uint8_t cmd, cid, rev;

	/* Address is hardwired to 010_110x */
	if ((ia->ia_addr & SMSCMON_ADDR_MASK) != SMSCMON_ADDR)
		return 0;

	iic_acquire_bus(ia->ia_tag, 0);

	cmd = SMSCMON_REG_COMPANY;
	if (iic_exec(ia->ia_tag, I2C_OP_READ_WITH_STOP,
	    ia->ia_addr, &cmd, sizeof cmd, &cid, sizeof cid, 0)) {
		iic_release_bus(ia->ia_tag, 0);
		return 0;
	}
	cmd = SMSCMON_REG_STEPPING;
	if (iic_exec(ia->ia_tag, I2C_OP_READ_WITH_STOP,
	    ia->ia_addr, &cmd, sizeof cmd, &rev, sizeof rev, 0)) {
		iic_release_bus(ia->ia_tag, 0);
		return 0;
	}

	if ( cid != SMSC_CID_47M192 || rev != SMSC_REV_47M192) {
		iic_release_bus(ia->ia_tag, 0);
		return 0;
	}

	iic_release_bus(ia->ia_tag, 0);
	return 1;
}

static void
smscmon_attach(device_t parent, device_t self, void *aux)
{
	struct smscmon_sc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;
	uint8_t cid, rev;
	int i;

	sc->sc_dev = self;
	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;
	sc->smscmon_readreg = smscmon_readreg;
	sc->smscmon_writereg = smscmon_writereg;
	sc->smscmon_sensors = NULL;

	cid = sc->smscmon_readreg(sc, SMSCMON_REG_COMPANY);
	rev = sc->smscmon_readreg(sc, SMSCMON_REG_STEPPING);
	switch (cid) {
	case SMSC_CID_47M192:
		if (rev == SMSC_REV_47M192) {
			smscmon_sensors_setup(sc, smscmon_lpc47m192);
			aprint_normal(": LPC47M192 hardware monitor\n");
		}
		break;
	default:
		/* unknown chip */
		break;
	}

	if (sc->smscmon_sensors == NULL) {
		aprint_normal(": unknown chip: cid 0x%02x rev 0x%02x\n",
		    cid, rev);
		return;
	}

	if ((sc->sc_sme = sysmon_envsys_create()) == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "unable to create sysmon structure\n");
		return;
	}

	for (i = 0; i < sc->numsensors; i++) {
		if (sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sensors[i])) {
			aprint_error_dev(sc->sc_dev,
			    "unable to attach sensor\n");
			sysmon_envsys_destroy(sc->sc_sme);
			return;
		}
	}

	sc->sc_sme->sme_name = device_xname(sc->sc_dev);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = smscmon_refresh;
	if (sysmon_envsys_register(sc->sc_sme)) {
		aprint_error_dev(sc->sc_dev,
		    "unable to register with sysmon\n");
		sysmon_envsys_destroy(sc->sc_sme);
		return;
	}
}

static uint8_t
smscmon_readreg(struct smscmon_sc *sc, int reg)
{
	uint8_t cmd, data;

	iic_acquire_bus(sc->sc_tag, 0);

	cmd = reg;
	iic_smbus_read_byte(sc->sc_tag, sc->sc_addr, cmd, &data, 0);

	iic_release_bus(sc->sc_tag, 0);

	return data;
}

static void
smscmon_writereg(struct smscmon_sc *sc, int reg, int val)
{
	uint8_t cmd, data;

	iic_acquire_bus(sc->sc_tag, 0);

	cmd = reg;
	data = val;
	iic_smbus_write_byte(sc->sc_tag, sc->sc_addr, cmd, data, 0);

	iic_release_bus(sc->sc_tag, 0);
}

static void
smscmon_sensors_setup(struct smscmon_sc *sc, struct smscmon_sensor *sens)
{
	int i;

	for (i = 0; sens[i].desc; i++) {
		strlcpy(sc->sensors[i].desc, sens[i].desc,
		    sizeof(sc->sensors[i].desc));
		sc->sensors[i].units = sens[i].type;
		sc->sensors[i].state = ENVSYS_SINVALID;
		sc->numsensors++;
	}
	sc->smscmon_sensors = sens;
}

static void
smscmon_refresh_volt(struct smscmon_sc *sc, envsys_data_t *edata)
{
	struct smscmon_sensor *sens = &sc->smscmon_sensors[edata->sensor];
	int data;

	data = (*sc->smscmon_readreg)(sc, sens->reg);
	if (data == 0xff) {
		edata->state = ENVSYS_SINVALID;
	} else {
		edata->value_cur =
		    (sens->vmax - sens->vmin)/255 * data + sens->vmin;
		edata->state = ENVSYS_SVALID;
	}
}

static void
smscmon_refresh_temp(struct smscmon_sc *sc, envsys_data_t *edata)
{
	struct smscmon_sensor *sens = &sc->smscmon_sensors[edata->sensor];
	int data;

	data = (*sc->smscmon_readreg)(sc, sens->reg);
	if (data == 0xff) {
		edata->state = ENVSYS_SINVALID;
	} else {
		/* convert data(degC) to uK */
		edata->value_cur = 273150000 + 1000000 * data;
		edata->state = ENVSYS_SVALID;
	}
}

static void
smscmon_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct smscmon_sc *sc = sme->sme_cookie;

	sc->smscmon_sensors[edata->sensor].refresh(sc, edata);
}
