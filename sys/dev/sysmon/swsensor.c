/*	$NetBSD: swsensor.c,v 1.15 2015/04/25 23:55:23 pgoyette Exp $ */
/*
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: swsensor.c,v 1.15 2015/04/25 23:55:23 pgoyette Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <dev/sysmon/sysmonvar.h>
#include <dev/sysmon/sysmon_envsysvar.h>

#include <prop/proplib.h>

#ifndef _MODULE
#include "opt_modular.h"
#endif

int swsensorattach(int);

static struct sysctllog *swsensor_sysctllog = NULL;

static int sensor_value_sysctl = 0;
static int sensor_state_sysctl = 0;

static struct sysmon_envsys *swsensor_sme;
static envsys_data_t swsensor_edata;

static int32_t sw_sensor_value;
static int32_t sw_sensor_state;
static int32_t sw_sensor_limit;
static int32_t sw_sensor_mode;
static int32_t sw_sensor_defprops;
sysmon_envsys_lim_t sw_sensor_deflims;

MODULE(MODULE_CLASS_DRIVER, swsensor, "sysmon_envsys");

/*
 * Set-up the sysctl interface for setting the sensor's cur_value
 */

static
void
sysctl_swsensor_setup(void)
{
	int ret;
	int node_sysctl_num;
	const struct sysctlnode *me = NULL;
	const struct sysctlnode *me2;

	KASSERT(swsensor_sysctllog == NULL);

	ret = sysctl_createv(&swsensor_sysctllog, 0, NULL, &me,
			     CTLFLAG_READWRITE,
			     CTLTYPE_NODE, "swsensor", NULL,
			     NULL, 0, NULL, 0,
			     CTL_HW, CTL_CREATE, CTL_EOL);
	if (ret != 0)
		return;

	node_sysctl_num = me->sysctl_num;
	ret = sysctl_createv(&swsensor_sysctllog, 0, NULL, &me2,
			     CTLFLAG_READWRITE,
			     CTLTYPE_INT, "cur_value", NULL,
			     NULL, 0, &sw_sensor_value, 0,
			     CTL_HW, node_sysctl_num, CTL_CREATE, CTL_EOL);

	if (ret == 0)
		sensor_value_sysctl = me2->sysctl_num;

	node_sysctl_num = me->sysctl_num;
	ret = sysctl_createv(&swsensor_sysctllog, 0, NULL, &me2,
			     CTLFLAG_READWRITE,
			     CTLTYPE_INT, "state", NULL,
			     NULL, 0, &sw_sensor_state, 0,
			     CTL_HW, node_sysctl_num, CTL_CREATE, CTL_EOL);

	if (ret == 0)
		sensor_state_sysctl = me2->sysctl_num;
}

/*
 * "Polling" routine to update sensor value
 */
static
void
swsensor_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{

	edata->value_cur = sw_sensor_value;

	/* If value outside of legal range, mark it invalid */
	if ((edata->flags & ENVSYS_FVALID_MIN &&
	     edata->value_cur < edata->value_min) ||
	    (edata->flags & ENVSYS_FVALID_MAX &&
	     edata->value_cur > edata->value_max)) {
		edata->state = ENVSYS_SINVALID;
		return;
	}

	/*
	 * Set state.  If we're handling the limits ourselves, do the
	 * compare; otherwise just assume the value is valid.
	 * If sensor state has been set from userland (via sysctl),
	 * just report that value.
	 */
	if (sw_sensor_state != ENVSYS_SVALID)
		edata->state = sw_sensor_state;
	else if ((sw_sensor_mode == 2) && (edata->upropset & PROP_CRITMIN) &&
	    (edata->upropset & PROP_DRIVER_LIMITS) &&
	    (edata->value_cur < edata->limits.sel_critmin))
		edata->state = ENVSYS_SCRITUNDER;
	else
		edata->state = ENVSYS_SVALID;
}

/*
 * Sensor get/set limit routines
 */

static void     
swsensor_get_limits(struct sysmon_envsys *sme, envsys_data_t *edata,
                  sysmon_envsys_lim_t *limits, uint32_t *props)  
{

	*props = PROP_CRITMIN | PROP_DRIVER_LIMITS;
	limits->sel_critmin = sw_sensor_limit;
}

static void     
swsensor_set_limits(struct sysmon_envsys *sme, envsys_data_t *edata,
                  sysmon_envsys_lim_t *limits, uint32_t *props)  
{

	if (limits == NULL) {
		limits = &sw_sensor_deflims;
		props = &sw_sensor_defprops;
	}
	if (*props & PROP_CRITMIN)
		sw_sensor_limit = limits->sel_critmin;

	/*
	 * If the limit we can handle (crit-min) is set, and no
	 * other limit is set, tell sysmon that the driver will
	 * handle the limit checking.
	 */
	if ((*props & PROP_LIMITS) == PROP_CRITMIN)
		*props |= PROP_DRIVER_LIMITS;
	else
		*props &= ~PROP_DRIVER_LIMITS;
}

/*
 * Module management
 */

static
int
swsensor_init(void *arg)
{
	int error, val = 0;
	const char *key, *str;
	prop_dictionary_t pd = (prop_dictionary_t)arg;
	prop_object_t po, obj;
	prop_object_iterator_t iter;
	prop_type_t type;
	const struct sme_descr_entry *descr;

	swsensor_sme = sysmon_envsys_create();
	if (swsensor_sme == NULL)
		return ENOTTY;

	swsensor_sme->sme_name = "swsensor";
	swsensor_sme->sme_cookie = &swsensor_edata;
	swsensor_sme->sme_refresh = swsensor_refresh;
	swsensor_sme->sme_set_limits = NULL;
	swsensor_sme->sme_get_limits = NULL;

	/* Set defaults in case no prop dictionary given */

	swsensor_edata.units = ENVSYS_INTEGER;
	swsensor_edata.flags = 0;
	sw_sensor_mode = 0;
	sw_sensor_value = 0;
	sw_sensor_limit = 0;

	/* Iterate over the provided dictionary, if any */
	if (pd != NULL) {
		iter = prop_dictionary_iterator(pd);
		if (iter == NULL)
			return ENOMEM;

		while ((obj = prop_object_iterator_next(iter)) != NULL) {
			key = prop_dictionary_keysym_cstring_nocopy(obj);
			po  = prop_dictionary_get_keysym(pd, obj);
			type = prop_object_type(po);
			if (type == PROP_TYPE_NUMBER)
				val = prop_number_integer_value(po);

			/* Sensor type/units */
			if (strcmp(key, "type") == 0) {
				if (type == PROP_TYPE_NUMBER) {
					descr = sme_find_table_entry(
							SME_DESC_UNITS, val);
					if (descr == NULL)
						return EINVAL;
					swsensor_edata.units = descr->type;
					continue;
				}
				if (type != PROP_TYPE_STRING)
					return EINVAL;
				str = prop_string_cstring_nocopy(po);
				descr = sme_find_table_desc(SME_DESC_UNITS,
							    str);
				if (descr == NULL)
					return EINVAL;
				swsensor_edata.units = descr->type;
				continue;
			}

			/* Sensor flags */
			if (strcmp(key, "flags") == 0) {
				if (type != PROP_TYPE_NUMBER)
					return EINVAL;
				swsensor_edata.flags = val;
				continue;
			}

			/* Sensor limit behavior
			 *	0 - simple sensor, no hw limits
			 *	1 - simple sensor, hw provides initial limit
			 *	2 - complex sensor, hw provides settable 
			 *	    limits and does its own limit checking
			 */
			if (strcmp(key, "mode") == 0) {
				if (type != PROP_TYPE_NUMBER)
					return EINVAL;
				sw_sensor_mode = val;
				if (sw_sensor_mode > 2)
					sw_sensor_mode = 2;
				else if (sw_sensor_mode < 0)
					sw_sensor_mode = 0;
				continue;
			}

			/* Grab any limit that might be specified */
			if (strcmp(key, "limit") == 0) {
				if (type != PROP_TYPE_NUMBER)
					return EINVAL;
				sw_sensor_limit = val;
				continue;
			}

			/* Grab the initial value */
			if (strcmp(key, "value") == 0) {
				if (type != PROP_TYPE_NUMBER)
					return EINVAL;
				sw_sensor_value = val;
				continue;
			}

			/* Grab value_min and value_max */
			if (strcmp(key, "value_min") == 0) {
				if (type != PROP_TYPE_NUMBER)
					return EINVAL;
				swsensor_edata.value_min = val;
				swsensor_edata.flags |= ENVSYS_FVALID_MIN;
				continue;
			}
			if (strcmp(key, "value_max") == 0) {
				if (type != PROP_TYPE_NUMBER)
					return EINVAL;
				swsensor_edata.value_max = val;
				swsensor_edata.flags |= ENVSYS_FVALID_MAX;
				continue;
			}

			/* See if sensor reports percentages vs raw values */
			if (strcmp(key, "percentage") == 0) {
				if (type != PROP_TYPE_BOOL)
					return EINVAL;
				if (prop_bool_true(po))
					swsensor_edata.flags |= ENVSYS_FPERCENT;
				continue;
			}

			/* Unrecognized dicttionary object */
#ifdef DEBUG
			printf("%s: unknown attribute %s\n", __func__, key);
#endif
			return EINVAL;

		} /* while */
		prop_object_iterator_release(iter);
	}

	/* Initialize limit processing */
	if (sw_sensor_mode >= 1)
		swsensor_sme->sme_get_limits = swsensor_get_limits;

	if (sw_sensor_mode == 2)
		swsensor_sme->sme_set_limits = swsensor_set_limits;

	if (sw_sensor_mode != 0) {
		swsensor_edata.flags |= ENVSYS_FMONLIMITS;
		swsensor_get_limits(swsensor_sme, &swsensor_edata,
		    &sw_sensor_deflims, &sw_sensor_defprops);
	}

	strlcpy(swsensor_edata.desc, "sensor", ENVSYS_DESCLEN);

	/* Wait for refresh to validate the sensor value */
	swsensor_edata.state = ENVSYS_SINVALID;
	sw_sensor_state = ENVSYS_SVALID;

	error = sysmon_envsys_sensor_attach(swsensor_sme, &swsensor_edata);
	if (error != 0) {
		aprint_error("sysmon_envsys_sensor_attach failed: %d\n", error);
		return error;
	}

	error = sysmon_envsys_register(swsensor_sme);
	if (error != 0) {
		aprint_error("sysmon_envsys_register failed: %d\n", error);
		return error;
	}

	sysctl_swsensor_setup();
	aprint_normal("swsensor: initialized\n");

	return 0;
}

static
int
swsensor_fini(void *arg)
{

	sysmon_envsys_unregister(swsensor_sme);

	sysctl_teardown(&swsensor_sysctllog);

	return 0;
}

static
int
swsensor_modcmd(modcmd_t cmd, void *arg)
{
	int ret;

	switch (cmd) {
	case MODULE_CMD_INIT:
		ret = swsensor_init(arg);
		break;

	case MODULE_CMD_FINI:
		ret = swsensor_fini(arg);
		break;

	case MODULE_CMD_STAT:
	default:
		ret = ENOTTY;
	}

	return ret;
}

int
swsensorattach(int n __unused)
{

#ifdef MODULAR
	/*
	 * Modular kernels will automatically load any built-in modules
	 * and call their modcmd() routine, so we don't need to do it
	 * again as part of pseudo-device configuration.
	 */
	return 0;
#else
	return swsensor_init(NULL);
#endif
}
