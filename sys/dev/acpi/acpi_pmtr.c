/*	$NetBSD: acpi_pmtr.c,v 1.8 2015/04/23 23:23:00 pgoyette Exp $ */

/*-
 * Copyright (c) 2011 Jukka Ruohonen <jruohonen@iki.fi>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_pmtr.c,v 1.8 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <dev/sysmon/sysmonvar.h>

#define _COMPONENT			ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME			("acpi_pmtr")

#define ACPIPMTR_CAP_FLAGS		0
#define ACPIPMTR_CAP_UNIT		1
#define ACPIPMTR_CAP_TYPE		2
#define ACPIPMTR_CAP_ACCURACY		3
#define ACPIPMTR_CAP_SAMPLING		4
#define ACPIPMTR_CAP_IVAL_MIN		5
#define ACPIPMTR_CAP_IVAL_MAX		6
#define ACPIPMTR_CAP_HYSTERESIS		7
#define ACPIPMTR_CAP_HWLIMIT		8
#define ACPIPMTR_CAP_HWLIMIT_MIN	9
#define ACPIPMTR_CAP_HWLIMIT_MAX	10
#define ACPIPMTR_CAP_COUNT		11
/*      ACPIPMTR_CAP_MODEL		11 */
/*      ACPIPMTR_CAP_SERIAL		12 */
/*      ACPIPMTR_CAP_OEM		13 */

#define ACPIPMTR_FLAGS_MEASURE		__BIT(0)
#define ACPIPMTR_FLAGS_TRIP		__BIT(1)
#define ACPIPMTR_FLAGS_HWLIMIT		__BIT(2)
#define ACPIPMTR_FLAGS_NOTIFY		__BIT(3)
#define ACPIPMTR_FLAGS_DISCHARGE	__BIT(8)

#define ACPIPMTR_POWER_INPUT		0x00
#define ACPIPMTR_POWER_OUTPUT		0x01

#define ACPIPMTR_NOTIFY_CAP		0x80
#define ACPIPMTR_NOTIFY_TRIP		0x81
#define ACPIPMTR_NOTIFY_HWLIMIT1	0x82
#define ACPIPMTR_NOTIFY_HWLIMIT2	0x83
#define ACPIPMTR_NOTIFY_INTERVAL	0x84

struct acpipmtr_softc {
	device_t		 sc_dev;
	struct acpi_devnode	*sc_node;
	struct sysmon_envsys	*sc_sme;
	envsys_data_t		 sc_sensor_i;
	envsys_data_t		 sc_sensor_o;
	uint32_t		 sc_cap[ACPIPMTR_CAP_COUNT];
	int32_t			 sc_interval;
	kmutex_t		 sc_mtx;
};

const char * const acpi_pmtr_ids[] = {
	"ACPI000D",
	NULL
};

static int	acpipmtr_match(device_t, cfdata_t, void *);
static void	acpipmtr_attach(device_t, device_t, void *);
static int	acpipmtr_detach(device_t, int);
static bool	acpipmtr_cap_get(device_t, bool);
static bool	acpipmtr_dev_print(device_t);
static bool	acpipmtr_sensor_init(device_t);
static void	acpipmtr_sensor_type(device_t);
static int32_t	acpipmtr_sensor_get(device_t, const char *);
static int32_t	acpipmtr_sensor_get_reading(device_t);
static int32_t	acpipmtr_sensor_get_interval(device_t);
static void	acpipmtr_sensor_refresh(struct sysmon_envsys*,envsys_data_t *);
static void	acpipmtr_notify(ACPI_HANDLE, uint32_t, void *);

CFATTACH_DECL_NEW(acpipmtr, sizeof(struct acpipmtr_softc),
    acpipmtr_match, acpipmtr_attach, acpipmtr_detach, NULL);

static int
acpipmtr_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, acpi_pmtr_ids);
}

static void
acpipmtr_attach(device_t parent, device_t self, void *aux)
{
	struct acpipmtr_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	uint32_t acc;

	sc->sc_sme = NULL;
	sc->sc_dev = self;
	sc->sc_node = aa->aa_node;

	aprint_naive("\n");
	aprint_normal(": ACPI Power Meter\n");

	(void)pmf_device_register(self, NULL, NULL);
	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_NONE);

	if (acpipmtr_cap_get(self, true) != true)
		return;

	if (acpipmtr_sensor_init(self) != true)
		return;

	(void)acpipmtr_dev_print(self);
	(void)acpi_register_notify(sc->sc_node, acpipmtr_notify);

	if ((acc = sc->sc_cap[ACPIPMTR_CAP_ACCURACY]) == 0)
		acc = 100000;

	aprint_verbose_dev(self,
	    "measuring %s power at %u.%u %% accuracy, %u ms sampling\n",
	    (sc->sc_cap[ACPIPMTR_CAP_TYPE] != 0) ? "output" : "input",
	    acc / 1000, acc % 1000, sc->sc_cap[ACPIPMTR_CAP_SAMPLING]);

	aprint_debug_dev(self, "%s hw-limits, capabilities 0x%02x\n",
	    (sc->sc_cap[ACPIPMTR_CAP_HWLIMIT] != 0) ? "rw" : "ro",
	     sc->sc_cap[ACPIPMTR_CAP_FLAGS]);
}

static int
acpipmtr_detach(device_t self, int flags)
{
	struct acpipmtr_softc *sc = device_private(self);

	pmf_device_deregister(self);
	acpi_deregister_notify(sc->sc_node);

	if (sc->sc_sme != NULL)
		sysmon_envsys_unregister(sc->sc_sme);

	mutex_destroy(&sc->sc_mtx);

	return 0;
}

static bool
acpipmtr_cap_get(device_t self, bool print)
{
	struct acpipmtr_softc *sc = device_private(self);
	ACPI_OBJECT *elm, *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	uint32_t i;

	for (i = 0; i < __arraycount(sc->sc_cap); i++)
		sc->sc_cap[i] = 0;

	rv = acpi_eval_struct(sc->sc_node->ad_handle, "_PMC", &buf);

	if (ACPI_FAILURE(rv))
		goto out;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE) {
		rv = AE_TYPE;
		goto out;
	}

	elm = obj->Package.Elements;

	if (obj->Package.Count != 14) {
		rv = AE_LIMIT;
		goto out;
	}

	CTASSERT(__arraycount(sc->sc_cap) == 11);

	for (i = 0; i < __arraycount(sc->sc_cap); i++) {

		if (elm[i].Type != ACPI_TYPE_INTEGER) {
			rv = AE_TYPE;
			goto out;
		}

		if (elm[i].Integer.Value > UINT32_MAX) {
			rv = AE_AML_NUMERIC_OVERFLOW;
			goto out;
		}

		sc->sc_cap[i] = elm[i].Integer.Value;
	}

	if (print != true)
		goto out;

	for (; i < 14; i++) {

		if (elm[i].Type != ACPI_TYPE_STRING)
			goto out;

		if (elm[i].String.Pointer == NULL)
			goto out;

		if (elm[i].String.Pointer[0] == '\0')
			goto out;
	}

	aprint_debug_dev(self, "%s, serial %s, "
	    "model %s\n", elm[13].String.Pointer,
	    elm[12].String.Pointer, elm[11].String.Pointer);

out:
	if (ACPI_FAILURE(rv))
		aprint_error_dev(self, "failed to evaluate _PMC: %s\n",
		    AcpiFormatException(rv));

	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	return (rv != AE_OK) ? false : true;
}

static bool
acpipmtr_dev_print(device_t self)
{
	struct acpipmtr_softc *sc = device_private(self);
	struct acpi_devnode *ad;
	ACPI_OBJECT *elm, *obj;
	ACPI_BUFFER buf;
	ACPI_HANDLE hdl;
	ACPI_STATUS rv;
	uint32_t i, n;

	/*
	 * The _PMD method returns a package of devices whose total power
	 * drawn should roughly correspond with the readings from the meter.
	 */
	rv = acpi_eval_struct(sc->sc_node->ad_handle, "_PMD", &buf);

	if (ACPI_FAILURE(rv))
		goto out;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE) {
		rv = AE_TYPE;
		goto out;
	}

	n = obj->Package.Count;

	if (n == 0) {
		rv = AE_LIMIT;
		goto out;
	}

	aprint_debug_dev(self, "measured devices: ");

	for (i = 0; i < n; i++) {

		elm = &obj->Package.Elements[i];
		rv = acpi_eval_reference_handle(elm, &hdl);

		if (ACPI_FAILURE(rv))
			continue;

		ad = acpi_match_node(hdl);

		if (ad == NULL)
			continue;

		aprint_debug("%s ", ad->ad_name);
	}

	aprint_debug("\n");

out:
	if (ACPI_FAILURE(rv))
		aprint_debug_dev(self, "failed to evaluate _PMD: %s\n",
		    AcpiFormatException(rv));

	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	return (rv != AE_OK) ? false : true;
}

static bool
acpipmtr_sensor_init(device_t self)
{
	struct acpipmtr_softc *sc = device_private(self);
	const size_t siz = sizeof(sc->sc_sensor_i.desc);
	int32_t val;

	val = acpipmtr_sensor_get_reading(self);
	sc->sc_interval = acpipmtr_sensor_get_interval(self);

	if (val < 0) {
		aprint_error_dev(self, "failed to get sensor reading\n");
		return false;
	}

	/* Always mW in ACPI 4.0. */
	if (sc->sc_cap[ACPIPMTR_CAP_UNIT] != 0)
		aprint_error_dev(self, "invalid measurement unit\n");

	sc->sc_sme = sysmon_envsys_create();

	sc->sc_sensor_i.units = ENVSYS_SWATTS;
	sc->sc_sensor_o.units = ENVSYS_SWATTS;
	sc->sc_sensor_i.value_cur = val * 1000;
	sc->sc_sensor_o.value_cur = val * 1000;

	acpipmtr_sensor_type(self);

	(void)strlcpy(sc->sc_sensor_i.desc, "input power", siz);
	(void)strlcpy(sc->sc_sensor_o.desc, "output power", siz);

	sc->sc_sme->sme_cookie = self;
	sc->sc_sme->sme_flags = SME_POLL_ONLY;
	sc->sc_sme->sme_name = device_xname(self);
	sc->sc_sme->sme_refresh = acpipmtr_sensor_refresh;

	if (sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensor_i) != 0)
		goto fail;

	if (sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensor_o) != 0)
		goto fail;

	if (sysmon_envsys_register(sc->sc_sme) != 0)
		goto fail;

	return true;

fail:
	aprint_error_dev(self, "failed to initialize sysmon\n");

	sysmon_envsys_destroy(sc->sc_sme);
	sc->sc_sme = NULL;

	return false;
}

static void
acpipmtr_sensor_type(device_t self)
{
	struct acpipmtr_softc *sc = device_private(self);

	mutex_enter(&sc->sc_mtx);

	switch (sc->sc_cap[ACPIPMTR_CAP_TYPE]) {

	case ACPIPMTR_POWER_INPUT:
		sc->sc_sensor_i.state = ENVSYS_SVALID;
		sc->sc_sensor_o.state = ENVSYS_SINVALID;
		break;

	case ACPIPMTR_POWER_OUTPUT:
		sc->sc_sensor_i.state = ENVSYS_SINVALID;
		sc->sc_sensor_o.state = ENVSYS_SVALID;
		break;

	default:
		sc->sc_sensor_i.state = ENVSYS_SINVALID;
		sc->sc_sensor_o.state = ENVSYS_SINVALID;
		break;
	}

	mutex_exit(&sc->sc_mtx);
}

static int32_t
acpipmtr_sensor_get(device_t self, const char *path)
{
	struct acpipmtr_softc *sc = device_private(self);
	ACPI_INTEGER val = 0;
	ACPI_STATUS rv;

	rv = acpi_eval_integer(sc->sc_node->ad_handle, path, &val);

	if (ACPI_FAILURE(rv))
		goto fail;

	if (val == 0 || val > INT32_MAX) {
		rv = AE_LIMIT;
		goto fail;
	}

	return val;

fail:
	aprint_debug_dev(self, "failed to evaluate "
	    "%s: %s\n", path, AcpiFormatException(rv));

	return -1;
}

static int32_t
acpipmtr_sensor_get_reading(device_t self)
{
	return acpipmtr_sensor_get(self, "_PMM");
}

static int32_t
acpipmtr_sensor_get_interval(device_t self)
{
	return acpipmtr_sensor_get(self, "_GAI");
}

static void
acpipmtr_sensor_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	device_t self = sme->sme_cookie;
	struct acpipmtr_softc *sc;
	int32_t val;

	sc = device_private(self);

	sc->sc_sensor_i.state = ENVSYS_SINVALID;
	sc->sc_sensor_o.state = ENVSYS_SINVALID;

	val = acpipmtr_sensor_get_reading(self) * 1000;

	if (val < 0)
		return;

	sc->sc_sensor_i.value_cur = val;
	sc->sc_sensor_o.value_cur = val;

	acpipmtr_sensor_type(self);
}

static void
acpipmtr_notify(ACPI_HANDLE hdl, uint32_t evt, void *aux)
{
	struct acpipmtr_softc *sc;
	device_t self = aux;
	int32_t val;

	sc = device_private(self);

	switch (evt) {

	case ACPIPMTR_NOTIFY_CAP:

		mutex_enter(&sc->sc_mtx);

		if (acpipmtr_cap_get(self, false) != true) {
			mutex_exit(&sc->sc_mtx);
			break;
		}

		mutex_exit(&sc->sc_mtx);

		acpipmtr_sensor_type(self);
		break;

	case ACPIPMTR_NOTIFY_INTERVAL:
		val = acpipmtr_sensor_get_interval(self);

		if (val < 0 || val == sc->sc_interval)
			break;

		aprint_debug_dev(self, "averaging interval changed "
		    "from %u ms to %u ms\n", sc->sc_interval, val);

		sc->sc_interval = val;
		break;

	case ACPIPMTR_NOTIFY_TRIP:	/* AE_SUPPORT */
	case ACPIPMTR_NOTIFY_HWLIMIT1:	/* AE_SUPPORT */
	case ACPIPMTR_NOTIFY_HWLIMIT2:	/* AE_SUPPORT */
		break;

	default:
		aprint_debug_dev(self, "unknown notify 0x%02x\n", evt);
	}
}

MODULE(MODULE_CLASS_DRIVER, acpipmtr, "sysmon_envsys");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
acpipmtr_modcmd(modcmd_t cmd, void *aux)
{
	int rv = 0;

	switch (cmd) {

	case MODULE_CMD_INIT:

#ifdef _MODULE
		rv = config_init_component(cfdriver_ioconf_acpipmtr,
		    cfattach_ioconf_acpipmtr, cfdata_ioconf_acpipmtr);
#endif
		break;

	case MODULE_CMD_FINI:

#ifdef _MODULE
		rv = config_fini_component(cfdriver_ioconf_acpipmtr,
		    cfattach_ioconf_acpipmtr, cfdata_ioconf_acpipmtr);
#endif
		break;

	default:
		rv = ENOTTY;
	}

	return rv;
}
