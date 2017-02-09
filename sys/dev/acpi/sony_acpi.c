/*	$NetBSD: sony_acpi.c,v 1.22 2014/02/25 18:30:09 pooka Exp $	*/

/*-
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
__KERNEL_RCSID(0, "$NetBSD: sony_acpi.c,v 1.22 2014/02/25 18:30:09 pooka Exp $");

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#define _COMPONENT          ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME            ("sony_acpi")

#define	SONY_NOTIFY_FnKeyEvent			0x92
#define	SONY_NOTIFY_BrightnessDownPressed	0x85
#define	SONY_NOTIFY_BrightnessDownReleased	0x05
#define	SONY_NOTIFY_BrightnessUpPressed		0x86
#define	SONY_NOTIFY_BrightnessUpReleased	0x06
#define	SONY_NOTIFY_DisplaySwitchPressed	0x87
#define	SONY_NOTIFY_DisplaySwitchReleased	0x07
#define	SONY_NOTIFY_ZoomPressed			0x8a
#define	SONY_NOTIFY_ZoomReleased		0x0a
#define	SONY_NOTIFY_SuspendPressed		0x8c
#define	SONY_NOTIFY_SuspendReleased		0x0c

struct sony_acpi_softc {
	device_t sc_dev;
	struct sysctllog *sc_log;
	struct acpi_devnode *sc_node;

#define	SONY_PSW_SLEEP		0
#define	SONY_PSW_DISPLAY_CYCLE	1
#define	SONY_PSW_ZOOM		2
#define	SONY_PSW_LAST		3
	struct sysmon_pswitch sc_smpsw[SONY_PSW_LAST];
	int sc_smpsw_valid;

#define	SONY_ACPI_QUIRK_FNINIT	0x01
	int sc_quirks;
	bool sc_has_pic;

	struct sony_acpi_pmstate {
		ACPI_INTEGER	brt;
	} sc_pmstate;
};

static const char * const sony_acpi_ids[] = {
	"SNY5001",
	NULL
};

static int	sony_acpi_match(device_t, cfdata_t, void *);
static void	sony_acpi_attach(device_t, device_t, void *);
static ACPI_STATUS sony_acpi_eval_set_integer(ACPI_HANDLE, const char *,
    ACPI_INTEGER, ACPI_INTEGER *);
static void	sony_acpi_quirk_setup(struct sony_acpi_softc *);
static void	sony_acpi_notify_handler(ACPI_HANDLE, uint32_t, void *);
static bool	sony_acpi_suspend(device_t, const pmf_qual_t *);
static bool	sony_acpi_resume(device_t, const pmf_qual_t *);
static void	sony_acpi_brightness_down(device_t);
static void	sony_acpi_brightness_up(device_t);
static ACPI_STATUS sony_acpi_find_pic(ACPI_HANDLE, uint32_t, void *, void **);

CFATTACH_DECL_NEW(sony_acpi, sizeof(struct sony_acpi_softc),
    sony_acpi_match, sony_acpi_attach, NULL, NULL);

static int
sony_acpi_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, sony_acpi_ids);
}

static int
sony_sysctl_helper(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	ACPI_INTEGER acpi_val;
	ACPI_STATUS rv;
	int val, old_val, error;
	char buf[SYSCTL_NAMELEN + 1], *ptr;
	struct sony_acpi_softc *sc = rnode->sysctl_data;

	(void)snprintf(buf, sizeof(buf), "G%s", rnode->sysctl_name);
	for (ptr = buf; *ptr; ptr++)
		*ptr = toupper((unsigned char)*ptr);

	rv = acpi_eval_integer(sc->sc_node->ad_handle, buf, &acpi_val);
	if (ACPI_FAILURE(rv)) {
#ifdef DIAGNOSTIC
		printf("%s: couldn't get `%s'\n", device_xname(sc->sc_dev), buf);
#endif
		return EIO;
	}
	val = old_val = acpi_val;

	node = *rnode;
	node.sysctl_data = &val;

	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	buf[0] = 'S';
	acpi_val = val;
	rv = sony_acpi_eval_set_integer(sc->sc_node->ad_handle, buf,
	    acpi_val, NULL);
	if (ACPI_FAILURE(rv)) {
#ifdef DIAGNOSTIC
		printf("%s: couldn't set `%s' to %d\n",
		    device_xname(sc->sc_dev), buf, val);
#endif
		return EIO;
	}
	return 0;
}

static ACPI_STATUS
sony_walk_cb(ACPI_HANDLE hnd, uint32_t v, void *context, void **status)
{
	struct sony_acpi_softc *sc = (void *)context;
	const struct sysctlnode *node, *snode;
	const char *name = acpi_name(hnd);
	ACPI_INTEGER acpi_val;
	char buf[SYSCTL_NAMELEN + 1], *ptr;
	int rv;

	if ((name = strrchr(name, '.')) == NULL)
		return AE_OK;

	name++;
	if ((*name != 'G') && (*name != 'S'))
		return AE_OK;

	(void)strlcpy(buf, name, sizeof(buf));
	*buf = 'G';

	/*
	 * We assume that if the 'get' of the name as an integer is
	 * successful it is ok.
	 */
	if (acpi_eval_integer(sc->sc_node->ad_handle, buf, &acpi_val))
		return AE_OK;

	for (ptr = buf; *ptr; ptr++)
		*ptr = tolower(*ptr);

	if ((rv = sysctl_createv(&sc->sc_log, 0, NULL, &snode, 0,
	    CTLTYPE_NODE, device_xname(sc->sc_dev),
	    SYSCTL_DESCR("sony controls"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
		goto out;

	if ((rv = sysctl_createv(&sc->sc_log, 0, &snode, &node,
	    CTLFLAG_READWRITE, CTLTYPE_INT, buf + 1, NULL,
	    sony_sysctl_helper, 0, (void *)sc, 0, CTL_CREATE, CTL_EOL)) != 0)
		goto out;

out:
#ifdef DIAGNOSTIC
	if (rv)
		printf("%s: sysctl_createv failed (rv = %d)\n",
		    device_xname(sc->sc_dev), rv);
#endif
	return AE_OK;
}

ACPI_STATUS
sony_acpi_eval_set_integer(ACPI_HANDLE handle, const char *path,
    ACPI_INTEGER val, ACPI_INTEGER *valp)
{
	ACPI_STATUS rv;
	ACPI_BUFFER buf;
	ACPI_OBJECT param, ret_val;
	ACPI_OBJECT_LIST params;

	if (handle == NULL)
		handle = ACPI_ROOT_OBJECT;

	params.Count = 1;
	params.Pointer = &param;

	param.Type = ACPI_TYPE_INTEGER;
	param.Integer.Value = val;

	buf.Pointer = &ret_val;
	buf.Length = sizeof(ret_val);

	rv = AcpiEvaluateObjectTyped(handle, path, &params, &buf,
	    ACPI_TYPE_INTEGER);

	if (ACPI_SUCCESS(rv) && valp)
		*valp = ret_val.Integer.Value;

	return rv;
}

static void
sony_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct sony_acpi_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	ACPI_STATUS rv;
	int i;

	aprint_naive(": Sony Miscellaneous Controller\n");
	aprint_normal(": Sony Miscellaneous Controller\n");

	sc->sc_node = aa->aa_node;
	sc->sc_dev = self;

	rv = AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT, 100,
	    sony_acpi_find_pic, NULL, sc, NULL);
	if (ACPI_FAILURE(rv))
		aprint_error_dev(self, "couldn't walk namespace: %s\n",
		    AcpiFormatException(rv));

	/*
	 * If we don't find an SNY6001 device, assume that we need the
	 * Fn key initialization sequence.
	 */
	if (sc->sc_has_pic == false)
		sc->sc_quirks |= SONY_ACPI_QUIRK_FNINIT;

	sony_acpi_quirk_setup(sc);

	/* Configure suspend button and hotkeys */
	sc->sc_smpsw[SONY_PSW_SLEEP].smpsw_name = device_xname(self);
	sc->sc_smpsw[SONY_PSW_SLEEP].smpsw_type = PSWITCH_TYPE_SLEEP;
	sc->sc_smpsw[SONY_PSW_DISPLAY_CYCLE].smpsw_name =
	    PSWITCH_HK_DISPLAY_CYCLE;
	sc->sc_smpsw[SONY_PSW_DISPLAY_CYCLE].smpsw_type = PSWITCH_TYPE_HOTKEY;
	sc->sc_smpsw[SONY_PSW_ZOOM].smpsw_name = PSWITCH_HK_ZOOM_BUTTON;
	sc->sc_smpsw[SONY_PSW_ZOOM].smpsw_type = PSWITCH_TYPE_HOTKEY;
	sc->sc_smpsw_valid = 1;

	for (i = 0; i < SONY_PSW_LAST; i++)
		if (sysmon_pswitch_register(&sc->sc_smpsw[i]) != 0) {
			aprint_error_dev(self, 
			    "couldn't register %s with sysmon\n",
			    sc->sc_smpsw[i].smpsw_name);
			sc->sc_smpsw_valid = 0;
		}

	(void)acpi_register_notify(sc->sc_node, sony_acpi_notify_handler);

	/* Install sysctl handler */
	rv = AcpiWalkNamespace(ACPI_TYPE_METHOD,
	    sc->sc_node->ad_handle, 1, sony_walk_cb, NULL, sc, NULL);

#ifdef DIAGNOSTIC
	if (ACPI_FAILURE(rv))
		aprint_error_dev(self, "Cannot walk ACPI namespace (%u)\n",
		    rv);
#endif

	if (!pmf_device_register(self, sony_acpi_suspend, sony_acpi_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	if (!pmf_event_register(self, PMFE_DISPLAY_BRIGHTNESS_UP,
				 sony_acpi_brightness_up, true))
		aprint_error_dev(self, "couldn't register BRIGHTNESS UP handler\n");

	if (!pmf_event_register(self, PMFE_DISPLAY_BRIGHTNESS_DOWN,
				 sony_acpi_brightness_down, true))
		aprint_error_dev(self, "couldn't register BRIGHTNESS DOWN handler\n");
}

static void
sony_acpi_quirk_setup(struct sony_acpi_softc *sc)
{
	ACPI_HANDLE hdl = sc->sc_node->ad_handle;

	if (sc->sc_quirks & SONY_ACPI_QUIRK_FNINIT) {
		/* Initialize extra Fn keys */
		sony_acpi_eval_set_integer(hdl, "SN02", 0x04, NULL);
		sony_acpi_eval_set_integer(hdl, "SN07", 0x02, NULL);
		sony_acpi_eval_set_integer(hdl, "SN02", 0x10, NULL);
		sony_acpi_eval_set_integer(hdl, "SN07", 0x00, NULL);
		sony_acpi_eval_set_integer(hdl, "SN03", 0x02, NULL);
		sony_acpi_eval_set_integer(hdl, "SN07", 0x101, NULL);
	}
}

static void
sony_acpi_notify_handler(ACPI_HANDLE hdl, uint32_t notify, void *opaque)
{
	device_t dv = opaque;
	struct sony_acpi_softc *sc = device_private(dv);
	ACPI_STATUS rv;
	ACPI_INTEGER arg;

	if (notify == SONY_NOTIFY_FnKeyEvent) {
		rv = sony_acpi_eval_set_integer(hdl, "SN07", 0x202, &arg);
		if (ACPI_FAILURE(rv))
			return;

		notify = arg & 0xff;
	}

	switch (notify) {
	case SONY_NOTIFY_BrightnessDownPressed:
		sony_acpi_brightness_down(dv);
		break;
	case SONY_NOTIFY_BrightnessUpPressed:
		sony_acpi_brightness_up(dv);
		break;
	case SONY_NOTIFY_BrightnessDownReleased:
	case SONY_NOTIFY_BrightnessUpReleased:
		break;
	case SONY_NOTIFY_SuspendPressed:
		if (!sc->sc_smpsw_valid)
			break;
		sysmon_pswitch_event(&sc->sc_smpsw[SONY_PSW_SLEEP],
		    PSWITCH_EVENT_PRESSED);
		break;
	case SONY_NOTIFY_SuspendReleased:
		break;
	case SONY_NOTIFY_DisplaySwitchPressed:
		if (!sc->sc_smpsw_valid)
			break;
		sysmon_pswitch_event(&sc->sc_smpsw[SONY_PSW_DISPLAY_CYCLE],
		    PSWITCH_EVENT_PRESSED);
		break;
	case SONY_NOTIFY_DisplaySwitchReleased:
		break;
	case SONY_NOTIFY_ZoomPressed:
		if (!sc->sc_smpsw_valid)
			break;
		sysmon_pswitch_event(&sc->sc_smpsw[SONY_PSW_ZOOM],
		    PSWITCH_EVENT_PRESSED);
		break;
	case SONY_NOTIFY_ZoomReleased:
		break;
	default:
		aprint_debug_dev(dv, "unknown notify event 0x%x\n", notify);
		break;
	}
}

static bool
sony_acpi_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct sony_acpi_softc *sc = device_private(dv);

	acpi_eval_integer(sc->sc_node->ad_handle, "GBRT", &sc->sc_pmstate.brt);

	return true;
}

static bool
sony_acpi_resume(device_t dv, const pmf_qual_t *qual)
{
	struct sony_acpi_softc *sc = device_private(dv);

	sony_acpi_eval_set_integer(sc->sc_node->ad_handle, "SBRT",
	    sc->sc_pmstate.brt, NULL);
	sony_acpi_quirk_setup(sc);

	return true;
}

static void
sony_acpi_brightness_up(device_t dv)
{
	struct sony_acpi_softc *sc = device_private(dv);
	ACPI_INTEGER arg;
	ACPI_STATUS rv;

	rv = acpi_eval_integer(sc->sc_node->ad_handle, "GBRT", &arg);
	if (ACPI_FAILURE(rv))
		return;
	if (arg >= 8)
		arg = 8;
	else
		arg++;
	sony_acpi_eval_set_integer(sc->sc_node->ad_handle, "SBRT", arg, NULL);
}

static void
sony_acpi_brightness_down(device_t dv)
{
	struct sony_acpi_softc *sc = device_private(dv);
	ACPI_INTEGER arg;
	ACPI_STATUS rv;

	rv = acpi_eval_integer(sc->sc_node->ad_handle, "GBRT", &arg);
	if (ACPI_FAILURE(rv))
		return;
	if (arg <= 0)
		arg = 0;
	else
		arg--;
	sony_acpi_eval_set_integer(sc->sc_node->ad_handle, "SBRT", arg, NULL);
}

static ACPI_STATUS
sony_acpi_find_pic(ACPI_HANDLE hdl, uint32_t level,
    void *opaque, void **status)
{
	struct sony_acpi_softc *sc = opaque;
	ACPI_STATUS rv;
	ACPI_DEVICE_INFO *devinfo;

	rv = AcpiGetObjectInfo(hdl, &devinfo);
	if (ACPI_FAILURE(rv) || devinfo == NULL)
		return AE_OK;	/* we don't want to stop searching */

	if ((devinfo->Valid & ACPI_VALID_HID) != 0 &&
	    devinfo->HardwareId.String &&
	    strncmp(devinfo->HardwareId.String, "SNY6001", 7) == 0)
		sc->sc_has_pic = true;

	ACPI_FREE(devinfo);

	return AE_OK;
}
