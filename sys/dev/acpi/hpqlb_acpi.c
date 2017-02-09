/* $NetBSD: hpqlb_acpi.c,v 1.10 2015/04/23 23:23:00 pgoyette Exp $ */

/*-
 * Copyright (c) 2008  Christoph Egger <cegger@netbsd.org>
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hpqlb_acpi.c,v 1.10 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <machine/pio.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <dev/isa/isareg.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>

#define _COMPONENT		ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME		("hpqlb_acpi")

struct hpqlb_softc {
	device_t sc_dev;
	struct acpi_devnode *sc_node;

	device_t sc_wskbddev;

#define HP_PSW_DISPLAY_CYCLE	0
#define HP_PSW_BRIGHTNESS_UP	1
#define HP_PSW_BRIGHTNESS_DOWN	2
#define HP_PSW_SLEEP		3
#define HP_PSW_LAST		4
	struct sysmon_pswitch sc_smpsw[HP_PSW_LAST];
	bool sc_smpsw_displaycycle_valid;
	bool sc_smpsw_sleep_valid;
};

#define HP_QLB_Quick			0x88
#define HP_QLB_DVD			0x8e
#define HP_QLB_FullBackward		0x90
#define HP_QLB_Play			0xa2
#define HP_QLB_FullForward		0x99
#define HP_QLB_Stop			0xa4
#define HP_QLB_VolumeMute		0xa0
#define HP_QLB_VolumeDown		0xae
#define HP_QLB_VolumeUp			0xb0

#define HP_QLB_Help			0xb1
#define HP_QLB_WWW			0xb2
#define HP_QLB_DisplayCycle		/* ??? */
#define HP_QLB_Sleep			0xdf
#define HP_QLB_Lock			0x8a
#define HP_QLB_BrightnessDown		/* ??? */
#define HP_QLB_BrightnessUp		/* ??? */
#define HP_QLB_ChasisOpen		0xe3

static int hpqlb_match(device_t, cfdata_t, void *);
static void hpqlb_attach(device_t, device_t, void *);
static int hpqlb_detach(device_t, int);

static int hpqlb_finalize(device_t);
static int hpqlb_hotkey_handler(struct wskbd_softc *, void *, u_int, int);

static void hpqlb_init(device_t);
static bool hpqlb_resume(device_t, const pmf_qual_t *);

CFATTACH_DECL_NEW(hpqlb, sizeof(struct hpqlb_softc),
    hpqlb_match, hpqlb_attach, hpqlb_detach, NULL);

static const char * const hpqlb_ids[] = {
	"HPQ0006",
	"HPQ0007",
	NULL
};

static int
hpqlb_match(device_t parent, cfdata_t match, void *opaque)
{
	struct acpi_attach_args *aa = opaque;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, hpqlb_ids);
}

static void
hpqlb_attach(device_t parent, device_t self, void *opaque)
{
	struct hpqlb_softc *sc = device_private(self);
	struct acpi_attach_args *aa = opaque;

	sc->sc_node = aa->aa_node;
	sc->sc_dev = self;

	aprint_naive("\n");
	aprint_normal(": HP Quick Launch Buttons\n");

	hpqlb_init(self);

	if (config_finalize_register(self, hpqlb_finalize) != 0)
		aprint_error_dev(self, "unable to register hpqlb finalizer\n");

	sc->sc_smpsw_displaycycle_valid = true;

	sc->sc_smpsw[HP_PSW_DISPLAY_CYCLE].smpsw_name =
	    PSWITCH_HK_DISPLAY_CYCLE;

	sc->sc_smpsw[HP_PSW_DISPLAY_CYCLE].smpsw_type =
	    PSWITCH_TYPE_HOTKEY;

	if (sysmon_pswitch_register(&sc->sc_smpsw[HP_PSW_DISPLAY_CYCLE]) != 0)
		sc->sc_smpsw_displaycycle_valid = false;

	sc->sc_smpsw_sleep_valid = true;
	sc->sc_smpsw[HP_PSW_SLEEP].smpsw_name = device_xname(self);
	sc->sc_smpsw[HP_PSW_SLEEP].smpsw_type = PSWITCH_TYPE_SLEEP;

	if (sysmon_pswitch_register(&sc->sc_smpsw[HP_PSW_SLEEP]) != 0)
		sc->sc_smpsw_sleep_valid = false;

	(void)pmf_device_register(self, NULL, hpqlb_resume);
}

static int
hpqlb_detach(device_t self, int flags)
{
	struct hpqlb_softc *sc = device_private(self);

	pmf_device_deregister(self);
	wskbd_hotkey_deregister(sc->sc_wskbddev);

	if (sc->sc_smpsw_sleep_valid != false)
		sysmon_pswitch_unregister(&sc->sc_smpsw[HP_PSW_SLEEP]);

	if (sc->sc_smpsw_displaycycle_valid != false)
		sysmon_pswitch_unregister(&sc->sc_smpsw[HP_PSW_DISPLAY_CYCLE]);

	return 0;
}

static int
hpqlb_hotkey_handler(struct wskbd_softc *wskbd_sc, void *cookie,
		u_int type, int value)
{
	struct hpqlb_softc *sc = cookie;
	int ret = 1;

	switch (value) {
	case HP_QLB_VolumeMute:
		if (type != WSCONS_EVENT_KEY_DOWN)
			break;
		pmf_event_inject(NULL, PMFE_AUDIO_VOLUME_TOGGLE);
		break;
	case HP_QLB_VolumeDown:
		if (type != WSCONS_EVENT_KEY_DOWN)
			break;
		pmf_event_inject(NULL, PMFE_AUDIO_VOLUME_DOWN);
		break;
	case HP_QLB_VolumeUp:
		if (type != WSCONS_EVENT_KEY_DOWN)
			break;
		pmf_event_inject(NULL, PMFE_AUDIO_VOLUME_UP);
		break;

#if 0
	case HP_QLB_DisplayCycle:		/* ??? */
		if (type != WSCONS_EVENT_KEY_DOWN)
			break;
		if (sc->sc_smpsw_displaycycle_valid == false)
			break;
		sysmon_pswitch_event(&sc->sc_smpsw[HP_PSW_DISPLAY_CYCLE],
			PSWITCH_EVENT_PRESSED);
		break;
#endif
	case HP_QLB_Sleep:
		if (type != WSCONS_EVENT_KEY_DOWN)
			break;
		if (sc->sc_smpsw_sleep_valid == false)
			break;
		sysmon_pswitch_event(&sc->sc_smpsw[HP_PSW_SLEEP],
			PSWITCH_EVENT_PRESSED);
		break;
#if 0
	case HP_QLB_BrightnessDown:	/* ??? */
		if (type != WSCONS_EVENT_KEY_DOWN)
			break;
		pmf_event_inject(NULL, PMFE_DISPLAY_BRIGHTNESS_DOWN);
		break;
	case HP_QLB_BrightnessUp:	/* ??? */
		if (type != WSCONS_EVENT_KEY_DOWN)
			break;
		pmf_event_inject(NULL, PMFE_DISPLAY_BRIGHTNESS_UP);
		break;
#endif
	case HP_QLB_ChasisOpen:
		if (type != WSCONS_EVENT_KEY_DOWN)
			break;
		pmf_event_inject(NULL, PMFE_CHASSIS_LID_OPEN);
		break;
	default:

		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "unknown hotkey "
			"0x%02x\n", value));
		ret = 0; /* Assume, this is no hotkey */
		break;
	}

	return ret;
}

static void
hpqlb_init(device_t self)
{

	/* HPQ0006: HP Quick Launch Buttons */
	/* HPQ0007: HP Remote Device */
	/* val 0, 1 or 7 == HPQ0006 */
	/* val not 0, 1 or 7 == HPQ0007 */

	/* Turn on Quick Launch Buttons */
	outb(IO_RTC+2, 0xaf);
	outb(IO_RTC+3, 7 /* val */);
}

static int
hpqlb_finalize(device_t self)
{
	device_t dv;
	deviter_t di;
	struct hpqlb_softc *sc = device_private(self);
	static int done_once = 0;

	/* Since we only handle real hardware, we only need to be
	 * called once.
	 */
	if (done_once)
		return 0;
	done_once = 1;

	for (dv = deviter_first(&di, DEVITER_F_ROOT_FIRST); dv != NULL;
	     dv = deviter_next(&di)) {
		if (!device_is_a(dv, "wskbd"))
			continue;

		/* Make sure, we don't get a wskbd from a USB keyboard.
		 * QLB only works on the wskbd attached on pckbd. */
		if (!device_is_a(device_parent(dv), "pckbd"))
			continue;

		aprint_normal_dev(self, "registering on %s\n",
				device_xname(dv));
		break;
	}
	deviter_release(&di);

	if (dv == NULL) {
		aprint_error_dev(self, "WARNING: no matching wskbd found\n");
		return 1;
	}

	sc->sc_wskbddev = dv;

	wskbd_hotkey_register(sc->sc_wskbddev, sc, hpqlb_hotkey_handler);

	return 0;
}

static bool
hpqlb_resume(device_t self, const pmf_qual_t *qual)
{

	hpqlb_init(self);

	return true;
}

MODULE(MODULE_CLASS_DRIVER, hpqlb, "sysmon_power");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
hpqlb_modcmd(modcmd_t cmd, void *aux)
{
	int rv = 0;

	switch (cmd) {

	case MODULE_CMD_INIT:

#ifdef _MODULE
		rv = config_init_component(cfdriver_ioconf_hpqlb,
		    cfattach_ioconf_hpqlb, cfdata_ioconf_hpqlb);
#endif
		break;

	case MODULE_CMD_FINI:

#ifdef _MODULE
		rv = config_fini_component(cfdriver_ioconf_hpqlb,
		    cfattach_ioconf_hpqlb, cfdata_ioconf_hpqlb);
#endif
		break;

	default:
		rv = ENOTTY;
	}

	return rv;
}
