/*	$NetBSD: wmi_eeepc.c,v 1.4 2015/04/23 23:23:00 pgoyette Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: wmi_eeepc.c,v 1.4 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/module.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/wmi/wmi_acpivar.h>

#include <dev/sysmon/sysmonvar.h>

#define _COMPONENT			ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME			("wmi_eeepc")


/*
 * Current brightness is reported by events in the range of 0x10 to 0x2f.
 * The low nibble containing a value from 0x0 to 0xA proportionate to
 * brightness, 0x0 indicates minimum illumination, not backlight off.
 */
#define WMI_EEEPC_HK_BACKLIGHT_STATUS_BRIGHTNESS_MASK 0x0f /* 0x0 to 0xA */
#define WMI_EEEPC_HK_BACKLIGHT_STATUS_DIRECTION_MASK (~WMI_EEEPC_HK_BACKLIGHT_STATUS_BRIGHTNESS_MASK)
#define WMI_EEEPC_HK_BACKLIGHT_STATUS_DIRECTION_INC 0x10
#define WMI_EEEPC_HK_BACKLIGHT_STATUS_DIRECTION_DEC 0x20

#define WMI_EEEPC_HK_VOLUME_UP		0x30
#define WMI_EEEPC_HK_VOLUME_DOWN	0x31
#define WMI_EEEPC_HK_VOLUME_MUTE	0x32
#define WMI_EEEPC_HK_EXPRESSGATE	0x5C	/* Also, "SuperHybrid" */
#define WMI_EEEPC_HK_TOUCHPAD		0x6B
#define WMI_EEEPC_HK_WIRELESS		0x88
#define WMI_EEEPC_HK_WEBCAM		0xBD
#define WMI_EEEPC_HK_DISPLAY_CYCLE	0xCC
#define WMI_EEEPC_HK_DISPLAY_SAVER	0xE8
#define WMI_EEEPC_HK_DISPLAY_OFF	0xE9
						/* Unlabeled keys on 1215T */ 
#define WMI_EEEPC_HK_UNKNOWN_xEC	0xEC	/* Fn+E */
#define WMI_EEEPC_HK_UNKNOWN_xED	0xED	/* Fn+D */
#define WMI_EEEPC_HK_UNKNOWN_xEE	0xEE	/* Fn+S */
#define WMI_EEEPC_HK_UNKNOWN_xEF	0xEF	/* Fn+F */

#define WMI_EEEPC_IS_BACKLIGHT_STATUS(x) (((x & WMI_EEEPC_HK_BACKLIGHT_STATUS_DIRECTION_MASK) == WMI_EEEPC_HK_BACKLIGHT_STATUS_DIRECTION_INC) || ((x & WMI_EEEPC_HK_BACKLIGHT_STATUS_DIRECTION_MASK) == WMI_EEEPC_HK_BACKLIGHT_STATUS_DIRECTION_DEC))

enum eeepc_smpsw {
	WMI_EEEPC_PSW_EXPRESSGATE = 0,
	WMI_EEEPC_PSW_TOUCHPAD,
	WMI_EEEPC_PSW_WIRELESS,
	WMI_EEEPC_PSW_WEBCAM,
	WMI_EEEPC_PSW_DISPLAY_CYCLE,
	WMI_EEEPC_PSW_DISPLAY_SAVER,
	WMI_EEEPC_PSW_DISPLAY_OFF,
	WMI_EEEPC_PSW_UNKNOWN_xEC,
	WMI_EEEPC_PSW_UNKNOWN_xED,
	WMI_EEEPC_PSW_UNKNOWN_xEE,
	WMI_EEEPC_PSW_UNKNOWN_xEF,
	WMI_EEEPC_PSW_COUNT	/* Must be last. */
};

#define WMI_EEEPC_GUID_EVENT		"ABBC0F72-8EA1-11D1-00A0-C90629100000"

struct wmi_eeepc_softc {
	device_t		sc_dev;
	device_t		sc_parent;
	struct sysmon_pswitch	sc_psw[WMI_EEEPC_PSW_COUNT];
	bool			sc_psw_valid[WMI_EEEPC_PSW_COUNT];
};

static int	wmi_eeepc_match(device_t, cfdata_t, void *);
static void	wmi_eeepc_attach(device_t, device_t, void *);
static int	wmi_eeepc_detach(device_t, int);
static void	wmi_eeepc_notify_handler(ACPI_HANDLE, uint32_t, void *);
static bool	wmi_eeepc_suspend(device_t, const pmf_qual_t *);
static bool	wmi_eeepc_resume(device_t, const pmf_qual_t *);

CFATTACH_DECL_NEW(wmieeepc, sizeof(struct wmi_eeepc_softc),
    wmi_eeepc_match, wmi_eeepc_attach, wmi_eeepc_detach, NULL);

static int
wmi_eeepc_match(device_t parent, cfdata_t match, void *aux)
{
	return acpi_wmi_guid_match(parent, WMI_EEEPC_GUID_EVENT);
}

static void
wmi_eeepc_attach(device_t parent, device_t self, void *aux)
{
	struct wmi_eeepc_softc *sc = device_private(self);
	ACPI_STATUS rv;

	sc->sc_dev = self;
	sc->sc_parent = parent;

	rv = acpi_wmi_event_register(parent, wmi_eeepc_notify_handler);

	if (ACPI_FAILURE(rv)) {
		aprint_error(": failed to install WMI notify handler\n");
		return;
	}

	aprint_naive("\n");
	aprint_normal(": Asus Eee PC WMI mappings\n");

	memset(sc->sc_psw, 0, sizeof(sc->sc_psw));
	sc->sc_psw[WMI_EEEPC_PSW_EXPRESSGATE].smpsw_name = "expressgate";
	sc->sc_psw[WMI_EEEPC_PSW_TOUCHPAD].smpsw_name = "touchpad-toggle";
	sc->sc_psw[WMI_EEEPC_PSW_WIRELESS].smpsw_name = "wireless-toggle";
	sc->sc_psw[WMI_EEEPC_PSW_WEBCAM].smpsw_name = "camera-button";
	sc->sc_psw[WMI_EEEPC_PSW_DISPLAY_CYCLE].smpsw_name = PSWITCH_HK_DISPLAY_CYCLE;
	sc->sc_psw[WMI_EEEPC_PSW_DISPLAY_SAVER].smpsw_name = PSWITCH_HK_LOCK_SCREEN;
	sc->sc_psw[WMI_EEEPC_PSW_DISPLAY_OFF].smpsw_name = "display-off";
	sc->sc_psw[WMI_EEEPC_PSW_UNKNOWN_xEC].smpsw_name = "unlabeled-xEC";
	sc->sc_psw[WMI_EEEPC_PSW_UNKNOWN_xED].smpsw_name = "unlabeled-xED";
	sc->sc_psw[WMI_EEEPC_PSW_UNKNOWN_xEE].smpsw_name = "unlabeled-xEE";
	sc->sc_psw[WMI_EEEPC_PSW_UNKNOWN_xEF].smpsw_name = "unlabeled-xEF";

	for (int i = 0; i < WMI_EEEPC_PSW_COUNT; i++) {
		KASSERT(sc->sc_psw[i].smpsw_name != NULL);
		sc->sc_psw[i].smpsw_type = PSWITCH_TYPE_HOTKEY;
		if (sysmon_pswitch_register(&sc->sc_psw[i]) == 0) {
			sc->sc_psw_valid[i] = true;
		} else {
			sc->sc_psw_valid[i] = false;
			aprint_error_dev(self,
			    "hotkey[%d] registration failed\n", i);
		}
	}

	(void)pmf_device_register(self, wmi_eeepc_suspend, wmi_eeepc_resume);
}

static int
wmi_eeepc_detach(device_t self, int flags)
{
	struct wmi_eeepc_softc *sc = device_private(self);
	device_t parent = sc->sc_parent;


	for (int i = 0; i < WMI_EEEPC_PSW_COUNT; i++) {
		if (sc->sc_psw_valid[i] == true) {
			sysmon_pswitch_unregister(&sc->sc_psw[i]);
			sc->sc_psw_valid[i] = false;
		}
	}

	(void)pmf_device_deregister(self);
	(void)acpi_wmi_event_deregister(parent);

	return 0;
}

static bool
wmi_eeepc_suspend(device_t self, const pmf_qual_t *qual)
{
	struct wmi_eeepc_softc *sc = device_private(self);
	device_t parent = sc->sc_parent;

	(void)acpi_wmi_event_deregister(parent);

	return true;
}

static bool
wmi_eeepc_resume(device_t self, const pmf_qual_t *qual)
{
	struct wmi_eeepc_softc *sc = device_private(self);
	device_t parent = sc->sc_parent;

	(void)acpi_wmi_event_register(parent, wmi_eeepc_notify_handler);

	return true;
}

static void
wmi_eeepc_pswitch_event(struct wmi_eeepc_softc *sc, enum eeepc_smpsw key)
{
	if (sc->sc_psw_valid[key] != true) {
		device_printf(sc->sc_dev, "hotkey[%d] not registered\n", key);
		return;
	}

	/*
	 * This function is called upon key release,
	 * but the default powerd scripts expect presses.
	 *
	 * Anyway, we may as well send the make and the break event.
	 */

	sysmon_pswitch_event(&sc->sc_psw[key], PSWITCH_EVENT_PRESSED);
	sysmon_pswitch_event(&sc->sc_psw[key], PSWITCH_EVENT_RELEASED);
}

static void
wmi_eeepc_notify_handler(ACPI_HANDLE hdl, uint32_t evt, void *aux)
{
	struct wmi_eeepc_softc *sc;
	device_t self = aux;
	ACPI_OBJECT *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	uint32_t val = 0; /* XXX GCC */

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
		rv = AE_LIMIT;
		goto out;
	}

	val = obj->Integer.Value;

	if (WMI_EEEPC_IS_BACKLIGHT_STATUS(val)) {
		/* We'll silently ignore these for now. */
		goto out;
	}

	switch (val) {

	case WMI_EEEPC_HK_VOLUME_UP:
		pmf_event_inject(NULL, PMFE_AUDIO_VOLUME_UP);
		break;

	case WMI_EEEPC_HK_VOLUME_DOWN:
		pmf_event_inject(NULL, PMFE_AUDIO_VOLUME_DOWN);
		break;

	case WMI_EEEPC_HK_VOLUME_MUTE:
		pmf_event_inject(NULL, PMFE_AUDIO_VOLUME_TOGGLE);
		break;

	case WMI_EEEPC_HK_EXPRESSGATE:
		wmi_eeepc_pswitch_event(sc, WMI_EEEPC_PSW_EXPRESSGATE);
		break;

	case WMI_EEEPC_HK_TOUCHPAD:
		wmi_eeepc_pswitch_event(sc, WMI_EEEPC_PSW_TOUCHPAD);
		break;

	case WMI_EEEPC_HK_WIRELESS:
		wmi_eeepc_pswitch_event(sc, WMI_EEEPC_PSW_WIRELESS);
		break;

	case WMI_EEEPC_HK_WEBCAM:
		wmi_eeepc_pswitch_event(sc, WMI_EEEPC_PSW_WEBCAM);
		break;

	case WMI_EEEPC_HK_DISPLAY_CYCLE:
		wmi_eeepc_pswitch_event(sc, WMI_EEEPC_PSW_DISPLAY_CYCLE);
		break;

	case WMI_EEEPC_HK_DISPLAY_SAVER:
		wmi_eeepc_pswitch_event(sc, WMI_EEEPC_PSW_DISPLAY_SAVER);
		break;

	case WMI_EEEPC_HK_DISPLAY_OFF:
		wmi_eeepc_pswitch_event(sc, WMI_EEEPC_PSW_DISPLAY_OFF);
		break;

	case WMI_EEEPC_HK_UNKNOWN_xEC:
		wmi_eeepc_pswitch_event(sc, WMI_EEEPC_PSW_UNKNOWN_xEC);
		break;

	case WMI_EEEPC_HK_UNKNOWN_xED:
		wmi_eeepc_pswitch_event(sc, WMI_EEEPC_PSW_UNKNOWN_xED);
		break;

	case WMI_EEEPC_HK_UNKNOWN_xEE:
		wmi_eeepc_pswitch_event(sc, WMI_EEEPC_PSW_UNKNOWN_xEE);
		break;

	case WMI_EEEPC_HK_UNKNOWN_xEF:
		wmi_eeepc_pswitch_event(sc, WMI_EEEPC_PSW_UNKNOWN_xEF);
		break;

	default:
		device_printf(self, "unknown key 0x%02X for event 0x%02X\n",
		    val, evt);
		break;
	}

out:
	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	if (ACPI_FAILURE(rv))
		device_printf(self, "failed to get data for "
		    "event 0x%02X: %s\n", evt, AcpiFormatException(rv));
}

MODULE(MODULE_CLASS_DRIVER, wmieeepc, "acpiwmi,sysmon_power");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
wmieeepc_modcmd(modcmd_t cmd, void *aux)
{
	int rv = 0;

	switch (cmd) {

	case MODULE_CMD_INIT:

#ifdef _MODULE
		rv = config_init_component(cfdriver_ioconf_wmieeepc,
		    cfattach_ioconf_wmieeepc, cfdata_ioconf_wmieeepc);
#endif
		break;

	case MODULE_CMD_FINI:

#ifdef _MODULE
		rv = config_fini_component(cfdriver_ioconf_wmieeepc,
		    cfattach_ioconf_wmieeepc, cfdata_ioconf_wmieeepc);
#endif
		break;

	default:
		rv = ENOTTY;
	}

	return rv;
}
