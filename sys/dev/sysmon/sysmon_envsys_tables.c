/* $NetBSD: sysmon_envsys_tables.c,v 1.12 2014/05/18 11:46:23 kardel Exp $ */

/*-
 * Copyright (c) 2007 Juan Romero Pardines.
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sysmon_envsys_tables.c,v 1.12 2014/05/18 11:46:23 kardel Exp $");

#include <sys/types.h>

#include <dev/sysmon/sysmonvar.h>
#include <dev/sysmon/sysmon_envsysvar.h>

/*
 * Available units type descriptions.
 */
static const struct sme_descr_entry sme_units_description[] = {
	{ ENVSYS_STEMP,		PENVSYS_TYPE_TEMP,	"Temperature" },
	{ ENVSYS_SFANRPM,	PENVSYS_TYPE_FAN,	"Fan" },
	{ ENVSYS_SVOLTS_AC,	PENVSYS_TYPE_VOLTAGE,	"Voltage AC" },
	{ ENVSYS_SVOLTS_DC,	PENVSYS_TYPE_VOLTAGE,	"Voltage DC" },
	{ ENVSYS_SOHMS,		PENVSYS_TYPE_RESISTANCE,"Ohms" },
	{ ENVSYS_SWATTS,	PENVSYS_TYPE_POWER,	"Watts" },
	{ ENVSYS_SAMPS,		PENVSYS_TYPE_POWER,	"Ampere" },
	{ ENVSYS_SWATTHOUR,	PENVSYS_TYPE_BATTERY,	"Watt hour" },
	{ ENVSYS_SAMPHOUR,	PENVSYS_TYPE_BATTERY,	"Ampere hour" },
	{ ENVSYS_INDICATOR,	PENVSYS_TYPE_INDICATOR,	"Indicator" },
	{ ENVSYS_INTEGER,	PENVSYS_TYPE_INDICATOR,	"Integer" },
	{ ENVSYS_DRIVE,		PENVSYS_TYPE_DRIVE,	"Drive" },
	{ ENVSYS_BATTERY_CAPACITY, PENVSYS_TYPE_BATTERY,"Battery capacity" },
	{ ENVSYS_BATTERY_CHARGE, -1,			"Battery charge" },
	{ ENVSYS_SRELHUMIDITY,	-1,			"relative Humidity" },
	{ -1,			-1,			"unknown" }
};

/*
 * Available sensor state descriptions.
 */
static const struct sme_descr_entry sme_state_description[] = {
	{ ENVSYS_SVALID,	-1, 	"valid" },
	{ ENVSYS_SINVALID,	-1, 	"invalid" },
	{ ENVSYS_SCRITICAL,	-1, 	"critical" },
	{ ENVSYS_SCRITUNDER,	-1, 	"critical-under" },
	{ ENVSYS_SCRITOVER,	-1, 	"critical-over" },
	{ ENVSYS_SWARNUNDER,	-1, 	"warning-under" },
	{ ENVSYS_SWARNOVER,	-1, 	"warning-over" },
	{ -1,			-1, 	"unknown" }
};

/*
 * Available drive state descriptions.
 */
static const struct sme_descr_entry sme_drivestate_description[] = {
	{ ENVSYS_DRIVE_EMPTY,		-1, 	"unknown" },
	{ ENVSYS_DRIVE_READY,		-1, 	"ready" },
	{ ENVSYS_DRIVE_POWERUP,		-1, 	"powering up" },
	{ ENVSYS_DRIVE_ONLINE,		-1, 	"online" },
	{ ENVSYS_DRIVE_OFFLINE, 	-1, 	"offline" },
	{ ENVSYS_DRIVE_IDLE,		-1, 	"idle" },
	{ ENVSYS_DRIVE_ACTIVE,		-1, 	"active" },
	{ ENVSYS_DRIVE_BUILD,		-1,	"building" },
	{ ENVSYS_DRIVE_REBUILD,		-1, 	"rebuilding" },
	{ ENVSYS_DRIVE_POWERDOWN,	-1, 	"powering down" },
	{ ENVSYS_DRIVE_FAIL,		-1, 	"failed" },
	{ ENVSYS_DRIVE_PFAIL,		-1, 	"degraded" },
	{ ENVSYS_DRIVE_MIGRATING,	-1,	"migrating" },
	{ ENVSYS_DRIVE_CHECK,		-1,	"checking" },
	{ -1,				-1, 	"unknown" }
};

/*
 * Available battery capacity descriptions.
 */
static const struct sme_descr_entry sme_batterycap_description[] = {
	{ ENVSYS_BATTERY_CAPACITY_NORMAL,	-1,	"NORMAL" },
	{ ENVSYS_BATTERY_CAPACITY_WARNING,	-1, 	"WARNING" },
	{ ENVSYS_BATTERY_CAPACITY_CRITICAL,	-1, 	"CRITICAL" },
	{ ENVSYS_BATTERY_CAPACITY_LOW,		-1,	"LOW" },
	{ -1,					-1, 	"UNKNOWN" }
};

/*
 * Available indicator descriptions.
 */
static const struct sme_descr_entry sme_indicator_description[] = {
	{ ENVSYS_INDICATOR_FALSE,		-1,	"FALSE" },
	{ ENVSYS_INDICATOR_TRUE,		-1, 	"TRUE" },
	{ -1,					-1, 	"UNKNOWN" }
};

static const struct sme_descr_entry *
sme_find_table(enum sme_descr_type table_id)
{
	switch (table_id) {
	case SME_DESC_UNITS:
		return sme_units_description;
		break;
	case SME_DESC_STATES:
		return sme_state_description;
		break;
	case SME_DESC_DRIVE_STATES:
		return sme_drivestate_description;
		break;
	case SME_DESC_BATTERY_CAPACITY:
		return sme_batterycap_description;
		break;
	case SME_DESC_INDICATOR:
		return sme_indicator_description;
		break;
	default:
		return NULL;
	}
}

/*
 * Returns the entry from specified table with type == key
 */
const struct sme_descr_entry *
sme_find_table_entry(enum sme_descr_type table_id, int key)
{
	const struct sme_descr_entry *table = sme_find_table(table_id);

	if (table != NULL)
		for (; table->type != -1; table++)
			if (table->type == key)
				return table;

	return NULL;
}

const struct sme_descr_entry *
sme_find_table_desc(enum sme_descr_type table_id, const char *str)
{
	const struct sme_descr_entry *table = sme_find_table(table_id);

	if (table != NULL)
		for (; table->type != -1; table++)
			if (strcmp(table->desc, str) == 0)
				return table;
	return NULL;
}
