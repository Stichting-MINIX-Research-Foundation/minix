/*	$NetBSD: fujbp_acpi.c,v 1.4 2014/02/25 18:30:09 pooka Exp $ */

/*-
 * Copyright (c) 2010, 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gregoire Sutre.
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

/*
 * ACPI Fujitsu Driver.
 *
 * Together with fujhk(4), this driver provides support for the ACPI devices
 * FUJ02B1 and FUJ02E3 that are commonly found in Fujitsu LifeBooks. The
 * driver does not support all features of these devices, in particular
 * volume control is not implemented.
 *
 * Information regarding the behavior of these devices was obtained from the
 * source code of the Linux and FreeBSD drivers, as well as from experiments on
 * a Fujitsu LifeBook P7120.
 *
 * The FUJ02B1 device is used to control the brightness level of the internal
 * display, the state (on/off) of the internal pointer, and the volume level of
 * the internal speakers or headphones.
 *
 * The FUJ02B1 device provides the following methods (or only a subset):
 *
 *	GSIF		supported hotkey status bits (bitmask for GHKS)
 *	GHKS		active hotkeys (bit field)
 *	{G,S}BLL	get/set the brightness level of the internal display
 *	{G,S}VOL	get/set the volume level of the internal speakers
 *	{G,S}MOU	get/set the switch state of the internal pointer
 *	RBLL		brightness radix (number of brightness levels)
 *	RVOL		volume radix (number of volume levels)
 *
 * Notifications are delivered to the FUJ02B1 device when functions hotkeys
 * (brightness, pointer) are released.  However, these notifications seem to be
 * purely informative: the BIOS already made the hardware changes corresponding
 * to the hotkey.
 *
 * Each bit in the value returned by GHKS remains set until the corresponding
 * get method (GBLL, GMOU or GVOL) is called.
 *
 * The FUJ02E3 device manages the laptop hotkeys (such as the `Eco' button) and
 * provides additional services (such as backlight on/off control) through the
 * FUNC method.
 *
 * The FUJ02E3 device provides the following methods (or only a subset):
 *
 *	GIRB		get next hotkey code from buffer
 *	FUNC		general-purpose method (four arguments)
 *
 * Notifications are delivered to the FUJ02E3 device when hotkeys are pressed
 * and when they are released.  The BIOS stores the corresponding codes in a
 * FIFO buffer, that can be read with the GIRB method.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fujbp_acpi.c,v 1.4 2014/02/25 18:30:09 pooka Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#define _COMPONENT			ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME			("fujbp_acpi")

/*
 * Notification value, bits returned by the GHKS method, and
 * modification status bits (from GBLL, GMOU, GIRB), respectively.
 */
#define FUJITSU_BP_NOTIFY		0x80
#define FUJITSU_BP_HKS_BRIGHTNESS	__BIT(0)
#define FUJITSU_BP_HKS_POINTER		__BIT(3)
#define FUJITSU_BP_MODMASK		0xc0000000

/*
 * ACPI Fujitsu brightness & pointer controller capabilities (methods).
 */
#define FUJITSU_BP_CAP_GHKS		__BIT(0)
#define FUJITSU_BP_CAP_RBLL		__BIT(1)
#define FUJITSU_BP_CAP_GBLL		__BIT(2)
#define FUJITSU_BP_CAP_SBLL		__BIT(3)
#define FUJITSU_BP_CAP_GMOU		__BIT(4)
#define FUJITSU_BP_CAP_SMOU		__BIT(5)

/*
 * fujitsu_bp_softc:
 *
 *	Software state of an ACPI Fujitsu brightness & pointer controller.
 *	Valid brightness levels range from 0 to (sc_brightness_nlevels - 1).
 */
struct fujitsu_bp_softc {
	device_t		 sc_dev;
	struct acpi_devnode	*sc_node;
	struct sysctllog	*sc_log;
	kmutex_t		 sc_mtx;
	uint16_t		 sc_caps;
	uint8_t			 sc_brightness_nlevels;
};

static const char * const fujitsu_bp_hid[] = {
	"FUJ02B1",
	NULL
};

static int	fujitsu_bp_match(device_t, cfdata_t, void *);
static void	fujitsu_bp_attach(device_t, device_t, void *);
static int	fujitsu_bp_detach(device_t, int);
static bool	fujitsu_bp_suspend(device_t, const pmf_qual_t *);
static bool	fujitsu_bp_resume(device_t, const pmf_qual_t *);
static void	fujitsu_bp_brightness_up(device_t);
static void	fujitsu_bp_brightness_down(device_t);
static uint16_t	fujitsu_bp_capabilities(const struct acpi_devnode *);
static void	fujitsu_bp_notify_handler(ACPI_HANDLE, uint32_t, void *);
static void	fujitsu_bp_event_callback(void *);
static void	fujitsu_bp_sysctl_setup(struct fujitsu_bp_softc *);
static int	fujitsu_bp_sysctl_brightness(SYSCTLFN_PROTO);
static int	fujitsu_bp_sysctl_pointer(SYSCTLFN_PROTO);
static int	fujitsu_bp_get_hks(struct fujitsu_bp_softc *, uint32_t *);
static int	fujitsu_bp_init_brightness(struct fujitsu_bp_softc *,uint8_t*);
static int	fujitsu_bp_get_brightness(struct fujitsu_bp_softc *,uint8_t *);
static int	fujitsu_bp_set_brightness(struct fujitsu_bp_softc *, uint8_t);
static int	fujitsu_bp_get_pointer(struct fujitsu_bp_softc *, bool *);
static int	fujitsu_bp_set_pointer(struct fujitsu_bp_softc *, bool);
static bool	fujitsu_bp_cap(ACPI_HANDLE, const char *, ACPI_OBJECT_TYPE);

CFATTACH_DECL_NEW(fujbp, sizeof(struct fujitsu_bp_softc),
    fujitsu_bp_match, fujitsu_bp_attach, fujitsu_bp_detach, NULL);

static int
fujitsu_bp_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, fujitsu_bp_hid);
}

static void
fujitsu_bp_attach(device_t parent, device_t self, void *aux)
{
	struct fujitsu_bp_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	struct acpi_devnode *ad = aa->aa_node;

	aprint_naive(": Fujitsu Brightness & Pointer\n");
	aprint_normal(": Fujitsu Brightness & Pointer\n");

	sc->sc_dev = self;
	sc->sc_node = ad;
	sc->sc_log = NULL;
	sc->sc_caps = fujitsu_bp_capabilities(ad);

	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_NONE);

	if (fujitsu_bp_init_brightness(sc, &sc->sc_brightness_nlevels))
		sc->sc_brightness_nlevels = 0;

	(void)acpi_register_notify(sc->sc_node, fujitsu_bp_notify_handler);
	(void)pmf_device_register(self, fujitsu_bp_suspend, fujitsu_bp_resume);

	fujitsu_bp_sysctl_setup(sc);

	(void)pmf_event_register(self, PMFE_DISPLAY_BRIGHTNESS_UP,
	    fujitsu_bp_brightness_up, true);

	(void)pmf_event_register(self, PMFE_DISPLAY_BRIGHTNESS_DOWN,
	    fujitsu_bp_brightness_down, true);
}

static int
fujitsu_bp_detach(device_t self, int flags)
{
	struct fujitsu_bp_softc *sc = device_private(self);

	pmf_event_deregister(self, PMFE_DISPLAY_BRIGHTNESS_DOWN,
	    fujitsu_bp_brightness_down, true);

	pmf_event_deregister(self, PMFE_DISPLAY_BRIGHTNESS_UP,
	    fujitsu_bp_brightness_up, true);

	pmf_device_deregister(self);

	if (sc->sc_log != NULL)
		sysctl_teardown(&sc->sc_log);

	acpi_deregister_notify(sc->sc_node);
	mutex_destroy(&sc->sc_mtx);

	return 0;
}

/*
 * On some LifeBook models, a call to the SMOU method is required to make the
 * internal pointer work after resume.  On the P7120, the internal pointer is
 * always enabled after resume.  If it was disabled before suspend, the BIOS
 * apparently believes that it is still disabled after resume.
 *
 * To prevent these problems, we disable the internal pointer on suspend and
 * enable it on resume.
 */
static bool
fujitsu_bp_suspend(device_t self, const pmf_qual_t *qual)
{
	struct fujitsu_bp_softc *sc = device_private(self);

	mutex_enter(&sc->sc_mtx);
	(void)fujitsu_bp_set_pointer(sc, false);
	mutex_exit(&sc->sc_mtx);

	return true;
}

static bool
fujitsu_bp_resume(device_t self, const pmf_qual_t *qual)
{
	struct fujitsu_bp_softc *sc = device_private(self);

	mutex_enter(&sc->sc_mtx);
	(void)fujitsu_bp_set_pointer(sc, true);
	mutex_exit(&sc->sc_mtx);

	return true;
}

static void
fujitsu_bp_brightness_up(device_t self)
{
	struct fujitsu_bp_softc *sc = device_private(self);
	uint8_t level;

	mutex_enter(&sc->sc_mtx);

	if (fujitsu_bp_get_brightness(sc, &level) == 0 &&
	    level < (uint8_t)(sc->sc_brightness_nlevels - 1))
		(void)fujitsu_bp_set_brightness(sc, level + 1);

	mutex_exit(&sc->sc_mtx);
}

static void
fujitsu_bp_brightness_down(device_t self)
{
	struct fujitsu_bp_softc *sc = device_private(self);
	uint8_t level;

	mutex_enter(&sc->sc_mtx);

	if (fujitsu_bp_get_brightness(sc, &level) == 0 && level > 0)
		(void)fujitsu_bp_set_brightness(sc, level - 1);

	mutex_exit(&sc->sc_mtx);
}

static uint16_t
fujitsu_bp_capabilities(const struct acpi_devnode *ad)
{
	uint16_t caps;

	caps = 0;

	if (fujitsu_bp_cap(ad->ad_handle, "GHKS", ACPI_TYPE_INTEGER))
		caps |= FUJITSU_BP_CAP_GHKS;

	if (fujitsu_bp_cap(ad->ad_handle, "RBLL", ACPI_TYPE_INTEGER))
		caps |= FUJITSU_BP_CAP_RBLL;

	if (fujitsu_bp_cap(ad->ad_handle, "GBLL", ACPI_TYPE_INTEGER))
		caps |= FUJITSU_BP_CAP_GBLL;

	if (fujitsu_bp_cap(ad->ad_handle, "SBLL", ACPI_TYPE_METHOD))
		caps |= FUJITSU_BP_CAP_SBLL;

	if (fujitsu_bp_cap(ad->ad_handle, "GMOU", ACPI_TYPE_INTEGER))
		caps |= FUJITSU_BP_CAP_GMOU;

	if (fujitsu_bp_cap(ad->ad_handle, "SMOU", ACPI_TYPE_METHOD))
		caps |= FUJITSU_BP_CAP_SMOU;

	return caps;
}

static void
fujitsu_bp_notify_handler(ACPI_HANDLE handle, uint32_t evt, void *context)
{
	struct fujitsu_bp_softc *sc = device_private(context);
	static const int handler = OSL_NOTIFY_HANDLER;

	switch (evt) {

	case FUJITSU_BP_NOTIFY:
		(void)AcpiOsExecute(handler, fujitsu_bp_event_callback, sc);
		break;

	default:
		aprint_debug_dev(sc->sc_dev, "unknown notify 0x%02X\n", evt);
	}
}

static void
fujitsu_bp_event_callback(void *arg)
{
	struct fujitsu_bp_softc *sc = arg;
	int error;
	uint32_t hks;
	uint8_t level;
	bool state;

	if (fujitsu_bp_get_hks(sc, &hks))
		return;

	if (hks & FUJITSU_BP_HKS_BRIGHTNESS) {
		mutex_enter(&sc->sc_mtx);
		error = fujitsu_bp_get_brightness(sc, &level);
		mutex_exit(&sc->sc_mtx);
		if (!error)
			aprint_verbose_dev(sc->sc_dev,
			    "brightness level is now: %"PRIu8"\n", level);
	}

	if (hks & FUJITSU_BP_HKS_POINTER) {
		mutex_enter(&sc->sc_mtx);
		error = fujitsu_bp_get_pointer(sc, &state);
		mutex_exit(&sc->sc_mtx);
		if (!error)
			aprint_verbose_dev(sc->sc_dev,
			    "internal pointer is now: %s\n",
			    state ? "enabled" : "disabled");
	}
}

static void
fujitsu_bp_sysctl_setup(struct fujitsu_bp_softc *sc)
{
	const struct sysctlnode *rnode;
	int access;
	uint8_t dummy_level;
	bool dummy_state;
	bool brightness, pointer;

	brightness = (fujitsu_bp_get_brightness(sc, &dummy_level) == 0);
	pointer = (fujitsu_bp_get_pointer(sc, &dummy_state) == 0);

	if (brightness || pointer) {
		if ((sysctl_createv(&sc->sc_log, 0, NULL, &rnode,
		    0, CTLTYPE_NODE, "acpi", NULL,
		    NULL, 0, NULL, 0,
		    CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
			goto fail;

		if ((sysctl_createv(&sc->sc_log, 0, &rnode, &rnode,
		    0, CTLTYPE_NODE, device_xname(sc->sc_dev),
		    SYSCTL_DESCR("Fujitsu brightness & pointer controls"),
		    NULL, 0, NULL, 0,
		    CTL_CREATE, CTL_EOL)) != 0)
			goto fail;
	}

	if (brightness) {
		if (sc->sc_caps & FUJITSU_BP_CAP_SBLL)
			access = CTLFLAG_READWRITE;
		else
			access = CTLFLAG_READONLY;

		(void)sysctl_createv(&sc->sc_log, 0, &rnode, NULL,
		    access, CTLTYPE_INT, "brightness",
		    SYSCTL_DESCR("Internal DFP brightness level"),
		    fujitsu_bp_sysctl_brightness, 0, (void *)sc, 0,
		    CTL_CREATE, CTL_EOL);
	}

	if (pointer) {
		if (sc->sc_caps & FUJITSU_BP_CAP_SMOU)
			access = CTLFLAG_READWRITE;
		else
			access = CTLFLAG_READONLY;

		(void)sysctl_createv(&sc->sc_log, 0, &rnode, NULL,
		    access, CTLTYPE_BOOL, "pointer",
		    SYSCTL_DESCR("Internal pointer switch state"),
		    fujitsu_bp_sysctl_pointer, 0, (void *)sc, 0,
		    CTL_CREATE, CTL_EOL);
	}

	return;

 fail:
	aprint_error_dev(sc->sc_dev, "couldn't add sysctl nodes\n");
}

static int
fujitsu_bp_sysctl_brightness(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct fujitsu_bp_softc *sc;
	int val, error;
	uint8_t level;

	node = *rnode;
	sc = node.sysctl_data;

	mutex_enter(&sc->sc_mtx);
	error = fujitsu_bp_get_brightness(sc, &level);
	mutex_exit(&sc->sc_mtx);

	if (error)
		return error;

	val = (int)level;
	node.sysctl_data = &val;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (val < 0 || val > (uint8_t)(sc->sc_brightness_nlevels - 1))
		return EINVAL;

	mutex_enter(&sc->sc_mtx);
	error = fujitsu_bp_set_brightness(sc, (uint8_t)val);
	mutex_exit(&sc->sc_mtx);

	return error;
}

static int
fujitsu_bp_sysctl_pointer(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct fujitsu_bp_softc *sc;
	bool val;
	int error;

	node = *rnode;
	sc = node.sysctl_data;

	mutex_enter(&sc->sc_mtx);
	error = fujitsu_bp_get_pointer(sc, &val);
	mutex_exit(&sc->sc_mtx);

	if (error)
		return error;

	node.sysctl_data = &val;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	mutex_enter(&sc->sc_mtx);
	error = fujitsu_bp_set_pointer(sc, val);
	mutex_exit(&sc->sc_mtx);

	return error;
}

static int
fujitsu_bp_get_hks(struct fujitsu_bp_softc *sc, uint32_t *valuep)
{
	ACPI_HANDLE hdl = sc->sc_node->ad_handle;
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	if (!(sc->sc_caps & FUJITSU_BP_CAP_GHKS))
		return ENODEV;

	rv = acpi_eval_integer(hdl, "GHKS", &val);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(sc->sc_dev, "failed to evaluate %s.%s: %s\n",
		    acpi_name(hdl), "GHKS", AcpiFormatException(rv));
		return EIO;
	}

	*valuep = (uint32_t)val;

	return 0;
}

static int
fujitsu_bp_init_brightness(struct fujitsu_bp_softc *sc, uint8_t *valuep)
{
	ACPI_HANDLE hdl = sc->sc_node->ad_handle;
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	if (!(sc->sc_caps & FUJITSU_BP_CAP_RBLL))
		return ENODEV;

	rv = acpi_eval_integer(hdl, "RBLL", &val);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(sc->sc_dev, "failed to evaluate %s.%s: %s\n",
		    acpi_name(hdl), "RBLL", AcpiFormatException(rv));
		return EIO;
	}

	if (val > UINT8_MAX)
		return ERANGE;

	*valuep = (uint8_t)val;

	return 0;
}

static int
fujitsu_bp_get_brightness(struct fujitsu_bp_softc *sc, uint8_t *valuep)
{
	ACPI_HANDLE hdl = sc->sc_node->ad_handle;
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	if (!(sc->sc_caps & FUJITSU_BP_CAP_GBLL))
		return ENODEV;

	rv = acpi_eval_integer(hdl, "GBLL", &val);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(sc->sc_dev, "failed to evaluate %s.%s: %s\n",
		    acpi_name(hdl), "GBLL", AcpiFormatException(rv));
		return EIO;
	}

	/* Clear modification bits. */
	val &= ~FUJITSU_BP_MODMASK;

	if (val > UINT8_MAX)
		return ERANGE;

	*valuep = (uint8_t)val;

	return 0;
}

static int
fujitsu_bp_set_brightness(struct fujitsu_bp_softc *sc, uint8_t val)
{
	ACPI_HANDLE hdl = sc->sc_node->ad_handle;
	ACPI_STATUS rv;

	if (!(sc->sc_caps & FUJITSU_BP_CAP_SBLL))
		return ENODEV;

	rv = acpi_eval_set_integer(hdl, "SBLL", val);

	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(sc->sc_dev, "failed to evaluate %s.%s: %s\n",
		    acpi_name(hdl), "SBLL", AcpiFormatException(rv));
		return EIO;
	}

	return 0;
}

static int
fujitsu_bp_get_pointer(struct fujitsu_bp_softc *sc, bool *valuep)
{
	ACPI_HANDLE hdl = sc->sc_node->ad_handle;
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	if (!(sc->sc_caps & FUJITSU_BP_CAP_GMOU))
		return ENODEV;

	rv = acpi_eval_integer(hdl, "GMOU", &val);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(sc->sc_dev, "failed to evaluate %s.%s: %s\n",
		    acpi_name(hdl), "GMOU", AcpiFormatException(rv));
		return EIO;
	}

	/* Clear modification bits. */
	val &= ~FUJITSU_BP_MODMASK;

	if (val > 1)
		return ERANGE;

	*valuep = (bool)val;

	return 0;
}

static int
fujitsu_bp_set_pointer(struct fujitsu_bp_softc *sc, bool val)
{
	ACPI_HANDLE hdl = sc->sc_node->ad_handle;
	ACPI_STATUS rv;

	if (!(sc->sc_caps & FUJITSU_BP_CAP_SMOU))
		return ENODEV;

	rv = acpi_eval_set_integer(hdl, "SMOU", val);

	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(sc->sc_dev, "failed to evaluate %s.%s: %s\n",
		    acpi_name(hdl), "SMOU", AcpiFormatException(rv));
		return EIO;
	}

	return 0;
}

/*
 * fujitusu_bp_cap:
 *
 *	Returns true if and only if (a) the object handle.path exists and
 *	(b) this object is a method or has the given type.
 */
static bool
fujitsu_bp_cap(ACPI_HANDLE handle, const char *path, ACPI_OBJECT_TYPE type)
{
	ACPI_HANDLE hdl;
	ACPI_OBJECT_TYPE typ;

	KASSERT(handle != NULL);

	if (ACPI_FAILURE(AcpiGetHandle(handle, path, &hdl)))
		return false;

	if (ACPI_FAILURE(AcpiGetType(hdl, &typ)))
		return false;

	if (typ != ACPI_TYPE_METHOD && typ != type)
		return false;

	return true;
}

MODULE(MODULE_CLASS_DRIVER, fujbp, NULL);

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
fujbp_modcmd(modcmd_t cmd, void *aux)
{
	int rv = 0;

	switch (cmd) {

	case MODULE_CMD_INIT:

#ifdef _MODULE
		rv = config_init_component(cfdriver_ioconf_fujbp,
		    cfattach_ioconf_fujbp, cfdata_ioconf_fujbp);
#endif
		break;

	case MODULE_CMD_FINI:

#ifdef _MODULE
		rv = config_fini_component(cfdriver_ioconf_fujbp,
		    cfattach_ioconf_fujbp, cfdata_ioconf_fujbp);
#endif
		break;

	default:
		rv = ENOTTY;
	}

	return rv;
}
