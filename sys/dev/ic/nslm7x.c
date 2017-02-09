/*	$NetBSD: nslm7x.c,v 1.62 2015/04/23 23:23:00 pgoyette Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Squier.
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
__KERNEL_RCSID(0, "$NetBSD: nslm7x.c,v 1.62 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/time.h>

#include <sys/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/sysmon/sysmonvar.h>

#include <dev/ic/nslm7xvar.h>

#include <sys/intr.h>

#if defined(LMDEBUG)
#define DPRINTF(x)	do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

/*
 * LM78-compatible chips can typically measure voltages up to 4.096 V.
 * To measure higher voltages the input is attenuated with (external)
 * resistors.  Negative voltages are measured using inverting op amps
 * and resistors.  So we have to convert the sensor values back to
 * real voltages by applying the appropriate resistor factor.
 */
#define RFACT_NONE	10000
#define RFACT(x, y)	(RFACT_NONE * ((x) + (y)) / (y))
#define NRFACT(x, y)	(-RFACT_NONE * (x) / (y))

#define LM_REFRESH_TIMO	(2 * hz)	/* 2 seconds */

static int lm_match(struct lm_softc *);
static int wb_match(struct lm_softc *);
static int def_match(struct lm_softc *);
static void wb_temp_diode_type(struct lm_softc *, int);

static void lm_refresh(void *);

static void lm_generic_banksel(struct lm_softc *, int);
static void lm_setup_sensors(struct lm_softc *, struct lm_sensor *);
static void lm_refresh_sensor_data(struct lm_softc *);
static void lm_refresh_volt(struct lm_softc *, int);
static void lm_refresh_temp(struct lm_softc *, int);
static void lm_refresh_fanrpm(struct lm_softc *, int);

static void wb_refresh_sensor_data(struct lm_softc *);
static void wb_w83637hf_refresh_vcore(struct lm_softc *, int);
static void wb_refresh_nvolt(struct lm_softc *, int);
static void wb_w83627ehf_refresh_nvolt(struct lm_softc *, int);
static void wb_refresh_temp(struct lm_softc *, int);
static void wb_refresh_fanrpm(struct lm_softc *, int);
static void wb_w83792d_refresh_fanrpm(struct lm_softc *, int);

static void as_refresh_temp(struct lm_softc *, int);

struct lm_chip {
	int (*chip_match)(struct lm_softc *);
};

static struct lm_chip lm_chips[] = {
	{ wb_match },
	{ lm_match },
	{ def_match } /* Must be last */
};

/* LM78/78J/79/81 */
static struct lm_sensor lm78_sensors[] = {
	/* Voltage */
	{
		.desc = "VCore A",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x20,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "VCore B",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x21,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "+3.3V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x22,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "+5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x23,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(68, 100)
	},
	{
		.desc = "+12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x24,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(30, 10)
	},
	{
		.desc = "-12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x25,
		.refresh = lm_refresh_volt,
		.rfact = NRFACT(240, 60)
	},
	{
		.desc = "-5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x26,
		.refresh = lm_refresh_volt,
		.rfact = NRFACT(100, 60)
	},
        
	/* Temperature */
	{
		.desc = "Temp0",
		.type = ENVSYS_STEMP,
		.bank = 0,
		.reg = 0x27,
		.refresh = lm_refresh_temp,
		.rfact = 0
	},

	/* Fans */
	{
		.desc = "Fan0",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x28,
		.refresh = lm_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan1",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x29,
		.refresh = lm_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan2",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x2a,
		.refresh = lm_refresh_fanrpm,
		.rfact = 0
	},

	{ .desc = NULL }
};

/* W83627HF */
static struct lm_sensor w83627hf_sensors[] = {
	/* Voltage */
	{
		.desc = "VCore A",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x20,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "VCore B",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x21,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "+3.3V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x22,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "+5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x23,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(34, 50)
	},
	{
		.desc = "+12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x24,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(28, 10)
	},
	{
		.desc = "-12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x25,
		.refresh = wb_refresh_nvolt,
		.rfact = RFACT(232, 56)
	},
	{
		.desc = "-5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x26,
		.refresh = wb_refresh_nvolt,
		.rfact = RFACT(120, 56)
	},
	{
		.desc = "5VSB",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 5,
		.reg = 0x50,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(17, 33)
	},
	{
		.desc = "VBAT",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 5,
		.reg = 0x51,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},

	/* Temperature */
	{
		.desc = "Temp0",
		.type = ENVSYS_STEMP,
		.bank = 0,
		.reg = 0x27,
		.refresh = lm_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp1",
		.type = ENVSYS_STEMP,
		.bank = 1,
		.reg = 0x50,
		.refresh = wb_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp2",
		.type = ENVSYS_STEMP,
		.bank = 2,
		.reg = 0x50,
		.refresh = wb_refresh_temp,
		.rfact = 0
	},

	/* Fans */
	{
		.desc = "Fan0",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x28,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan1",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x29,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan2",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x2a,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},

	{ .desc = NULL }
};

/* W8627EHF */

/*
 * The W83627EHF can measure voltages up to 2.048 V instead of the
 * traditional 4.096 V.  For measuring positive voltages, this can be
 * accounted for by halving the resistor factor.  Negative voltages
 * need special treatment, also because the reference voltage is 2.048 V
 * instead of the traditional 3.6 V.
 */
static struct lm_sensor w83627ehf_sensors[] = {
	/* Voltage */
	{
		.desc = "VCore",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x20,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE / 2
	},
	{
		.desc = "+12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x21,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(56, 10) / 2
	},
	{
		.desc = "+3.3V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x22,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(34, 34) / 2
	},
	{
		.desc = "VIN3",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x23,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(34, 34) / 2
	},
	{
		.desc = "-12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x24,
		.refresh = wb_w83627ehf_refresh_nvolt,
		.rfact = 0
	},
	{
		.desc = "VIN5",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x25,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE / 2
	},
	{
		.desc = "VIN6",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x26,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE / 2
	},
	{
		.desc = "3.3VSB",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 5,
		.reg = 0x50,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(34, 34) / 2
	},
	{
		.desc = "VBAT",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 5,
		.reg = 0x51,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE / 2
	},
	{
		.desc = "VIN8",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 5,
		.reg = 0x52,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE / 2
	},

	/* Temperature */
	{
		.desc = "Temp0",
		.type = ENVSYS_STEMP,
		.bank = 0,
		.reg = 0x27,
		.refresh = lm_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp1",
		.type = ENVSYS_STEMP,
		.bank = 1,
		.reg = 0x50,
		.refresh = wb_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp2",
		.type = ENVSYS_STEMP,
		.bank = 2,
		.reg = 0x50,
		.refresh = wb_refresh_temp,
		.rfact = 0
	},

	/* Fans */
	{
		.desc = "Fan0",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x28,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan1",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x29,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan2",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x2a,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},

	{ .desc = NULL }
};

/*  W83627DHG */
static struct lm_sensor w83627dhg_sensors[] = {
	/* Voltage */
	{
		.desc = "VCore",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x20,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE / 2
	},
	{
		.desc = "+12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x21,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(56, 10) / 2
	},
	{
		.desc = "AVCC",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x22,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(34, 34) / 2
	},
	{
		.desc = "+3.3V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x23,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(34, 34) / 2
	},
	{
		.desc = "-12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x24,
		.refresh = wb_w83627ehf_refresh_nvolt,
		.rfact = 0
	},
	{
		.desc = "+5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x25,
		.refresh = lm_refresh_volt,
		.rfact = 16000
	},
	{
		.desc = "VIN3",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x26,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "+3.3VSB",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 5,
		.reg = 0x50,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(34, 34) / 2
	},
	{
		.desc = "VBAT",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 5,
		.reg = 0x51,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(34, 34) / 2
	},

	/* Temperature */
	{
		.desc = "MB Temperature",
		.type = ENVSYS_STEMP,
		.bank = 0,
		.reg = 0x27,
		.refresh = lm_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "CPU Temperature",
		.type = ENVSYS_STEMP,
		.bank = 1,
		.reg = 0x50,
		.refresh = lm_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Aux Temp",
		.type = ENVSYS_STEMP,
		.bank = 2,
		.reg = 0x50,
		.refresh = lm_refresh_temp,
		.rfact = 0
	},

	/* Fans */
	{
		.desc = "System Fan",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x28,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "CPU Fan",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x29,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Aux Fan",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x2a,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},

	{ .desc = NULL }
};

/* W83637HF */
static struct lm_sensor w83637hf_sensors[] = {
	/* Voltage */
	{
		.desc = "VCore",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x20,
		.refresh = wb_w83637hf_refresh_vcore,
		.rfact = 0
	},
	{
		.desc = "+12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x21,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(28, 10)
	},
	{
		.desc = "+3.3V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x22,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "+5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x23,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(34, 51)
	},
	{
		.desc = "-12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x24,
		.refresh = wb_refresh_nvolt,
		.rfact = RFACT(232, 56)
	},
	{
		.desc = "5VSB",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 5,
		.reg = 0x50,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(34, 51)
	},
	{
		.desc = "VBAT",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 5,
		.reg = 0x51,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},

	/* Temperature */
	{
		.desc = "Temp0",
		.type = ENVSYS_STEMP,
		.bank = 0,
		.reg = 0x27,
		.refresh = lm_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp1",
		.type = ENVSYS_STEMP,
		.bank = 1,
		.reg = 0x50,
		.refresh = wb_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp2",
		.type = ENVSYS_STEMP,
		.bank = 2,
		.reg = 0x50,
		.refresh = wb_refresh_temp,
		.rfact = 0
	},

	/* Fans */
	{
		.desc = "Fan0",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x28,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan1",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x29,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan2",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x2a,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},

	{ .desc = NULL }
};

/* W83697HF */
static struct lm_sensor w83697hf_sensors[] = {
	/* Voltage */
	{
		.desc = "VCore",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x20,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "+3.3V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x22,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "+5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x23,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(34, 50)
	},
	{
		.desc = "+12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x24,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(28, 10)
	},
	{
		.desc = "-12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x25,
		.refresh = wb_refresh_nvolt,
		.rfact = RFACT(232, 56)
	},
	{
		.desc = "-5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x26,
		.refresh = wb_refresh_nvolt,
		.rfact = RFACT(120, 56)
	},
	{
		.desc = "5VSB",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 5,
		.reg = 0x50,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(17, 33)
	},
	{
		.desc = "VBAT",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 5,
		.reg = 0x51,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},

	/* Temperature */
	{
		.desc = "Temp0",
		.type = ENVSYS_STEMP,
		.bank = 0,
		.reg = 0x27,
		.refresh = lm_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp1",
		.type = ENVSYS_STEMP,
		.bank = 1,
		.reg = 0x50,
		.refresh = wb_refresh_temp,
		.rfact = 0
	},

	/* Fans */
	{
		.desc = "Fan0",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x28,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan1",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x29,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},

	{ .desc = NULL }
};

/* W83781D */

/*
 * The datasheet doesn't mention the (internal) resistors used for the
 * +5V, but using the values from the W83782D datasheets seems to
 * provide sensible results.
 */
static struct lm_sensor w83781d_sensors[] = {
	/* Voltage */
	{
		.desc = "VCore A",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x20,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "VCore B",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x21,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "+3.3V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x22,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "+5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x23,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(34, 50)
	},
	{
		.desc = "+12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x24,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(28, 10)
	},
	{
		.desc = "-12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x25,
		.refresh = lm_refresh_volt,
		.rfact = NRFACT(2100, 604)
	},
	{
		.desc = "-5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x26,
		.refresh = lm_refresh_volt,
		.rfact = NRFACT(909, 604)
	},

	/* Temperature */
	{
		.desc = "Temp0",
		.type = ENVSYS_STEMP,
		.bank = 0,
		.reg = 0x27,
		.refresh = lm_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp1",
		.type = ENVSYS_STEMP,
		.bank = 1,
		.reg = 0x50,
		.refresh = wb_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp2",
		.type = ENVSYS_STEMP,
		.bank = 2,
		.reg = 0x50,
		.refresh = wb_refresh_temp,
		.rfact = 0
	},

	/* Fans */
	{
		.desc = "Fan0",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x28,
		.refresh = lm_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan1",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x29,
		.refresh = lm_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan2",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x2a,
		.refresh = lm_refresh_fanrpm,
		.rfact = 0
	},

	{ .desc = NULL }
};

/* W83782D */
static struct lm_sensor w83782d_sensors[] = {
	/* Voltage */
	{
		.desc = "VCore",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x20,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "VINR0",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x21,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "+3.3V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x22,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "+5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x23,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(34, 50)
	},
	{
		.desc = "+12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x24,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(28, 10)
	},
	{
		.desc = "-12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x25,
		.refresh = wb_refresh_nvolt,
		.rfact = RFACT(232, 56)
	},
	{
		.desc = "-5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x26,
		.refresh = wb_refresh_nvolt,
		.rfact = RFACT(120, 56)
	},
	{
		.desc = "5VSB",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 5,
		.reg = 0x50,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(17, 33)
	},
	{
		.desc = "VBAT",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 5,
		.reg = 0x51,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},

	/* Temperature */
	{
		.desc = "Temp0",
		.type = ENVSYS_STEMP,
		.bank = 0,
		.reg = 0x27,
		.refresh = lm_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp1",
		.type = ENVSYS_STEMP,
		.bank = 1,
		.reg = 0x50,
		.refresh = wb_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp2",
		.type = ENVSYS_STEMP,
		.bank = 2,
		.reg = 0x50,
		.refresh = wb_refresh_temp,
		.rfact = 0
	},

	/* Fans */
	{
		.desc = "Fan0",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x28,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan1",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x29,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan2",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x2a,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},

	{ .desc = NULL }
};

/* W83783S */
static struct lm_sensor w83783s_sensors[] = {
	/* Voltage */
	{
		.desc = "VCore",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x20,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "+3.3V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x22,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "+5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x23,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(34, 50)
	},
	{
		.desc = "+12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x24,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(28, 10)
	},
	{
		.desc = "-12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x25,
		.refresh = wb_refresh_nvolt,
		.rfact = RFACT(232, 56)
	},
	{
		.desc = "-5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x26,
		.refresh = wb_refresh_nvolt,
		.rfact = RFACT(120, 56)
	},

	/* Temperature */
	{
		.desc = "Temp0",
		.type = ENVSYS_STEMP,
		.bank = 0,
		.reg = 0x27,
		.refresh = lm_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp1",
		.type = ENVSYS_STEMP,
		.bank = 1,
		.reg = 0x50,
		.refresh = wb_refresh_temp,
		.rfact = 0
	},

	/* Fans */
	{
		.desc = "Fan0",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x28,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan1",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x29,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan2",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x2a,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},

	{ .desc = NULL }
};

/* W83791D */
static struct lm_sensor w83791d_sensors[] = {
	/* Voltage */
	{
		.desc = "VCore",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x20,
		.refresh = lm_refresh_volt,
		.rfact = 10000
	},
	{
		.desc = "VINR0",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x21,
		.refresh = lm_refresh_volt,
		.rfact = 10000
	},
	{
		.desc = "+3.3V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x22,
		.refresh = lm_refresh_volt,
		.rfact = 10000
	},
	{
		.desc = "+5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x23,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(34, 50)
	},
	{
		.desc = "+12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x24,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(28, 10)
	},
	{
		.desc = "-12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x25,
		.refresh = wb_refresh_nvolt,
		.rfact = RFACT(232, 56)
	},
	{
		.desc = "-5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x26,
		.refresh = wb_refresh_nvolt,
		.rfact = RFACT(120, 56)
	},
	{
		.desc = "5VSB",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0xb0,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(17, 33)
	},
	{
		.desc = "VBAT",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0xb1,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "VINR1",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0xb2,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},

	/* Temperature */
	{
		.desc = "Temp0",
		.type = ENVSYS_STEMP,
		.bank = 0,
		.reg = 0x27,
		.refresh = lm_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp1",
		.type = ENVSYS_STEMP,
		.bank = 0,
		.reg = 0xc0,
		.refresh = wb_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp2",
		.type = ENVSYS_STEMP,
		.bank = 0,
		.reg = 0xc8,
		.refresh = wb_refresh_temp,
		.rfact = 0
	},

	/* Fans */
	{
		.desc = "Fan0",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x28,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan1",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x29,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan2",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x2a,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan3",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0xba,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan4",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0xbb,
		.refresh = wb_refresh_fanrpm,
		.rfact = 0
	},

        { .desc = NULL }
};

/* W83792D */
static struct lm_sensor w83792d_sensors[] = {
	/* Voltage */
	{
		.desc = "VCore A",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x20,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "VCore B",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x21,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "+3.3V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x22,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "-5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x23,
		.refresh = wb_refresh_nvolt,
		.rfact = RFACT(120, 56)
	},
	{
		.desc = "+12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x24,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(28, 10)
	},
	{
		.desc = "-12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x25,
		.refresh = wb_refresh_nvolt,
		.rfact = RFACT(232, 56)
	},
	{
		.desc = "+5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x26,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(34, 50)
	},
	{
		.desc = "5VSB",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0xb0,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(17, 33)
	},
	{
		.desc = "VBAT",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0xb1,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},

	/* Temperature */
	{
		.desc = "Temp0",
		.type = ENVSYS_STEMP,
		.bank = 0,
		.reg = 0x27,
		.refresh = lm_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp1",
		.type = ENVSYS_STEMP,
		.bank = 0,
		.reg = 0xc0,
		.refresh = wb_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp2",
		.type = ENVSYS_STEMP,
		.bank = 0,
		.reg = 0xc8,
		.refresh = wb_refresh_temp,
		.rfact = 0
	},

	/* Fans */
	{
		.desc = "Fan0",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x28,
		.refresh = wb_w83792d_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan1",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x29,
		.refresh = wb_w83792d_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan2",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x2a,
		.refresh = wb_w83792d_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan3",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0xb8,
		.refresh = wb_w83792d_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan4",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0xb9,
		.refresh = wb_w83792d_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan5",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0xba,
		.refresh = wb_w83792d_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan6",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0xbe,
		.refresh = wb_w83792d_refresh_fanrpm,
		.rfact = 0
	},

	{ .desc = NULL }
};

/* AS99127F */
static struct lm_sensor as99127f_sensors[] = {
	/* Voltage */
	{
		.desc = "VCore A",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x20,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "VCore B",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x21,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "+3.3V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x22,
		.refresh = lm_refresh_volt,
		.rfact = RFACT_NONE
	},
	{
		.desc = "+5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x23,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(34, 50)
	},
	{
		.desc = "+12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x24,
		.refresh = lm_refresh_volt,
		.rfact = RFACT(28, 10)
	},
	{
		.desc = "-12V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x25,
		.refresh = wb_refresh_nvolt,
		.rfact = RFACT(232, 56)
	},
	{
		.desc = "-5V",
		.type = ENVSYS_SVOLTS_DC,
		.bank = 0,
		.reg = 0x26,
		.refresh = wb_refresh_nvolt,
		.rfact = RFACT(120, 56)
	},

	/* Temperature */
	{
		.desc = "Temp0",
		.type = ENVSYS_STEMP,
		.bank = 0,
		.reg = 0x27,
		.refresh = lm_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp1",
		.type = ENVSYS_STEMP,
		.bank = 1,
		.reg = 0x50,
		.refresh = as_refresh_temp,
		.rfact = 0
	},
	{
		.desc = "Temp2",
		.type = ENVSYS_STEMP,
		.bank = 2,
		.reg = 0x50,
		.refresh = as_refresh_temp,
		.rfact = 0
	},

	/* Fans */
	{
		.desc = "Fan0",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x28,
		.refresh = lm_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan1",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x29,
		.refresh = lm_refresh_fanrpm,
		.rfact = 0
	},
	{
		.desc = "Fan2",
		.type = ENVSYS_SFANRPM,
		.bank = 0,
		.reg = 0x2a,
		.refresh = lm_refresh_fanrpm,
		.rfact = 0
	},

	{ .desc = NULL }
};

static void
lm_generic_banksel(struct lm_softc *lmsc, int bank)
{
	(*lmsc->lm_writereg)(lmsc, WB_BANKSEL, bank);
}

/*
 * bus independent probe
 *
 * prerequisites:  lmsc contains valid lm_{read,write}reg() routines
 * and associated bus access data is present in attachment's softc
 */
int
lm_probe(struct lm_softc *lmsc)
{
	uint8_t cr;
	int rv;

	/* Perform LM78 reset */
	/*(*lmsc->lm_writereg)(lmsc, LMD_CONFIG, 0x80); */

	cr = (*lmsc->lm_readreg)(lmsc, LMD_CONFIG);

	/* XXX - spec says *only* 0x08! */
	if ((cr == 0x08) || (cr == 0x01) || (cr == 0x03) || (cr == 0x06))
		rv = 1;
	else
		rv = 0;

	DPRINTF(("%s: rv = %d, cr = %x\n", __func__, rv, cr));

	return rv;
}

void
lm_attach(struct lm_softc *lmsc)
{
	uint32_t i;

	for (i = 0; i < __arraycount(lm_chips); i++)
		if (lm_chips[i].chip_match(lmsc))
			break;

	/* Start the monitoring loop */
	(*lmsc->lm_writereg)(lmsc, LMD_CONFIG, 0x01);

	lmsc->sc_sme = sysmon_envsys_create();
	/* Initialize sensors */
	for (i = 0; i < lmsc->numsensors; i++) {
		lmsc->sensors[i].state = ENVSYS_SINVALID;
		if (sysmon_envsys_sensor_attach(lmsc->sc_sme,
						&lmsc->sensors[i])) {
			sysmon_envsys_destroy(lmsc->sc_sme);
			return;
		}
	}

	/*
	 * Setup the callout to refresh sensor data every 2 seconds.
	 */
	callout_init(&lmsc->sc_callout, 0);
	callout_setfunc(&lmsc->sc_callout, lm_refresh, lmsc);
	callout_schedule(&lmsc->sc_callout, LM_REFRESH_TIMO);

	/*
	 * Hook into the System Monitor.
	 */
	lmsc->sc_sme->sme_name = device_xname(lmsc->sc_dev);
	lmsc->sc_sme->sme_flags = SME_DISABLE_REFRESH;

	if (sysmon_envsys_register(lmsc->sc_sme)) {
		aprint_error_dev(lmsc->sc_dev,
		    "unable to register with sysmon\n");
		sysmon_envsys_destroy(lmsc->sc_sme);
	}
}

/*
 * Stop, destroy the callout and unregister the driver with the
 * sysmon_envsys(9) framework.
 */
void
lm_detach(struct lm_softc *lmsc)
{
	callout_halt(&lmsc->sc_callout, NULL);
	callout_destroy(&lmsc->sc_callout);
	sysmon_envsys_unregister(lmsc->sc_sme);
}

static void
lm_refresh(void *arg)
{
	struct lm_softc *lmsc = arg;

	lmsc->refresh_sensor_data(lmsc);
	callout_schedule(&lmsc->sc_callout, LM_REFRESH_TIMO);
}

static int
lm_match(struct lm_softc *sc)
{
	const char *model = NULL;
	int chipid;

	/* See if we have an LM78/LM78J/LM79 or LM81 */
	chipid = (*sc->lm_readreg)(sc, LMD_CHIPID) & LM_ID_MASK;
	switch(chipid) {
	case LM_ID_LM78:
		model = "LM78";
		break;
	case LM_ID_LM78J:
		model = "LM78J";
		break;
	case LM_ID_LM79:
		model = "LM79";
		break;
	case LM_ID_LM81:
		model = "LM81";
		break;
	default:
		return 0;
	}

	aprint_naive("\n");
	aprint_normal("\n");
	aprint_normal_dev(sc->sc_dev,
	    "National Semiconductor %s Hardware monitor\n", model);

	lm_setup_sensors(sc, lm78_sensors);
	sc->refresh_sensor_data = lm_refresh_sensor_data;
	return 1;
}

static int
def_match(struct lm_softc *sc)
{
	int chipid;

	chipid = (*sc->lm_readreg)(sc, LMD_CHIPID) & LM_ID_MASK;
	aprint_naive("\n");
	aprint_normal("\n");
	aprint_error_dev(sc->sc_dev, "Unknown chip (ID %d)\n", chipid);

	lm_setup_sensors(sc, lm78_sensors);
	sc->refresh_sensor_data = lm_refresh_sensor_data;
	return 1;
}

static void
wb_temp_diode_type(struct lm_softc *sc, int diode_type)
{
	int regval, banksel;

	banksel = (*sc->lm_readreg)(sc, WB_BANKSEL);
	switch (diode_type) {
	    case 1:	/* Switch to Pentium-II diode mode */
		lm_generic_banksel(sc, WB_BANKSEL_B0);
		regval = (*sc->lm_readreg)(sc, WB_BANK0_VBAT);
		regval |= 0x0e;
		(*sc->lm_writereg)(sc, WB_BANK0_VBAT, regval);
		regval = (*sc->lm_readreg)(sc, WB_BANK0_RESVD1);
		regval |= 0x70;
		(*sc->lm_writereg)(sc, WB_BANK0_RESVD1, 0x0);
		lm_generic_banksel(sc, banksel);
		aprint_verbose_dev(sc->sc_dev, "Pentium-II diode temp sensors\n");
		break;
	    case 2:	/* Switch to 2N3904 mode */
		lm_generic_banksel(sc, WB_BANKSEL_B0);
		regval = (*sc->lm_readreg)(sc, WB_BANK0_VBAT);
		regval |= 0xe;
		(*sc->lm_writereg)(sc, WB_BANK0_VBAT, regval);
		regval = (*sc->lm_readreg)(sc, WB_BANK0_RESVD1);
		regval &= ~0x70;
		(*sc->lm_writereg)(sc, WB_BANK0_RESVD1, 0x0);
		lm_generic_banksel(sc, banksel);
		aprint_verbose_dev(sc->sc_dev, "2N3904 bipolar temp sensors\n");
		break;
	    case 4:	/* Switch to generic thermistor mode */
		lm_generic_banksel(sc, WB_BANKSEL_B0);
		regval = (*sc->lm_readreg)(sc, WB_BANK0_VBAT);
		regval &= ~0xe;
		(*sc->lm_writereg)(sc, WB_BANK0_VBAT, regval);
		lm_generic_banksel(sc, banksel);
		aprint_verbose_dev(sc->sc_dev, "Thermistor temp sensors\n");
		break;
	    case 0:	/* Unspecified - use default */
		aprint_verbose_dev(sc->sc_dev, "Using default temp sensors\n");
		break;
	    default:
		aprint_error_dev(sc->sc_dev,
				 "Ignoring invalid temp sensor mode %d\n",
				 diode_type);
		break;
	}
}

static int
wb_match(struct lm_softc *sc)
{
	const char *model = NULL;
	int banksel, vendid, cf_flags;

	aprint_naive("\n");
	aprint_normal("\n");
	/* Read vendor ID */
	banksel = (*sc->lm_readreg)(sc, WB_BANKSEL);
	lm_generic_banksel(sc, WB_BANKSEL_HBAC);
	vendid = (*sc->lm_readreg)(sc, WB_VENDID) << 8;
	lm_generic_banksel(sc, 0);
	vendid |= (*sc->lm_readreg)(sc, WB_VENDID);
	DPRINTF(("%s: winbond vend id 0x%x\n", __func__, vendid));
	if (vendid != WB_VENDID_WINBOND && vendid != WB_VENDID_ASUS)
		return 0;

	/* Read device/chip ID */
	lm_generic_banksel(sc, WB_BANKSEL_B0);
	(void)(*sc->lm_readreg)(sc, LMD_CHIPID);
	sc->chipid = (*sc->lm_readreg)(sc, WB_BANK0_CHIPID);
	lm_generic_banksel(sc, banksel);
	cf_flags = device_cfdata(sc->sc_dev)->cf_flags;
	DPRINTF(("%s: winbond chip id 0x%x\n", __func__, sc->chipid));

	switch(sc->chipid) {
	case WB_CHIPID_W83627HF:
		model = "W83627HF";
		lm_setup_sensors(sc, w83627hf_sensors);
		wb_temp_diode_type(sc, cf_flags);
		break;
	case WB_CHIPID_W83627THF:
		model = "W83627THF";
		lm_generic_banksel(sc, WB_BANKSEL_B0);
		if ((*sc->lm_readreg)(sc, WB_BANK0_CONFIG) & WB_CONFIG_VMR9)
			sc->vrm9 = 1;
		lm_generic_banksel(sc, banksel);
		lm_setup_sensors(sc, w83637hf_sensors);
		wb_temp_diode_type(sc, cf_flags);
		break;
	case WB_CHIPID_W83627EHF_A:
		model = "W83627EHF-A";
		lm_setup_sensors(sc, w83627ehf_sensors);
		break;
	case WB_CHIPID_W83627EHF:
		model = "W83627EHF";
		lm_setup_sensors(sc, w83627ehf_sensors);
		wb_temp_diode_type(sc, cf_flags);
		break;
	case WB_CHIPID_W83627DHG:
		model = "W83627DHG";
		lm_setup_sensors(sc, w83627dhg_sensors);
		wb_temp_diode_type(sc, cf_flags);
		break;
	case WB_CHIPID_W83637HF:
		model = "W83637HF";
		lm_generic_banksel(sc, WB_BANKSEL_B0);
		if ((*sc->lm_readreg)(sc, WB_BANK0_CONFIG) & WB_CONFIG_VMR9)
			sc->vrm9 = 1;
		lm_generic_banksel(sc, banksel);
		lm_setup_sensors(sc, w83637hf_sensors);
		wb_temp_diode_type(sc, cf_flags);
		break;
	case WB_CHIPID_W83697HF:
		model = "W83697HF";
		lm_setup_sensors(sc, w83697hf_sensors);
		wb_temp_diode_type(sc, cf_flags);
		break;
	case WB_CHIPID_W83781D:
	case WB_CHIPID_W83781D_2:
		model = "W83781D";
		lm_setup_sensors(sc, w83781d_sensors);
		break;
	case WB_CHIPID_W83782D:
		model = "W83782D";
		lm_setup_sensors(sc, w83782d_sensors);
		wb_temp_diode_type(sc, cf_flags);
		break;
	case WB_CHIPID_W83783S:
		model = "W83783S";
		lm_setup_sensors(sc, w83783s_sensors);
		wb_temp_diode_type(sc, cf_flags);
		break;
	case WB_CHIPID_W83791D:
		model = "W83791D";
		lm_setup_sensors(sc, w83791d_sensors);
		wb_temp_diode_type(sc, cf_flags);
		break;
	case WB_CHIPID_W83791SD:
		model = "W83791SD";
		break;
	case WB_CHIPID_W83792D:
		model = "W83792D";
		lm_setup_sensors(sc, w83792d_sensors);
		break;
	case WB_CHIPID_AS99127F:
		if (vendid == WB_VENDID_ASUS) {
			model = "AS99127F";
			lm_setup_sensors(sc, w83781d_sensors);
		} else {
			model = "AS99127F rev 2";
			lm_setup_sensors(sc, as99127f_sensors);
		}
		break;
	default:
		aprint_normal_dev(sc->sc_dev,
		    "unknown Winbond chip (ID 0x%x)\n", sc->chipid);
		/* Handle as a standard LM78. */
		lm_setup_sensors(sc, lm78_sensors);
		sc->refresh_sensor_data = lm_refresh_sensor_data;
		return 1;
	}

	aprint_normal_dev(sc->sc_dev, "Winbond %s Hardware monitor\n", model);

	sc->refresh_sensor_data = wb_refresh_sensor_data;
	return 1;
}

static void
lm_setup_sensors(struct lm_softc *sc, struct lm_sensor *sensors)
{
	int i;

	for (i = 0; sensors[i].desc; i++) {
		sc->sensors[i].units = sensors[i].type;
		if (sc->sensors[i].units == ENVSYS_SVOLTS_DC)
			sc->sensors[i].flags = ENVSYS_FCHANGERFACT;
		strlcpy(sc->sensors[i].desc, sensors[i].desc,
		    sizeof(sc->sensors[i].desc));
		sc->numsensors++;
	}
	sc->lm_sensors = sensors;
}

static void
lm_refresh_sensor_data(struct lm_softc *sc)
{
	int i;

	for (i = 0; i < sc->numsensors; i++)
		sc->lm_sensors[i].refresh(sc, i);
}

static void
lm_refresh_volt(struct lm_softc *sc, int n)
{
	int data;

	data = (*sc->lm_readreg)(sc, sc->lm_sensors[n].reg);
	if (data == 0xff) {
		sc->sensors[n].state = ENVSYS_SINVALID;
	} else {
		sc->sensors[n].value_cur = (data << 4);
		if (sc->sensors[n].rfact) {
			sc->sensors[n].value_cur *= sc->sensors[n].rfact;
			sc->sensors[n].value_cur /= 10;
		} else {
			sc->sensors[n].value_cur *= sc->lm_sensors[n].rfact;
			sc->sensors[n].value_cur /= 10;
			sc->sensors[n].rfact = sc->lm_sensors[n].rfact;
		}
		sc->sensors[n].state = ENVSYS_SVALID;
	}

	DPRINTF(("%s: volt[%d] data=0x%x value_cur=%d\n",
	    __func__, n, data, sc->sensors[n].value_cur));
}

static void
lm_refresh_temp(struct lm_softc *sc, int n)
{
	int data;

	/*
	 * The data sheet suggests that the range of the temperature
	 * sensor is between -55 degC and +125 degC.
	 */
	data = (*sc->lm_readreg)(sc, sc->lm_sensors[n].reg);
	if (data > 0x7d && data < 0xc9)
		sc->sensors[n].state = ENVSYS_SINVALID;
	else {
		if (data & 0x80)
			data -= 0x100;
		sc->sensors[n].state = ENVSYS_SVALID;
		sc->sensors[n].value_cur = data * 1000000 + 273150000;
	}
	DPRINTF(("%s: temp[%d] data=0x%x value_cur=%d\n",
	    __func__, n, data, sc->sensors[n].value_cur));
}

static void
lm_refresh_fanrpm(struct lm_softc *sc, int n)
{
	int data, divisor = 1;

	/*
	 * We might get more accurate fan readings by adjusting the
	 * divisor, but that might interfere with APM or other SMM
	 * BIOS code reading the fan speeds.
	 */

	/* FAN3 has a fixed fan divisor. */
	if (sc->lm_sensors[n].reg == LMD_FAN1 ||
	    sc->lm_sensors[n].reg == LMD_FAN2) {
		data = (*sc->lm_readreg)(sc, LMD_VIDFAN);
		if (sc->lm_sensors[n].reg == LMD_FAN1)
			divisor = (data >> 4) & 0x03;
		else
			divisor = (data >> 6) & 0x03;
	}

	data = (*sc->lm_readreg)(sc, sc->lm_sensors[n].reg);
	if (data == 0xff || data == 0x00)
		sc->sensors[n].state = ENVSYS_SINVALID;
	else {
		sc->sensors[n].state = ENVSYS_SVALID;
		sc->sensors[n].value_cur = 1350000 / (data << divisor);
	}
	DPRINTF(("%s: fan[%d] data=0x%x value_cur=%d\n",
	    __func__, n, data, sc->sensors[n].value_cur));
}

static void
wb_refresh_sensor_data(struct lm_softc *sc)
{
	int banksel, bank, i;

	/*
	 * Properly save and restore bank selection register.
	 */
	banksel = bank = sc->lm_readreg(sc, WB_BANKSEL);
	for (i = 0; i < sc->numsensors; i++) {
		if (bank != sc->lm_sensors[i].bank) {
			bank = sc->lm_sensors[i].bank;
			lm_generic_banksel(sc, bank);
		}
		sc->lm_sensors[i].refresh(sc, i);
	}
	lm_generic_banksel(sc, banksel);
}

static void
wb_w83637hf_refresh_vcore(struct lm_softc *sc, int n)
{
	int data;

	data = (*sc->lm_readreg)(sc, sc->lm_sensors[n].reg);
	/*
	 * Depending on the voltage detection method,
	 * one of the following formulas is used:
	 *	VRM8 method: value = raw * 0.016V
	 *	VRM9 method: value = raw * 0.00488V + 0.70V
	 */
	if (sc->vrm9)
		sc->sensors[n].value_cur = (data * 4880) + 700000;
	else
		sc->sensors[n].value_cur = (data * 16000);
	sc->sensors[n].state = ENVSYS_SVALID;
	DPRINTF(("%s: volt[%d] data=0x%x value_cur=%d\n",
	   __func__, n, data, sc->sensors[n].value_cur));
}

static void
wb_refresh_nvolt(struct lm_softc *sc, int n)
{
	int data;

	data = (*sc->lm_readreg)(sc, sc->lm_sensors[n].reg);
	sc->sensors[n].value_cur = ((data << 4) - WB_VREF);
	if (sc->sensors[n].rfact)
		sc->sensors[n].value_cur *= sc->sensors[n].rfact;
	else
		sc->sensors[n].value_cur *= sc->lm_sensors[n].rfact;

	sc->sensors[n].value_cur /= 10;
	sc->sensors[n].value_cur += WB_VREF * 1000;
	sc->sensors[n].state = ENVSYS_SVALID;
	DPRINTF(("%s: volt[%d] data=0x%x value_cur=%d\n",
	     __func__, n , data, sc->sensors[n].value_cur));
}

static void
wb_w83627ehf_refresh_nvolt(struct lm_softc *sc, int n)
{
	int data;

	data = (*sc->lm_readreg)(sc, sc->lm_sensors[n].reg);
	sc->sensors[n].value_cur = ((data << 3) - WB_W83627EHF_VREF);
	if (sc->sensors[n].rfact)
		sc->sensors[n].value_cur *= sc->sensors[n].rfact;
	else	
		sc->sensors[n].value_cur *= RFACT(232, 10);

	sc->sensors[n].value_cur /= 10;
	sc->sensors[n].value_cur += WB_W83627EHF_VREF * 1000;
	sc->sensors[n].state = ENVSYS_SVALID;
	DPRINTF(("%s: volt[%d] data=0x%x value_cur=%d\n",
	    __func__, n , data, sc->sensors[n].value_cur));
}

static void
wb_refresh_temp(struct lm_softc *sc, int n)
{
	int data;

	/*
	 * The data sheet suggests that the range of the temperature
	 * sensor is between -55 degC and +125 degC.  However, values
	 * around -48 degC seem to be a very common bogus values.
	 * Since such values are unreasonably low, we use -45 degC for
	 * the lower limit instead.
	 */
	data = (*sc->lm_readreg)(sc, sc->lm_sensors[n].reg) << 1;
	data += (*sc->lm_readreg)(sc, sc->lm_sensors[n].reg + 1) >> 7;
	if (data > 0xfffffff || (data > 0x0fa && data < 0x1a6)) {
		sc->sensors[n].state = ENVSYS_SINVALID;
	} else {
		if (data & 0x100)
			data -= 0x200;
		sc->sensors[n].state = ENVSYS_SVALID;
		sc->sensors[n].value_cur = data * 500000 + 273150000;
	}
	DPRINTF(("%s: temp[%d] data=0x%x value_cur=%d\n",
	    __func__, n , data, sc->sensors[n].value_cur));
}

static void
wb_refresh_fanrpm(struct lm_softc *sc, int n)
{
	int fan, data, divisor = 0;

	/* 
	 * This is madness; the fan divisor bits are scattered all
	 * over the place.
	 */

	if (sc->lm_sensors[n].reg == LMD_FAN1 ||
	    sc->lm_sensors[n].reg == LMD_FAN2 ||
	    sc->lm_sensors[n].reg == LMD_FAN3) {
		data = (*sc->lm_readreg)(sc, WB_BANK0_VBAT);
		fan = (sc->lm_sensors[n].reg - LMD_FAN1);
		if ((data >> 5) & (1 << fan))
			divisor |= 0x04;
	}

	if (sc->lm_sensors[n].reg == LMD_FAN1 ||
	    sc->lm_sensors[n].reg == LMD_FAN2) {
		data = (*sc->lm_readreg)(sc, LMD_VIDFAN);
		if (sc->lm_sensors[n].reg == LMD_FAN1)
			divisor |= (data >> 4) & 0x03;
		else
			divisor |= (data >> 6) & 0x03;
	} else if (sc->lm_sensors[n].reg == LMD_FAN3) {
		data = (*sc->lm_readreg)(sc, WB_PIN);
		divisor |= (data >> 6) & 0x03;
	} else if (sc->lm_sensors[n].reg == WB_BANK0_FAN4 ||
		   sc->lm_sensors[n].reg == WB_BANK0_FAN5) {
		data = (*sc->lm_readreg)(sc, WB_BANK0_FAN45);
		if (sc->lm_sensors[n].reg == WB_BANK0_FAN4)
			divisor |= (data >> 0) & 0x07;
		else
			divisor |= (data >> 4) & 0x07;
	}

	data = (*sc->lm_readreg)(sc, sc->lm_sensors[n].reg);
	if (data >= 0xff || data == 0x00)
		sc->sensors[n].state = ENVSYS_SINVALID;
	else {
		sc->sensors[n].state = ENVSYS_SVALID;
		sc->sensors[n].value_cur = 1350000 / (data << divisor);
	}
	DPRINTF(("%s: fan[%d] data=0x%x value_cur=%d\n",
	    __func__, n , data, sc->sensors[n].value_cur));
}

static void
wb_w83792d_refresh_fanrpm(struct lm_softc *sc, int n)
{
	int reg, shift, data, divisor = 1;

	shift = 0;

	switch (sc->lm_sensors[n].reg) {
	case 0x28:
		reg = 0x47; shift = 0;
		break;
	case 0x29:
		reg = 0x47; shift = 4;
		break;
	case 0x2a:
		reg = 0x5b; shift = 0;
		break;
	case 0xb8:
		reg = 0x5b; shift = 4;
		break;
	case 0xb9:
		reg = 0x5c; shift = 0;
		break;
	case 0xba:
		reg = 0x5c; shift = 4;
		break;
	case 0xbe:
		reg = 0x9e; shift = 0;
		break;
	default:
		reg = 0;
		break;
	}

	data = (*sc->lm_readreg)(sc, sc->lm_sensors[n].reg);
	if (data == 0xff || data == 0x00)
		sc->sensors[n].state = ENVSYS_SINVALID;
	else {
		if (reg != 0)
			divisor = ((*sc->lm_readreg)(sc, reg) >> shift) & 0x7;
		sc->sensors[n].state = ENVSYS_SVALID;
		sc->sensors[n].value_cur = 1350000 / (data << divisor);
	}
	DPRINTF(("%s: fan[%d] data=0x%x value_cur=%d\n",
	    __func__, n , data, sc->sensors[n].value_cur));
}

static void
as_refresh_temp(struct lm_softc *sc, int n)
{
	int data;

	/*
	 * It seems a shorted temperature diode produces an all-ones
	 * bit pattern.
	 */
	data = (*sc->lm_readreg)(sc, sc->lm_sensors[n].reg) << 1;
	data += (*sc->lm_readreg)(sc, sc->lm_sensors[n].reg + 1) >> 7;
	if (data == 0x1ff)
		sc->sensors[n].state = ENVSYS_SINVALID;
	else {
		if (data & 0x100)
			data -= 0x200;
		sc->sensors[n].state = ENVSYS_SVALID;
		sc->sensors[n].value_cur = data * 500000 + 273150000;
	}
	DPRINTF(("%s: temp[%d] data=0x%x value_cur=%d\n",
	    __func__, n, data, sc->sensors[n].value_cur));
}

MODULE(MODULE_CLASS_DRIVER, lm, "sysmon_envsys");

static int
lm_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
	case MODULE_CMD_FINI:
		return 0;
	default:
		return ENOTTY;
	}
}
