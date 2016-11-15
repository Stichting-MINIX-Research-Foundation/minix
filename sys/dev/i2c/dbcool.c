/*	$NetBSD: dbcool.c,v 1.43 2015/04/23 23:23:00 pgoyette Exp $ */

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Goyette
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
 * a driver for the dbCool(tm) family of environmental controllers
 *
 * Data sheets for the various supported chips are available at
 *
 *	http://www.onsemi.com/pub/Collateral/ADM1027-D.PDF
 *	http://www.onsemi.com/pub/Collateral/ADM1030-D.PDF
 *	http://www.onsemi.com/pub/Collateral/ADT7463-D.PDF
 *	http://www.onsemi.com/pub/Collateral/ADT7466.PDF
 *	http://www.onsemi.com/pub/Collateral/ADT7467-D.PDF
 *	http://www.onsemi.com/pub/Collateral/ADT7468-D.PDF
 *	http://www.onsemi.com/pub/Collateral/ADT7473-D.PDF
 *	http://www.onsemi.com/pub/Collateral/ADT7475-D.PDF
 *	http://www.onsemi.com/pub/Collateral/ADT7476-D.PDF
 *	http://www.onsemi.com/pub/Collateral/ADT7490-D.PDF
 *	http://www.smsc.com/media/Downloads_Public/Data_Sheets/6d103s.pdf
 *
 * (URLs are correct as of October 5, 2008)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dbcool.c,v 1.43 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/module.h>

#include <dev/i2c/dbcool_var.h>
#include <dev/i2c/dbcool_reg.h>

/* Config interface */
static int dbcool_match(device_t, cfdata_t, void *);
static void dbcool_attach(device_t, device_t, void *);
static int dbcool_detach(device_t, int);

/* Device attributes */
static int dbcool_supply_voltage(struct dbcool_softc *);
static bool dbcool_islocked(struct dbcool_softc *);

/* Sensor read functions */
static void dbcool_refresh(struct sysmon_envsys *, envsys_data_t *);
static int dbcool_read_rpm(struct dbcool_softc *, uint8_t);
static int dbcool_read_temp(struct dbcool_softc *, uint8_t, bool);
static int dbcool_read_volt(struct dbcool_softc *, uint8_t, int, bool);

/* Sensor get/set limit functions */
static void dbcool_get_limits(struct sysmon_envsys *, envsys_data_t *,
			      sysmon_envsys_lim_t *, uint32_t *);
static void dbcool_get_temp_limits(struct dbcool_softc *, int,
				   sysmon_envsys_lim_t *, uint32_t *);
static void dbcool_get_volt_limits(struct dbcool_softc *, int,
				   sysmon_envsys_lim_t *, uint32_t *);
static void dbcool_get_fan_limits(struct dbcool_softc *, int,
				  sysmon_envsys_lim_t *, uint32_t *);

static void dbcool_set_limits(struct sysmon_envsys *, envsys_data_t *,
			      sysmon_envsys_lim_t *, uint32_t *);
static void dbcool_set_temp_limits(struct dbcool_softc *, int,
				   sysmon_envsys_lim_t *, uint32_t *);
static void dbcool_set_volt_limits(struct dbcool_softc *, int,
				   sysmon_envsys_lim_t *, uint32_t *);
static void dbcool_set_fan_limits(struct dbcool_softc *, int,
				  sysmon_envsys_lim_t *, uint32_t *);

/* SYSCTL Helpers */
SYSCTL_SETUP_PROTO(sysctl_dbcoolsetup);
static int sysctl_dbcool_temp(SYSCTLFN_PROTO);
static int sysctl_adm1030_temp(SYSCTLFN_PROTO);
static int sysctl_adm1030_trange(SYSCTLFN_PROTO);
static int sysctl_dbcool_duty(SYSCTLFN_PROTO);
static int sysctl_dbcool_behavior(SYSCTLFN_PROTO);
static int sysctl_dbcool_slope(SYSCTLFN_PROTO);
static int sysctl_dbcool_thyst(SYSCTLFN_PROTO);

/* Set-up subroutines */
static void dbcool_setup_controllers(struct dbcool_softc *);
static int  dbcool_setup_sensors(struct dbcool_softc *);
static int  dbcool_attach_sensor(struct dbcool_softc *, int);
static int  dbcool_attach_temp_control(struct dbcool_softc *, int,
	struct chip_id *);

#ifdef DBCOOL_DEBUG
static int sysctl_dbcool_reg_select(SYSCTLFN_PROTO);
static int sysctl_dbcool_reg_access(SYSCTLFN_PROTO);
#endif /* DBCOOL_DEBUG */

/*
 * Descriptions for SYSCTL entries
 */
struct dbc_sysctl_info {
	const char *name;
	const char *desc;
	bool lockable;
	int (*helper)(SYSCTLFN_PROTO);
};

static struct dbc_sysctl_info dbc_sysctl_table[] = {
	/*
	 * The first several entries must remain in the same order as the 
	 * corresponding entries in enum dbc_pwm_params
	 */
	{ "behavior",		"operating behavior and temp selector",
		true, sysctl_dbcool_behavior },
	{ "min_duty",		"minimum fan controller PWM duty cycle",
		true, sysctl_dbcool_duty },
	{ "max_duty",		"maximum fan controller PWM duty cycle",
		true, sysctl_dbcool_duty },
	{ "cur_duty",		"current fan controller PWM duty cycle",
		false, sysctl_dbcool_duty },

	/*
	 * The rest of these should be in the order in which they
	 * are to be stored in the sysctl tree;  the table index is
	 * used as the high-order bits of the sysctl_num to maintain
	 * the sequence.
	 *
	 * If you rearrange the order of these items, be sure to
	 * update the sysctl_index in the XXX_sensor_table[] for
	 * the various chips!
	 */
	{ "Trange",		"temp slope/range to reach 100% duty cycle",
		true, sysctl_dbcool_slope },
	{ "Tmin",		"temp at which to start fan controller",
		true, sysctl_dbcool_temp },
	{ "Ttherm",		"temp at which THERM is asserted",
		true, sysctl_dbcool_temp },
	{ "Thyst",		"temp hysteresis for stopping fan controller",
		true, sysctl_dbcool_thyst },
	{ "Tmin",		"temp at which to start fan controller",
		true, sysctl_adm1030_temp },
	{ "Trange",		"temp slope/range to reach 100% duty cycle",
		true, sysctl_adm1030_trange },
};

static const char *dbc_sensor_names[] = {
	"l_temp",  "r1_temp", "r2_temp", "Vccp",   "Vcc",    "fan1",
	"fan2",    "fan3",    "fan4",    "AIN1",   "AIN2",   "V2dot5",
	"V5",      "V12",     "Vtt",     "Imon",   "VID"
};

/*
 * Following table derived from product data-sheets
 */
static int64_t nominal_voltages[] = {
	-1,		/* Vcc can be either 3.3 or 5.0V
			   at 3/4 scale                  */
	 2249939,	/* Vccp         2.25V 3/4 scale  */
	 2497436,	/* 2.5VIN       2.5V  3/4 scale  */
	 5002466,	/* 5VIN         5V    3/4 scale  */
	12000000,	/* 12VIN       12V    3/4 scale  */
	 1690809,	/* Vtt, Imon    2.25V full scale */
	 1689600,	/* AIN1, AIN2   2.25V full scale */
	       0
};

/*
 * Sensor-type, { val-reg, hilim-reg, lolim-reg}, name-idx, sysctl-table-idx,
 *	nom-voltage-index
 */
struct dbcool_sensor ADT7490_sensor_table[] = {
	{ DBC_TEMP, {	DBCOOL_LOCAL_TEMP,
			DBCOOL_LOCAL_HIGHLIM,
			DBCOOL_LOCAL_LOWLIM },		0, 0, 0 },
	{ DBC_TEMP, {	DBCOOL_REMOTE1_TEMP,
			DBCOOL_REMOTE1_HIGHLIM,
			DBCOOL_REMOTE1_LOWLIM },	1, 0, 0 },
	{ DBC_TEMP, {	DBCOOL_REMOTE2_TEMP,
			DBCOOL_REMOTE2_HIGHLIM,
			DBCOOL_REMOTE2_LOWLIM },	2, 0, 0 },
	{ DBC_VOLT, {	DBCOOL_VCCP,
			DBCOOL_VCCP_HIGHLIM,
			DBCOOL_VCCP_LOWLIM },		3, 0, 1 },
	{ DBC_VOLT, {	DBCOOL_VCC,
			DBCOOL_VCC_HIGHLIM,
			DBCOOL_VCC_LOWLIM },		4, 0, 0 },
	{ DBC_VOLT, {	DBCOOL_25VIN,
			DBCOOL_25VIN_HIGHLIM,
			DBCOOL_25VIN_LOWLIM },		11, 0, 2 },
	{ DBC_VOLT, {	DBCOOL_5VIN,
			DBCOOL_5VIN_HIGHLIM,
			DBCOOL_5VIN_LOWLIM },		12, 0, 3 },
	{ DBC_VOLT, {	DBCOOL_12VIN,
			DBCOOL_12VIN_HIGHLIM,
			DBCOOL_12VIN_LOWLIM },		13, 0, 4 },
	{ DBC_VOLT, {	DBCOOL_VTT,
			DBCOOL_VTT_HIGHLIM,
			DBCOOL_VTT_LOWLIM },		14, 0, 5 },
	{ DBC_VOLT, {	DBCOOL_IMON,
			DBCOOL_IMON_HIGHLIM,
			DBCOOL_IMON_LOWLIM },		15, 0, 5 },
	{ DBC_FAN,  {	DBCOOL_FAN1_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH1_MIN_LSB },		5, 0, 0 },
	{ DBC_FAN,  {	DBCOOL_FAN2_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH2_MIN_LSB },		6, 0, 0 },
	{ DBC_FAN,  {	DBCOOL_FAN3_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH3_MIN_LSB },		7, 0, 0 },
	{ DBC_FAN,  {	DBCOOL_FAN4_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH4_MIN_LSB },		8, 0, 0 },
	{ DBC_VID,  {	DBCOOL_VID_REG,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		16, 0, 0 },
	{ DBC_CTL,  {	DBCOOL_LOCAL_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0, 5, 0 },
	{ DBC_CTL,  {	DBCOOL_LOCAL_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0, 6, 0 },
	{ DBC_CTL,  {	DBCOOL_R1_LCL_TMIN_HYST | 0x80,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0, 7, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE1_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1, 5, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE1_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1, 6, 0 },
	{ DBC_CTL,  {	DBCOOL_R1_LCL_TMIN_HYST,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1, 7, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE2_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		2, 5, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE2_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		2, 6, 0 },
	{ DBC_CTL,  {	DBCOOL_R2_TMIN_HYST,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		2, 7, 0 },
	{ DBC_EOF,  { 0, 0, 0 }, 0, 0, 0 }
};

struct dbcool_sensor ADT7476_sensor_table[] = {
	{ DBC_TEMP, {	DBCOOL_LOCAL_TEMP,
			DBCOOL_LOCAL_HIGHLIM,
			DBCOOL_LOCAL_LOWLIM },		0, 0, 0 },
	{ DBC_TEMP, {	DBCOOL_REMOTE1_TEMP,
			DBCOOL_REMOTE1_HIGHLIM,
			DBCOOL_REMOTE1_LOWLIM },	1, 0, 0 },
	{ DBC_TEMP, {	DBCOOL_REMOTE2_TEMP,
			DBCOOL_REMOTE2_HIGHLIM,
			DBCOOL_REMOTE2_LOWLIM },	2, 0, 0 },
	{ DBC_VOLT, {	DBCOOL_VCCP,
			DBCOOL_VCCP_HIGHLIM,
			DBCOOL_VCCP_LOWLIM },		3, 0, 1 },
	{ DBC_VOLT, {	DBCOOL_VCC,
			DBCOOL_VCC_HIGHLIM,
			DBCOOL_VCC_LOWLIM },		4, 0, 0 },
	{ DBC_VOLT, {	DBCOOL_25VIN,
			DBCOOL_25VIN_HIGHLIM,
			DBCOOL_25VIN_LOWLIM },		11, 0, 2 },
	{ DBC_VOLT, {	DBCOOL_5VIN,
			DBCOOL_5VIN_HIGHLIM,
			DBCOOL_5VIN_LOWLIM },		12, 0, 3 },
	{ DBC_VOLT, {	DBCOOL_12VIN,
			DBCOOL_12VIN_HIGHLIM,
			DBCOOL_12VIN_LOWLIM },		13, 0, 4 },
	{ DBC_FAN,  {	DBCOOL_FAN1_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH1_MIN_LSB },		5, 0, 0 },
	{ DBC_FAN,  {	DBCOOL_FAN2_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH2_MIN_LSB },		6, 0, 0 },
	{ DBC_FAN,  {	DBCOOL_FAN3_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH3_MIN_LSB },		7, 0, 0 },
	{ DBC_FAN,  {	DBCOOL_FAN4_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH4_MIN_LSB },		8, 0, 0 },
	{ DBC_VID,  {	DBCOOL_VID_REG,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		16, 0, 0 },
	{ DBC_CTL,  {	DBCOOL_LOCAL_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0, 5, 0 },
	{ DBC_CTL,  {	DBCOOL_LOCAL_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0, 6, 0 },
	{ DBC_CTL,  {	DBCOOL_R1_LCL_TMIN_HYST | 0x80,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0, 7, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE1_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1, 5, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE1_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1, 6, 0 },
	{ DBC_CTL,  {	DBCOOL_R1_LCL_TMIN_HYST,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1, 7, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE2_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		2, 5, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE2_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		2, 6, 0 },
	{ DBC_CTL,  {	DBCOOL_R2_TMIN_HYST,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		2, 7, 0 },
	{ DBC_EOF,  { 0, 0, 0 }, 0, 0, 0 }
};

struct dbcool_sensor ADT7475_sensor_table[] = {
	{ DBC_TEMP, {	DBCOOL_LOCAL_TEMP,
			DBCOOL_LOCAL_HIGHLIM,
			DBCOOL_LOCAL_LOWLIM },		0, 0, 0 },
	{ DBC_TEMP, {	DBCOOL_REMOTE1_TEMP,
			DBCOOL_REMOTE1_HIGHLIM,
			DBCOOL_REMOTE1_LOWLIM },	1, 0, 0 },
	{ DBC_TEMP, {	DBCOOL_REMOTE2_TEMP,
			DBCOOL_REMOTE2_HIGHLIM,
			DBCOOL_REMOTE2_LOWLIM },	2, 0, 0 },
	{ DBC_VOLT, {	DBCOOL_VCCP,
			DBCOOL_VCCP_HIGHLIM,
			DBCOOL_VCCP_LOWLIM },		3, 0, 1 },
	{ DBC_VOLT, {	DBCOOL_VCC,
			DBCOOL_VCC_HIGHLIM,
			DBCOOL_VCC_LOWLIM },		4, 0, 0 },
	{ DBC_FAN,  {	DBCOOL_FAN1_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH1_MIN_LSB },		5, 0, 0 },
	{ DBC_FAN,  {	DBCOOL_FAN2_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH2_MIN_LSB },		6, 0, 0 },
	{ DBC_FAN,  {	DBCOOL_FAN3_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH3_MIN_LSB },		7, 0, 0 },
	{ DBC_FAN,  {	DBCOOL_FAN4_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH4_MIN_LSB },		8, 0, 0 },
	{ DBC_CTL,  {	DBCOOL_LOCAL_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0, 5, 0 },
	{ DBC_CTL,  {	DBCOOL_LOCAL_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0, 6, 0 },
	{ DBC_CTL,  {	DBCOOL_R1_LCL_TMIN_HYST | 0x80,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0, 7, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE1_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1, 5, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE1_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1, 6, 0 },
	{ DBC_CTL,  {	DBCOOL_R1_LCL_TMIN_HYST,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1, 7, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE2_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		2, 5, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE2_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		2, 6, 0 },
	{ DBC_CTL,  {	DBCOOL_R2_TMIN_HYST,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		2, 7, 0 },
	{ DBC_EOF,  { 0, 0, 0 }, 0, 0, 0 }
};

/*
 * The registers of dbcool_power_control must be in the same order as
 * in enum dbc_pwm_params
 */
struct dbcool_power_control ADT7475_power_table[] = {
	{ { DBCOOL_PWM1_CTL, DBCOOL_PWM1_MINDUTY,
	    DBCOOL_PWM1_MAXDUTY, DBCOOL_PWM1_CURDUTY },
		"fan_control_1" },
	{ { DBCOOL_PWM2_CTL, DBCOOL_PWM2_MINDUTY,
	    DBCOOL_PWM2_MAXDUTY, DBCOOL_PWM2_CURDUTY },
		"fan_control_2" },
	{ { DBCOOL_PWM3_CTL, DBCOOL_PWM3_MINDUTY, 
	    DBCOOL_PWM3_MAXDUTY, DBCOOL_PWM3_CURDUTY },
		"fan_control_3" },
	{ { 0, 0, 0, 0 }, NULL }
};

struct dbcool_sensor ADT7466_sensor_table[] = {
	{ DBC_TEMP, {	DBCOOL_ADT7466_LCL_TEMP_MSB,
			DBCOOL_ADT7466_LCL_TEMP_HILIM,
			DBCOOL_ADT7466_LCL_TEMP_LOLIM }, 0,  0, 0 },
	{ DBC_TEMP, {	DBCOOL_ADT7466_REM_TEMP_MSB,
			DBCOOL_ADT7466_REM_TEMP_HILIM,
			DBCOOL_ADT7466_REM_TEMP_LOLIM }, 1,  0, 0 },
	{ DBC_VOLT, {	DBCOOL_ADT7466_VCC,
			DBCOOL_ADT7466_VCC_HILIM,
			DBCOOL_ADT7466_VCC_LOLIM },	4,  0, 0 },
	{ DBC_VOLT, {	DBCOOL_ADT7466_AIN1,
			DBCOOL_ADT7466_AIN1_HILIM,
			DBCOOL_ADT7466_AIN1_LOLIM },	9,  0, 6 },
	{ DBC_VOLT, {	DBCOOL_ADT7466_AIN2,
			DBCOOL_ADT7466_AIN2_HILIM,
			DBCOOL_ADT7466_AIN2_LOLIM },	10, 0, 6 },
	{ DBC_FAN,  {	DBCOOL_ADT7466_FANA_LSB,
			DBCOOL_NO_REG,
			DBCOOL_ADT7466_FANA_LOLIM_LSB }, 5,  0, 0 },
	{ DBC_FAN,  {	DBCOOL_ADT7466_FANB_LSB,
			DBCOOL_NO_REG,
			DBCOOL_ADT7466_FANB_LOLIM_LSB }, 6,  0, 0 },
	{ DBC_EOF,  { 0, 0, 0 }, 0, 0, 0 }
};

struct dbcool_sensor ADM1027_sensor_table[] = {
	{ DBC_TEMP, {	DBCOOL_LOCAL_TEMP,
			DBCOOL_LOCAL_HIGHLIM,
			DBCOOL_LOCAL_LOWLIM },		0, 0, 0 },
	{ DBC_TEMP, {	DBCOOL_REMOTE1_TEMP,
			DBCOOL_REMOTE1_HIGHLIM,
			DBCOOL_REMOTE1_LOWLIM },	1, 0, 0 },
	{ DBC_TEMP, {	DBCOOL_REMOTE2_TEMP,
			DBCOOL_REMOTE2_HIGHLIM,
			DBCOOL_REMOTE2_LOWLIM },	2, 0, 0 },
	{ DBC_VOLT, {	DBCOOL_VCCP,
			DBCOOL_VCCP_HIGHLIM,
			DBCOOL_VCCP_LOWLIM },		3, 0, 1 },
	{ DBC_VOLT, {	DBCOOL_VCC,
			DBCOOL_VCC_HIGHLIM,
			DBCOOL_VCC_LOWLIM },		4, 0, 0 },
	{ DBC_VOLT, {	DBCOOL_25VIN,
			DBCOOL_25VIN_HIGHLIM,
			DBCOOL_25VIN_LOWLIM },		11, 0, 2 },
	{ DBC_VOLT, {	DBCOOL_5VIN,
			DBCOOL_5VIN_HIGHLIM,
			DBCOOL_5VIN_LOWLIM },		12, 0, 3 },
	{ DBC_VOLT, {	DBCOOL_12VIN,
			DBCOOL_12VIN_HIGHLIM,
			DBCOOL_12VIN_LOWLIM },		13, 0, 4 },
	{ DBC_FAN,  {	DBCOOL_FAN1_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH1_MIN_LSB },		5, 0, 0 },
	{ DBC_FAN,  {	DBCOOL_FAN2_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH2_MIN_LSB },		6, 0, 0 },
	{ DBC_FAN,  {	DBCOOL_FAN3_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH3_MIN_LSB },		7, 0, 0 },
	{ DBC_FAN,  {	DBCOOL_FAN4_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH4_MIN_LSB },		8, 0, 0 },
	{ DBC_VID,  {	DBCOOL_VID_REG,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		16, 0, 0 },
	{ DBC_CTL,  {	DBCOOL_LOCAL_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0, 5, 0 },
	{ DBC_CTL,  {	DBCOOL_LOCAL_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0, 6, 0 },
	{ DBC_CTL,  {	DBCOOL_R1_LCL_TMIN_HYST | 0x80,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0, 7, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE1_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1, 5, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE1_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1, 6, 0 },
	{ DBC_CTL,  {	DBCOOL_R1_LCL_TMIN_HYST,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1, 7, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE2_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		2, 5, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE2_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		2, 6, 0 },
	{ DBC_CTL,  {	DBCOOL_R2_TMIN_HYST,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		2, 7, 0 },
	{ DBC_EOF,  { 0, 0, 0 }, 0, 0, 0 }
};

struct dbcool_sensor ADM1030_sensor_table[] = {
	{ DBC_TEMP, {	DBCOOL_ADM1030_L_TEMP,
			DBCOOL_ADM1030_L_HI_LIM,
			DBCOOL_ADM1030_L_LO_LIM },	0,  0, 0 },
	{ DBC_TEMP, {	DBCOOL_ADM1030_R_TEMP,
			DBCOOL_ADM1030_R_HI_LIM,
			DBCOOL_ADM1030_R_LO_LIM },	1,  0, 0 },
	{ DBC_FAN,  {	DBCOOL_ADM1030_FAN_TACH,
			DBCOOL_NO_REG,
			DBCOOL_ADM1030_FAN_LO_LIM },	5,  0, 0 },
	{ DBC_CTL,  {	DBCOOL_ADM1030_L_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0,  8, 0 },
	{ DBC_CTL,  {	DBCOOL_ADM1030_L_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0,  9, 0 },
	{ DBC_CTL,  {	DBCOOL_ADM1030_L_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0,  6, 0 },
	{ DBC_CTL,  {	DBCOOL_ADM1030_R_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1,  8, 0 },
	{ DBC_CTL,  {	DBCOOL_ADM1030_R_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1,  9, 0 },
	{ DBC_CTL,  {	DBCOOL_ADM1030_R_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1,  6, 0 },
	{ DBC_EOF,  {0, 0, 0 }, 0, 0, 0 }
};

struct dbcool_power_control ADM1030_power_table[] = {   
	{ { DBCOOL_ADM1030_CFG1,  DBCOOL_NO_REG, DBCOOL_NO_REG,
	    DBCOOL_ADM1030_FAN_SPEED_CFG },
	  "fan_control_1" },
	{ { 0, 0, 0, 0 }, NULL }
};

struct dbcool_sensor ADM1031_sensor_table[] = {
	{ DBC_TEMP, {	DBCOOL_ADM1030_L_TEMP,
			DBCOOL_ADM1030_L_HI_LIM,
			DBCOOL_ADM1030_L_LO_LIM },	0,  0, 0 },
	{ DBC_TEMP, {	DBCOOL_ADM1030_R_TEMP,
			DBCOOL_ADM1030_R_HI_LIM,
			DBCOOL_ADM1030_R_LO_LIM },	1,  0, 0 },
	{ DBC_TEMP, {	DBCOOL_ADM1031_R2_TEMP,
			DBCOOL_ADM1031_R2_HI_LIM,
			DBCOOL_ADM1031_R2_LO_LIM },	2,  0, 0 },
	{ DBC_FAN,  {	DBCOOL_ADM1030_FAN_TACH,
			DBCOOL_NO_REG,
			DBCOOL_ADM1030_FAN_LO_LIM },	5,  0, 0 },
	{ DBC_FAN,  {	DBCOOL_ADM1031_FAN2_TACH,
			DBCOOL_NO_REG,
			DBCOOL_ADM1031_FAN2_LO_LIM },	6,  0, 0 },
	{ DBC_CTL,  {	DBCOOL_ADM1030_L_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0,  8, 0 },
	{ DBC_CTL,  {	DBCOOL_ADM1030_L_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0,  9, 0 },
	{ DBC_CTL,  {	DBCOOL_ADM1030_L_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0,  6, 0 },
	{ DBC_CTL,  {	DBCOOL_ADM1030_R_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1,  8, 0 },
	{ DBC_CTL,  {	DBCOOL_ADM1030_R_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1,  9, 0 },
	{ DBC_CTL,  {	DBCOOL_ADM1030_R_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1,  6, 0 },
	{ DBC_CTL,  {	DBCOOL_ADM1031_R2_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		2,  8, 0 },
	{ DBC_CTL,  {	DBCOOL_ADM1031_R2_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		2,  9, 0 },
	{ DBC_CTL,  {	DBCOOL_ADM1031_R2_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		2,  6, 0 },
	{ DBC_EOF,  {0, 0, 0 }, 0, 0, 0 }
};

struct dbcool_power_control ADM1031_power_table[] = {   
	{ { DBCOOL_ADM1030_CFG1,  DBCOOL_NO_REG, DBCOOL_NO_REG,
	    DBCOOL_ADM1030_FAN_SPEED_CFG },
	  "fan_control_1" },
	{ { DBCOOL_ADM1030_CFG1,  DBCOOL_NO_REG, DBCOOL_NO_REG,
	    DBCOOL_ADM1030_FAN_SPEED_CFG },
	  "fan_control_2" },
	{ { 0, 0, 0, 0 }, NULL }
};

struct dbcool_sensor EMC6D103S_sensor_table[] = {
	{ DBC_TEMP, {	DBCOOL_LOCAL_TEMP,
			DBCOOL_LOCAL_HIGHLIM,
			DBCOOL_LOCAL_LOWLIM },		0, 0, 0 },
	{ DBC_TEMP, {	DBCOOL_REMOTE1_TEMP,
			DBCOOL_REMOTE1_HIGHLIM,
			DBCOOL_REMOTE1_LOWLIM },	1, 0, 0 },
	{ DBC_TEMP, {	DBCOOL_REMOTE2_TEMP,
			DBCOOL_REMOTE2_HIGHLIM,
			DBCOOL_REMOTE2_LOWLIM },	2, 0, 0 },
	{ DBC_VOLT, {	DBCOOL_VCCP,
			DBCOOL_VCCP_HIGHLIM,
			DBCOOL_VCCP_LOWLIM },		3, 0, 1 },
	{ DBC_VOLT, {	DBCOOL_VCC,
			DBCOOL_VCC_HIGHLIM,
			DBCOOL_VCC_LOWLIM },		4, 0, 0 },
	{ DBC_VOLT, {	DBCOOL_25VIN,
			DBCOOL_25VIN_HIGHLIM,
			DBCOOL_25VIN_LOWLIM },		11, 0, 2 },
	{ DBC_VOLT, {	DBCOOL_5VIN,
			DBCOOL_5VIN_HIGHLIM,
			DBCOOL_5VIN_LOWLIM },		12, 0, 3 },
	{ DBC_VOLT, {	DBCOOL_12VIN,
			DBCOOL_12VIN_HIGHLIM,
			DBCOOL_12VIN_LOWLIM },		13, 0, 4 },
	{ DBC_FAN,  {	DBCOOL_FAN1_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH1_MIN_LSB },		5, 0, 0 },
	{ DBC_FAN,  {	DBCOOL_FAN2_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH2_MIN_LSB },		6, 0, 0 },
	{ DBC_FAN,  {	DBCOOL_FAN3_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH3_MIN_LSB },		7, 0, 0 },
	{ DBC_FAN,  {	DBCOOL_FAN4_TACH_LSB,
			DBCOOL_NO_REG,
			DBCOOL_TACH4_MIN_LSB },		8, 0, 0 },
	{ DBC_VID,  {	DBCOOL_VID_REG,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		16, 0, 0 },
	{ DBC_CTL,  {	DBCOOL_LOCAL_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0, 5, 0 },
	{ DBC_CTL,  {	DBCOOL_LOCAL_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		0, 6, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE1_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1, 5, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE1_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		1, 6, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE2_TMIN,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		2, 5, 0 },
	{ DBC_CTL,  {	DBCOOL_REMOTE2_TTHRESH,
			DBCOOL_NO_REG,
			DBCOOL_NO_REG },		2, 6, 0 },
	{ DBC_EOF,  { 0, 0, 0 }, 0, 0, 0 }
};

struct chip_id chip_table[] = {
	{ DBCOOL_COMPANYID, ADT7490_DEVICEID, ADT7490_REV_ID,
		ADT7490_sensor_table, ADT7475_power_table,
		DBCFLAG_TEMPOFFSET | DBCFLAG_HAS_MAXDUTY | DBCFLAG_HAS_PECI,
		90000 * 60, "ADT7490" },
	{ DBCOOL_COMPANYID, ADT7476_DEVICEID, 0xff,
		ADT7476_sensor_table, ADT7475_power_table,
		DBCFLAG_TEMPOFFSET | DBCFLAG_HAS_MAXDUTY,
		90000 * 60, "ADT7476" },
	{ DBCOOL_COMPANYID, ADT7475_DEVICEID, 0xff,
		ADT7475_sensor_table, ADT7475_power_table,
		DBCFLAG_TEMPOFFSET | DBCFLAG_HAS_MAXDUTY | DBCFLAG_HAS_SHDN,
		90000 * 60, "ADT7475" },
	{ DBCOOL_COMPANYID, ADT7473_DEVICEID, ADT7473_REV_ID1,
		ADT7475_sensor_table, ADT7475_power_table,
		DBCFLAG_TEMPOFFSET | DBCFLAG_HAS_MAXDUTY | DBCFLAG_HAS_SHDN,
		90000 * 60, "ADT7460/ADT7463" },
	{ DBCOOL_COMPANYID, ADT7473_DEVICEID, ADT7473_REV_ID2,
		ADT7475_sensor_table, ADT7475_power_table,
		DBCFLAG_TEMPOFFSET | DBCFLAG_HAS_MAXDUTY | DBCFLAG_HAS_SHDN,
		90000 * 60, "ADT7463-1" },
	{ DBCOOL_COMPANYID, ADT7468_DEVICEID, 0xff,
		ADT7476_sensor_table, ADT7475_power_table,
		DBCFLAG_TEMPOFFSET  | DBCFLAG_MULTI_VCC | DBCFLAG_HAS_MAXDUTY |
		    DBCFLAG_4BIT_VER | DBCFLAG_HAS_SHDN,
		90000 * 60, "ADT7467/ADT7468" },
	{ DBCOOL_COMPANYID, ADT7466_DEVICEID, 0xff,
		ADT7466_sensor_table, NULL,
		DBCFLAG_ADT7466 | DBCFLAG_TEMPOFFSET | DBCFLAG_HAS_SHDN,
		82000 * 60, "ADT7466" },
	{ DBCOOL_COMPANYID, ADT7463_DEVICEID, ADT7463_REV_ID1,
		ADM1027_sensor_table, ADT7475_power_table,
		DBCFLAG_MULTI_VCC | DBCFLAG_4BIT_VER | DBCFLAG_HAS_SHDN,
		90000 * 60, "ADT7463" },
	{ DBCOOL_COMPANYID, ADT7463_DEVICEID, ADT7463_REV_ID2,
		ADM1027_sensor_table, ADT7475_power_table,
		DBCFLAG_MULTI_VCC | DBCFLAG_4BIT_VER | DBCFLAG_HAS_SHDN |
		    DBCFLAG_HAS_VID_SEL,
		90000 * 60, "ADT7463" },
	{ DBCOOL_COMPANYID, ADM1027_DEVICEID, ADM1027_REV_ID,
		ADM1027_sensor_table, ADT7475_power_table,
		DBCFLAG_MULTI_VCC | DBCFLAG_4BIT_VER,
		90000 * 60, "ADM1027" },
	{ DBCOOL_COMPANYID, ADM1030_DEVICEID, 0xff,
		ADM1030_sensor_table, ADM1030_power_table,
		DBCFLAG_ADM1030 | DBCFLAG_NO_READBYTE,
		11250 * 60, "ADM1030" },
	{ DBCOOL_COMPANYID, ADM1031_DEVICEID, 0xff,
		ADM1031_sensor_table, ADM1030_power_table,
		DBCFLAG_ADM1030 | DBCFLAG_NO_READBYTE,
		11250 * 60, "ADM1031" },
	{ SMSC_COMPANYID, EMC6D103S_DEVICEID, EMC6D103S_REV_ID,
		EMC6D103S_sensor_table, ADT7475_power_table,
		DBCFLAG_4BIT_VER,
		90000 * 60, "EMC6D103S" },
	{ 0, 0, 0, NULL, NULL, 0, 0, NULL }
};

static const char *behavior[] = {
	"remote1",	"local",	"remote2",	"full-speed",
	"disabled",	"local+remote2","all-temps",	"manual"
};

static char dbcool_cur_behav[16];

CFATTACH_DECL_NEW(dbcool, sizeof(struct dbcool_softc),
    dbcool_match, dbcool_attach, dbcool_detach, NULL);

int
dbcool_match(device_t parent, cfdata_t cf, void *aux)
{
	struct i2c_attach_args *ia = aux;
	struct dbcool_chipset dc;
	dc.dc_tag = ia->ia_tag;
	dc.dc_addr = ia->ia_addr;
	dc.dc_chip = NULL;
	dc.dc_readreg = dbcool_readreg;
	dc.dc_writereg = dbcool_writereg;

	/* no probing if we attach to iic, but verify chip id  and address */
	if ((ia->ia_addr & DBCOOL_ADDRMASK) != DBCOOL_ADDR)
		return 0;
	if (dbcool_chip_ident(&dc) >= 0)
		return 1;

	return 0;
}

void
dbcool_attach(device_t parent, device_t self, void *aux)
{
	struct dbcool_softc *sc = device_private(self);
	struct i2c_attach_args *args = aux;
	uint8_t ver;

	sc->sc_dc.dc_addr = args->ia_addr;
	sc->sc_dc.dc_tag = args->ia_tag;
	sc->sc_dc.dc_chip = NULL;
	sc->sc_dc.dc_readreg = dbcool_readreg;
	sc->sc_dc.dc_writereg = dbcool_writereg;
	(void)dbcool_chip_ident(&sc->sc_dc);
	sc->sc_dev = self;

	aprint_naive("\n");
	aprint_normal("\n");

	ver = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_REVISION_REG);
	if (sc->sc_dc.dc_chip->flags & DBCFLAG_4BIT_VER)
	        if (sc->sc_dc.dc_chip->company == SMSC_COMPANYID)
	        {
		        aprint_normal_dev(self, "SMSC %s Controller "
			        "(rev 0x%02x, stepping 0x%02x)\n", sc->sc_dc.dc_chip->name,
        			ver >> 4, ver & 0x0f);
	        } else {
		        aprint_normal_dev(self, "%s dBCool(tm) Controller "
			        "(rev 0x%02x, stepping 0x%02x)\n", sc->sc_dc.dc_chip->name,
        			ver >> 4, ver & 0x0f);
                }
	else
		aprint_normal_dev(self, "%s dBCool(tm) Controller "
			"(rev 0x%04x)\n", sc->sc_dc.dc_chip->name, ver);

	sc->sc_sysctl_log = NULL;

#ifdef _MODULE
	sysctl_dbcoolsetup(&sc->sc_sysctl_log);
#endif

	dbcool_setup(self);

	if (!pmf_device_register(self, dbcool_pmf_suspend, dbcool_pmf_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
dbcool_detach(device_t self, int flags)
{
	struct dbcool_softc *sc = device_private(self);

	pmf_device_deregister(self);

	sysmon_envsys_unregister(sc->sc_sme);

	sysctl_teardown(&sc->sc_sysctl_log);

	sc->sc_sme = NULL;
	return 0;
}

/* On suspend, we save the state of the SHDN bit, then set it */
bool dbcool_pmf_suspend(device_t dev, const pmf_qual_t *qual)
{
	struct dbcool_softc *sc = device_private(dev);
	uint8_t reg, bit, cfg;

	if ((sc->sc_dc.dc_chip->flags & DBCFLAG_HAS_SHDN) == 0)
		return true;
 
	if (sc->sc_dc.dc_chip->flags & DBCFLAG_ADT7466) {
		reg = DBCOOL_ADT7466_CONFIG2;
		bit = DBCOOL_ADT7466_CFG2_SHDN;
	} else {
		reg = DBCOOL_CONFIG2_REG;
		bit = DBCOOL_CFG2_SHDN;
	}
	cfg = sc->sc_dc.dc_readreg(&sc->sc_dc, reg);
	sc->sc_suspend = cfg & bit;
	cfg |= bit;
	sc->sc_dc.dc_writereg(&sc->sc_dc, reg, cfg);

	return true;
}

/* On resume, we restore the previous state of the SHDN bit (which
   we saved in sc_suspend) */
bool dbcool_pmf_resume(device_t dev, const pmf_qual_t *qual)
{
	struct dbcool_softc *sc = device_private(dev);
	uint8_t reg, cfg;

	if ((sc->sc_dc.dc_chip->flags & DBCFLAG_HAS_SHDN) == 0)
		return true;
 
	if (sc->sc_dc.dc_chip->flags & DBCFLAG_ADT7466) {
		reg = DBCOOL_ADT7466_CONFIG2;
	} else {
		reg = DBCOOL_CONFIG2_REG;
	}
	cfg = sc->sc_dc.dc_readreg(&sc->sc_dc, reg);
	cfg &= ~sc->sc_suspend;
	sc->sc_dc.dc_writereg(&sc->sc_dc, reg, cfg);

	return true;

}

uint8_t
dbcool_readreg(struct dbcool_chipset *dc, uint8_t reg)
{
	uint8_t data = 0;

	if (iic_acquire_bus(dc->dc_tag, 0) != 0)
		return data;

	if (dc->dc_chip == NULL || dc->dc_chip->flags & DBCFLAG_NO_READBYTE) {
		/* ADM1027 doesn't support i2c read_byte protocol */
		if (iic_smbus_send_byte(dc->dc_tag, dc->dc_addr, reg, 0) != 0)
			goto bad;
		(void)iic_smbus_receive_byte(dc->dc_tag, dc->dc_addr, &data, 0);
	} else
		(void)iic_smbus_read_byte(dc->dc_tag, dc->dc_addr, reg, &data,
					  0);

bad:
	iic_release_bus(dc->dc_tag, 0);
	return data;
}

void 
dbcool_writereg(struct dbcool_chipset *dc, uint8_t reg, uint8_t val)
{
	if (iic_acquire_bus(dc->dc_tag, 0) != 0)
		return;
        
	(void)iic_smbus_write_byte(dc->dc_tag, dc->dc_addr, reg, val, 0);

	iic_release_bus(dc->dc_tag, 0);
}

static bool
dbcool_islocked(struct dbcool_softc *sc)
{
	uint8_t cfg_reg;

	if (sc->sc_dc.dc_chip->flags & DBCFLAG_ADM1030)
		return 0;

	if (sc->sc_dc.dc_chip->flags & DBCFLAG_ADT7466)
		cfg_reg = DBCOOL_ADT7466_CONFIG1;
	else
		cfg_reg = DBCOOL_CONFIG1_REG;

	if (sc->sc_dc.dc_readreg(&sc->sc_dc, cfg_reg) & DBCOOL_CFG1_LOCK)
		return 1;
	else
		return 0;
}

static int
dbcool_read_temp(struct dbcool_softc *sc, uint8_t reg, bool extres)
{
	uint8_t	t1, t2, t3, val, ext = 0;
	int temp;

	if (sc->sc_dc.dc_chip->flags & DBCFLAG_ADT7466) {
		/*
		 * ADT7466 temps are in strange location
		 */
		ext = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_ADT7466_CONFIG1);
		val = sc->sc_dc.dc_readreg(&sc->sc_dc, reg);
		if (extres)
			ext = sc->sc_dc.dc_readreg(&sc->sc_dc, reg + 1);
	} else if (sc->sc_dc.dc_chip->flags & DBCFLAG_ADM1030) {
		/*
		 * ADM1030 temps are in their own special place, too
		 */
		if (extres) {
			ext = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_ADM1030_TEMP_EXTRES);
			if (reg == DBCOOL_ADM1030_L_TEMP)
				ext >>= 6;
			else if (reg == DBCOOL_ADM1031_R2_TEMP)
				ext >>= 4;
			else
				ext >>= 1;
			ext &= 0x03;
		}
		val = sc->sc_dc.dc_readreg(&sc->sc_dc, reg);
	} else if (extres) {
		ext = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_EXTRES2_REG);

		/* Read all msb regs to unlatch them */
		t1 = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_12VIN);
		t1 = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_REMOTE1_TEMP);
		t2 = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_REMOTE2_TEMP);
		t3 = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_LOCAL_TEMP);
		switch (reg) {
		case DBCOOL_REMOTE1_TEMP:
			val = t1;
			ext >>= 2;
			break;
		case DBCOOL_LOCAL_TEMP:
			val = t3;
			ext >>= 4;
			break;
		case DBCOOL_REMOTE2_TEMP:
			val = t2;
			ext >>= 6;
			break;
		default:
			val = 0;
			break;
		}
		ext &= 0x03;
	}
	else
		val = sc->sc_dc.dc_readreg(&sc->sc_dc, reg);

	/* Check for invalid temp values */
	if ((sc->sc_temp_offset == 0 && val == 0x80) ||
	    (sc->sc_temp_offset != 0 && val == 0))
		return 0;

	/* If using offset mode, adjust, else treat as signed */
	if (sc->sc_temp_offset) {
		temp = val;
		temp -= sc->sc_temp_offset;
	} else
		temp = (int8_t)val;

	/* Convert degC to uK and include extended precision bits */
	temp *= 1000000;
	temp +=  250000 * (int)ext;
	temp += 273150000U;

	return temp;
}

static int
dbcool_read_rpm(struct dbcool_softc *sc, uint8_t reg)
{
	int rpm;
	uint8_t rpm_lo, rpm_hi;

	rpm_lo = sc->sc_dc.dc_readreg(&sc->sc_dc, reg);
	if (sc->sc_dc.dc_chip->flags & DBCFLAG_ADM1030)
		rpm_hi = (rpm_lo == 0xff)?0xff:0x0;
	else
		rpm_hi = sc->sc_dc.dc_readreg(&sc->sc_dc, reg + 1);

	rpm = (rpm_hi << 8) | rpm_lo;
	if (rpm == 0xffff)
		return 0;	/* 0xffff indicates stalled/failed fan */

	/* don't divide by zero */
	return (rpm == 0)? 0 : (sc->sc_dc.dc_chip->rpm_dividend / rpm);
}

/* Provide chip's supply voltage, in microvolts */
static int
dbcool_supply_voltage(struct dbcool_softc *sc)
{
	if (sc->sc_dc.dc_chip->flags & DBCFLAG_MULTI_VCC) {
		if (sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_CONFIG1_REG) & DBCOOL_CFG1_Vcc)
			return 5002500;
		else
			return 3300000;
	} else if (sc->sc_dc.dc_chip->flags & DBCFLAG_ADT7466) {
		if (sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_ADT7466_CONFIG1) &
			    DBCOOL_ADT7466_CFG1_Vcc)
			return 5000000;
		else
			return 3300000;
	} else
		return 3300000;
}

/*
 * Nominal voltages are calculated in microvolts
 */
static int
dbcool_read_volt(struct dbcool_softc *sc, uint8_t reg, int nom_idx, bool extres)
{
	uint8_t ext = 0, v1, v2, v3, v4, val;
	int64_t ret;
	int64_t nom;

	nom = nominal_voltages[nom_idx];
	if (nom < 0)
		nom = sc->sc_supply_voltage;

	/* ADT7466 voltages are in strange locations with only 8-bits */
	if (sc->sc_dc.dc_chip->flags & DBCFLAG_ADT7466)
		val = sc->sc_dc.dc_readreg(&sc->sc_dc, reg);
	else
	/*
	 * It's a "normal" dbCool chip - check for regs that
	 * share extended resolution bits since we have to
	 * read all the MSB registers to unlatch them.
	 */
	if (!extres)
		val = sc->sc_dc.dc_readreg(&sc->sc_dc, reg);
	else if (reg == DBCOOL_12VIN) {
		ext = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_EXTRES2_REG) & 0x03;
		val = sc->sc_dc.dc_readreg(&sc->sc_dc, reg);
		(void)dbcool_read_temp(sc, DBCOOL_LOCAL_TEMP, true);
	} else if (reg == DBCOOL_VTT || reg == DBCOOL_IMON) {
		ext = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_EXTRES_VTT_IMON);
		v1 = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_IMON);
		v2 = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_VTT);
		if (reg == DBCOOL_IMON) {
			val = v1;
			ext >>= 6;
		} else
			val = v2;
			ext >>= 4;
		ext &= 0x0f;
	} else {
		ext = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_EXTRES1_REG);
		v1 = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_25VIN);
		v2 = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_VCCP);
		v3 = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_VCC);
		v4 = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_5VIN);

		switch (reg) {
		case DBCOOL_25VIN:
			val = v1;
			break;
		case DBCOOL_VCCP:
			val = v2;
			ext >>= 2;
			break;
		case DBCOOL_VCC:
			val = v3;
			ext >>= 4;
			break;
		case DBCOOL_5VIN:
			val = v4;
			ext >>= 6;
			break;
		default:
			val = nom = 0;
		}
		ext &= 0x03;
	}

	/* 
	 * Scale the nominal value by the 10-bit fraction
	 *
	 * Returned value is in microvolts.
	 */
	ret = val;
	ret <<= 2;
	ret |= ext;
	ret = (ret * nom) / 0x300;

	return ret;
}

static int
sysctl_dbcool_temp(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct dbcool_softc *sc;
	int reg, error;
	uint8_t chipreg;
	uint8_t newreg;

	node = *rnode;
	sc = (struct dbcool_softc *)node.sysctl_data;
	chipreg = node.sysctl_num & 0xff;

	if (sc->sc_temp_offset) {
		reg = sc->sc_dc.dc_readreg(&sc->sc_dc, chipreg);
		reg -= sc->sc_temp_offset;
	} else
		reg = (int8_t)sc->sc_dc.dc_readreg(&sc->sc_dc, chipreg);

	node.sysctl_data = &reg;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (error || newp == NULL)
		return error;

	/* We were asked to update the value - sanity check before writing */	
	if (*(int *)node.sysctl_data < -64 ||
	    *(int *)node.sysctl_data > 127 + sc->sc_temp_offset)
		return EINVAL;

	newreg = *(int *)node.sysctl_data;
	newreg += sc->sc_temp_offset;
	sc->sc_dc.dc_writereg(&sc->sc_dc, chipreg, newreg);
	return 0;
}

static int
sysctl_adm1030_temp(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct dbcool_softc *sc;
	int reg, error;
	uint8_t chipreg, oldreg, newreg;

	node = *rnode;
	sc = (struct dbcool_softc *)node.sysctl_data;
	chipreg = node.sysctl_num & 0xff;

	oldreg = (int8_t)sc->sc_dc.dc_readreg(&sc->sc_dc, chipreg);
	reg = (oldreg >> 1) & ~0x03;

	node.sysctl_data = &reg;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (error || newp == NULL)
		return error;

	/* We were asked to update the value - sanity check before writing */	
	if (*(int *)node.sysctl_data < 0 || *(int *)node.sysctl_data > 127)
		return EINVAL;

	newreg = *(int *)node.sysctl_data;
	newreg &= ~0x03;
	newreg <<= 1;
	newreg |= (oldreg & 0x07);
	sc->sc_dc.dc_writereg(&sc->sc_dc, chipreg, newreg);
	return 0;
}

static int
sysctl_adm1030_trange(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct dbcool_softc *sc;
	int reg, error, newval;
	uint8_t chipreg, oldreg, newreg;

	node = *rnode;
	sc = (struct dbcool_softc *)node.sysctl_data;
	chipreg = node.sysctl_num & 0xff;

	oldreg = (int8_t)sc->sc_dc.dc_readreg(&sc->sc_dc, chipreg);
	reg = oldreg & 0x07;

	node.sysctl_data = &reg;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (error || newp == NULL)
		return error;

	/* We were asked to update the value - sanity check before writing */	
	newval = *(int *)node.sysctl_data;

	if (newval == 5)
		newreg = 0;
	else if (newval == 10)
		newreg = 1;
	else if (newval == 20)
		newreg = 2;
	else if (newval == 40)
		newreg = 3;
	else if (newval == 80)
		newreg = 4;
	else
		return EINVAL;

	newreg |= (oldreg & ~0x07);
	sc->sc_dc.dc_writereg(&sc->sc_dc, chipreg, newreg);
	return 0;
}

static int
sysctl_dbcool_duty(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct dbcool_softc *sc;
	int reg, error;
	uint8_t chipreg, oldreg, newreg;

	node = *rnode;
	sc = (struct dbcool_softc *)node.sysctl_data;
	chipreg = node.sysctl_num & 0xff;

	oldreg = sc->sc_dc.dc_readreg(&sc->sc_dc, chipreg);
	reg = (uint32_t)oldreg;
	if (sc->sc_dc.dc_chip->flags & DBCFLAG_ADM1030)
		reg = ((reg & 0x0f) * 100) / 15;
	else
		reg = (reg * 100) / 255;
	node.sysctl_data = &reg;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (error || newp == NULL)
		return error;

	/* We were asked to update the value - sanity check before writing */	
	if (*(int *)node.sysctl_data < 0 || *(int *)node.sysctl_data > 100)
		return EINVAL;

	if (sc->sc_dc.dc_chip->flags & DBCFLAG_ADM1030) {
		newreg = *(uint8_t *)(node.sysctl_data) * 15 / 100;
		newreg |= oldreg & 0xf0;
	} else
		newreg = *(uint8_t *)(node.sysctl_data) * 255 / 100;
	sc->sc_dc.dc_writereg(&sc->sc_dc, chipreg, newreg);
	return 0;
}

static int
sysctl_dbcool_behavior(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct dbcool_softc *sc;
	int i, reg, error;
	uint8_t chipreg, oldreg, newreg;

	node = *rnode;
	sc = (struct dbcool_softc *)node.sysctl_data;
	chipreg = node.sysctl_num & 0xff;
	
	oldreg = sc->sc_dc.dc_readreg(&sc->sc_dc, chipreg);

	if (sc->sc_dc.dc_chip->flags & DBCFLAG_ADM1030) {
		if ((sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_ADM1030_CFG2) & 1) == 0)
			reg = 4;
		else if ((oldreg & 0x80) == 0)
			reg = 7;
		else if ((oldreg & 0x60) == 0)
			reg = 4;
		else
			reg = 6;
	} else
		reg = (oldreg >> 5) & 0x07;

	strlcpy(dbcool_cur_behav, behavior[reg], sizeof(dbcool_cur_behav));
	node.sysctl_data = dbcool_cur_behav;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (error || newp == NULL)
		return error;

	/* We were asked to update the value - convert string to value */
	newreg = __arraycount(behavior);
	for (i = 0; i < __arraycount(behavior); i++)
		if (strcmp(node.sysctl_data, behavior[i]) == 0)
			break;
	if (i >= __arraycount(behavior))
		return EINVAL;

	if (sc->sc_dc.dc_chip->flags & DBCFLAG_ADM1030) {
		/*
		 * ADM1030 splits fan controller behavior across two
		 * registers.  We also do not support Auto-Filter mode
		 * nor do we support Manual-RPM-feedback.
		 */
		if (newreg == 4) {
			oldreg = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_ADM1030_CFG2);
			oldreg &= ~0x01;
			sc->sc_dc.dc_writereg(&sc->sc_dc, DBCOOL_ADM1030_CFG2, oldreg);
		} else {
			if (newreg == 0)
				newreg = 4;
			else if (newreg == 6)
				newreg = 7;
			else if (newreg == 7)
				newreg = 0;
			else
				return EINVAL;
			newreg <<= 5;
			newreg |= (oldreg & 0x1f);
			sc->sc_dc.dc_writereg(&sc->sc_dc, chipreg, newreg);
			oldreg = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_ADM1030_CFG2) | 1;
			sc->sc_dc.dc_writereg(&sc->sc_dc, DBCOOL_ADM1030_CFG2, oldreg);
		}
	} else {
		newreg = (sc->sc_dc.dc_readreg(&sc->sc_dc, chipreg) & 0x1f) | (i << 5);
		sc->sc_dc.dc_writereg(&sc->sc_dc, chipreg, newreg);
	}
	return 0;
}

static int
sysctl_dbcool_slope(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct dbcool_softc *sc;
	int reg, error;
	uint8_t chipreg;
	uint8_t newreg;

	node = *rnode;
	sc = (struct dbcool_softc *)node.sysctl_data;
	chipreg = node.sysctl_num & 0xff;
	
	reg = (sc->sc_dc.dc_readreg(&sc->sc_dc, chipreg) >> 4) & 0x0f;
	node.sysctl_data = &reg;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (error || newp == NULL)
		return error;

	/* We were asked to update the value - sanity check before writing */	
	if (*(int *)node.sysctl_data < 0 || *(int *)node.sysctl_data > 0x0f)
		return EINVAL;

	newreg = (sc->sc_dc.dc_readreg(&sc->sc_dc, chipreg) & 0x0f) |
		  (*(int *)node.sysctl_data << 4);
	sc->sc_dc.dc_writereg(&sc->sc_dc, chipreg, newreg);
	return 0;
}

static int
sysctl_dbcool_thyst(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct dbcool_softc *sc;
	int reg, error;
	uint8_t chipreg;
	uint8_t newreg, newhyst;

	node = *rnode;
	sc = (struct dbcool_softc *)node.sysctl_data;
	chipreg = node.sysctl_num & 0x7f;

	/* retrieve 4-bit value */
	newreg = sc->sc_dc.dc_readreg(&sc->sc_dc, chipreg);
	if ((node.sysctl_num & 0x80) == 0)
		reg = newreg >> 4;
	else
		reg = newreg;
	reg = reg & 0x0f;

	node.sysctl_data = &reg;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (error || newp == NULL)
		return error;

	/* We were asked to update the value - sanity check before writing */	
	newhyst = *(int *)node.sysctl_data;
	if (newhyst > 0x0f)
		return EINVAL;

	/* Insert new value into field and update register */
	if ((node.sysctl_num & 0x80) == 0) {
		newreg &= 0x0f;
		newreg |= (newhyst << 4);
	} else {
		newreg &= 0xf0;
		newreg |= newhyst;
	}
	sc->sc_dc.dc_writereg(&sc->sc_dc, chipreg, newreg);
	return 0;
}

#ifdef DBCOOL_DEBUG

/*
 * These routines can be used for debugging.  reg_select is used to
 * select any arbitrary register in the device.  reg_access is used
 * to read (and optionally update) the selected register.
 *
 * No attempt is made to validate the data passed.  If you use these
 * routines, you are assumed to know what you're doing!
 *
 * Caveat user
 */
static int
sysctl_dbcool_reg_select(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct dbcool_softc *sc;
	int reg, error;

	node = *rnode;
	sc = (struct dbcool_softc *)node.sysctl_data;
	
	reg = sc->sc_user_reg;
	node.sysctl_data = &reg;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (error || newp == NULL)
		return error;

	sc->sc_user_reg = *(int *)node.sysctl_data;
	return 0;
}

static int
sysctl_dbcool_reg_access(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct dbcool_softc *sc;
	int reg, error;
	uint8_t chipreg;
	uint8_t newreg;

	node = *rnode;
	sc = (struct dbcool_softc *)node.sysctl_data;
	chipreg = sc->sc_user_reg;
	
	reg = sc->sc_dc.dc_readreg(&sc->sc_dc, chipreg);
	node.sysctl_data = &reg;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (error || newp == NULL)
		return error;

	newreg = *(int *)node.sysctl_data;
	sc->sc_dc.dc_writereg(&sc->sc_dc, chipreg, newreg);
	return 0;
}
#endif /* DBCOOL_DEBUG */

/*
 * Encode an index number and register number for use as a sysctl_num
 * so we can select the correct device register later.
 */
#define	DBC_PWM_SYSCTL(seq, reg)	((seq << 8) | reg)

void
dbcool_setup(device_t self)
{
	struct dbcool_softc *sc = device_private(self);
	const struct sysctlnode *me = NULL;
#ifdef DBCOOL_DEBUG
	struct sysctlnode *node = NULL;
#endif
	uint8_t cfg_val, cfg_reg;
	int ret, error;

	/*
	 * Some chips are capable of reporting an extended temperature range
	 * by default.  On these models, config register 5 bit 0 can be set
	 * to 1 for compatability with other chips that report 2s complement.
	 */
	if (sc->sc_dc.dc_chip->flags & DBCFLAG_ADT7466) {
		if (sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_ADT7466_CONFIG1) & 0x80)
			sc->sc_temp_offset = 64;
		else
			sc->sc_temp_offset = 0;
	} else if (sc->sc_dc.dc_chip->flags & DBCFLAG_TEMPOFFSET) {
		if (sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_CONFIG5_REG) &
			    DBCOOL_CFG5_TWOSCOMP)
			sc->sc_temp_offset = 0;
		else
			sc->sc_temp_offset = 64;
	} else
		sc->sc_temp_offset = 0;

	/* Determine Vcc for this chip */
	sc->sc_supply_voltage = dbcool_supply_voltage(sc);

	ret = sysctl_createv(&sc->sc_sysctl_log, 0, NULL, &me,
	       CTLFLAG_READWRITE,
	       CTLTYPE_NODE, device_xname(self), NULL,
	       NULL, 0, NULL, 0,
	       CTL_HW, CTL_CREATE, CTL_EOL);
	if (ret == 0)
		sc->sc_root_sysctl_num = me->sysctl_num;
	else
		sc->sc_root_sysctl_num = 0;

	aprint_debug_dev(self,
		"Supply voltage %"PRId64".%06"PRId64"V, %s temp range\n",
		sc->sc_supply_voltage / 1000000,
		sc->sc_supply_voltage % 1000000,
		sc->sc_temp_offset ? "extended" : "normal");

	/* Create the sensors for this device */
	sc->sc_sme = sysmon_envsys_create();
	if (dbcool_setup_sensors(sc))
		goto out;

	if (sc->sc_root_sysctl_num != 0) {
		/* If supported, create sysctl tree for fan PWM controllers */
		if (sc->sc_dc.dc_chip->power != NULL)
			dbcool_setup_controllers(sc);

#ifdef DBCOOL_DEBUG
		ret = sysctl_createv(&sc->sc_sysctl_log, 0, NULL,
			(void *)&node,
			CTLFLAG_READWRITE, CTLTYPE_INT, "reg_select", NULL,
			sysctl_dbcool_reg_select,
			0, (void *)sc, sizeof(int),
			CTL_HW, me->sysctl_num, CTL_CREATE, CTL_EOL);
		if (node != NULL)
			node->sysctl_data = sc;

		ret = sysctl_createv(&sc->sc_sysctl_log, 0, NULL,
			(void *)&node,
			CTLFLAG_READWRITE, CTLTYPE_INT, "reg_access", NULL,
			sysctl_dbcool_reg_access,
			0, (void *)sc, sizeof(int),
			CTL_HW, me->sysctl_num, CTL_CREATE, CTL_EOL);
		if (node != NULL)
			node->sysctl_data = sc;
#endif /* DBCOOL_DEBUG */
	}

	/*
	 * Read and rewrite config register to activate device
	 */
	if (sc->sc_dc.dc_chip->flags & DBCFLAG_ADM1030)
		cfg_reg = DBCOOL_ADM1030_CFG1;
	else if (sc->sc_dc.dc_chip->flags & DBCFLAG_ADT7466)
		cfg_reg = DBCOOL_ADT7466_CONFIG1;
	else
		cfg_reg = DBCOOL_CONFIG1_REG;
	cfg_val = sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_CONFIG1_REG);
	if ((cfg_val & DBCOOL_CFG1_START) == 0) {
		cfg_val |= DBCOOL_CFG1_START;
		sc->sc_dc.dc_writereg(&sc->sc_dc, cfg_reg, cfg_val);
	}
	if (dbcool_islocked(sc))
		aprint_normal_dev(self, "configuration locked\n");

	sc->sc_sme->sme_name = device_xname(self);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = dbcool_refresh;
	sc->sc_sme->sme_set_limits = dbcool_set_limits;
	sc->sc_sme->sme_get_limits = dbcool_get_limits;

	if ((error = sysmon_envsys_register(sc->sc_sme)) != 0) {
		aprint_error_dev(self,
		    "unable to register with sysmon (%d)\n", error);
		goto out;
	}
	
	return;

out:
	sysmon_envsys_destroy(sc->sc_sme);
}

static int
dbcool_setup_sensors(struct dbcool_softc *sc)
{
	int i;
	int error = 0;
	uint8_t	vid_reg, vid_val;
	struct chip_id *chip = sc->sc_dc.dc_chip;

	for (i=0; chip->table[i].type != DBC_EOF; i++) {
		if (i < DBCOOL_MAXSENSORS)
			sc->sc_sysctl_num[i] = -1;
		else if (chip->table[i].type != DBC_CTL) {
			aprint_normal_dev(sc->sc_dev, "chip table too big!\n");
			break;
		}
		switch (chip->table[i].type) {
		case DBC_TEMP:
			sc->sc_sensor[i].units = ENVSYS_STEMP;
			sc->sc_sensor[i].state = ENVSYS_SINVALID;
			sc->sc_sensor[i].flags |= ENVSYS_FMONLIMITS;
			sc->sc_sensor[i].flags |= ENVSYS_FHAS_ENTROPY;
			error = dbcool_attach_sensor(sc, i);
			break;
		case DBC_VOLT:
			/*
			 * If 12V-In pin has been reconfigured as 6th bit
			 * of VID code, don't create a 12V-In sensor
			 */
			if ((chip->flags & DBCFLAG_HAS_VID_SEL) &&
			    (chip->table[i].reg.val_reg == DBCOOL_12VIN) &&
			    (sc->sc_dc.dc_readreg(&sc->sc_dc, DBCOOL_VID_REG) &
					0x80))
				break;

			sc->sc_sensor[i].units = ENVSYS_SVOLTS_DC;
			sc->sc_sensor[i].state = ENVSYS_SINVALID;
			sc->sc_sensor[i].flags |= ENVSYS_FMONLIMITS;
			sc->sc_sensor[i].flags |= ENVSYS_FHAS_ENTROPY;
			error = dbcool_attach_sensor(sc, i);
			break;
		case DBC_FAN:
			sc->sc_sensor[i].units = ENVSYS_SFANRPM;
			sc->sc_sensor[i].state = ENVSYS_SINVALID;
			sc->sc_sensor[i].flags |= ENVSYS_FMONLIMITS;
			sc->sc_sensor[i].flags |= ENVSYS_FHAS_ENTROPY;
			error = dbcool_attach_sensor(sc, i);
			break;
		case DBC_VID:
			sc->sc_sensor[i].units = ENVSYS_INTEGER;
			sc->sc_sensor[i].state = ENVSYS_SINVALID;
			sc->sc_sensor[i].flags |= ENVSYS_FMONNOTSUPP;

			/* retrieve 5- or 6-bit value */
			vid_reg = chip->table[i].reg.val_reg;
			vid_val = sc->sc_dc.dc_readreg(&sc->sc_dc, vid_reg);
			if (chip->flags & DBCFLAG_HAS_VID_SEL)
				vid_val &= 0x3f;
			else
				vid_val &= 0x1f;
			sc->sc_sensor[i].value_cur = vid_val;

			error = dbcool_attach_sensor(sc, i);
			break;
		case DBC_CTL:
			error = dbcool_attach_temp_control(sc, i, chip);
			if (error) {
				aprint_error_dev(sc->sc_dev,
						"attach index %d failed %d\n",
						i, error);
				error = 0;
			}
			break;
		default:
			aprint_error_dev(sc->sc_dev,
				"sensor_table index %d has bad type %d\n",
				i, chip->table[i].type);
			break;
		}
		if (error)
			break;
	}
	return error;
}

static int
dbcool_attach_sensor(struct dbcool_softc *sc, int idx)
{
	int name_index;
	int error = 0;

	name_index = sc->sc_dc.dc_chip->table[idx].name_index;
	strlcpy(sc->sc_sensor[idx].desc, dbc_sensor_names[name_index],
		sizeof(sc->sc_sensor[idx].desc));
	sc->sc_regs[idx] = &sc->sc_dc.dc_chip->table[idx].reg;
	sc->sc_nom_volt[idx] = sc->sc_dc.dc_chip->table[idx].nom_volt_index;

	error = sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensor[idx]);
	return error;
}

static int
dbcool_attach_temp_control(struct dbcool_softc *sc, int idx,
			   struct chip_id *chip)
{
	const struct sysctlnode *me2 = NULL, *node;
	int j, ret, sysctl_index, rw_flag;
	uint8_t	sysctl_reg;
	char name[SYSCTL_NAMELEN];

	/* Search for the corresponding temp sensor */
	for (j = 0; j < idx; j++) {
		if (j >= DBCOOL_MAXSENSORS || chip->table[j].type != DBC_TEMP)
			continue;
		if (chip->table[j].name_index == chip->table[idx].name_index)
			break;
	}
	if (j >= idx)	/* Temp sensor not found */
		return ENOENT;

	/* create sysctl node for the sensor if not one already there */
	if (sc->sc_sysctl_num[j] == -1) {
		ret = sysctl_createv(&sc->sc_sysctl_log, 0, NULL, &me2,
				     CTLFLAG_READWRITE,
				     CTLTYPE_NODE, sc->sc_sensor[j].desc, NULL,
				     NULL, 0, NULL, 0,
				     CTL_HW, sc->sc_root_sysctl_num, CTL_CREATE,
					CTL_EOL);
		if (me2 != NULL)
			sc->sc_sysctl_num[j] = me2->sysctl_num;
		else
			return ret;
	}
	/* add sysctl leaf node for this control variable */
	sysctl_index = chip->table[idx].sysctl_index;
	sysctl_reg = chip->table[idx].reg.val_reg;
	strlcpy(name, dbc_sysctl_table[sysctl_index].name, sizeof(name));
	if (dbc_sysctl_table[sysctl_index].lockable && dbcool_islocked(sc))
		rw_flag = CTLFLAG_READONLY | CTLFLAG_OWNDESC;
	else
		rw_flag = CTLFLAG_READWRITE | CTLFLAG_OWNDESC;
	ret = sysctl_createv(&sc->sc_sysctl_log, 0, NULL, &node, rw_flag,
			     CTLTYPE_INT, name,
			     SYSCTL_DESCR(dbc_sysctl_table[sysctl_index].desc),
			     dbc_sysctl_table[sysctl_index].helper,
			     0, (void *)sc, sizeof(int),
			     CTL_HW, sc->sc_root_sysctl_num,
				sc->sc_sysctl_num[j],
				DBC_PWM_SYSCTL(idx, sysctl_reg), CTL_EOL);

	return ret;
}

static void
dbcool_setup_controllers(struct dbcool_softc *sc)
{
	int i, j, rw_flag;
	uint8_t sysctl_reg;
	struct chip_id *chip = sc->sc_dc.dc_chip;
	const struct sysctlnode *me2 = NULL;
	const struct sysctlnode *node = NULL;
	char name[SYSCTL_NAMELEN];

	for (i = 0; chip->power[i].desc != NULL; i++) {
		snprintf(name, sizeof(name), "fan_ctl_%d", i);
		sysctl_createv(&sc->sc_sysctl_log, 0, NULL, &me2,
		       CTLFLAG_READWRITE | CTLFLAG_OWNDESC,
		       CTLTYPE_NODE, name, NULL,
		       NULL, 0, NULL, 0,
		       CTL_HW, sc->sc_root_sysctl_num, CTL_CREATE, CTL_EOL);

		for (j = DBC_PWM_BEHAVIOR; j < DBC_PWM_LAST_PARAM; j++) {
			if (j == DBC_PWM_MAX_DUTY &&
			    (chip->flags & DBCFLAG_HAS_MAXDUTY) == 0)
				continue;
			sysctl_reg = chip->power[i].power_regs[j];
			if (sysctl_reg == DBCOOL_NO_REG)
				continue;
			strlcpy(name, dbc_sysctl_table[j].name, sizeof(name));
			if (dbc_sysctl_table[j].lockable && dbcool_islocked(sc))
				rw_flag = CTLFLAG_READONLY | CTLFLAG_OWNDESC;
			else
				rw_flag = CTLFLAG_READWRITE | CTLFLAG_OWNDESC;
			(sysctl_createv)(&sc->sc_sysctl_log, 0, NULL,
				&node, rw_flag,
				(j == DBC_PWM_BEHAVIOR)?
					CTLTYPE_STRING:CTLTYPE_INT,
				name,
				SYSCTL_DESCR(dbc_sysctl_table[j].desc),
				dbc_sysctl_table[j].helper,
				0, sc, 
				( j == DBC_PWM_BEHAVIOR)?
					sizeof(dbcool_cur_behav): sizeof(int),
				CTL_HW, sc->sc_root_sysctl_num, me2->sysctl_num,
				DBC_PWM_SYSCTL(j, sysctl_reg), CTL_EOL);
		}
	}
}

static void
dbcool_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct dbcool_softc *sc=sme->sme_cookie;
	int i, nom_volt_idx, cur;
	struct reg_list *reg;
	
	i = edata->sensor;
	reg = sc->sc_regs[i];
	
	edata->state = ENVSYS_SVALID;
	switch (edata->units)
	{
		case ENVSYS_STEMP:
			cur = dbcool_read_temp(sc, reg->val_reg, true);
			break;
		case ENVSYS_SVOLTS_DC:
			nom_volt_idx = sc->sc_nom_volt[i];
			cur = dbcool_read_volt(sc, reg->val_reg, nom_volt_idx,
						true);
			break;
		case ENVSYS_SFANRPM:
			cur = dbcool_read_rpm(sc, reg->val_reg);
			break;
		case ENVSYS_INTEGER:
			return;
		default:
			edata->state = ENVSYS_SINVALID;
			return;
	}

	if (cur == 0 && (edata->units != ENVSYS_SFANRPM))
		edata->state = ENVSYS_SINVALID;

	/*
	 * If fan is "stalled" but has no low limit, treat
	 * it as though the fan is not installed.
	 */
	else if (edata->units == ENVSYS_SFANRPM && cur == 0 &&
			!(edata->upropset & (PROP_CRITMIN | PROP_WARNMIN)))
		edata->state = ENVSYS_SINVALID;

	edata->value_cur = cur;
}

int
dbcool_chip_ident(struct dbcool_chipset *dc)
{
	/* verify this is a supported dbCool chip */
	uint8_t c_id, d_id, r_id;
	int i;

	c_id = dc->dc_readreg(dc, DBCOOL_COMPANYID_REG);
	d_id = dc->dc_readreg(dc, DBCOOL_DEVICEID_REG);
	r_id = dc->dc_readreg(dc, DBCOOL_REVISION_REG);

	/* The EMC6D103S only supports read_byte and since dc->dc_chip is
	 * NULL when we call dc->dc_readreg above we use
	 * send_byte/receive_byte which doesn't work.
	 *
	 * So if we only get 0's back then try again with dc->dc_chip
	 * set to the EMC6D103S_DEVICEID and which doesn't have
	 * DBCFLAG_NO_READBYTE set so read_byte will be used
	 */
	if ((c_id == 0) && (d_id == 0) && (r_id == 0)) {
		for (i = 0; chip_table[i].company != 0; i++)
			if ((SMSC_COMPANYID == chip_table[i].company) &&
			    (EMC6D103S_DEVICEID == chip_table[i].device)) {
				dc->dc_chip = &chip_table[i];
				break;
			}
		c_id = dc->dc_readreg(dc, DBCOOL_COMPANYID_REG);
 		d_id = dc->dc_readreg(dc, DBCOOL_DEVICEID_REG);
 		r_id = dc->dc_readreg(dc, DBCOOL_REVISION_REG);
	}
 
	for (i = 0; chip_table[i].company != 0; i++)
		if ((c_id == chip_table[i].company) &&
		    (d_id == chip_table[i].device ||
		    chip_table[i].device == 0xff) &&
		    (r_id == chip_table[i].rev ||
		    chip_table[i].rev == 0xff)) {
			dc->dc_chip = &chip_table[i];
			return i;
		}

	aprint_verbose("dbcool_chip_ident: addr 0x%02x c_id 0x%02x d_id 0x%02x"
			" r_id 0x%02x: No match.\n", dc->dc_addr, c_id, d_id,
			r_id);

	return -1;
}  

/*
 * Retrieve sensor limits from the chip registers
 */
static void
dbcool_get_limits(struct sysmon_envsys *sme, envsys_data_t *edata,
		  sysmon_envsys_lim_t *limits, uint32_t *props)
{
	int index = edata->sensor;
	struct dbcool_softc *sc = sme->sme_cookie;

	*props &= ~(PROP_CRITMIN | PROP_CRITMAX);
	switch (edata->units) {
	    case ENVSYS_STEMP:
		dbcool_get_temp_limits(sc, index, limits, props);
		break;
	    case ENVSYS_SVOLTS_DC:
		dbcool_get_volt_limits(sc, index, limits, props);
		break;
	    case ENVSYS_SFANRPM:
		dbcool_get_fan_limits(sc, index, limits, props);

	    /* FALLTHROUGH */
	    default:
		break;
	}
	*props &= ~PROP_DRIVER_LIMITS;

	/* If both limits provided, make sure they're sane */
	if ((*props & PROP_CRITMIN) &&
	    (*props & PROP_CRITMAX) &&
	    (limits->sel_critmin >= limits->sel_critmax)) 
		*props &= ~(PROP_CRITMIN | PROP_CRITMAX);

	/*
	 * If this is the first time through, save these values
	 * in case user overrides them and then requests a reset.
	 */
	if (sc->sc_defprops[index] == 0) {
		sc->sc_defprops[index] = *props | PROP_DRIVER_LIMITS;
		sc->sc_deflims[index]  = *limits;
	}
}

static void
dbcool_get_temp_limits(struct dbcool_softc *sc, int idx,
		       sysmon_envsys_lim_t *lims, uint32_t *props)
{
	struct reg_list *reg = sc->sc_regs[idx];
	uint8_t	lo_lim, hi_lim;

	lo_lim = sc->sc_dc.dc_readreg(&sc->sc_dc, reg->lo_lim_reg);
	hi_lim = sc->sc_dc.dc_readreg(&sc->sc_dc, reg->hi_lim_reg);

	if (sc->sc_temp_offset) {
		if (lo_lim > 0x01) {
			lims->sel_critmin = lo_lim - sc->sc_temp_offset;
			*props |= PROP_CRITMIN;
		}
		if (hi_lim != 0xff) {
			lims->sel_critmax = hi_lim - sc->sc_temp_offset;
			*props |= PROP_CRITMAX;
		}
	} else {
		if (lo_lim != 0x80 && lo_lim != 0x81) {
			lims->sel_critmin = (int8_t)lo_lim;
			*props |= PROP_CRITMIN;
		}

		if (hi_lim != 0x7f) {
			lims->sel_critmax = (int8_t)hi_lim;
			*props |= PROP_CRITMAX;
		}
	}

	/* Convert temp limits to microKelvin */
	lims->sel_critmin *= 1000000;
	lims->sel_critmin += 273150000;
	lims->sel_critmax *= 1000000;
	lims->sel_critmax += 273150000;
}

static void
dbcool_get_volt_limits(struct dbcool_softc *sc, int idx,
		       sysmon_envsys_lim_t *lims, uint32_t *props)
{
	struct reg_list *reg = sc->sc_regs[idx];
	int64_t limit;
	int nom;

	nom = nominal_voltages[sc->sc_dc.dc_chip->table[idx].nom_volt_index];
	if (nom < 0)
		nom = dbcool_supply_voltage(sc);
	nom *= 1000000;		/* scale for microvolts */

	limit = sc->sc_dc.dc_readreg(&sc->sc_dc, reg->lo_lim_reg);
	if (limit != 0x00 && limit != 0xff) {
		limit *= nom;
		limit /= 0xc0;
		lims->sel_critmin = limit;
		*props |= PROP_CRITMIN;
	}
	limit = sc->sc_dc.dc_readreg(&sc->sc_dc, reg->hi_lim_reg);
	if (limit != 0x00 && limit != 0xff) {
		limit *= nom;
		limit /= 0xc0;
		lims->sel_critmax = limit;
		*props |= PROP_CRITMAX;
	}
}

static void
dbcool_get_fan_limits(struct dbcool_softc *sc, int idx,
		      sysmon_envsys_lim_t *lims, uint32_t *props)
{
	struct reg_list *reg = sc->sc_regs[idx];
	int32_t	limit;

	limit = dbcool_read_rpm(sc, reg->lo_lim_reg);
	if (limit) {
		lims->sel_critmin = limit;
		*props |= PROP_CRITMIN;
	}
}

/*
 * Update sensor limits in the chip registers
 */
static void
dbcool_set_limits(struct sysmon_envsys *sme, envsys_data_t *edata,
		  sysmon_envsys_lim_t *limits, uint32_t *props)
{
	int index = edata->sensor;
	struct dbcool_softc *sc = sme->sme_cookie;

	if (limits == NULL) {
		limits = &sc->sc_deflims[index];
		props  = &sc->sc_defprops[index];
	}
	switch (edata->units) {
	    case ENVSYS_STEMP:
		dbcool_set_temp_limits(sc, index, limits, props);
		break;
	    case ENVSYS_SVOLTS_DC:
		dbcool_set_volt_limits(sc, index, limits, props);
		break;
	    case ENVSYS_SFANRPM:
		dbcool_set_fan_limits(sc, index, limits, props);

	    /* FALLTHROUGH */
	    default:
		break;
	}
	*props &= ~PROP_DRIVER_LIMITS;
}

static void
dbcool_set_temp_limits(struct dbcool_softc *sc, int idx,
		       sysmon_envsys_lim_t *lims, uint32_t *props)
{
	struct reg_list *reg = sc->sc_regs[idx];
	int32_t	limit;

	if (*props & PROP_CRITMIN) {
		limit = lims->sel_critmin - 273150000;
		limit /= 1000000;
		if (sc->sc_temp_offset) {
			limit += sc->sc_temp_offset;
			if (limit < 0)
				limit = 0;
			else if (limit > 255)
				limit = 255;
		} else {
			if (limit < -127)
				limit = -127;
			else if (limit > 127)
				limit = 127;
		}
		sc->sc_dc.dc_writereg(&sc->sc_dc, reg->lo_lim_reg,
				      (uint8_t)limit);
	} else if (*props & PROP_DRIVER_LIMITS) {
		if (sc->sc_temp_offset)
			limit = 0x00;
		else
			limit = 0x80;
		sc->sc_dc.dc_writereg(&sc->sc_dc, reg->lo_lim_reg,
				      (uint8_t)limit);
	}

	if (*props & PROP_CRITMAX) {
		limit = lims->sel_critmax - 273150000;
		limit /= 1000000;
		if (sc->sc_temp_offset) {
			limit += sc->sc_temp_offset;
			if (limit < 0)
				limit = 0;
			else if (limit > 255)
				limit = 255;
		} else {
			if (limit < -127)
				limit = -127;
			else if (limit > 127)
				limit = 127;
		}
		sc->sc_dc.dc_writereg(&sc->sc_dc, reg->hi_lim_reg,
				      (uint8_t)limit);
	} else if (*props & PROP_DRIVER_LIMITS) {
		if (sc->sc_temp_offset)
			limit = 0xff;
		else
			limit = 0x7f;
		sc->sc_dc.dc_writereg(&sc->sc_dc, reg->hi_lim_reg,
				      (uint8_t)limit);
	}
}

static void
dbcool_set_volt_limits(struct dbcool_softc *sc, int idx,
		       sysmon_envsys_lim_t *lims, uint32_t *props)
{
	struct reg_list *reg = sc->sc_regs[idx];
	int64_t limit;
	int nom;

	nom = nominal_voltages[sc->sc_dc.dc_chip->table[idx].nom_volt_index];
	if (nom < 0)
		nom = dbcool_supply_voltage(sc);
	nom *= 1000000;		/* scale for microvolts */

	if (*props & PROP_CRITMIN) {
		limit = lims->sel_critmin;
		limit *= 0xc0;
		limit /= nom;
		if (limit > 0xff)
			limit = 0xff;
		else if (limit < 0)
			limit = 0;
		sc->sc_dc.dc_writereg(&sc->sc_dc, reg->lo_lim_reg, limit);
	} else if (*props & PROP_DRIVER_LIMITS)
		sc->sc_dc.dc_writereg(&sc->sc_dc, reg->lo_lim_reg, 0);

	if (*props & PROP_CRITMAX) {
		limit = lims->sel_critmax;
		limit *= 0xc0;
		limit /= nom;
		if (limit > 0xff)
			limit = 0xff;
		else if (limit < 0)
			limit = 0;
		sc->sc_dc.dc_writereg(&sc->sc_dc, reg->hi_lim_reg, limit);
	} else if (*props & PROP_DRIVER_LIMITS)
		sc->sc_dc.dc_writereg(&sc->sc_dc, reg->hi_lim_reg, 0xff);
}

static void
dbcool_set_fan_limits(struct dbcool_softc *sc, int idx,
		      sysmon_envsys_lim_t *lims, uint32_t *props)
{
	struct reg_list *reg = sc->sc_regs[idx];
	int32_t	limit, dividend;

	if (*props & PROP_CRITMIN) {
		limit = lims->sel_critmin;
		if (limit == 0)
			limit = 0xffff;
		else {
			if (sc->sc_dc.dc_chip->flags & DBCFLAG_ADM1030)
				dividend = 11250 * 60;
			else
				dividend = 90000 * 60;
			limit = limit / dividend;
			if (limit > 0xffff)
				limit = 0xffff;
		}
		sc->sc_dc.dc_writereg(&sc->sc_dc, reg->lo_lim_reg,
				      limit & 0xff);
		limit >>= 8;
		sc->sc_dc.dc_writereg(&sc->sc_dc, reg->lo_lim_reg + 1,
				      limit & 0xff);
	} else if (*props & PROP_DRIVER_LIMITS) {
		sc->sc_dc.dc_writereg(&sc->sc_dc, reg->lo_lim_reg, 0xff);
		sc->sc_dc.dc_writereg(&sc->sc_dc, reg->lo_lim_reg + 1, 0xff);
	}
}

MODULE(MODULE_CLASS_DRIVER, dbcool, "i2cexec,sysmon_envsys");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
dbcool_modcmd(modcmd_t cmd, void *opaque)
{
	int error = 0;
#ifdef _MODULE
	static struct sysctllog *dbcool_sysctl_clog;
#endif

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_dbcool,
		    cfattach_ioconf_dbcool, cfdata_ioconf_dbcool);
		sysctl_dbcoolsetup(&dbcool_sysctl_clog);
#endif
		return error;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_dbcool,
		    cfattach_ioconf_dbcool, cfdata_ioconf_dbcool);
		sysctl_teardown(&dbcool_sysctl_clog);
#endif
		return error;
	default:
		return ENOTTY;
	}
}
