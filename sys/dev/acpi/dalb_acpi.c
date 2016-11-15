/*	$NetBSD: dalb_acpi.c,v 1.18 2015/04/23 23:23:00 pgoyette Exp $	*/

/*-
 * Copyright (c) 2008 Christoph Egger <cegger@netbsd.org>
 * Copyright (c) 2008 Jared D. McNeill <jmcneill@netbsd.org>
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
__KERNEL_RCSID(0, "$NetBSD: dalb_acpi.c,v 1.18 2015/04/23 23:23:00 pgoyette Exp $");

/*
 * Direct Application Launch Button:
 * http://www.microsoft.com/whdc/system/vista/DirAppLaunch.mspx
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#define _COMPONENT          ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME            ("dalb_acpi")

#define DALB_ID_INVALID		-1

struct acpi_dalb_softc {
	device_t sc_dev;
	struct acpi_devnode *sc_node;

	ACPI_INTEGER sc_usageid;

	/* There's one PNP0C32 ACPI device for each button.
	 * Therefore, one instance is enough. */
	struct sysmon_pswitch sc_smpsw;
	bool sc_smpsw_valid;
};

static int	acpi_dalb_match(device_t, cfdata_t, void *);
static void	acpi_dalb_attach(device_t, device_t, void *);
static int	acpi_dalb_detach(device_t, int);
static void	acpi_dalb_notify_handler(ACPI_HANDLE, uint32_t, void *);
static bool	acpi_dalb_resume(device_t, const pmf_qual_t *);

static void	acpi_dalb_get_wakeup_hotkeys(void *opaque);
static void	acpi_dalb_get_runtime_hotkeys(void *opaque);

CFATTACH_DECL_NEW(acpidalb, sizeof(struct acpi_dalb_softc),
    acpi_dalb_match, acpi_dalb_attach, acpi_dalb_detach, NULL);

static const char * const acpi_dalb_ids[] = {
        "PNP0C32", /* Direct Application Launch Button */
        NULL
};

#define DALB_SYSTEM_WAKEUP	0x02
#define DALB_SYSTEM_RUNTIME	0x80

static int
acpi_dalb_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, acpi_dalb_ids);
}

static void
acpi_dalb_sysmon_init(struct acpi_dalb_softc *sc)
{
	sc->sc_smpsw_valid = true;
	sc->sc_smpsw.smpsw_name = device_xname(sc->sc_dev);
	sc->sc_smpsw.smpsw_type = PSWITCH_TYPE_HOTKEY;
	if (sysmon_pswitch_register(&sc->sc_smpsw)) {
		aprint_error_dev(sc->sc_dev,
			"couldn't register sleep with sysmon\n");
		sc->sc_smpsw_valid = false;
	}
}


static void
acpi_dalb_init(device_t dev)
{
	struct acpi_dalb_softc *sc = device_private(dev);
	ACPI_OBJECT *obj;
	ACPI_STATUS rv;
	ACPI_BUFFER ret;

	rv = acpi_eval_struct(sc->sc_node->ad_handle, "GHID", &ret);

	if (ACPI_FAILURE(rv) || ret.Pointer == NULL) {
		aprint_error_dev(dev,
			"couldn't enable notify handler: (%s)\n",
			AcpiFormatException(rv));
		return;
	}

	obj = ret.Pointer;

	if (obj->Type != ACPI_TYPE_BUFFER) {
		sc->sc_usageid = DALB_ID_INVALID;
		aprint_debug_dev(dev, "invalid ACPI type: %u\n", obj->Type);
		goto out;
	}

	switch (obj->Buffer.Length) {
	case 1:
		sc->sc_usageid = *(uint8_t *)obj->Buffer.Pointer;
		break;
	case 2:
		sc->sc_usageid = le16toh(*(uint16_t *)obj->Buffer.Pointer);
		break;
	case 4:
		sc->sc_usageid = le32toh(*(uint32_t *)obj->Buffer.Pointer);
		break;
	default:
		aprint_debug_dev(dev, "unhandled ret.Length: 0x%lx\n",
			(unsigned long)obj->Buffer.Length);
		sc->sc_usageid = DALB_ID_INVALID;
		break;
	}

out:
	ACPI_FREE(ret.Pointer);
}

static void
acpi_dalb_attach(device_t parent, device_t self, void *aux)
{
	struct acpi_dalb_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;

	aprint_naive("\n");
	aprint_normal(": Direct Application Launch Button\n");

	sc->sc_dev = self;
	sc->sc_node = aa->aa_node;

	config_interrupts(self, acpi_dalb_init);

	(void)pmf_device_register(self, NULL, acpi_dalb_resume);
	(void)acpi_register_notify(sc->sc_node, acpi_dalb_notify_handler);

	sc->sc_smpsw_valid = false;
	acpi_dalb_sysmon_init(sc);
}

static int
acpi_dalb_detach(device_t self, int flags)
{
	struct acpi_dalb_softc *sc = device_private(self);

	pmf_device_deregister(self);
	acpi_deregister_notify(sc->sc_node);
	sysmon_pswitch_unregister(&sc->sc_smpsw);

	return 0;
}

static void
acpi_dalb_notify_handler(ACPI_HANDLE hdl, uint32_t notify, void *opaque)
{
	device_t dev = opaque;
	struct acpi_dalb_softc *sc = device_private(dev);
	ACPI_STATUS rv;

	switch (notify) {
	case DALB_SYSTEM_WAKEUP:
		rv = AcpiOsExecute(OSL_NOTIFY_HANDLER,
			acpi_dalb_get_wakeup_hotkeys, dev);
		break;
	case DALB_SYSTEM_RUNTIME:
		rv = AcpiOsExecute(OSL_NOTIFY_HANDLER,
			acpi_dalb_get_runtime_hotkeys, dev);
		break;

	default:
		aprint_error_dev(dev,
			"unknown notification event 0x%x from button 0x%x\n",
			notify, (uint32_t)sc->sc_usageid);
		return;
	}

	if (ACPI_FAILURE(rv))
		aprint_error_dev(dev, "couldn't queue hotkey handler: %s\n",
			AcpiFormatException(rv));
}

static void
acpi_dalb_get_wakeup_hotkeys(void *opaque)
{
	device_t dev = opaque;
	struct acpi_dalb_softc *sc = device_private(dev);

	if (!sc->sc_smpsw_valid)
		return;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO,
		"invoking %s (wakeup)\n", sc->sc_smpsw.smpsw_name));

	sysmon_pswitch_event(&sc->sc_smpsw, PSWITCH_EVENT_PRESSED);
}

static void
acpi_dalb_get_runtime_hotkeys(void *opaque)
{
	device_t dev = opaque;
	struct acpi_dalb_softc *sc = device_private(dev);

	if (!sc->sc_smpsw_valid)
		return;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO,
		"invoking %s (runtime)\n", sc->sc_smpsw.smpsw_name));

	sysmon_pswitch_event(&sc->sc_smpsw, PSWITCH_EVENT_PRESSED);
}

static bool
acpi_dalb_resume(device_t dev, const pmf_qual_t *qual)
{
	struct acpi_dalb_softc *sc = device_private(dev);
	ACPI_STATUS rv;
	ACPI_BUFFER ret;

	rv = acpi_eval_struct(sc->sc_node->ad_handle, "GHID", &ret);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(dev, "couldn't evaluate GHID: %s\n",
		    AcpiFormatException(rv));
		return false;
	}
	if (ret.Pointer)
		ACPI_FREE(ret.Pointer);

	return true;
}

MODULE(MODULE_CLASS_DRIVER, acpidalb, "sysmon_power");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
acpidalb_modcmd(modcmd_t cmd, void *aux)
{
	int rv = 0;

	switch (cmd) {

	case MODULE_CMD_INIT:

#ifdef _MODULE
		rv = config_init_component(cfdriver_ioconf_acpidalb,
		    cfattach_ioconf_acpidalb, cfdata_ioconf_acpidalb);
#endif
		break;

	case MODULE_CMD_FINI:

#ifdef _MODULE
		rv = config_fini_component(cfdriver_ioconf_acpidalb,
		    cfattach_ioconf_acpidalb, cfdata_ioconf_acpidalb);
#endif
		break;

	default:
		rv = ENOTTY;
	}

	return rv;
}
