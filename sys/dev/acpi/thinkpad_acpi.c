/* $NetBSD: thinkpad_acpi.c,v 1.45 2015/04/23 23:23:00 pgoyette Exp $ */

/*-
 * Copyright (c) 2007 Jared D. McNeill <jmcneill@invisible.ca>
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
__KERNEL_RCSID(0, "$NetBSD: thinkpad_acpi.c,v 1.45 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_ecvar.h>
#include <dev/acpi/acpi_power.h>

#include <dev/isa/isareg.h>

#define _COMPONENT		ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME		("thinkpad_acpi")

#define	THINKPAD_NTEMPSENSORS	8
#define	THINKPAD_NFANSENSORS	1
#define	THINKPAD_NSENSORS	(THINKPAD_NTEMPSENSORS + THINKPAD_NFANSENSORS)

typedef struct thinkpad_softc {
	device_t		sc_dev;
	device_t		sc_ecdev;
	struct acpi_devnode	*sc_node;
	ACPI_HANDLE		sc_powhdl;
	ACPI_HANDLE		sc_cmoshdl;

#define	TP_PSW_SLEEP		0	/* FnF4 */
#define	TP_PSW_HIBERNATE	1	/* FnF12 */
#define	TP_PSW_DISPLAY_CYCLE	2	/* FnF7 */
#define	TP_PSW_LOCK_SCREEN	3	/* FnF2 */
#define	TP_PSW_BATTERY_INFO	4	/* FnF3 */
#define	TP_PSW_EJECT_BUTTON	5	/* FnF9 */
#define	TP_PSW_ZOOM_BUTTON	6	/* FnSPACE */
#define	TP_PSW_VENDOR_BUTTON	7	/* ThinkVantage */
#define	TP_PSW_FNF1_BUTTON	8	/* FnF1 */
#define	TP_PSW_WIRELESS_BUTTON	9	/* FnF5 */
#define	TP_PSW_WWAN_BUTTON	10	/* FnF6 */
#define	TP_PSW_POINTER_BUTTON	11	/* FnF8 */
#define	TP_PSW_FNF10_BUTTON	12	/* FnF10 */
#define	TP_PSW_FNF11_BUTTON	13	/* FnF11 */
#define	TP_PSW_BRIGHTNESS_UP	14
#define	TP_PSW_BRIGHTNESS_DOWN	15
#define	TP_PSW_THINKLIGHT	16
#define	TP_PSW_VOLUME_UP	17
#define	TP_PSW_VOLUME_DOWN	18
#define	TP_PSW_VOLUME_MUTE	19
#define	TP_PSW_LAST		20

	struct sysmon_pswitch	sc_smpsw[TP_PSW_LAST];
	bool			sc_smpsw_valid;

	struct sysmon_envsys	*sc_sme;
	envsys_data_t		sc_sensor[THINKPAD_NSENSORS];

	int			sc_display_state;
} thinkpad_softc_t;

/* Hotkey events */
#define	THINKPAD_NOTIFY_FnF1		0x001
#define	THINKPAD_NOTIFY_LockScreen	0x002
#define	THINKPAD_NOTIFY_BatteryInfo	0x003
#define	THINKPAD_NOTIFY_SleepButton	0x004
#define	THINKPAD_NOTIFY_WirelessSwitch	0x005
#define	THINKPAD_NOTIFY_wWANSwitch	0x006
#define	THINKPAD_NOTIFY_DisplayCycle	0x007
#define	THINKPAD_NOTIFY_PointerSwitch	0x008
#define	THINKPAD_NOTIFY_EjectButton	0x009
#define	THINKPAD_NOTIFY_FnF10		0x00a	/* XXX: Not seen on T61 */
#define	THINKPAD_NOTIFY_FnF11		0x00b
#define	THINKPAD_NOTIFY_HibernateButton	0x00c
#define	THINKPAD_NOTIFY_BrightnessUp	0x010
#define	THINKPAD_NOTIFY_BrightnessDown	0x011
#define	THINKPAD_NOTIFY_ThinkLight	0x012
#define	THINKPAD_NOTIFY_Zoom		0x014
#define	THINKPAD_NOTIFY_VolumeUp	0x015	/* XXX: Not seen on T61 */
#define	THINKPAD_NOTIFY_VolumeDown	0x016	/* XXX: Not seen on T61 */
#define	THINKPAD_NOTIFY_VolumeMute	0x017	/* XXX: Not seen on T61 */
#define	THINKPAD_NOTIFY_ThinkVantage	0x018

#define	THINKPAD_CMOS_BRIGHTNESS_UP	0x04
#define	THINKPAD_CMOS_BRIGHTNESS_DOWN	0x05

#define	THINKPAD_HKEY_VERSION		0x0100

#define	THINKPAD_DISPLAY_LCD		0x01
#define	THINKPAD_DISPLAY_CRT		0x02
#define	THINKPAD_DISPLAY_DVI		0x08
#define	THINKPAD_DISPLAY_ALL \
	(THINKPAD_DISPLAY_LCD | THINKPAD_DISPLAY_CRT | THINKPAD_DISPLAY_DVI)

static int	thinkpad_match(device_t, cfdata_t, void *);
static void	thinkpad_attach(device_t, device_t, void *);
static int	thinkpad_detach(device_t, int);

static ACPI_STATUS thinkpad_mask_init(thinkpad_softc_t *, uint32_t);
static void	thinkpad_notify_handler(ACPI_HANDLE, uint32_t, void *);
static void	thinkpad_get_hotkeys(void *);

static void	thinkpad_sensors_init(thinkpad_softc_t *);
static void	thinkpad_sensors_refresh(struct sysmon_envsys *, envsys_data_t *);
static void	thinkpad_temp_refresh(struct sysmon_envsys *, envsys_data_t *);
static void	thinkpad_fan_refresh(struct sysmon_envsys *, envsys_data_t *);

static void	thinkpad_wireless_toggle(thinkpad_softc_t *);
static void	thinkpad_wwan_toggle(thinkpad_softc_t *);

static bool	thinkpad_resume(device_t, const pmf_qual_t *);
static void	thinkpad_brightness_up(device_t);
static void	thinkpad_brightness_down(device_t);
static uint8_t	thinkpad_brightness_read(thinkpad_softc_t *sc);
static void	thinkpad_cmos(thinkpad_softc_t *, uint8_t);

CFATTACH_DECL_NEW(thinkpad, sizeof(thinkpad_softc_t),
    thinkpad_match, thinkpad_attach, thinkpad_detach, NULL);

static const char * const thinkpad_ids[] = {
	"IBM0068",
	"LEN0068",
	NULL
};

static int
thinkpad_match(device_t parent, cfdata_t match, void *opaque)
{
	struct acpi_attach_args *aa = (struct acpi_attach_args *)opaque;
	ACPI_INTEGER ver;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	if (!acpi_match_hid(aa->aa_node->ad_devinfo, thinkpad_ids))
		return 0;

	/* We only support hotkey version 0x0100 */
	if (ACPI_FAILURE(acpi_eval_integer(aa->aa_node->ad_handle, "MHKV",
	    &ver)))
		return 0;

	if (ver != THINKPAD_HKEY_VERSION)
		return 0;

	/* Cool, looks like we're good to go */
	return 1;
}

static void
thinkpad_attach(device_t parent, device_t self, void *opaque)
{
	thinkpad_softc_t *sc = device_private(self);
	struct acpi_attach_args *aa = (struct acpi_attach_args *)opaque;
	struct sysmon_pswitch *psw;
	device_t curdev;
	deviter_t di;
	ACPI_STATUS rv;
	ACPI_INTEGER val;
	int i;

	sc->sc_dev = self;
	sc->sc_powhdl = NULL;
	sc->sc_cmoshdl = NULL;
	sc->sc_node = aa->aa_node;
	sc->sc_display_state = THINKPAD_DISPLAY_LCD;

	aprint_naive("\n");
	aprint_normal("\n");

	sc->sc_ecdev = NULL;
	for (curdev = deviter_first(&di, DEVITER_F_ROOT_FIRST);
	     curdev != NULL; curdev = deviter_next(&di))
		if (device_is_a(curdev, "acpiecdt") ||
		    device_is_a(curdev, "acpiec")) {
			sc->sc_ecdev = curdev;
			break;
		}
	deviter_release(&di);

	if (sc->sc_ecdev)
		aprint_debug_dev(self, "using EC at %s\n",
		    device_xname(sc->sc_ecdev));

	/* Get the supported event mask */
	rv = acpi_eval_integer(sc->sc_node->ad_handle, "MHKA", &val);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(self, "couldn't evaluate MHKA: %s\n",
		    AcpiFormatException(rv));
		goto fail;
	}

	/* Enable all supported events */
	rv = thinkpad_mask_init(sc, val);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(self, "couldn't set event mask: %s\n",
		    AcpiFormatException(rv));
		goto fail;
	}

	(void)acpi_register_notify(sc->sc_node, thinkpad_notify_handler);

	/*
	 * Obtain a handle for CMOS commands. This is used by T61.
	 */
	(void)AcpiGetHandle(NULL, "\\UCMS", &sc->sc_cmoshdl);

	/*
	 * Obtain a handle to the power resource available on many models.
	 * Since pmf(9) is not yet integrated with the ACPI power resource
	 * code, this must be turned on manually upon resume. Otherwise the
	 * system may, for instance, resume from S3 with usb(4) powered down.
	 */
	(void)AcpiGetHandle(NULL, "\\_SB.PCI0.LPC.EC.PUBS", &sc->sc_powhdl);

	/* Register power switches with sysmon */
	psw = sc->sc_smpsw;
	sc->sc_smpsw_valid = true;

	psw[TP_PSW_SLEEP].smpsw_name = device_xname(self);
	psw[TP_PSW_SLEEP].smpsw_type = PSWITCH_TYPE_SLEEP;
#if notyet
	psw[TP_PSW_HIBERNATE].smpsw_name = device_xname(self);
	mpsw[TP_PSW_HIBERNATE].smpsw_type = PSWITCH_TYPE_HIBERNATE;
#endif
	for (i = TP_PSW_DISPLAY_CYCLE; i < TP_PSW_LAST; i++)
		sc->sc_smpsw[i].smpsw_type = PSWITCH_TYPE_HOTKEY;
	psw[TP_PSW_DISPLAY_CYCLE].smpsw_name = PSWITCH_HK_DISPLAY_CYCLE;
	psw[TP_PSW_LOCK_SCREEN].smpsw_name = PSWITCH_HK_LOCK_SCREEN;
	psw[TP_PSW_BATTERY_INFO].smpsw_name = PSWITCH_HK_BATTERY_INFO;
	psw[TP_PSW_EJECT_BUTTON].smpsw_name = PSWITCH_HK_EJECT_BUTTON;
	psw[TP_PSW_ZOOM_BUTTON].smpsw_name = PSWITCH_HK_ZOOM_BUTTON;
	psw[TP_PSW_VENDOR_BUTTON].smpsw_name = PSWITCH_HK_VENDOR_BUTTON;
#ifndef THINKPAD_NORMAL_HOTKEYS
	psw[TP_PSW_FNF1_BUTTON].smpsw_name     = PSWITCH_HK_FNF1_BUTTON;
	psw[TP_PSW_WIRELESS_BUTTON].smpsw_name = PSWITCH_HK_WIRELESS_BUTTON;
	psw[TP_PSW_WWAN_BUTTON].smpsw_name     = PSWITCH_HK_WWAN_BUTTON;
	psw[TP_PSW_POINTER_BUTTON].smpsw_name  = PSWITCH_HK_POINTER_BUTTON;
	psw[TP_PSW_FNF10_BUTTON].smpsw_name    = PSWITCH_HK_FNF10_BUTTON;
	psw[TP_PSW_FNF11_BUTTON].smpsw_name    = PSWITCH_HK_FNF11_BUTTON;
	psw[TP_PSW_BRIGHTNESS_UP].smpsw_name   = PSWITCH_HK_BRIGHTNESS_UP;
	psw[TP_PSW_BRIGHTNESS_DOWN].smpsw_name = PSWITCH_HK_BRIGHTNESS_DOWN;
	psw[TP_PSW_THINKLIGHT].smpsw_name      = PSWITCH_HK_THINKLIGHT;
	psw[TP_PSW_VOLUME_UP].smpsw_name       = PSWITCH_HK_VOLUME_UP;
	psw[TP_PSW_VOLUME_DOWN].smpsw_name     = PSWITCH_HK_VOLUME_DOWN;
	psw[TP_PSW_VOLUME_MUTE].smpsw_name     = PSWITCH_HK_VOLUME_MUTE;
#endif /* THINKPAD_NORMAL_HOTKEYS */

	for (i = 0; i < TP_PSW_LAST; i++) {
		/* not supported yet */
		if (i == TP_PSW_HIBERNATE)
			continue;
		if (sysmon_pswitch_register(&sc->sc_smpsw[i]) != 0) {
			aprint_error_dev(self,
			    "couldn't register with sysmon\n");
			sc->sc_smpsw_valid = false;
			break;
		}
	}

	/* Register temperature and fan sensors with envsys */
	thinkpad_sensors_init(sc);

fail:
	if (!pmf_device_register(self, NULL, thinkpad_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
	if (!pmf_event_register(self, PMFE_DISPLAY_BRIGHTNESS_UP,
	    thinkpad_brightness_up, true))
		aprint_error_dev(self, "couldn't register event handler\n");
	if (!pmf_event_register(self, PMFE_DISPLAY_BRIGHTNESS_DOWN,
	    thinkpad_brightness_down, true))
		aprint_error_dev(self, "couldn't register event handler\n");
}

static int
thinkpad_detach(device_t self, int flags)
{
	struct thinkpad_softc *sc = device_private(self);
	int i;

	acpi_deregister_notify(sc->sc_node);

	for (i = 0; i < TP_PSW_LAST; i++)
		sysmon_pswitch_unregister(&sc->sc_smpsw[i]);

	if (sc->sc_sme != NULL)
		sysmon_envsys_unregister(sc->sc_sme);

	pmf_device_deregister(self);

	pmf_event_deregister(self, PMFE_DISPLAY_BRIGHTNESS_UP,
	    thinkpad_brightness_up, true);

	pmf_event_deregister(self, PMFE_DISPLAY_BRIGHTNESS_DOWN,
	    thinkpad_brightness_down, true);

	return 0;
}

static void
thinkpad_notify_handler(ACPI_HANDLE hdl, uint32_t notify, void *opaque)
{
	device_t self = opaque;
	thinkpad_softc_t *sc;

	sc = device_private(self);

	if (notify != 0x80) {
		aprint_debug_dev(self, "unknown notify 0x%02x\n", notify);
		return;
	}

	(void)AcpiOsExecute(OSL_NOTIFY_HANDLER, thinkpad_get_hotkeys, sc);
}

static void
thinkpad_get_hotkeys(void *opaque)
{
	thinkpad_softc_t *sc = (thinkpad_softc_t *)opaque;
	device_t self = sc->sc_dev;
	ACPI_STATUS rv;
	ACPI_INTEGER val;
	int type, event;

	for (;;) {
		rv = acpi_eval_integer(sc->sc_node->ad_handle, "MHKP", &val);
		if (ACPI_FAILURE(rv)) {
			aprint_error_dev(self, "couldn't evaluate MHKP: %s\n",
			    AcpiFormatException(rv));
			return;
		}

		if (val == 0)
			return;

		type = (val & 0xf000) >> 12;
		event = val & 0x0fff;

		if (type != 1)
			/* Only type 1 events are supported for now */
			continue;

		switch (event) {
		case THINKPAD_NOTIFY_BrightnessUp:
			thinkpad_brightness_up(self);
#ifndef THINKPAD_NORMAL_HOTKEYS
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(&sc->sc_smpsw[TP_PSW_BRIGHTNESS_UP],
			    PSWITCH_EVENT_PRESSED);
#endif
			break;
		case THINKPAD_NOTIFY_BrightnessDown:
			thinkpad_brightness_down(self);
#ifndef THINKPAD_NORMAL_HOTKEYS
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(&sc->sc_smpsw[TP_PSW_BRIGHTNESS_DOWN],
			    PSWITCH_EVENT_PRESSED);
#endif
			break;
		case THINKPAD_NOTIFY_WirelessSwitch:
			thinkpad_wireless_toggle(sc);
#ifndef THINKPAD_NORMAL_HOTKEYS
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(&sc->sc_smpsw[TP_PSW_WIRELESS_BUTTON],
			    PSWITCH_EVENT_PRESSED);
#endif
			break;
		case THINKPAD_NOTIFY_wWANSwitch:
			thinkpad_wwan_toggle(sc);
#ifndef THINKPAD_NORMAL_HOTKEYS
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(&sc->sc_smpsw[TP_PSW_WWAN_BUTTON],
			    PSWITCH_EVENT_PRESSED);
#endif
			break;
		case THINKPAD_NOTIFY_SleepButton:
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(&sc->sc_smpsw[TP_PSW_SLEEP],
			    PSWITCH_EVENT_PRESSED);
			break;
		case THINKPAD_NOTIFY_HibernateButton:
#if notyet
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(&sc->sc_smpsw[TP_PSW_HIBERNATE],
			    PSWITCH_EVENT_PRESSED);
#endif
			break;
		case THINKPAD_NOTIFY_DisplayCycle:
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(
			    &sc->sc_smpsw[TP_PSW_DISPLAY_CYCLE],
			    PSWITCH_EVENT_PRESSED);
			break;
		case THINKPAD_NOTIFY_LockScreen:
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(
			    &sc->sc_smpsw[TP_PSW_LOCK_SCREEN],
			    PSWITCH_EVENT_PRESSED);
			break;
		case THINKPAD_NOTIFY_BatteryInfo:
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(
			    &sc->sc_smpsw[TP_PSW_BATTERY_INFO],
			    PSWITCH_EVENT_PRESSED);
			break;
		case THINKPAD_NOTIFY_EjectButton:
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(
			    &sc->sc_smpsw[TP_PSW_EJECT_BUTTON],
			    PSWITCH_EVENT_PRESSED);
			break;
		case THINKPAD_NOTIFY_Zoom:
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(
			    &sc->sc_smpsw[TP_PSW_ZOOM_BUTTON],
			    PSWITCH_EVENT_PRESSED);
			break;
		case THINKPAD_NOTIFY_ThinkVantage:
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(
			    &sc->sc_smpsw[TP_PSW_VENDOR_BUTTON],
			    PSWITCH_EVENT_PRESSED);
			break;
#ifndef THINKPAD_NORMAL_HOTKEYS
		case THINKPAD_NOTIFY_FnF1:
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(&sc->sc_smpsw[TP_PSW_FNF1_BUTTON],
			    PSWITCH_EVENT_PRESSED);
			break;
		case THINKPAD_NOTIFY_PointerSwitch:
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(&sc->sc_smpsw[TP_PSW_POINTER_BUTTON],
			    PSWITCH_EVENT_PRESSED);
			break;
		case THINKPAD_NOTIFY_FnF11:
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(&sc->sc_smpsw[TP_PSW_FNF11_BUTTON],
			    PSWITCH_EVENT_PRESSED);
			break;
		case THINKPAD_NOTIFY_ThinkLight:
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(&sc->sc_smpsw[TP_PSW_THINKLIGHT],
			    PSWITCH_EVENT_PRESSED);
			break;
		/*
		 * For some reason the next four aren't seen on my T61.
		 */
		case THINKPAD_NOTIFY_FnF10:
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(&sc->sc_smpsw[TP_PSW_FNF10_BUTTON],
			    PSWITCH_EVENT_PRESSED);
			break;
		case THINKPAD_NOTIFY_VolumeUp:
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(&sc->sc_smpsw[TP_PSW_VOLUME_UP],
			    PSWITCH_EVENT_PRESSED);
			break;
		case THINKPAD_NOTIFY_VolumeDown:
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(&sc->sc_smpsw[TP_PSW_VOLUME_DOWN],
			    PSWITCH_EVENT_PRESSED);
			break;
		case THINKPAD_NOTIFY_VolumeMute:
			if (sc->sc_smpsw_valid == false)
				break;
			sysmon_pswitch_event(&sc->sc_smpsw[TP_PSW_VOLUME_MUTE],
			    PSWITCH_EVENT_PRESSED);
			break;
#else
		case THINKPAD_NOTIFY_FnF1:
		case THINKPAD_NOTIFY_PointerSwitch:
		case THINKPAD_NOTIFY_FnF10:
		case THINKPAD_NOTIFY_FnF11:
		case THINKPAD_NOTIFY_ThinkLight:
		case THINKPAD_NOTIFY_VolumeUp:
		case THINKPAD_NOTIFY_VolumeDown:
		case THINKPAD_NOTIFY_VolumeMute:
			/* XXXJDM we should deliver hotkeys as keycodes */
			break;
#endif /* THINKPAD_NORMAL_HOTKEYS */
		default:
			aprint_debug_dev(self, "notify event 0x%03x\n", event);
			break;
		}
	}
}

static ACPI_STATUS
thinkpad_mask_init(thinkpad_softc_t *sc, uint32_t mask)
{
	ACPI_OBJECT param[2];
	ACPI_OBJECT_LIST params;
	ACPI_STATUS rv;
	int i;

	/* Update hotkey mask */
	params.Count = 2;
	params.Pointer = param;
	param[0].Type = param[1].Type = ACPI_TYPE_INTEGER;

	for (i = 0; i < 32; i++) {
		param[0].Integer.Value = i + 1;
		param[1].Integer.Value = (((1 << i) & mask) != 0);

		rv = AcpiEvaluateObject(sc->sc_node->ad_handle, "MHKM",
		    &params, NULL);
		if (ACPI_FAILURE(rv))
			return rv;
	}

	/* Enable hotkey events */
	rv = acpi_eval_set_integer(sc->sc_node->ad_handle, "MHKC", 1);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(sc->sc_dev, "couldn't enable hotkeys: %s\n",
		    AcpiFormatException(rv));
		return rv;
	}

	/* Claim ownership of brightness control */
	(void)acpi_eval_set_integer(sc->sc_node->ad_handle, "PWMS", 0);

	return AE_OK;
}

static void
thinkpad_sensors_init(thinkpad_softc_t *sc)
{
	int i, j;

	if (sc->sc_ecdev == NULL)
		return;	/* no chance of this working */

	sc->sc_sme = sysmon_envsys_create();

	for (i = j = 0; i < THINKPAD_NTEMPSENSORS; i++) {

		sc->sc_sensor[i].units = ENVSYS_STEMP;
		sc->sc_sensor[i].state = ENVSYS_SINVALID;
		sc->sc_sensor[i].flags = ENVSYS_FHAS_ENTROPY;

		(void)snprintf(sc->sc_sensor[i].desc,
		    sizeof(sc->sc_sensor[i].desc), "temperature %d", i);

		if (sysmon_envsys_sensor_attach(sc->sc_sme,
			&sc->sc_sensor[i]) != 0)
			goto fail;
	}

	for (i = THINKPAD_NTEMPSENSORS; i < THINKPAD_NSENSORS; i++, j++) {

		sc->sc_sensor[i].units = ENVSYS_SFANRPM;
		sc->sc_sensor[i].state = ENVSYS_SINVALID;
		sc->sc_sensor[i].flags = ENVSYS_FHAS_ENTROPY;

		(void)snprintf(sc->sc_sensor[i].desc,
		    sizeof(sc->sc_sensor[i].desc), "fan speed %d", j);

		if (sysmon_envsys_sensor_attach(sc->sc_sme,
			&sc->sc_sensor[i]) != 0)
			goto fail;
	}

	sc->sc_sme->sme_name = device_xname(sc->sc_dev);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = thinkpad_sensors_refresh;

	if (sysmon_envsys_register(sc->sc_sme) != 0)
		goto fail;

	return;

fail:
	aprint_error_dev(sc->sc_dev, "failed to initialize sysmon\n");
	sysmon_envsys_destroy(sc->sc_sme);
	sc->sc_sme = NULL;
}

static void
thinkpad_sensors_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	switch (edata->units) {
	case ENVSYS_STEMP:
		thinkpad_temp_refresh(sme, edata);
		break;
	case ENVSYS_SFANRPM:
		thinkpad_fan_refresh(sme, edata);
		break;
	default:
		break;
	}
}

static void
thinkpad_temp_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	thinkpad_softc_t *sc = sme->sme_cookie;
	char sname[5] = "TMP?";
	ACPI_INTEGER val;
	ACPI_STATUS rv;
	int temp;

	sname[3] = '0' + edata->sensor;
	rv = acpi_eval_integer(acpiec_get_handle(sc->sc_ecdev), sname, &val);
	if (ACPI_FAILURE(rv)) {
		edata->state = ENVSYS_SINVALID;
		return;
	}
	temp = (int)val;
	if (temp > 127 || temp < -127) {
		edata->state = ENVSYS_SINVALID;
		return;
	}

	edata->value_cur = temp * 1000000 + 273150000;
	edata->state = ENVSYS_SVALID;
}

static void
thinkpad_fan_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	thinkpad_softc_t *sc = sme->sme_cookie;
	ACPI_INTEGER lo;
	ACPI_INTEGER hi;
	ACPI_STATUS rv;
	int rpm;

	/*
	 * Read the low byte first to avoid a firmware bug.
	 */
	rv = acpiec_bus_read(sc->sc_ecdev, 0x84, &lo, 1);
	if (ACPI_FAILURE(rv)) {
		edata->state = ENVSYS_SINVALID;
		return;
	}
	rv = acpiec_bus_read(sc->sc_ecdev, 0x85, &hi, 1);
	if (ACPI_FAILURE(rv)) {
		edata->state = ENVSYS_SINVALID;
		return;
	}
	rpm = ((((int)hi) << 8) | ((int)lo));
	if (rpm < 0) {
		edata->state = ENVSYS_SINVALID;
		return;
	}

	edata->value_cur = rpm;
	edata->state = ENVSYS_SVALID;
}

static void
thinkpad_wireless_toggle(thinkpad_softc_t *sc)
{
	/* Ignore return value, as the hardware may not support bluetooth */
	(void)AcpiEvaluateObject(sc->sc_node->ad_handle, "BTGL", NULL, NULL);
	(void)AcpiEvaluateObject(sc->sc_node->ad_handle, "GWAN", NULL, NULL);
}

static void
thinkpad_wwan_toggle(thinkpad_softc_t *sc)
{
	/* Ignore return value, as the hardware may not support wireless WAN */
	(void)AcpiEvaluateObject(sc->sc_node->ad_handle, "WTGL", NULL, NULL);
}

static uint8_t
thinkpad_brightness_read(thinkpad_softc_t *sc)
{
	uint32_t val = 0;

	AcpiOsWritePort(IO_RTC, 0x6c, 8);
	AcpiOsReadPort(IO_RTC + 1, &val, 8);

	return val & 7;
}

static void
thinkpad_brightness_up(device_t self)
{
	thinkpad_softc_t *sc = device_private(self);

	if (thinkpad_brightness_read(sc) == 7)
		return;

	thinkpad_cmos(sc, THINKPAD_CMOS_BRIGHTNESS_UP);
}

static void
thinkpad_brightness_down(device_t self)
{
	thinkpad_softc_t *sc = device_private(self);

	if (thinkpad_brightness_read(sc) == 0)
		return;

	thinkpad_cmos(sc, THINKPAD_CMOS_BRIGHTNESS_DOWN);
}

static void
thinkpad_cmos(thinkpad_softc_t *sc, uint8_t cmd)
{
	ACPI_STATUS rv;

	if (sc->sc_cmoshdl == NULL)
		return;

	rv = acpi_eval_set_integer(sc->sc_cmoshdl, NULL, cmd);

	if (ACPI_FAILURE(rv))
		aprint_error_dev(sc->sc_dev, "couldn't evaluate CMOS: %s\n",
		    AcpiFormatException(rv));
}

static bool
thinkpad_resume(device_t dv, const pmf_qual_t *qual)
{
	thinkpad_softc_t *sc = device_private(dv);

	if (sc->sc_powhdl == NULL)
		return true;

	(void)acpi_power_res(sc->sc_powhdl, sc->sc_node->ad_handle, true);

	return true;
}

MODULE(MODULE_CLASS_DRIVER, thinkpad, "sysmon_envsys,sysmon_power");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
thinkpad_modcmd(modcmd_t cmd, void *aux)
{
	int rv = 0;

	switch (cmd) {

	case MODULE_CMD_INIT:

#ifdef _MODULE
		rv = config_init_component(cfdriver_ioconf_thinkpad,
		    cfattach_ioconf_thinkpad, cfdata_ioconf_thinkpad);
#endif
		break;

	case MODULE_CMD_FINI:

#ifdef _MODULE
		rv = config_fini_component(cfdriver_ioconf_thinkpad,
		    cfattach_ioconf_thinkpad, cfdata_ioconf_thinkpad);
#endif
		break;

	default:
		rv = ENOTTY;
	}

	return rv;
}
