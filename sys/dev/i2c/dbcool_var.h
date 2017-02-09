/*	$NetBSD: dbcool_var.h,v 1.14 2011/08/02 14:06:15 pgoyette Exp $ */

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
 * A driver for dbCool(tm) family of environmental controllers
 */

#ifndef DBCOOLVAR_H
#define DBCOOLVAR_H

#ifdef DEBUG
#define DBCOOL_DEBUG
#endif

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dbcool_var.h,v 1.14 2011/08/02 14:06:15 pgoyette Exp $");

#include <dev/i2c/i2cvar.h>

#include <dev/sysmon/sysmonvar.h>

#include <dev/i2c/dbcool_reg.h>

enum dbc_pwm_params {
	DBC_PWM_BEHAVIOR = 0,
	DBC_PWM_MIN_DUTY,
	DBC_PWM_MAX_DUTY,
	DBC_PWM_CUR_DUTY,
	DBC_PWM_LAST_PARAM
};

enum dbc_sensor_type {
	DBC_CTL = 0,
	DBC_TEMP,
	DBC_VOLT,
	DBC_FAN,
	DBC_VID,
	DBC_EOF
};

#define	DBCFLAG_TEMPOFFSET	0x0001
#define	DBCFLAG_HAS_MAXDUTY	0x0002
#define	DBCFLAG_HAS_SHDN	0x0004
#define	DBCFLAG_MULTI_VCC	0x0008
#define	DBCFLAG_4BIT_VER	0x0010
#define	DBCFLAG_OBSOLETE	0x0020	/* was DBCFLAG_HAS_VID */
#define	DBCFLAG_HAS_VID_SEL	0x0040
#define	DBCFLAG_HAS_PECI	0x0080
#define	DBCFLAG_NO_READBYTE	0x1000
#define	DBCFLAG_ADM1030		0x2000
#define	DBCFLAG_ADT7466		0x4000

/* Maximum sensors for any dbCool device */
#define DBCOOL_MAXSENSORS       16

struct reg_list {
	uint8_t val_reg;
	uint8_t hi_lim_reg;
	uint8_t lo_lim_reg;
};

struct dbcool_sensor {
	enum dbc_sensor_type type;
	struct reg_list	reg;
	int name_index;
	int sysctl_index;
	int nom_volt_index;
};

/*
 * The members of dbcool_power_control need to stay in the same order
 * as the enum dbc_pwm_params above
 */
struct dbcool_power_control {
	uint8_t	power_regs[DBC_PWM_LAST_PARAM];
	const char *desc;
};

struct chip_id;

struct dbcool_chipset {
	i2c_tag_t dc_tag;
	i2c_addr_t dc_addr;
	void (*dc_writereg)(struct dbcool_chipset *, uint8_t, uint8_t);
	uint8_t (*dc_readreg)(struct dbcool_chipset *, uint8_t);
	struct chip_id *dc_chip;
};

struct dbcool_softc {
	device_t sc_dev;
	struct sysmon_envsys *sc_sme;
	struct dbcool_chipset sc_dc;
	envsys_data_t sc_sensor[DBCOOL_MAXSENSORS];
	sysmon_envsys_lim_t sc_deflims[DBCOOL_MAXSENSORS];
	uint32_t sc_defprops[DBCOOL_MAXSENSORS];
	int sc_root_sysctl_num;
	int sc_sysctl_num[DBCOOL_MAXSENSORS];
	struct reg_list *sc_regs[DBCOOL_MAXSENSORS];
	int sc_nom_volt[DBCOOL_MAXSENSORS];
	int sc_temp_offset;
	int64_t sc_supply_voltage;
	bool sc_suspend;
	struct sysctllog *sc_sysctl_log;
#ifdef DBCOOL_DEBUG
	uint8_t sc_user_reg;
#endif
};

struct chip_id {
	uint8_t company;
	uint8_t device;
	uint8_t rev;
	struct dbcool_sensor *table;
	struct dbcool_power_control *power;
	int flags;
	int rpm_dividend;
	const char *name;
};

/*
 * Expose some routines for the macppc's ki2c match/attach routines
 */
uint8_t dbcool_readreg(struct dbcool_chipset *, uint8_t);
void dbcool_writereg(struct dbcool_chipset *, uint8_t, uint8_t);
void dbcool_setup(device_t); 
int dbcool_chip_ident(struct dbcool_chipset *);
bool dbcool_pmf_suspend(device_t, const pmf_qual_t *);
bool dbcool_pmf_resume(device_t, const pmf_qual_t *);

#endif	/* def DBCOOLVAR_H */
