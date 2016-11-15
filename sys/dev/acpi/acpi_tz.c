/* $NetBSD: acpi_tz.c,v 1.88 2015/04/23 23:23:00 pgoyette Exp $ */

/*
 * Copyright (c) 2003 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * ACPI Thermal Zone driver
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_tz.c,v 1.88 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/kmem.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_power.h>

#define _COMPONENT		ACPI_TZ_COMPONENT
ACPI_MODULE_NAME		("acpi_tz")

#define ACPI_NOTIFY_TZ_ZONE	0x80
#define ACPI_NOTIFY_TZ_TRIP	0x81
#define ACPI_NOTIFY_TZ_DEVLIST	0x82

#define ATZ_F_CRITICAL		0x01	/* zone critical */
#define ATZ_F_HOT		0x02	/* zone hot */
#define ATZ_F_PASSIVE		0x04	/* zone passive cooling */
#define ATZ_F_PASSIVEONLY	0x08	/* zone is passive cooling only */

#define ATZ_ACTIVE_NONE		  -1

/*
 * The constants are as follows:
 *
 *   ATZ_TZP_RATE	default polling interval (30 seconds) if no _TZP
 *   ATZ_NLEVELS	number of cooling levels for _ACx and _ALx
 *   ATZ_ZEROC		0 C, measured in 0.1 Kelvin
 *   ATZ_TMP_INVALID	temporarily invalid temperature
 *   ATZ_ZONE_EXPIRE	zone info refetch interval (15 minutes)
 */
#define ATZ_TZP_RATE		300
#define ATZ_NLEVELS		10
#define ATZ_ZEROC		2732
#define ATZ_TMP_INVALID		0xffffffff
#define ATZ_ZONE_EXPIRE		9000

/*
 * All temperatures are reported in 0.1 Kelvin.
 * The ACPI specification assumes that K = C + 273.2
 * rather than the nominal 273.15 used by envsys(4).
 */
#define	ATZ2UKELVIN(t) ((t) * 100000 - 50000)

struct acpitz_zone {
	ACPI_BUFFER		 al[ATZ_NLEVELS];
	uint32_t		 ac[ATZ_NLEVELS];
	uint32_t		 crt;
	uint32_t		 hot;
	uint32_t		 rtv;
	uint32_t		 psv;
	uint32_t		 tc1;
	uint32_t		 tc2;
	uint32_t		 tmp;
	uint32_t		 prevtmp;
	uint32_t		 tzp;
	uint32_t		 fanmin;
	uint32_t		 fanmax;
	uint32_t		 fancurrent;
};

struct acpitz_softc {
	struct acpi_devnode	*sc_node;
	struct sysmon_envsys	*sc_sme;
	struct acpitz_zone	 sc_zone;
	struct callout		 sc_callout;
	envsys_data_t		 sc_temp_sensor;
	envsys_data_t		 sc_fan_sensor;
	int			 sc_active;
	int			 sc_flags;
	int			 sc_zone_expire;
	bool			 sc_first;
	bool			 sc_have_fan;
	struct cpu_info	       **sc_psl;
	size_t			 sc_psl_size;
};

static int		acpitz_match(device_t, cfdata_t, void *);
static void		acpitz_attach(device_t, device_t, void *);
static int		acpitz_detach(device_t, int);
static void		acpitz_get_status(void *);
static void		acpitz_get_zone(void *, int);
static void		acpitz_get_zone_quiet(void *);
static char	       *acpitz_celcius_string(int);
static void		acpitz_power_off(struct acpitz_softc *);
static void		acpitz_power_zone(struct acpitz_softc *, int, int);
static void		acpitz_sane_temp(uint32_t *tmp);
static ACPI_STATUS	acpitz_switch_cooler(ACPI_OBJECT *, void *);
static void		acpitz_notify_handler(ACPI_HANDLE, uint32_t, void *);
static int		acpitz_get_integer(device_t, const char *, uint32_t *);
static void		acpitz_tick(void *);
static void		acpitz_init_envsys(device_t);
static void		acpitz_get_limits(struct sysmon_envsys *,
					  envsys_data_t *,
					  sysmon_envsys_lim_t *, uint32_t *);
static int		acpitz_get_fanspeed(device_t, uint32_t *,
					    uint32_t *, uint32_t *);
#ifdef notyet
static ACPI_STATUS	acpitz_set_fanspeed(device_t, uint32_t);
#endif
static void		acpitz_print_processor_list(device_t);

CFATTACH_DECL_NEW(acpitz, sizeof(struct acpitz_softc),
    acpitz_match, acpitz_attach, acpitz_detach, NULL);

/*
 * acpitz_match: autoconf(9) match routine
 */
static int
acpitz_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_THERMAL)
		return 0;

	return 1;
}

/*
 * acpitz_attach: autoconf(9) attach routine
 */
static void
acpitz_attach(device_t parent, device_t self, void *aux)
{
	struct acpitz_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	sc->sc_first = true;
	sc->sc_have_fan = false;
	sc->sc_node = aa->aa_node;
	sc->sc_zone.tzp = ATZ_TZP_RATE;

	aprint_naive("\n");
	acpitz_print_processor_list(self);
	aprint_normal("\n");

	/*
	 * The _TZP (ACPI 4.0, p. 430) defines the recommended
	 * polling interval (in tenths of seconds). A value zero
	 * means that polling "should not be necessary".
	 */
	rv = acpi_eval_integer(sc->sc_node->ad_handle, "_TZP", &val);

	if (ACPI_SUCCESS(rv) && val != 0)
		sc->sc_zone.tzp = val;

	aprint_debug_dev(self, "polling interval %d.%d seconds\n",
	    sc->sc_zone.tzp / 10, sc->sc_zone.tzp % 10);

	sc->sc_zone_expire = ATZ_ZONE_EXPIRE / sc->sc_zone.tzp;

	/*
	 * XXX: The fan controls seen here are available on
	 *	some HP laptops. Arguably these should not
	 *	appear in a generic device driver like this.
	 */
	if (acpitz_get_fanspeed(self, &sc->sc_zone.fanmin,
		&sc->sc_zone.fanmax, &sc->sc_zone.fancurrent) == 0)
		sc->sc_have_fan = true;

	acpitz_get_zone(self, 1);
	acpitz_get_status(self);

	(void)pmf_device_register(self, NULL, NULL);
	(void)acpi_power_register(sc->sc_node->ad_handle);
	(void)acpi_register_notify(sc->sc_node, acpitz_notify_handler);

	callout_init(&sc->sc_callout, CALLOUT_MPSAFE);
	callout_setfunc(&sc->sc_callout, acpitz_tick, self);

	acpitz_init_envsys(self);

	callout_schedule(&sc->sc_callout, sc->sc_zone.tzp * hz / 10);
}

static int
acpitz_detach(device_t self, int flags)
{
	struct acpitz_softc *sc = device_private(self);
	ACPI_HANDLE hdl;
	ACPI_BUFFER al;
	ACPI_STATUS rv;
	int i;

	callout_halt(&sc->sc_callout, NULL);
	callout_destroy(&sc->sc_callout);

	pmf_device_deregister(self);
	acpi_deregister_notify(sc->sc_node);

	/*
	 * Although the device itself should not contain any power
	 * resources, we have possibly used the resources of active
	 * cooling devices. To unregister these, first fetch a fresh
	 * active cooling zone, and then detach the resources from
	 * the reference handles contained in the cooling zone.
	 */
	acpitz_get_zone(self, 0);

	for (i = 0; i < ATZ_NLEVELS; i++) {

		if (sc->sc_zone.al[i].Pointer == NULL)
			continue;

		al = sc->sc_zone.al[i];
		rv = acpi_eval_reference_handle(al.Pointer, &hdl);

		if (ACPI_SUCCESS(rv))
			acpi_power_deregister(hdl);

		ACPI_FREE(sc->sc_zone.al[i].Pointer);
	}

	if (sc->sc_psl)
		kmem_free(sc->sc_psl, sc->sc_psl_size);

	if (sc->sc_sme != NULL)
		sysmon_envsys_unregister(sc->sc_sme);

	return 0;
}

static void
acpitz_get_zone_quiet(void *opaque)
{
	acpitz_get_zone(opaque, 0);
}

static void
acpitz_get_status(void *opaque)
{
	device_t dv = opaque;
	struct acpitz_softc *sc = device_private(dv);
	uint32_t tmp, fmin, fmax, fcurrent;
	int active, changed, flags, i;

	sc->sc_zone_expire--;

	if (sc->sc_zone_expire <= 0) {
		sc->sc_zone_expire = ATZ_ZONE_EXPIRE / sc->sc_zone.tzp;

		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			"%s: zone refetch forced\n", device_xname(dv)));

		acpitz_get_zone(dv, 0);
	}

	if (acpitz_get_integer(dv, "_TMP", &tmp) != 0)
		return;

	sc->sc_zone.prevtmp = sc->sc_zone.tmp;
	sc->sc_zone.tmp = tmp;

	if (sc->sc_first != false)
		sc->sc_zone.prevtmp = tmp; /* XXX: Sanity check? */

	if (acpitz_get_fanspeed(dv, &fmin, &fmax, &fcurrent) == 0) {

		if (fcurrent != ATZ_TMP_INVALID)
			sc->sc_zone.fancurrent = fcurrent;
	}

	sc->sc_temp_sensor.state = ENVSYS_SVALID;
	sc->sc_temp_sensor.value_cur = ATZ2UKELVIN(sc->sc_zone.tmp);

	sc->sc_fan_sensor.state = ENVSYS_SVALID;
	sc->sc_fan_sensor.value_cur = sc->sc_zone.fancurrent;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "%s: zone temperature is %s C\n",
		device_xname(dv), acpitz_celcius_string(sc->sc_zone.tmp)));

	/*
	 * XXX: Passive cooling is not yet supported.
	 */
	if ((sc->sc_flags & ATZ_F_PASSIVEONLY) != 0)
		return;

	/*
	 * As noted in ACPI 4.0 (p. 420), the temperature
	 * thresholds are conveyed in the optional _ACx
	 * object (x = 0 ... 9). The smaller the x, the
	 * greater the cooling level. We prefer to keep
	 * the highest cooling mode when in "active".
	 */
	active = ATZ_ACTIVE_NONE;

	for (i = ATZ_NLEVELS - 1; i >= 0; i--) {

		if (sc->sc_zone.ac[i] == ATZ_TMP_INVALID)
			continue;

		if (sc->sc_zone.ac[i] <= tmp)
			active = i;
	}

	flags = sc->sc_flags & ~(ATZ_F_CRITICAL | ATZ_F_HOT | ATZ_F_PASSIVE);

	if (sc->sc_zone.psv != ATZ_TMP_INVALID && tmp >= sc->sc_zone.psv)
		flags |= ATZ_F_PASSIVE;

	if (sc->sc_zone.hot != ATZ_TMP_INVALID && tmp >= sc->sc_zone.hot)
		flags |= ATZ_F_HOT;

	if (sc->sc_zone.crt != ATZ_TMP_INVALID && tmp >= sc->sc_zone.crt)
		flags |= ATZ_F_CRITICAL;

	if (flags != sc->sc_flags) {

		changed = (sc->sc_flags ^ flags) & flags;
		sc->sc_flags = flags;

		if ((changed & ATZ_F_CRITICAL) != 0) {
			sc->sc_temp_sensor.state = ENVSYS_SCRITOVER;

			aprint_debug_dev(dv, "zone went critical, %s C\n",
			    acpitz_celcius_string(tmp));

		} else if ((changed & ATZ_F_HOT) != 0) {
			sc->sc_temp_sensor.state = ENVSYS_SCRITOVER;

			aprint_debug_dev(dv, "zone went hot, %s C\n",
			    acpitz_celcius_string(tmp));
		}
	}

	/* Power on the fans. */
	if (sc->sc_active != active) {

		if (sc->sc_active != ATZ_ACTIVE_NONE)
			acpitz_power_zone(sc, sc->sc_active, 0);

		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "%s: active cooling "
			"level %d\n", device_xname(dv), active));

		if (active != ATZ_ACTIVE_NONE)
			acpitz_power_zone(sc, active, 1);

		sc->sc_active = active;
	}
}

static char *
acpitz_celcius_string(int dk)
{
	static char buf[10];
	int dc;

	dc = abs(dk - ATZ_ZEROC);

	(void)snprintf(buf, sizeof(buf), "%s%d.%d",
	    (dk >= ATZ_ZEROC) ? "" : "-", dc / 10, dc % 10);

	return buf;
}

static ACPI_STATUS
acpitz_switch_cooler(ACPI_OBJECT *obj, void *arg)
{
	int flag, pwr_state;
	ACPI_HANDLE cooler;
	ACPI_STATUS rv;

	/*
	 * The _ALx object is a package in which the elements
	 * are reference handles to an active cooling device
	 * (typically PNP0C0B, ACPI fan device). Try to turn
	 * on (or off) the power resources behind these handles
	 * to start (or terminate) the active cooling.
	 */
	flag = *(int *)arg;
	pwr_state = (flag != 0) ? ACPI_STATE_D0 : ACPI_STATE_D3;

	rv = acpi_eval_reference_handle(obj, &cooler);

	if (ACPI_FAILURE(rv))
		return rv;

	(void)acpi_power_set(cooler, pwr_state);

	return AE_OK;
}

/*
 * acpitz_power_zone:
 *
 *	Power on or off the i:th part of the zone zone.
 */
static void
acpitz_power_zone(struct acpitz_softc *sc, int i, int on)
{

	KASSERT(i >= 0 && i < ATZ_NLEVELS);

	(void)acpi_foreach_package_object(sc->sc_zone.al[i].Pointer,
	    acpitz_switch_cooler, &on);
}


/*
 * acpitz_power_off:
 *
 *	Power off parts of the zone.
 */
static void
acpitz_power_off(struct acpitz_softc *sc)
{
	int i;

	for (i = 0 ; i < ATZ_NLEVELS; i++) {

		if (sc->sc_zone.al[i].Pointer == NULL)
			continue;

		acpitz_power_zone(sc, i, 0);
	}

	sc->sc_active = ATZ_ACTIVE_NONE;
	sc->sc_flags &= ~(ATZ_F_CRITICAL | ATZ_F_HOT | ATZ_F_PASSIVE);
}

static void
acpitz_get_zone(void *opaque, int verbose)
{
	device_t dv = opaque;
	struct acpitz_softc *sc = device_private(dv);
	int comma, i, valid_levels;
	ACPI_OBJECT *obj;
	ACPI_STATUS rv;
	char buf[5];

	if (sc->sc_first != true) {
		acpitz_power_off(sc);

		for (i = 0; i < ATZ_NLEVELS; i++) {

			if (sc->sc_zone.al[i].Pointer != NULL)
				ACPI_FREE(sc->sc_zone.al[i].Pointer);

			sc->sc_zone.al[i].Pointer = NULL;
		}
	}

	valid_levels = 0;

	for (i = 0; i < ATZ_NLEVELS; i++) {

		(void)snprintf(buf, sizeof(buf), "_AC%d", i);

		if (acpitz_get_integer(dv, buf, &sc->sc_zone.ac[i]))
			continue;

		(void)snprintf(buf, sizeof(buf), "_AL%d", i);

		rv = acpi_eval_struct(sc->sc_node->ad_handle, buf,
		    &sc->sc_zone.al[i]);

		if (ACPI_FAILURE(rv)) {
			sc->sc_zone.al[i].Pointer = NULL;
			continue;
		}

		obj = sc->sc_zone.al[i].Pointer;

		if (obj->Type != ACPI_TYPE_PACKAGE || obj->Package.Count == 0) {
			sc->sc_zone.al[i].Pointer = NULL;
			ACPI_FREE(obj);
			continue;
		}

		if (sc->sc_first != false)
			aprint_normal_dev(dv, "active cooling level %d: %sC\n",
			    i, acpitz_celcius_string(sc->sc_zone.ac[i]));

		valid_levels++;
	}

	/*
	 * A brief summary (ACPI 4.0, section 11.4):
	 *
	 *    _TMP : current temperature (in tenths of degrees)
	 *    _CRT : critical trip-point at which to shutdown
	 *    _HOT : critical trip-point at which to go to S4
	 *    _PSV : passive cooling policy threshold
	 *    _TC1 : thermal constant for passive cooling
	 *    _TC2 : thermal constant for passive cooling
	 */
	(void)acpitz_get_integer(dv, "_TMP", &sc->sc_zone.tmp);
	(void)acpitz_get_integer(dv, "_CRT", &sc->sc_zone.crt);
	(void)acpitz_get_integer(dv, "_HOT", &sc->sc_zone.hot);
	(void)acpitz_get_integer(dv, "_PSV", &sc->sc_zone.psv);
	(void)acpitz_get_integer(dv, "_TC1", &sc->sc_zone.tc1);
	(void)acpitz_get_integer(dv, "_TC2", &sc->sc_zone.tc2);

	/*
	 * If _RTV is not present or present and zero,
	 * values are absolute (see ACPI 4.0, 425).
	 */
	acpitz_get_integer(dv, "_RTV", &sc->sc_zone.rtv);

	if (sc->sc_zone.rtv == ATZ_TMP_INVALID)
		sc->sc_zone.rtv = 0;

	acpitz_sane_temp(&sc->sc_zone.tmp);
	acpitz_sane_temp(&sc->sc_zone.crt);
	acpitz_sane_temp(&sc->sc_zone.hot);
	acpitz_sane_temp(&sc->sc_zone.psv);

	if (verbose != 0) {
		comma = 0;

		aprint_verbose_dev(dv, "levels: ");

		if (sc->sc_zone.crt != ATZ_TMP_INVALID) {
			aprint_verbose("critical %s C",
			    acpitz_celcius_string(sc->sc_zone.crt));
			comma = 1;
		}

		if (sc->sc_zone.hot != ATZ_TMP_INVALID) {
			aprint_verbose("%shot %s C", comma ? ", " : "",
			    acpitz_celcius_string(sc->sc_zone.hot));
			comma = 1;
		}

		if (sc->sc_zone.psv != ATZ_TMP_INVALID) {
			aprint_verbose("%spassive %s C", comma ? ", " : "",
			    acpitz_celcius_string(sc->sc_zone.psv));
			comma = 1;
		}

		if (valid_levels == 0) {
			sc->sc_flags |= ATZ_F_PASSIVEONLY;

			if (sc->sc_first != false)
				aprint_verbose("%spassive cooling", comma ?
				    ", " : "");
		}

		aprint_verbose("\n");
	}

	for (i = 0; i < ATZ_NLEVELS; i++)
		acpitz_sane_temp(&sc->sc_zone.ac[i]);

	acpitz_power_off(sc);
	sc->sc_first = false;
}

static void
acpitz_notify_handler(ACPI_HANDLE hdl, uint32_t notify, void *opaque)
{
	ACPI_OSD_EXEC_CALLBACK func = NULL;
	device_t dv = opaque;

	switch (notify) {

	case ACPI_NOTIFY_TZ_ZONE:
		func = acpitz_get_status;
		break;

	case ACPI_NOTIFY_TZ_TRIP:
	case ACPI_NOTIFY_TZ_DEVLIST:
		func = acpitz_get_zone_quiet;
		break;

	default:
		aprint_debug_dev(dv, "unknown notify 0x%02X\n", notify);
		return;
	}

	(void)AcpiOsExecute(OSL_NOTIFY_HANDLER, func, dv);
}

static void
acpitz_sane_temp(uint32_t *tmp)
{
	/* Sane temperatures are beteen 0 and 150 C. */
	if (*tmp < ATZ_ZEROC || *tmp > ATZ_ZEROC + 1500)
		*tmp = ATZ_TMP_INVALID;
}

static int
acpitz_get_integer(device_t dv, const char *cm, uint32_t *val)
{
	struct acpitz_softc *sc = device_private(dv);
	ACPI_INTEGER tmp;
	ACPI_STATUS rv;

	rv = acpi_eval_integer(sc->sc_node->ad_handle, cm, &tmp);

	if (ACPI_FAILURE(rv)) {
		*val = ATZ_TMP_INVALID;

		ACPI_DEBUG_PRINT((ACPI_DB_DEBUG_OBJECT,
			"%s: failed to evaluate %s: %s\n",
			device_xname(dv), cm, AcpiFormatException(rv)));

		return 1;
	}

	*val = tmp;

	return 0;
}

static int
acpitz_get_fanspeed(device_t dv,
    uint32_t *fanmin, uint32_t *fanmax, uint32_t *fancurrent)
{
	struct acpitz_softc *sc = device_private(dv);
	ACPI_INTEGER fmin, fmax, fcurr;
	ACPI_HANDLE handle;
	ACPI_STATUS rv;
	int rc = 0;

	handle = sc->sc_node->ad_handle;

	rv = acpi_eval_integer(handle, "FMIN", &fmin);

	if (ACPI_FAILURE(rv)) {
		fmin = ATZ_TMP_INVALID;
		rc = 1;
	}

	rv = acpi_eval_integer(handle, "FMAX", &fmax);

	if (ACPI_FAILURE(rv)) {
		fmax = ATZ_TMP_INVALID;
		rc = 1;
	}
	rv = acpi_eval_integer(handle, "FRSP", &fcurr);

	if (ACPI_FAILURE(rv)) {
		fcurr = ATZ_TMP_INVALID;
		rc = 1;
	}

	if (fanmin != NULL)
		*fanmin = fmin;

	if (fanmax != NULL)
		*fanmax = fmax;

	if (fancurrent != NULL)
		*fancurrent = fcurr;

	return rc;
}

#ifdef notyet
static ACPI_STATUS
acpitz_set_fanspeed(device_t dv, uint32_t fanspeed)
{
	struct acpitz_softc *sc = device_private(dv);
	ACPI_HANDLE handle;
	ACPI_STATUS rv;

	handle = sc->sc_node->ad_handle;

	rv = acpi_eval_set_integer(handle, "FSSP", fanspeed);

	if (ACPI_FAILURE(rv))
		aprint_debug_dev(dv, "failed to set fan speed to %u RPM: %s\n",
		    fanspeed, AcpiFormatException(rv));

	return rv;
}
#endif

static void
acpitz_print_processor_list(device_t dv)
{
	struct acpitz_softc *sc = device_private(dv);
	ACPI_HANDLE handle = sc->sc_node->ad_handle;
	ACPI_OBJECT *obj, *pref;
	ACPI_HANDLE prhandle;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	struct cpu_info *ci;
	unsigned int i, cnt;

	rv = acpi_eval_struct(handle, "_PSL", &buf);

	if (ACPI_FAILURE(rv) || buf.Pointer == NULL)
		return;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE || obj->Package.Count == 0)
		goto done;

	sc->sc_psl_size = sizeof(ci) * (obj->Package.Count + 1);
	sc->sc_psl = kmem_zalloc(sc->sc_psl_size, KM_SLEEP);
	for (cnt = i = 0; i < obj->Package.Count; i++) {

		pref = &obj->Package.Elements[i];
		rv = acpi_eval_reference_handle(pref, &prhandle);

		if (ACPI_FAILURE(rv))
			continue;

		ci = acpi_match_cpu_handle(prhandle);

		if (ci == NULL)
			continue;

		if (cnt == 0)
			aprint_normal(":");

		aprint_normal(" %s", device_xname(ci->ci_dev));

		if (sc->sc_psl)
			sc->sc_psl[cnt] = ci;
		++cnt;
	}

done:
	ACPI_FREE(buf.Pointer);
}

static void
acpitz_tick(void *opaque)
{
	device_t dv = opaque;
	struct acpitz_softc *sc = device_private(dv);

	(void)AcpiOsExecute(OSL_NOTIFY_HANDLER, acpitz_get_status, dv);

	callout_schedule(&sc->sc_callout, sc->sc_zone.tzp * hz / 10);
}

static void
acpitz_init_envsys(device_t dv)
{
	const int flags = ENVSYS_FMONLIMITS | ENVSYS_FMONNOTSUPP |
			  ENVSYS_FHAS_ENTROPY;
	struct acpitz_softc *sc = device_private(dv);
	unsigned int i;

	sc->sc_sme = sysmon_envsys_create();

	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_name = device_xname(dv);
	sc->sc_sme->sme_flags = SME_DISABLE_REFRESH;
	sc->sc_sme->sme_get_limits = acpitz_get_limits;

	sc->sc_temp_sensor.flags = flags;
	sc->sc_temp_sensor.units = ENVSYS_STEMP;
	sc->sc_temp_sensor.state = ENVSYS_SINVALID;

	memset(sc->sc_temp_sensor.desc, 0, sizeof(sc->sc_temp_sensor.desc));
	if (sc->sc_psl) {
		for (i = 0; sc->sc_psl[i] != NULL; i++) {
			if (i > 0)
				strlcat(sc->sc_temp_sensor.desc, "/",
				    sizeof(sc->sc_temp_sensor.desc));
			strlcat(sc->sc_temp_sensor.desc,
			    device_xname(sc->sc_psl[i]->ci_dev),
			    sizeof(sc->sc_temp_sensor.desc));
		}
		strlcat(sc->sc_temp_sensor.desc, " ",
		    sizeof(sc->sc_temp_sensor.desc));
	}
	strlcat(sc->sc_temp_sensor.desc, "temperature",
	    sizeof(sc->sc_temp_sensor.desc));

	if (sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_temp_sensor))
		goto out;

	if (sc->sc_have_fan != false) {

		sc->sc_fan_sensor.flags = flags;
		sc->sc_fan_sensor.units = ENVSYS_SFANRPM;
		sc->sc_fan_sensor.state = ENVSYS_SINVALID;

		(void)strlcpy(sc->sc_fan_sensor.desc,
		    "FAN", sizeof(sc->sc_fan_sensor.desc));

		/* Ignore error because fan sensor is optional. */
		(void)sysmon_envsys_sensor_attach(sc->sc_sme,
		    &sc->sc_fan_sensor);
	}

	if (sysmon_envsys_register(sc->sc_sme) == 0)
		return;

out:
	aprint_error_dev(dv, "unable to register with sysmon\n");

	sysmon_envsys_destroy(sc->sc_sme);
	sc->sc_sme = NULL;
}

static void
acpitz_get_limits(struct sysmon_envsys *sme, envsys_data_t *edata,
		  sysmon_envsys_lim_t *limits, uint32_t *props)
{
	struct acpitz_softc *sc = sme->sme_cookie;

	switch (edata->units) {
	case ENVSYS_STEMP:
		*props = 0;
		if (sc->sc_zone.hot != ATZ_TMP_INVALID) {
			*props |= PROP_CRITMAX;
			limits->sel_critmax = ATZ2UKELVIN(sc->sc_zone.hot);
		} else if (sc->sc_zone.crt != ATZ_TMP_INVALID) {
			*props |= PROP_CRITMAX;
			limits->sel_critmax = ATZ2UKELVIN(sc->sc_zone.crt);
		}
		break;

	case ENVSYS_SFANRPM:
		*props = 0;
		if (sc->sc_zone.fanmin != ATZ_TMP_INVALID) {
			*props |= PROP_WARNMIN;
			limits->sel_warnmin = sc->sc_zone.fanmin;
		}
		if (sc->sc_zone.fanmax != ATZ_TMP_INVALID) {
			*props |= PROP_WARNMAX;
			limits->sel_warnmax = sc->sc_zone.fanmax;
		}
		break;
	}
}

MODULE(MODULE_CLASS_DRIVER, acpitz, "sysmon_envsys");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
acpitz_modcmd(modcmd_t cmd, void *aux)
{
	int rv = 0;

	switch (cmd) {

	case MODULE_CMD_INIT:

#ifdef _MODULE
		rv = config_init_component(cfdriver_ioconf_acpitz,
		    cfattach_ioconf_acpitz, cfdata_ioconf_acpitz);
#endif
		break;

	case MODULE_CMD_FINI:

#ifdef _MODULE
		rv = config_fini_component(cfdriver_ioconf_acpitz,
		    cfattach_ioconf_acpitz, cfdata_ioconf_acpitz);
#endif
		break;

	default:
		rv = ENOTTY;
	}

	return rv;
}
