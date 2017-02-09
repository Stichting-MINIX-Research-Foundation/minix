/*	$NetBSD: wmi_msi.c,v 1.5 2011/02/16 13:15:49 jruoho Exp $ */

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jukka Ruohonen.
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
__KERNEL_RCSID(0, "$NetBSD: wmi_msi.c,v 1.5 2011/02/16 13:15:49 jruoho Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/module.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/wmi/wmi_acpivar.h>

#define _COMPONENT			ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME			("wmi_msi")

#define WMI_MSI_HOTKEY_BRIGHTNESS_UP	0xD0
#define WMI_MSI_HOTKEY_BRIGHTNESS_DOWN	0xD1
#define WMI_MSI_HOTKEY_VOLUME_UP	0xD2
#define WMI_MSI_HOTKEY_VOLUME_DOWN	0xD3
/*      WMI_MSI_HOTKEY_UNKNOWN		0xXXXX */

#define WMI_MSI_GUID_EVENT		"B6F3EEF2-3D2F-49DC-9DE3-85BCE18C62F2"

struct wmi_msi_softc {
	device_t		sc_dev;
	device_t		sc_parent;
};

static int	wmi_msi_match(device_t, cfdata_t, void *);
static void	wmi_msi_attach(device_t, device_t, void *);
static int	wmi_msi_detach(device_t, int);
static void	wmi_msi_notify_handler(ACPI_HANDLE, uint32_t, void *);
static bool	wmi_msi_suspend(device_t, const pmf_qual_t *);
static bool	wmi_msi_resume(device_t, const pmf_qual_t *);

CFATTACH_DECL_NEW(wmimsi, sizeof(struct wmi_msi_softc),
    wmi_msi_match, wmi_msi_attach, wmi_msi_detach, NULL);

static int
wmi_msi_match(device_t parent, cfdata_t match, void *aux)
{
	return acpi_wmi_guid_match(parent, WMI_MSI_GUID_EVENT);
}

static void
wmi_msi_attach(device_t parent, device_t self, void *aux)
{
	struct wmi_msi_softc *sc = device_private(self);
	ACPI_STATUS rv;

	sc->sc_dev = self;
	sc->sc_parent = parent;

	rv = acpi_wmi_event_register(parent, wmi_msi_notify_handler);

	if (ACPI_FAILURE(rv)) {
		aprint_error(": failed to install WMI notify handler\n");
		return;
	}

	aprint_naive("\n");
	aprint_normal(": MSI WMI mappings\n");

	(void)pmf_device_register(self, wmi_msi_suspend, wmi_msi_resume);
}

static int
wmi_msi_detach(device_t self, int flags)
{
	struct wmi_msi_softc *sc = device_private(self);
	device_t parent = sc->sc_parent;

	(void)pmf_device_deregister(self);
	(void)acpi_wmi_event_deregister(parent);

	return 0;
}

static bool
wmi_msi_suspend(device_t self, const pmf_qual_t *qual)
{
	struct wmi_msi_softc *sc = device_private(self);
	device_t parent = sc->sc_parent;

	(void)acpi_wmi_event_deregister(parent);

	return true;
}

static bool
wmi_msi_resume(device_t self, const pmf_qual_t *qual)
{
	struct wmi_msi_softc *sc = device_private(self);
	device_t parent = sc->sc_parent;

	(void)acpi_wmi_event_register(parent, wmi_msi_notify_handler);

	return true;
}

static void
wmi_msi_notify_handler(ACPI_HANDLE hdl, uint32_t evt, void *aux)
{
	struct wmi_msi_softc *sc;
	device_t self = aux;
	ACPI_OBJECT *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	uint32_t val;

	buf.Pointer = NULL;

	sc = device_private(self);
	rv = acpi_wmi_event_get(sc->sc_parent, evt, &buf);

	if (ACPI_FAILURE(rv))
		goto out;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_INTEGER) {
		rv = AE_TYPE;
		goto out;
	}

	if (obj->Integer.Value > UINT32_MAX) {
		rv = AE_AML_NUMERIC_OVERFLOW;
		goto out;
	}

	val = obj->Integer.Value;

	switch (val) {

	case WMI_MSI_HOTKEY_BRIGHTNESS_DOWN:
		pmf_event_inject(NULL, PMFE_DISPLAY_BRIGHTNESS_DOWN);
		break;

	case WMI_MSI_HOTKEY_BRIGHTNESS_UP:
		pmf_event_inject(NULL, PMFE_DISPLAY_BRIGHTNESS_UP);
		break;

	case WMI_MSI_HOTKEY_VOLUME_DOWN:
		pmf_event_inject(NULL, PMFE_AUDIO_VOLUME_DOWN);
		break;

	case WMI_MSI_HOTKEY_VOLUME_UP:
		pmf_event_inject(NULL, PMFE_AUDIO_VOLUME_UP);
		break;

	default:
		aprint_normal_dev(sc->sc_dev,
		    "unknown key 0x%02X for event 0x%02X\n", val, evt);
		break;
	}

out:
	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	if (ACPI_FAILURE(rv))
		aprint_error_dev(sc->sc_dev, "failed to get data for "
		    "event 0x%02X: %s\n", evt, AcpiFormatException(rv));
}

MODULE(MODULE_CLASS_DRIVER, wmimsi, "acpiwmi");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
wmimsi_modcmd(modcmd_t cmd, void *aux)
{
	int rv = 0;

	switch (cmd) {

	case MODULE_CMD_INIT:

#ifdef _MODULE
		rv = config_init_component(cfdriver_ioconf_wmimsi,
		    cfattach_ioconf_wmimsi, cfdata_ioconf_wmimsi);
#endif
		break;

	case MODULE_CMD_FINI:

#ifdef _MODULE
		rv = config_fini_component(cfdriver_ioconf_wmimsi,
		    cfattach_ioconf_wmimsi, cfdata_ioconf_wmimsi);
#endif
		break;

	default:
		rv = ENOTTY;
	}

	return rv;
}
