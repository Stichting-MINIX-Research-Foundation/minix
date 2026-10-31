/*	$NetBSD: acpi_fan.c,v 1.8 2015/04/23 23:23:00 pgoyette Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: acpi_fan.c,v 1.8 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/module.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_power.h>

#include <dev/sysmon/sysmonvar.h>

#define _COMPONENT		ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME		("acpi_fan")

struct acpifan_softc {
	device_t		 sc_dev;
	struct acpi_devnode	*sc_node;
	struct sysmon_envsys	*sc_sme;
	envsys_data_t		 sc_sensor;
};

const char * const acpi_fan_ids[] = {
	"PNP0C0B",
	NULL
};

static int	acpifan_match(device_t, cfdata_t, void *);
static void	acpifan_attach(device_t, device_t, void *);
static int	acpifan_detach(device_t, int);
static bool	acpifan_suspend(device_t, const pmf_qual_t *);
static bool	acpifan_resume(device_t, const pmf_qual_t *);
static bool	acpifan_shutdown(device_t, int);
static bool	acpifan_sensor_init(device_t);
static void	acpifan_sensor_state(void *);
static void	acpifan_sensor_refresh(struct sysmon_envsys *,envsys_data_t *);

CFATTACH_DECL_NEW(acpifan, sizeof(struct acpifan_softc),
    acpifan_match, acpifan_attach, acpifan_detach, NULL);

static int
acpifan_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, acpi_fan_ids);
}

static void
acpifan_attach(device_t parent, device_t self, void *aux)
{
	struct acpifan_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	ACPI_HANDLE tmp;
	ACPI_STATUS rv;

	sc->sc_sme = NULL;
	sc->sc_dev = self;
	sc->sc_node = aa->aa_node;

	aprint_naive("\n");
	aprint_normal(": ACPI Fan\n");

	if (acpifan_sensor_init(self) != true)
		aprint_error_dev(self, "failed to initialize\n");

	(void)acpi_power_register(sc->sc_node->ad_handle);
	(void)pmf_device_register1(self, acpifan_suspend,
	    acpifan_resume, acpifan_shutdown);

	rv = AcpiGetHandle(sc->sc_node->ad_handle, "_FIF", &tmp);

	if (ACPI_SUCCESS(rv))
		aprint_verbose_dev(self, "ACPI 4.0 functionality present\n");
}

static int
acpifan_detach(device_t self, int flags)
{
	struct acpifan_softc *sc = device_private(self);

	(void)acpi_power_set(sc->sc_node->ad_handle, ACPI_STATE_D0);

	pmf_device_deregister(self);
	acpi_power_deregister(sc->sc_node->ad_handle);

	if (sc->sc_sme != NULL)
		sysmon_envsys_unregister(sc->sc_sme);

	return 0;
}

static bool
acpifan_suspend(device_t self, const pmf_qual_t *qual)
{
	struct acpifan_softc *sc = device_private(self);

	(void)acpi_power_set(sc->sc_node->ad_handle, ACPI_STATE_D0);

	return true;
}

static bool
acpifan_resume(device_t self, const pmf_qual_t *qual)
{
	struct acpifan_softc *sc = device_private(self);

	(void)acpi_power_set(sc->sc_node->ad_handle, ACPI_STATE_D3);

	return true;
}

static bool
acpifan_shutdown(device_t self, int how)
{
	struct acpifan_softc *sc = device_private(self);

	(void)acpi_power_set(sc->sc_node->ad_handle, ACPI_STATE_D0);

	return true;
}

static bool
acpifan_sensor_init(device_t self)
{
	struct acpifan_softc *sc = device_private(self);
	int state;

	if (acpi_power_get(sc->sc_node->ad_handle, &state) != true)
		return false;

	sc->sc_sme = sysmon_envsys_create();

	acpifan_sensor_state(self);
	sc->sc_sensor.units = ENVSYS_INDICATOR;

	(void)strlcpy(sc->sc_sensor.desc, "state", sizeof(sc->sc_sensor.desc));

	sc->sc_sme->sme_cookie = self;
	sc->sc_sme->sme_flags = SME_POLL_ONLY;
	sc->sc_sme->sme_name = device_xname(self);
	sc->sc_sme->sme_refresh = acpifan_sensor_refresh;

	if (sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensor) != 0)
		goto fail;

	if (sysmon_envsys_register(sc->sc_sme) != 0)
		goto fail;

	return true;

fail:
	sysmon_envsys_destroy(sc->sc_sme);
	sc->sc_sme = NULL;

	return false;
}

static void
acpifan_sensor_state(void *arg)
{
	struct acpifan_softc *sc;
	device_t self;
	int state;

	self = arg;
	sc = device_private(self);
	state = ACPI_STATE_ERROR;

	(void)acpi_power_get(sc->sc_node->ad_handle, &state);

	switch (state) {

	case ACPI_STATE_D0:
	case ACPI_STATE_D1:
	case ACPI_STATE_D2:
		sc->sc_sensor.value_cur = 1;
		sc->sc_sensor.state = ENVSYS_SVALID;
		break;

	case ACPI_STATE_D3:
		sc->sc_sensor.value_cur = 0;
		sc->sc_sensor.state = ENVSYS_SVALID;
		break;

	default:
		sc->sc_sensor.state = ENVSYS_SINVALID;
		break;
	}
}

static void
acpifan_sensor_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	device_t self = sme->sme_cookie;

	(void)AcpiOsExecute(OSL_NOTIFY_HANDLER, acpifan_sensor_state, self);
}

MODULE(MODULE_CLASS_DRIVER, acpifan, "sysmon_envsys");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
acpifan_modcmd(modcmd_t cmd, void *aux)
{
	int rv = 0;

	switch (cmd) {

	case MODULE_CMD_INIT:

#ifdef _MODULE
		rv = config_init_component(cfdriver_ioconf_acpifan,
		    cfattach_ioconf_acpifan, cfdata_ioconf_acpifan);
#endif
		break;

	case MODULE_CMD_FINI:

#ifdef _MODULE
		rv = config_fini_component(cfdriver_ioconf_acpifan,
		    cfattach_ioconf_acpifan, cfdata_ioconf_acpifan);
#endif
		break;

	default:
		rv = ENOTTY;
	}

	return rv;
}
