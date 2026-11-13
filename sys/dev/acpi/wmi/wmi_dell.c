/*	$NetBSD: wmi_dell.c,v 1.11 2017/12/03 23:43:00 christos Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: wmi_dell.c,v 1.11 2017/12/03 23:43:00 christos Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/module.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/wmi/wmi_acpivar.h>

#include <dev/sysmon/sysmonvar.h>

#ifdef WMI_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

#define _COMPONENT			ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME			("wmi_dell")

#define WMI_DELL_PSW_DISPLAY_CYCLE	0
#define WMI_DELL_PSW_COUNT		1

#define WMI_DELL_GUID_EVENT		"9DBB5994-A997-11DA-B012-B622A1EF5492"
#define WMI_DELL_GUID_DESC		"8D9DDCBC-A997-11DA-B012-B622A1EF5492"

struct wmi_dell_softc {
	device_t		sc_dev;
	device_t		sc_parent;
	int			sc_version;
	struct sysmon_pswitch	sc_smpsw[WMI_DELL_PSW_COUNT];
	bool			sc_smpsw_valid;
};

#define WMI_DELLA_PMF	0x0
#define WMI_DELLA_PSW	0x1
#define WMI_DELLA_IGN	0x2

const struct wmi_dell_actions {
	u_int	wda_action;
	u_int	wda_type;
	u_int	wda_subtype;
	u_int	wda_data;
} wmi_dell_actions[] = {
	/* type 0 */
	/* brightness control */
	{WMI_DELLA_PMF, 0x0000, 0xe005, PMFE_DISPLAY_BRIGHTNESS_DOWN},
	{WMI_DELLA_PMF, 0x0000, 0xe006, PMFE_DISPLAY_BRIGHTNESS_UP},
	{WMI_DELLA_PSW, 0x0000, 0xe00b, WMI_DELL_PSW_DISPLAY_CYCLE},

	{WMI_DELLA_PMF, 0x0000, 0xe008, PMFE_RADIO_TOGGLE},
	{WMI_DELLA_IGN, 0x0000, 0xe00c, 0}, /* keyboard illumination */

	/* volume control */
	{WMI_DELLA_PMF, 0x0000, 0xe020, PMFE_AUDIO_VOLUME_TOGGLE},
	{WMI_DELLA_PMF, 0x0000, 0xe02e, PMFE_AUDIO_VOLUME_DOWN},
	{WMI_DELLA_PMF, 0x0000, 0xe030, PMFE_AUDIO_VOLUME_UP},
	{WMI_DELLA_PMF, 0x0000, 0xe0f8, PMFE_AUDIO_VOLUME_DOWN},
	{WMI_DELLA_PMF, 0x0000, 0xe0f9, PMFE_AUDIO_VOLUME_UP},


	/* type 0x10 */
	{WMI_DELLA_PMF, 0x0010, 0x0057, PMFE_DISPLAY_BRIGHTNESS_DOWN},
	{WMI_DELLA_PMF, 0x0010, 0x0058, PMFE_DISPLAY_BRIGHTNESS_UP},
	{WMI_DELLA_IGN, 0x0010, 0x0151, 0}, /* Fn-lock */
	{WMI_DELLA_IGN, 0x0010, 0x0152, 0}, /* keyboard illumination */
	{WMI_DELLA_PMF, 0x0010, 0x0153, PMFE_RADIO_TOGGLE},
	{WMI_DELLA_IGN, 0x0010, 0x0155, 0}, /* Stealth mode toggle */
	{WMI_DELLA_IGN, 0x0010, 0xE035, 0}, /* Fn-lock */

	/* type 0x11 */
	{WMI_DELLA_IGN, 0x0011, 0x02eb5, 0}, /* keyboard illumination */
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
	ACPI_BUFFER obuf;
	ACPI_OBJECT *obj;
	uint32_t *data;
	int e;

	sc->sc_dev = self;
	sc->sc_parent = parent;
	sc->sc_smpsw_valid = true;

	rv = acpi_wmi_event_register(parent, wmi_dell_notify_handler);

	if (ACPI_FAILURE(rv)) {
		aprint_error(": failed to install WMI notify handler\n");
		return;
	}

	memset(&obuf, 0, sizeof(obuf));
	rv = acpi_wmi_data_query(parent, WMI_DELL_GUID_DESC, 0, &obuf);
	if (ACPI_FAILURE(rv)) {
		aprint_error(": failed to query WMI descriptor: %s\n",
		    AcpiFormatException(rv));
		return;
	}
	obj = obuf.Pointer;
	if (obj->Type != ACPI_TYPE_BUFFER) {
		aprint_error(": wrong type %d for WMI descriptor\n", obj->Type);
		return;
	}
	if (obj->Buffer.Length != 128) {
		aprint_error(": wrong len %d for WMI descriptor",
		    obj->Buffer.Length);
		if (obj->Buffer.Length < 16) {
			aprint_error("\n");
			return;
		}
	}
	data = (uint32_t *)obj->Buffer.Pointer;
#define WMI_LLED 	0x4C4C4544 
#define WMI_IMWsp	0x494D5720
	if (data[0] != WMI_LLED || data[1] != WMI_IMWsp) {
		aprint_error(": wrong WMI descriptor signature %#x %#x",
		    data[0], data[1]);
	}
	sc->sc_version = data[2];
	aprint_naive("\n");
	aprint_normal(": Dell WMI mappings version %d\n", sc->sc_version);

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
wmi_dell_action(struct wmi_dell_softc *sc, uint16_t *data, int len)
{
	size_t i;
	for (i = 0; i < __arraycount(wmi_dell_actions); i++) {
		const struct wmi_dell_actions *wda = &wmi_dell_actions[i];
		if (wda->wda_type == data[0] &&
		    wda->wda_subtype == data[1]) {
			switch(wda->wda_action) {
			case WMI_DELLA_IGN:
				DPRINTF((" ignored"));
				return;
			case WMI_DELLA_PMF:
				DPRINTF((" pmf %d", wda->wda_data));
				pmf_event_inject(NULL, wda->wda_data);
				return;
			case WMI_DELLA_PSW:
				DPRINTF((" psw %d", wda->wda_data));
				sysmon_pswitch_event(
				    &sc->sc_smpsw[wda->wda_data],
				    PSWITCH_EVENT_PRESSED);
				return;
			default:
				aprint_debug_dev(sc->sc_dev,
				    "unknown dell wmi action %d\n",
				    wda->wda_action);
				return;
			}

		}
	}
	aprint_debug_dev(sc->sc_dev, "unknown event %#4X %#4X\n",
	    data[0], data[1]);
}

static void
wmi_dell_notify_handler(ACPI_HANDLE hdl, uint32_t evt, void *aux)
{
	struct wmi_dell_softc *sc;
	device_t self = aux;
	ACPI_OBJECT *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	uint16_t *data, *end;
	int i, len;

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

	data = (void *)(&obj->Buffer.Pointer[0]);
	end = (void *)(&obj->Buffer.Pointer[obj->Buffer.Length]);

	DPRINTF(("wmi_dell_notify_handler buffer len %d\n",
	    obj->Buffer.Length));
	while (data < end) {
		DPRINTF(("wmi_dell_notify_handler len %d", data[0]));
		if (data[0] == 0) {
			DPRINTF(("\n"));
			break;
		}
		len = data[0] + 1;

		if (&data[len] >= end) {
			DPRINTF(("\n"));
			break;
		}
		if (len < 2) {
			DPRINTF(("\n"));
			continue;
		}
		for (i = 1; i < len; i++)
			DPRINTF((" %#04X", data[i]));
		wmi_dell_action(sc, &data[1], len - 1);
		DPRINTF(("\n"));
		data = &data[len];
		/* 
		 * WMI interface version 0 don't clear the buffer from previous
		 * event, so if the current event is smaller than the previous
		 * one there will be garbage after the current event.
		 * workaround by processing only the first event
		 */
		if (sc->sc_version == 0)
			break;
	}

out:
	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	if (ACPI_FAILURE(rv))
		aprint_error_dev(sc->sc_dev, "failed to get data for "
		    "event %#02X: %s\n", evt, AcpiFormatException(rv));
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
