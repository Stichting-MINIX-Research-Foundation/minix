/*	$NetBSD: acpi_bat.c,v 1.116 2018/08/10 17:11:56 riastradh Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum of By Noon Software, Inc.
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
 * Copyright 2001 Bill Sommerfeld.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ACPI Battery Driver.
 *
 * ACPI defines two different battery device interfaces: "Control
 * Method" batteries, in which AML methods are defined in order to get
 * battery status and set battery alarm thresholds, and a "Smart
 * Battery" device, which is an SMbus device accessed through the ACPI
 * Embedded Controller device.
 *
 * This driver is for the "Control Method"-style battery only.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_bat.c,v 1.116 2018/08/10 17:11:56 riastradh Exp $");

#include <sys/param.h>
#include <sys/condvar.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#define _COMPONENT		 ACPI_BAT_COMPONENT
ACPI_MODULE_NAME		 ("acpi_bat")

#define	ACPI_NOTIFY_BAT_STATUS	 0x80
#define	ACPI_NOTIFY_BAT_INFO	 0x81

/*
 * Sensor indexes.
 */
enum {
	ACPIBAT_PRESENT		 = 0,
	ACPIBAT_DVOLTAGE	 = 1,
	ACPIBAT_VOLTAGE		 = 2,
	ACPIBAT_DCAPACITY	 = 3,
	ACPIBAT_LFCCAPACITY	 = 4,
	ACPIBAT_CAPACITY	 = 5,
	ACPIBAT_CHARGERATE	 = 6,
	ACPIBAT_DISCHARGERATE	 = 7,
	ACPIBAT_CHARGING	 = 8,
	ACPIBAT_CHARGE_STATE	 = 9,
	ACPIBAT_COUNT		 = 10
};

/*
 * Battery Information, _BIF
 * (ACPI 3.0, sec. 10.2.2.1).
 */
enum {
	ACPIBAT_BIF_UNIT	 = 0,
	ACPIBAT_BIF_DCAPACITY	 = 1,
	ACPIBAT_BIF_LFCCAPACITY	 = 2,
	ACPIBAT_BIF_TECHNOLOGY	 = 3,
	ACPIBAT_BIF_DVOLTAGE	 = 4,
	ACPIBAT_BIF_WCAPACITY	 = 5,
	ACPIBAT_BIF_LCAPACITY	 = 6,
	ACPIBAT_BIF_GRANULARITY1 = 7,
	ACPIBAT_BIF_GRANULARITY2 = 8,
	ACPIBAT_BIF_MODEL	 = 9,
	ACPIBAT_BIF_SERIAL	 = 10,
	ACPIBAT_BIF_TYPE	 = 11,
	ACPIBAT_BIF_OEM		 = 12,
	ACPIBAT_BIF_COUNT	 = 13
};

/*
 * Battery Status, _BST
 * (ACPI 3.0, sec. 10.2.2.3).
 */
enum {
	ACPIBAT_BST_STATE	 = 0,
	ACPIBAT_BST_RATE	 = 1,
	ACPIBAT_BST_CAPACITY	 = 2,
	ACPIBAT_BST_VOLTAGE	 = 3,
	ACPIBAT_BST_COUNT	 = 4
};

struct acpibat_softc {
	struct acpi_devnode	*sc_node;
	struct sysmon_envsys	*sc_sme;
	struct timeval		 sc_last;
	envsys_data_t		*sc_sensor;
	kmutex_t		 sc_mutex;
	kcondvar_t		 sc_condvar;
	int32_t			 sc_dcapacity;
	int32_t			 sc_dvoltage;
	int32_t			 sc_lcapacity;
	int32_t			 sc_wcapacity;
	int                      sc_present;
};

static const char * const bat_hid[] = {
	"PNP0C0A",
	NULL
};

#define ACPIBAT_PWRUNIT_MA	0x00000001  /* mA not mW */
#define ACPIBAT_ST_DISCHARGING	0x00000001  /* battery is discharging */
#define ACPIBAT_ST_CHARGING	0x00000002  /* battery is charging */
#define ACPIBAT_ST_CRITICAL	0x00000004  /* battery is critical */

/*
 * A value used when _BST or _BIF is temporarily unknown.
 */
#define ACPIBAT_VAL_UNKNOWN	0xFFFFFFFF

#define ACPIBAT_VAL_ISVALID(x)						      \
	(((x) != ACPIBAT_VAL_UNKNOWN) ? ENVSYS_SVALID : ENVSYS_SINVALID)

static int	    acpibat_match(device_t, cfdata_t, void *);
static void	    acpibat_attach(device_t, device_t, void *);
static int	    acpibat_detach(device_t, int);
static int          acpibat_get_sta(device_t);
static ACPI_OBJECT *acpibat_get_object(ACPI_HANDLE, const char *, uint32_t);
static void         acpibat_get_info(device_t);
static void	    acpibat_print_info(device_t, ACPI_OBJECT *);
static void         acpibat_get_status(device_t);
static void         acpibat_update_info(void *);
static void         acpibat_update_status(void *);
static void         acpibat_init_envsys(device_t);
static void         acpibat_notify_handler(ACPI_HANDLE, uint32_t, void *);
static void         acpibat_refresh(struct sysmon_envsys *, envsys_data_t *);
static bool	    acpibat_resume(device_t, const pmf_qual_t *);
static void	    acpibat_get_limits(struct sysmon_envsys *, envsys_data_t *,
				       sysmon_envsys_lim_t *, uint32_t *);

CFATTACH_DECL_NEW(acpibat, sizeof(struct acpibat_softc),
    acpibat_match, acpibat_attach, acpibat_detach, NULL);

/*
 * acpibat_match:
 *
 *	Autoconfiguration `match' routine.
 */
static int
acpibat_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, bat_hid);
}

/*
 * acpibat_attach:
 *
 *	Autoconfiguration `attach' routine.
 */
static void
acpibat_attach(device_t parent, device_t self, void *aux)
{
	struct acpibat_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	ACPI_HANDLE tmp;
	ACPI_STATUS rv;

	aprint_naive(": ACPI Battery\n");
	aprint_normal(": ACPI Battery\n");

	sc->sc_node = aa->aa_node;

	sc->sc_present = 0;
	sc->sc_dvoltage = 0;
	sc->sc_dcapacity = 0;
	sc->sc_lcapacity = 0;
	sc->sc_wcapacity = 0;

	sc->sc_sme = NULL;
	sc->sc_sensor = NULL;

	mutex_init(&sc->sc_mutex, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&sc->sc_condvar, device_xname(self));

	(void)pmf_device_register(self, NULL, acpibat_resume);
	(void)acpi_register_notify(sc->sc_node, acpibat_notify_handler);

	sc->sc_sensor = kmem_zalloc(ACPIBAT_COUNT *
	    sizeof(*sc->sc_sensor), KM_SLEEP);

	if (sc->sc_sensor == NULL)
		return;

	config_interrupts(self, acpibat_init_envsys);

	/*
	 * If this is ever seen, the driver should be extended.
	 */
	rv = AcpiGetHandle(sc->sc_node->ad_handle, "_BIX", &tmp);

	if (ACPI_SUCCESS(rv))
		aprint_verbose_dev(self, "ACPI 4.0 functionality present\n");
}

/*
 * acpibat_detach:
 *
 *	Autoconfiguration `detach' routine.
 */
static int
acpibat_detach(device_t self, int flags)
{
	struct acpibat_softc *sc = device_private(self);

	acpi_deregister_notify(sc->sc_node);

	cv_destroy(&sc->sc_condvar);
	mutex_destroy(&sc->sc_mutex);

	if (sc->sc_sme != NULL)
		sysmon_envsys_unregister(sc->sc_sme);

	if (sc->sc_sensor != NULL)
		kmem_free(sc->sc_sensor, ACPIBAT_COUNT *
		    sizeof(*sc->sc_sensor));

	pmf_device_deregister(self);

	return 0;
}

/*
 * acpibat_get_sta:
 *
 *	Evaluate whether the battery is present or absent.
 *
 *	Returns: 0 for no battery, 1 for present, and -1 on error.
 */
static int
acpibat_get_sta(device_t dv)
{
	struct acpibat_softc *sc = device_private(dv);
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	rv = acpi_eval_integer(sc->sc_node->ad_handle, "_STA", &val);

	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(dv, "failed to evaluate _STA\n");
		return -1;
	}

	sc->sc_sensor[ACPIBAT_PRESENT].state = ENVSYS_SVALID;

	if ((val & ACPI_STA_BATTERY_PRESENT) == 0) {
		sc->sc_sensor[ACPIBAT_PRESENT].value_cur = 0;
		return 0;
	}

	sc->sc_sensor[ACPIBAT_PRESENT].value_cur = 1;

	return 1;
}

static ACPI_OBJECT *
acpibat_get_object(ACPI_HANDLE hdl, const char *pth, uint32_t count)
{
	ACPI_OBJECT *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;

	rv = acpi_eval_struct(hdl, pth, &buf);

	if (ACPI_FAILURE(rv))
		return NULL;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE) {
		ACPI_FREE(buf.Pointer);
		return NULL;
	}

	if (obj->Package.Count != count) {
		ACPI_FREE(buf.Pointer);
		return NULL;
	}

	return obj;
}

/*
 * acpibat_get_info:
 *
 * 	Get the battery info.
 */
static void
acpibat_get_info(device_t dv)
{
	struct acpibat_softc *sc = device_private(dv);
	ACPI_HANDLE hdl = sc->sc_node->ad_handle;
	ACPI_OBJECT *elm, *obj;
	ACPI_STATUS rv = AE_OK;
	int capunit, i, rateunit;
	uint64_t val;

	obj = acpibat_get_object(hdl, "_BIF", ACPIBAT_BIF_COUNT);

	if (obj == NULL) {
		rv = AE_ERROR;
		goto out;
	}

	elm = obj->Package.Elements;

	for (i = ACPIBAT_BIF_UNIT; i < ACPIBAT_BIF_MODEL; i++) {

		if (elm[i].Type != ACPI_TYPE_INTEGER) {
			rv = AE_TYPE;
			goto out;
		}

		if (elm[i].Integer.Value != ACPIBAT_VAL_UNKNOWN &&
		    elm[i].Integer.Value >= INT_MAX) {
			rv = AE_LIMIT;
			goto out;
		}
	}

	switch (elm[ACPIBAT_BIF_UNIT].Integer.Value) {
	case ACPIBAT_PWRUNIT_MA:
		capunit = ENVSYS_SAMPHOUR;
		rateunit = ENVSYS_SAMPS;
		break;
	default:
		capunit = ENVSYS_SWATTHOUR;
		rateunit = ENVSYS_SWATTS;
		break;
	}

	sc->sc_sensor[ACPIBAT_DCAPACITY].units = capunit;
	sc->sc_sensor[ACPIBAT_LFCCAPACITY].units = capunit;
	sc->sc_sensor[ACPIBAT_CHARGERATE].units = rateunit;
	sc->sc_sensor[ACPIBAT_DISCHARGERATE].units = rateunit;
	sc->sc_sensor[ACPIBAT_CAPACITY].units = capunit;

	/* Design capacity. */
	val = elm[ACPIBAT_BIF_DCAPACITY].Integer.Value;
	sc->sc_sensor[ACPIBAT_DCAPACITY].value_cur = val * 1000;
	sc->sc_sensor[ACPIBAT_DCAPACITY].state = ACPIBAT_VAL_ISVALID(val);

	/* Last full charge capacity. */
	val = elm[ACPIBAT_BIF_LFCCAPACITY].Integer.Value;
	sc->sc_sensor[ACPIBAT_LFCCAPACITY].value_cur = val * 1000;
	sc->sc_sensor[ACPIBAT_LFCCAPACITY].state = ACPIBAT_VAL_ISVALID(val);

	/* Design voltage. */
	val = elm[ACPIBAT_BIF_DVOLTAGE].Integer.Value;
	sc->sc_sensor[ACPIBAT_DVOLTAGE].value_cur = val * 1000;
	sc->sc_sensor[ACPIBAT_DVOLTAGE].state = ACPIBAT_VAL_ISVALID(val);

	/* Design low and warning capacity. */
	sc->sc_lcapacity = elm[ACPIBAT_BIF_LCAPACITY].Integer.Value * 1000;
	sc->sc_wcapacity = elm[ACPIBAT_BIF_WCAPACITY].Integer.Value * 1000;

	/*
	 * Initialize the maximum of current capacity
	 * to the last known full charge capacity.
	 */
	val = sc->sc_sensor[ACPIBAT_LFCCAPACITY].value_cur;
	sc->sc_sensor[ACPIBAT_CAPACITY].value_max = val;

	acpibat_print_info(dv, elm);

out:
	if (obj != NULL)
		ACPI_FREE(obj);

	if (ACPI_FAILURE(rv))
		aprint_error_dev(dv, "failed to evaluate _BIF: %s\n",
		    AcpiFormatException(rv));
}

/*
 * acpibat_print_info:
 *
 * 	Display the battery info.
 */
static void
acpibat_print_info(device_t dv, ACPI_OBJECT *elm)
{
	struct acpibat_softc *sc = device_private(dv);
	const char *tech, *unit;
	int32_t dcap, dvol;
	int i;

	for (i = ACPIBAT_BIF_OEM; i > ACPIBAT_BIF_GRANULARITY2; i--) {

		if (elm[i].Type != ACPI_TYPE_STRING)
			return;

		if (elm[i].String.Pointer == NULL)
			return;

		if (elm[i].String.Pointer[0] == '\0')
			return;
	}

	dcap = elm[ACPIBAT_BIF_DCAPACITY].Integer.Value;
	dvol = elm[ACPIBAT_BIF_DVOLTAGE].Integer.Value;

	/*
	 * Try to detect whether the battery was switched.
	 */
	if (sc->sc_dcapacity == dcap && sc->sc_dvoltage == dvol)
		return;
	else {
		sc->sc_dcapacity = dcap;
		sc->sc_dvoltage = dvol;
	}

	tech = (elm[ACPIBAT_BIF_TECHNOLOGY].Integer.Value != 0) ?
	    "rechargeable" : "non-rechargeable";

	aprint_normal_dev(dv, "%s %s %s battery\n",
	    elm[ACPIBAT_BIF_OEM].String.Pointer,
	    elm[ACPIBAT_BIF_TYPE].String.Pointer, tech);

	aprint_debug_dev(dv, "model number %s, serial number %s\n",
	    elm[ACPIBAT_BIF_MODEL].String.Pointer,
	    elm[ACPIBAT_BIF_SERIAL].String.Pointer);

#define SCALE(x) (((int)x) / 1000000), ((((int)x) % 1000000) / 1000)

	/*
	 * These values are defined as follows (ACPI 4.0, p. 388):
	 *
	 * Granularity 1.	"Battery capacity granularity between low
	 *			 and warning in [mAh] or [mWh]. That is,
	 *			 this is the smallest increment in capacity
	 *			 that the battery is capable of measuring."
	 *
	 * Granularity 2.	"Battery capacity granularity between warning
	 *			 and full in [mAh] or [mWh]. [...]"
	 */
	switch (elm[ACPIBAT_BIF_UNIT].Integer.Value) {
	case ACPIBAT_PWRUNIT_MA:
		unit = "Ah";
		break;
	default:
		unit = "Wh";
		break;
	}

	aprint_verbose_dev(dv, "granularity: "
	    "low->warn %d.%03d %s, warn->full %d.%03d %s\n",
	    SCALE(elm[ACPIBAT_BIF_GRANULARITY1].Integer.Value * 1000), unit,
	    SCALE(elm[ACPIBAT_BIF_GRANULARITY2].Integer.Value * 1000), unit);
}

/*
 * acpibat_get_status:
 *
 *	Get the current battery status.
 */
static void
acpibat_get_status(device_t dv)
{
	struct acpibat_softc *sc = device_private(dv);
	ACPI_HANDLE hdl = sc->sc_node->ad_handle;
	ACPI_OBJECT *elm, *obj;
	ACPI_STATUS rv = AE_OK;
	int i, rate, state;
	uint64_t val;

	obj = acpibat_get_object(hdl, "_BST", ACPIBAT_BST_COUNT);

	if (obj == NULL) {
		rv = AE_ERROR;
		goto out;
	}

	elm = obj->Package.Elements;

	for (i = ACPIBAT_BST_STATE; i < ACPIBAT_BST_COUNT; i++) {

		if (elm[i].Type != ACPI_TYPE_INTEGER) {
			rv = AE_TYPE;
			goto out;
		}
	}

	state = elm[ACPIBAT_BST_STATE].Integer.Value;

	if ((state & ACPIBAT_ST_CHARGING) != 0) {
		/* XXX rate can be invalid */
		rate = elm[ACPIBAT_BST_RATE].Integer.Value;
		sc->sc_sensor[ACPIBAT_CHARGERATE].state = ENVSYS_SVALID;
		sc->sc_sensor[ACPIBAT_CHARGERATE].value_cur = rate * 1000;
		sc->sc_sensor[ACPIBAT_DISCHARGERATE].state = ENVSYS_SINVALID;
		sc->sc_sensor[ACPIBAT_CHARGING].state = ENVSYS_SVALID;
		sc->sc_sensor[ACPIBAT_CHARGING].value_cur = 1;
	} else if ((state & ACPIBAT_ST_DISCHARGING) != 0) {
		rate = elm[ACPIBAT_BST_RATE].Integer.Value;
		sc->sc_sensor[ACPIBAT_DISCHARGERATE].state = ENVSYS_SVALID;
		sc->sc_sensor[ACPIBAT_DISCHARGERATE].value_cur = rate * 1000;
		sc->sc_sensor[ACPIBAT_CHARGERATE].state = ENVSYS_SINVALID;
		sc->sc_sensor[ACPIBAT_CHARGING].state = ENVSYS_SVALID;
		sc->sc_sensor[ACPIBAT_CHARGING].value_cur = 0;
	} else {
		sc->sc_sensor[ACPIBAT_CHARGING].state = ENVSYS_SVALID;
		sc->sc_sensor[ACPIBAT_CHARGING].value_cur = 0;
		sc->sc_sensor[ACPIBAT_CHARGERATE].state = ENVSYS_SINVALID;
		sc->sc_sensor[ACPIBAT_DISCHARGERATE].state = ENVSYS_SINVALID;
	}

	/* Remaining capacity. */
	val = elm[ACPIBAT_BST_CAPACITY].Integer.Value;
	sc->sc_sensor[ACPIBAT_CAPACITY].value_cur = val * 1000;
	sc->sc_sensor[ACPIBAT_CAPACITY].state = ACPIBAT_VAL_ISVALID(val);

	/* Battery voltage. */
	val = elm[ACPIBAT_BST_VOLTAGE].Integer.Value;
	sc->sc_sensor[ACPIBAT_VOLTAGE].value_cur = val * 1000;
	sc->sc_sensor[ACPIBAT_VOLTAGE].state = ACPIBAT_VAL_ISVALID(val);

	sc->sc_sensor[ACPIBAT_CHARGE_STATE].state = ENVSYS_SVALID;
	sc->sc_sensor[ACPIBAT_CHARGE_STATE].value_cur =
	    ENVSYS_BATTERY_CAPACITY_NORMAL;

	if (sc->sc_sensor[ACPIBAT_CAPACITY].value_cur < sc->sc_wcapacity) {
		sc->sc_sensor[ACPIBAT_CAPACITY].state = ENVSYS_SWARNUNDER;
		sc->sc_sensor[ACPIBAT_CHARGE_STATE].value_cur =
		    ENVSYS_BATTERY_CAPACITY_WARNING;
	}

	if (sc->sc_sensor[ACPIBAT_CAPACITY].value_cur < sc->sc_lcapacity) {
		sc->sc_sensor[ACPIBAT_CAPACITY].state = ENVSYS_SCRITUNDER;
		sc->sc_sensor[ACPIBAT_CHARGE_STATE].value_cur =
		    ENVSYS_BATTERY_CAPACITY_LOW;
	}

	if ((state & ACPIBAT_ST_CRITICAL) != 0) {
		sc->sc_sensor[ACPIBAT_CAPACITY].state = ENVSYS_SCRITICAL;
		sc->sc_sensor[ACPIBAT_CHARGE_STATE].value_cur =
		    ENVSYS_BATTERY_CAPACITY_CRITICAL;
	}

out:
	if (obj != NULL)
		ACPI_FREE(obj);

	if (ACPI_FAILURE(rv))
		aprint_error_dev(dv, "failed to evaluate _BST: %s\n",
		    AcpiFormatException(rv));
}

static void
acpibat_update_info(void *arg)
{
	device_t dv = arg;
	struct acpibat_softc *sc = device_private(dv);
	int i, rv;

	mutex_enter(&sc->sc_mutex);

	rv = acpibat_get_sta(dv);

	if (rv > 0) {
		acpibat_get_info(dv);

		/*
		 * If the status changed, update the limits.
		 */
		if (sc->sc_present == 0 &&
		    sc->sc_sensor[ACPIBAT_CAPACITY].value_max > 0)
			sysmon_envsys_update_limits(sc->sc_sme,
			    &sc->sc_sensor[ACPIBAT_CAPACITY]);
	} else {
		i = (rv < 0) ? 0 : ACPIBAT_DVOLTAGE;

		while (i < ACPIBAT_COUNT) {
			sc->sc_sensor[i].state = ENVSYS_SINVALID;
			i++;
		}
	}

	sc->sc_present = rv;

	mutex_exit(&sc->sc_mutex);
}

static void
acpibat_update_status(void *arg)
{
	device_t dv = arg;
	struct acpibat_softc *sc = device_private(dv);
	int i, rv;

	mutex_enter(&sc->sc_mutex);

	rv = acpibat_get_sta(dv);

	if (rv > 0) {

		if (sc->sc_present == 0)
			acpibat_get_info(dv);

		acpibat_get_status(dv);
	} else {
		i = (rv < 0) ? 0 : ACPIBAT_DVOLTAGE;

		while (i < ACPIBAT_COUNT) {
			sc->sc_sensor[i].state = ENVSYS_SINVALID;
			i++;
		}
	}

	sc->sc_present = rv;
	microtime(&sc->sc_last);

	cv_broadcast(&sc->sc_condvar);
	mutex_exit(&sc->sc_mutex);
}

/*
 * acpibat_notify_handler:
 *
 *	Callback from ACPI interrupt handler to notify us of an event.
 */
static void
acpibat_notify_handler(ACPI_HANDLE handle, uint32_t notify, void *context)
{
	static const int handler = OSL_NOTIFY_HANDLER;
	device_t dv = context;

	switch (notify) {

	case ACPI_NOTIFY_BUS_CHECK:
		break;

	case ACPI_NOTIFY_BAT_INFO:
	case ACPI_NOTIFY_DEVICE_CHECK:
		(void)AcpiOsExecute(handler, acpibat_update_info, dv);
		break;

	case ACPI_NOTIFY_BAT_STATUS:
		(void)AcpiOsExecute(handler, acpibat_update_status, dv);
		break;

	default:
		aprint_error_dev(dv, "unknown notify: 0x%02X\n", notify);
	}
}

static void
acpibat_init_envsys(device_t dv)
{
	struct acpibat_softc *sc = device_private(dv);
	int i;

#define INITDATA(index, unit, string)					\
	do {								\
		sc->sc_sensor[index].state = ENVSYS_SVALID;		\
		sc->sc_sensor[index].units = unit;			\
		(void)strlcpy(sc->sc_sensor[index].desc, string,	\
		    sizeof(sc->sc_sensor[index].desc));			\
	} while (/* CONSTCOND */ 0)

	INITDATA(ACPIBAT_PRESENT, ENVSYS_INDICATOR, "present");
	INITDATA(ACPIBAT_DCAPACITY, ENVSYS_SWATTHOUR, "design cap");
	INITDATA(ACPIBAT_LFCCAPACITY, ENVSYS_SWATTHOUR, "last full cap");
	INITDATA(ACPIBAT_DVOLTAGE, ENVSYS_SVOLTS_DC, "design voltage");
	INITDATA(ACPIBAT_VOLTAGE, ENVSYS_SVOLTS_DC, "voltage");
	INITDATA(ACPIBAT_CHARGERATE, ENVSYS_SWATTS, "charge rate");
	INITDATA(ACPIBAT_DISCHARGERATE, ENVSYS_SWATTS, "discharge rate");
	INITDATA(ACPIBAT_CAPACITY, ENVSYS_SWATTHOUR, "charge");
	INITDATA(ACPIBAT_CHARGING, ENVSYS_BATTERY_CHARGE, "charging");
	INITDATA(ACPIBAT_CHARGE_STATE, ENVSYS_BATTERY_CAPACITY, "charge state");

#undef INITDATA

	sc->sc_sensor[ACPIBAT_CHARGE_STATE].value_cur =
		ENVSYS_BATTERY_CAPACITY_NORMAL;

	sc->sc_sensor[ACPIBAT_CAPACITY].flags |=
	    ENVSYS_FPERCENT | ENVSYS_FVALID_MAX | ENVSYS_FMONLIMITS;

	sc->sc_sensor[ACPIBAT_CHARGE_STATE].flags |= ENVSYS_FMONSTCHANGED;

	/* Disable userland monitoring on these sensors. */
	sc->sc_sensor[ACPIBAT_VOLTAGE].flags = ENVSYS_FMONNOTSUPP;
	sc->sc_sensor[ACPIBAT_CHARGERATE].flags = ENVSYS_FMONNOTSUPP;
	sc->sc_sensor[ACPIBAT_DISCHARGERATE].flags = ENVSYS_FMONNOTSUPP;
	sc->sc_sensor[ACPIBAT_DCAPACITY].flags = ENVSYS_FMONNOTSUPP;
	sc->sc_sensor[ACPIBAT_LFCCAPACITY].flags = ENVSYS_FMONNOTSUPP;
	sc->sc_sensor[ACPIBAT_DVOLTAGE].flags = ENVSYS_FMONNOTSUPP;

	/* Attach rnd(9) to the (dis)charge rates. */
	sc->sc_sensor[ACPIBAT_CHARGERATE].flags |= ENVSYS_FHAS_ENTROPY;
	sc->sc_sensor[ACPIBAT_DISCHARGERATE].flags |= ENVSYS_FHAS_ENTROPY;

	sc->sc_sme = sysmon_envsys_create();

	for (i = 0; i < ACPIBAT_COUNT; i++) {

		if (sysmon_envsys_sensor_attach(sc->sc_sme,
			&sc->sc_sensor[i]))
			goto fail;
	}

	sc->sc_sme->sme_name = device_xname(dv);
	sc->sc_sme->sme_cookie = dv;
	sc->sc_sme->sme_refresh = acpibat_refresh;
	sc->sc_sme->sme_class = SME_CLASS_BATTERY;
	sc->sc_sme->sme_flags = SME_POLL_ONLY | SME_INIT_REFRESH;
	sc->sc_sme->sme_get_limits = acpibat_get_limits;

	acpibat_update_info(dv);
	acpibat_update_status(dv);

	if (sysmon_envsys_register(sc->sc_sme))
		goto fail;

	return;

fail:
	aprint_error_dev(dv, "failed to initialize sysmon\n");

	sysmon_envsys_destroy(sc->sc_sme);
	kmem_free(sc->sc_sensor, ACPIBAT_COUNT * sizeof(*sc->sc_sensor));

	sc->sc_sme = NULL;
	sc->sc_sensor = NULL;
}

static void
acpibat_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	device_t self = sme->sme_cookie;
	struct acpibat_softc *sc;
	struct timeval tv, tmp;
	ACPI_STATUS rv;

	sc = device_private(self);

	tmp.tv_sec = 10;
	tmp.tv_usec = 0;

	microtime(&tv);
	timersub(&tv, &tmp, &tv);

	if (timercmp(&tv, &sc->sc_last, <) != 0)
		return;

	if (mutex_tryenter(&sc->sc_mutex) == 0)
		return;

	rv = AcpiOsExecute(OSL_NOTIFY_HANDLER, acpibat_update_status, self);

	if (ACPI_SUCCESS(rv))
		cv_timedwait(&sc->sc_condvar, &sc->sc_mutex, hz);

	mutex_exit(&sc->sc_mutex);
}

static bool
acpibat_resume(device_t dv, const pmf_qual_t *qual)
{

	(void)AcpiOsExecute(OSL_NOTIFY_HANDLER, acpibat_update_info, dv);
	(void)AcpiOsExecute(OSL_NOTIFY_HANDLER, acpibat_update_status, dv);

	return true;
}

static void
acpibat_get_limits(struct sysmon_envsys *sme, envsys_data_t *edata,
    sysmon_envsys_lim_t *limits, uint32_t *props)
{
	device_t dv = sme->sme_cookie;
	struct acpibat_softc *sc = device_private(dv);

	if (edata->sensor != ACPIBAT_CAPACITY)
		return;

	limits->sel_critmin = sc->sc_lcapacity;
	limits->sel_warnmin = sc->sc_wcapacity;

	*props |= PROP_BATTCAP | PROP_BATTWARN | PROP_DRIVER_LIMITS;
}

MODULE(MODULE_CLASS_DRIVER, acpibat, "sysmon_envsys");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
acpibat_modcmd(modcmd_t cmd, void *aux)
{
	int rv = 0;

	switch (cmd) {

	case MODULE_CMD_INIT:

#ifdef _MODULE
		rv = config_init_component(cfdriver_ioconf_acpibat,
		    cfattach_ioconf_acpibat, cfdata_ioconf_acpibat);
#endif
		break;

	case MODULE_CMD_FINI:

#ifdef _MODULE
		rv = config_fini_component(cfdriver_ioconf_acpibat,
		    cfattach_ioconf_acpibat, cfdata_ioconf_acpibat);
#endif
		break;

	default:
		rv = ENOTTY;
	}

	return rv;
}
