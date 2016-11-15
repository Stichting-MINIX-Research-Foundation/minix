/*	$NetBSD: wmi_dell.c,v 1.9 2015/04/23 23:23:00 pgoyette Exp $ */

/*-
 * Copyright (c) 2009, 2010 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: wmi_dell.c,v 1.9 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/module.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/wmi/wmi_acpivar.h>

#include <dev/sysmon/sysmonvar.h>

#define _COMPONENT			ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME			("wmi_dell")

#define WMI_DELL_HOTKEY_BRIGHTNESS_DOWN	0xE005
#define WMI_DELL_HOTKEY_BRIGHTNESS_UP	0xE006
#define WMI_DELL_HOTKEY_DISPLAY_CYCLE	0xE00B
#define WMI_DELL_HOTKEY_VOLUME_MUTE	0xE020
#define WMI_DELL_HOTKEY_VOLUME_DOWN	0xE02E
#define WMI_DELL_HOTKEY_VOLUME_UP	0xE030
/*      WMI_DELL_HOTKEY_UNKNOWN		0xXXXX */

#define WMI_DELL_PSW_DISPLAY_CYCLE	0
#define WMI_DELL_PSW_COUNT		1

#define WMI_DELL_GUID_EVENT		"9DBB5994-A997-11DA-B012-B622A1EF5492"

struct wmi_dell_softc {
	device_t		sc_dev;
	device_t		sc_parent;
	struct sysmon_pswitch	sc_smpsw[WMI_DELL_PSW_COUNT];
	bool			sc_smpsw_valid;
};

static int	wmi_dell_match(device_t, cfdata_t, void *);
static void	wmi_dell_attach(device_t, device_t, void *);
static int	wmi_dell_detach(device_t, int);
static void	wmi_dell_notify_handler(ACPI_HANDLE, uint32_t, void *);
static bool	wmi_dell_suspend(device_t, const pmf_qual_t *);
static bool	wmi_dell_resume(device_t, const pmf_qual_t *);

CFATTACH_DECL_NEW(wmidell, sizeof(struct wmi_dell_softc),
    wmi_dell_match, wmi_dell_attach, wmi_dell_detach, NULL);

static int
wmi_dell_match(device_t parent, cfdata_t match, void *aux)
{
	return acpi_wmi_guid_match(parent, WMI_DELL_GUID_EVENT);
}

static void
wmi_dell_attach(device_t parent, device_t self, void *aux)
{
	struct wmi_dell_softc *sc = device_private(self);
	ACPI_STATUS rv;
	int e;

	sc->sc_dev = self;
	sc->sc_parent = parent;
	sc->sc_smpsw_valid = true;

	rv = acpi_wmi_event_register(parent, wmi_dell_notify_handler);

	if (ACPI_FAILURE(rv)) {
		aprint_error(": failed to install WMI notify handler\n");
		return;
	}

	aprint_naive("\n");
	aprint_normal(": Dell WMI mappings\n");

	sc->sc_smpsw[WMI_DELL_PSW_DISPLAY_CYCLE].smpsw_name =
	    PSWITCH_HK_DISPLAY_CYCLE;

	sc->sc_smpsw[WMI_DELL_PSW_DISPLAY_CYCLE].smpsw_type =
	    PSWITCH_TYPE_HOTKEY;

	e = sysmon_pswitch_register(&sc->sc_smpsw[WMI_DELL_PSW_DISPLAY_CYCLE]);

	if (e != 0)
		sc->sc_smpsw_valid = false;

	(void)pmf_device_register(self, wmi_dell_suspend, wmi_dell_resume);
}

static int
wmi_dell_detach(device_t self, int flags)
{
	struct wmi_dell_softc *sc = device_private(self);
	device_t parent = sc->sc_parent;
	size_t i;

	(void)pmf_device_deregister(self);
	(void)acpi_wmi_event_deregister(parent);

	if (sc->sc_smpsw_valid != true)
		return 0;

	for (i = 0; i < __arraycount(sc->sc_smpsw); i++)
		sysmon_pswitch_unregister(&sc->sc_smpsw[i]);

	return 0;
}

static bool
wmi_dell_suspend(device_t self, const pmf_qual_t *qual)
{
	struct wmi_dell_softc *sc = device_private(self);
	device_t parent = sc->sc_parent;

	(void)acpi_wmi_event_deregister(parent);

	return true;
}

static bool
wmi_dell_resume(device_t self, const pmf_qual_t *qual)
{
	struct wmi_dell_softc *sc = device_private(self);
	device_t parent = sc->sc_parent;

	(void)acpi_wmi_event_register(parent, wmi_dell_notify_handler);

	return true;
}

static void
wmi_dell_notify_handler(ACPI_HANDLE hdl, uint32_t evt, void *aux)
{
	struct wmi_dell_softc *sc;
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

	if (obj->Type != ACPI_TYPE_BUFFER) {
		rv = AE_TYPE;
		goto out;
	}

	val = obj->Buffer.Pointer[1] & 0xFFFF;

	switch (val) {

	case WMI_DELL_HOTKEY_BRIGHTNESS_DOWN:
		pmf_event_inject(NULL, PMFE_DISPLAY_BRIGHTNESS_DOWN);
		break;

	case WMI_DELL_HOTKEY_BRIGHTNESS_UP:
		pmf_event_inject(NULL, PMFE_DISPLAY_BRIGHTNESS_UP);
		break;

	case WMI_DELL_HOTKEY_DISPLAY_CYCLE:

		if (sc->sc_smpsw_valid != true) {
			rv = AE_ABORT_METHOD;
			break;
		}

		sysmon_pswitch_event(&sc->sc_smpsw[WMI_DELL_PSW_DISPLAY_CYCLE],
		    PSWITCH_EVENT_PRESSED);
		break;

	case WMI_DELL_HOTKEY_VOLUME_MUTE:
		pmf_event_inject(NULL, PMFE_AUDIO_VOLUME_TOGGLE);
		break;

	case WMI_DELL_HOTKEY_VOLUME_DOWN:
		pmf_event_inject(NULL, PMFE_AUDIO_VOLUME_DOWN);
		break;

	case WMI_DELL_HOTKEY_VOLUME_UP:
		pmf_event_inject(NULL, PMFE_AUDIO_VOLUME_UP);
		break;

	default:
		aprint_debug_dev(sc->sc_dev,
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

MODULE(MODULE_CLASS_DRIVER, wmidell, "acpiwmi,sysmon_power");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
wmidell_modcmd(modcmd_t cmd, void *aux)
{
	int rv = 0;

	switch (cmd) {

	case MODULE_CMD_INIT:

#ifdef _MODULE
		rv = config_init_component(cfdriver_ioconf_wmidell,
		    cfattach_ioconf_wmidell, cfdata_ioconf_wmidell);
#endif
		break;

	case MODULE_CMD_FINI:

#ifdef _MODULE
		rv = config_fini_component(cfdriver_ioconf_wmidell,
		    cfattach_ioconf_wmidell, cfdata_ioconf_wmidell);
#endif
		break;

	default:
		rv = ENOTTY;
	}

	return rv;
}
