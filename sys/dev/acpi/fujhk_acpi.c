/*	$NetBSD: fujhk_acpi.c,v 1.4 2015/04/23 23:23:00 pgoyette Exp $ */

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fujhk_acpi.c,v 1.4 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <dev/sysmon/sysmonvar.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#define _COMPONENT			ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME			("fujhk_acpi")

#define FUJITSU_HK_NOTIFY		0x80
#define FUJITSU_HK_MODMASK		0xc0000000

/* Values returned by the GIRB method. */
#define FUJITSU_HK_IRB_HOTKEY_RELEASE	0x0
#define FUJITSU_HK_IRB_HOTKEY_PRESS(x)	(0x410 | x)

/* Hotkey index of a value returned by the GIRB method. */
#define FUJITSU_HK_IRB_HOTKEY_INDEX(x)	(x & 0x3)

/* Values for the first argument of the FUNC method. */
#define FUJITSU_FUNC_TARGET_BACKLIGHT	0x1004

/* Values for the second argument of the FUNC method. */
#define FUJITSU_FUNC_COMMAND_SET	0x1
#define FUJITSU_FUNC_COMMAND_GET	0x2

/* Backlight values for the FUNC method. */
#define FUJITSU_FUNC_BACKLIGHT_ON	0x0
#define FUJITSU_FUNC_BACKLIGHT_REDUCED	0x2
#define FUJITSU_FUNC_BACKLIGHT_OFF	0x3

/* Value returned by FUNC on invalid arguments. */
#define FUJITSU_FUNC_INVALID_ARGS	0x80000000

/* ACPI Fujitsu hotkeys controller capabilities (methods). */
#define FUJITSU_HK_CAP_GIRB	__BIT(0)
#define FUJITSU_HK_CAP_FUNC	__BIT(1)

struct fujitsu_hk_softc {
	device_t		 sc_dev;
	struct acpi_devnode	*sc_node;
	struct sysctllog	*sc_log;
	kmutex_t		 sc_mtx;
	uint16_t		 sc_caps;
#define	FUJITSU_HK_PSW_COUNT	4
	struct sysmon_pswitch	 sc_smpsw[FUJITSU_HK_PSW_COUNT];
	char			 sc_smpsw_name[FUJITSU_HK_PSW_COUNT][16];
};

static const char * const fujitsu_hk_hid[] = {
	"FUJ02E3",
	NULL
};

static int	fujitsu_hk_match(device_t, cfdata_t, void *);
static void	fujitsu_hk_attach(device_t, device_t, void *);
static int	fujitsu_hk_detach(device_t, int);
static bool	fujitsu_hk_suspend(device_t, const pmf_qual_t *);
static bool	fujitsu_hk_resume(device_t, const pmf_qual_t *);
static uint16_t	fujitsu_hk_capabilities(const struct acpi_devnode *);
static void	fujitsu_hk_notify_handler(ACPI_HANDLE, uint32_t, void *);
static void	fujitsu_hk_event_callback(void *);
static void	fujitsu_hk_sysctl_setup(struct fujitsu_hk_softc *);
static int	fujitsu_hk_sysctl_backlight(SYSCTLFN_PROTO);
static int	fujitsu_hk_get_irb(struct fujitsu_hk_softc *, uint32_t *);
static int	fujitsu_hk_get_backlight(struct fujitsu_hk_softc *, bool *);
static int	fujitsu_hk_set_backlight(struct fujitsu_hk_softc *, bool);
static bool	fujitsu_hk_cap(ACPI_HANDLE, const char *, ACPI_OBJECT_TYPE);
static ACPI_STATUS fujitsu_hk_eval_nary_integer(ACPI_HANDLE,
				const char *, const
				ACPI_INTEGER *, uint8_t, ACPI_INTEGER *);

CFATTACH_DECL_NEW(fujhk, sizeof(struct fujitsu_hk_softc),
    fujitsu_hk_match, fujitsu_hk_attach, fujitsu_hk_detach, NULL);

static int
fujitsu_hk_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, fujitsu_hk_hid);
}

static void
fujitsu_hk_attach(device_t parent, device_t self, void *aux)
{
	struct fujitsu_hk_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	struct acpi_devnode *ad = aa->aa_node;
	int i;

	aprint_naive(": Fujitsu Hotkeys\n");
	aprint_normal(": Fujitsu Hotkeys\n");

	sc->sc_dev = self;
	sc->sc_node = ad;
	sc->sc_log = NULL;
	sc->sc_caps = fujitsu_hk_capabilities(ad);

	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_NONE);

	for (i = 0; i < FUJITSU_HK_PSW_COUNT; i++) {
		(void)snprintf(sc->sc_smpsw_name[i],
		    sizeof(sc->sc_smpsw_name[i]), "%s-%d",
		    device_xname(self), i);
		sc->sc_smpsw[i].smpsw_name = sc->sc_smpsw_name[i];
		sc->sc_smpsw[i].smpsw_type = PSWITCH_TYPE_HOTKEY;
		(void)sysmon_pswitch_register(&sc->sc_smpsw[i]);
	}

	fujitsu_hk_sysctl_setup(sc);

	(void)pmf_device_register(self, fujitsu_hk_suspend, fujitsu_hk_resume);
	(void)acpi_register_notify(sc->sc_node, fujitsu_hk_notify_handler);
}

static int
fujitsu_hk_detach(device_t self, int flags)
{
	struct fujitsu_hk_softc *sc = device_private(self);
	int i;

	pmf_device_deregister(self);

	if (sc->sc_log != NULL)
		sysctl_teardown(&sc->sc_log);

	acpi_deregister_notify(sc->sc_node);

	for (i = 0; i < FUJITSU_HK_PSW_COUNT; i++)
		sysmon_pswitch_unregister(&sc->sc_smpsw[i]);

	mutex_destroy(&sc->sc_mtx);

	return 0;
}

/*
 * On the P7120, the backlight needs to be enabled after resume, since the
 * laptop wakes up with the backlight off (even if it was on before suspend).
 */
static bool
fujitsu_hk_suspend(device_t self, const pmf_qual_t *qual)
{
	struct fujitsu_hk_softc *sc = device_private(self);

	mutex_enter(&sc->sc_mtx);
	(void)fujitsu_hk_set_backlight(sc, false);
	mutex_exit(&sc->sc_mtx);

	return true;
}

static bool
fujitsu_hk_resume(device_t self, const pmf_qual_t *qual)
{
	struct fujitsu_hk_softc *sc = device_private(self);

	mutex_enter(&sc->sc_mtx);
	(void)fujitsu_hk_set_backlight(sc, true);
	mutex_exit(&sc->sc_mtx);

	return true;
}

static uint16_t
fujitsu_hk_capabilities(const struct acpi_devnode *ad)
{
	uint16_t caps;

	caps = 0;

	if (fujitsu_hk_cap(ad->ad_handle, "GIRB", ACPI_TYPE_INTEGER))
		caps |= FUJITSU_HK_CAP_GIRB;

	if (fujitsu_hk_cap(ad->ad_handle, "FUNC", ACPI_TYPE_METHOD))
		caps |= FUJITSU_HK_CAP_FUNC;

	return caps;
}

static void
fujitsu_hk_notify_handler(ACPI_HANDLE handle, uint32_t evt, void *context)
{
	struct fujitsu_hk_softc *sc = device_private(context);
	static const int handler = OSL_NOTIFY_HANDLER;

	switch (evt) {

	case FUJITSU_HK_NOTIFY:
		(void)AcpiOsExecute(handler, fujitsu_hk_event_callback, sc);
		break;

	default:
		aprint_debug_dev(sc->sc_dev, "unknown notify 0x%02X\n", evt);
	}
}

static void
fujitsu_hk_event_callback(void *arg)
{
	struct fujitsu_hk_softc *sc = arg;
	const int max_irb_buffer_size = 100;
	uint32_t irb;
	int i, index;

	for (i = 0; i < max_irb_buffer_size; i++) {
		if (fujitsu_hk_get_irb(sc, &irb) || irb == 0)
			return;

		switch (irb & ~FUJITSU_HK_MODMASK) {
		case FUJITSU_HK_IRB_HOTKEY_RELEASE:
			/* Hotkey button release event (nothing to do). */
			break;
		case FUJITSU_HK_IRB_HOTKEY_PRESS(0):
		case FUJITSU_HK_IRB_HOTKEY_PRESS(1):
		case FUJITSU_HK_IRB_HOTKEY_PRESS(2):
		case FUJITSU_HK_IRB_HOTKEY_PRESS(3):
			/* Hotkey button press event. */
			index = FUJITSU_HK_IRB_HOTKEY_INDEX(irb);
			sysmon_pswitch_event(&sc->sc_smpsw[index],
			    PSWITCH_EVENT_PRESSED);
			break;
		default:
			aprint_error_dev(sc->sc_dev,
			    "unknown GIRB result: 0x%"PRIx32"\n", irb);
			break;
		}
	}
}

static void
fujitsu_hk_sysctl_setup(struct fujitsu_hk_softc *sc)
{
	const struct sysctlnode *rnode;
	bool dummy_state;

	if (fujitsu_hk_get_backlight(sc, &dummy_state) == 0) {
		if ((sysctl_createv(&sc->sc_log, 0, NULL, &rnode,
		    0, CTLTYPE_NODE, "acpi", NULL,
		    NULL, 0, NULL, 0,
		    CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
			goto fail;

		if ((sysctl_createv(&sc->sc_log, 0, &rnode, &rnode,
		    0, CTLTYPE_NODE, device_xname(sc->sc_dev),
		    SYSCTL_DESCR("Fujitsu hotkeys controls"),
		    NULL, 0, NULL, 0,
		    CTL_CREATE, CTL_EOL)) != 0)
			goto fail;

		(void)sysctl_createv(&sc->sc_log, 0, &rnode, NULL,
		    CTLFLAG_READWRITE, CTLTYPE_BOOL, "backlight",
		    SYSCTL_DESCR("Internal DFP backlight switch state"),
		    fujitsu_hk_sysctl_backlight, 0, (void *)sc, 0,
		    CTL_CREATE, CTL_EOL);
	}

	return;

 fail:
	aprint_error_dev(sc->sc_dev, "couldn't add sysctl nodes\n");
}

static int
fujitsu_hk_sysctl_backlight(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct fujitsu_hk_softc *sc;
	bool val;
	int error;

	node = *rnode;
	sc = node.sysctl_data;

	mutex_enter(&sc->sc_mtx);
	error = fujitsu_hk_get_backlight(sc, &val);
	mutex_exit(&sc->sc_mtx);

	if (error)
		return error;

	node.sysctl_data = &val;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	mutex_enter(&sc->sc_mtx);
	error = fujitsu_hk_set_backlight(sc, val);
	mutex_exit(&sc->sc_mtx);

	return error;
}

static int
fujitsu_hk_get_irb(struct fujitsu_hk_softc *sc, uint32_t *valuep)
{
	ACPI_HANDLE hdl = sc->sc_node->ad_handle;
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	if (!(sc->sc_caps & FUJITSU_HK_CAP_GIRB))
		return ENODEV;

	rv = acpi_eval_integer(hdl, "GIRB", &val);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(sc->sc_dev, "failed to evaluate %s.%s: %s\n",
		    acpi_name(hdl), "GIRB", AcpiFormatException(rv));
		return EIO;
	}

	*valuep = (uint32_t)val;

	return 0;
}

static int
fujitsu_hk_get_backlight(struct fujitsu_hk_softc *sc, bool *valuep)
{
	ACPI_HANDLE hdl = sc->sc_node->ad_handle;
	ACPI_INTEGER args[] = {
		FUJITSU_FUNC_TARGET_BACKLIGHT,
		FUJITSU_FUNC_COMMAND_GET,
		0x4,
		0x0
	};
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	if (!(sc->sc_caps & FUJITSU_HK_CAP_FUNC))
		return ENODEV;

	rv = fujitsu_hk_eval_nary_integer(hdl, "FUNC", args, 4, &val);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(sc->sc_dev, "failed to evaluate %s.%s: %s\n",
		    acpi_name(hdl), "FUNC", AcpiFormatException(rv));
		return EIO;
	}

	if (val == FUJITSU_FUNC_INVALID_ARGS)
		return ENODEV;

	if (val == FUJITSU_FUNC_BACKLIGHT_ON)
		*valuep = true;
	else if (val == FUJITSU_FUNC_BACKLIGHT_OFF)
		*valuep = false;
	else
		return ERANGE;

	return 0;
}

static int
fujitsu_hk_set_backlight(struct fujitsu_hk_softc *sc, bool value)
{
	ACPI_HANDLE hdl = sc->sc_node->ad_handle;
	ACPI_INTEGER args[] = {
		FUJITSU_FUNC_TARGET_BACKLIGHT,
		FUJITSU_FUNC_COMMAND_SET,
		0x4,
		0x0
	};
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	if (!(sc->sc_caps & FUJITSU_HK_CAP_FUNC))
		return ENODEV;

	if (value)
		args[3] = FUJITSU_FUNC_BACKLIGHT_ON;
	else
		args[3] = FUJITSU_FUNC_BACKLIGHT_OFF;

	rv = fujitsu_hk_eval_nary_integer(hdl, "FUNC", args, 4, &val);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(sc->sc_dev, "failed to evaluate %s.%s: %s\n",
		    acpi_name(hdl), "FUNC", AcpiFormatException(rv));
		return EIO;
	}

	if (val == FUJITSU_FUNC_INVALID_ARGS)
		return ENODEV;

	return 0;
}

/*
 * fujitsu_hk_cap:
 *
 *	Returns true if and only if (a) the object handle.path exists and
 *	(b) this object is a method or has the given type.
 */
static bool
fujitsu_hk_cap(ACPI_HANDLE handle, const char *path, ACPI_OBJECT_TYPE type)
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

/*
 * fujitsu_hk_eval_nary_integer:
 *
 *	Evaluate an object that takes as input an arbitrary (possible null)
 *	number of integer parameters.  If res is not NULL, then *res is filled
 *	with the result of the evaluation, and AE_NULL_OBJECT is returned if
 *	the evaluation produced no result.
 */
static ACPI_STATUS
fujitsu_hk_eval_nary_integer(ACPI_HANDLE handle, const char *path, const
    ACPI_INTEGER *args, uint8_t count, ACPI_INTEGER *res)
{
	ACPI_OBJECT_LIST paramlist;
	ACPI_OBJECT retobj, objpool[4], *argobjs;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	uint8_t i;

	/* Require that (args == NULL) if and only if (count == 0). */
	KASSERT((args != NULL || count == 0) && (args == NULL || count != 0));

	/* The object pool should be large enough for our callers. */
	KASSERT(count <= __arraycount(objpool));

	if (handle == NULL)
		handle = ACPI_ROOT_OBJECT;

	/* Convert the given array args into an array of ACPI objects. */
	argobjs = objpool;
	for (i = 0; i < count; i++) {
		argobjs[i].Type = ACPI_TYPE_INTEGER;
		argobjs[i].Integer.Value = args[i];
	}

	paramlist.Count = count;
	paramlist.Pointer = argobjs;

	(void)memset(&retobj, 0, sizeof(retobj));
	buf.Pointer = &retobj;
	buf.Length = sizeof(retobj);

	rv = AcpiEvaluateObject(handle, path, &paramlist, &buf);

	if (ACPI_FAILURE(rv))
		return rv;

	/*
	 * If a return value is expected and desired (i.e. res != NULL),
	 * then copy the result into *res.
	 */
	if (res != NULL) {
		if (buf.Length == 0)
			return AE_NULL_OBJECT;

		if (retobj.Type != ACPI_TYPE_INTEGER)
			return AE_TYPE;

		*res = retobj.Integer.Value;
	}

	return AE_OK;
}

MODULE(MODULE_CLASS_DRIVER, fujhk, "sysmon_power");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
fujhk_modcmd(modcmd_t cmd, void *aux)
{
	int rv = 0;

	switch (cmd) {

	case MODULE_CMD_INIT:

#ifdef _MODULE
		rv = config_init_component(cfdriver_ioconf_fujhk,
		    cfattach_ioconf_fujhk, cfdata_ioconf_fujhk);
#endif
		break;

	case MODULE_CMD_FINI:

#ifdef _MODULE
		rv = config_fini_component(cfdriver_ioconf_fujhk,
		    cfattach_ioconf_fujhk, cfdata_ioconf_fujhk);
#endif
		break;

	default:
		rv = ENOTTY;
	}

	return rv;
}
