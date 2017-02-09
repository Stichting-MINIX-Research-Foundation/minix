/*	$$NetBSD: pwdog.c,v 1.8 2015/04/23 23:23:01 pgoyette Exp $ */
/*	$OpenBSD: pwdog.c,v 1.7 2010/04/08 00:23:53 tedu Exp $ */

/*
 * Copyright (c) 2006, 2011 Marc Balmer <mbalmer@NetBSD.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/sysmon/sysmonvar.h>

struct pwdog_softc {
	device_t		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_iosize;

	struct sysmon_wdog	sc_smw;
	bool			sc_smw_valid;
};

/* registers */
#define PWDOG_ACTIVATE	0
#define PWDOG_DISABLE	1

/* maximum timeout period in seconds */
#define PWDOG_MAX_PERIOD	(12*60)	/* 12 minutes */

static int pwdog_match(device_t, cfdata_t, void *);
static void pwdog_attach(device_t, device_t, void *);
static int pwdog_detach(device_t, int);
static bool pwdog_suspend(device_t, const pmf_qual_t *);
static bool pwdog_resume(device_t, const pmf_qual_t *);
static int pwdog_setmode(struct sysmon_wdog *);
static int pwdog_tickle(struct sysmon_wdog *);

CFATTACH_DECL_NEW(
    pwdog,
    sizeof(struct pwdog_softc),
    pwdog_match,
    pwdog_attach,
    pwdog_detach,
    NULL
);

static int
pwdog_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_QUANCOM &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_QUANCOM_PWDOG1)
		return 1;
	return 0;
}

void
pwdog_attach(device_t parent, device_t self, void *aux)
{
	struct pwdog_softc *sc = device_private(self);
	struct pci_attach_args *const pa = (struct pci_attach_args *)aux;
	pcireg_t memtype;

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, PCI_MAPREG_START);
	if (pci_mapreg_map(pa, PCI_MAPREG_START, memtype, 0, &sc->sc_iot,
	    &sc->sc_ioh, NULL, &sc->sc_iosize)) {
		aprint_error("\n");
		aprint_error_dev(self, "PCI %s region not found\n",
		    memtype == PCI_MAPREG_TYPE_IO ? "I/O" : "memory");
		return;
	}
	printf("\n");

	sc->sc_dev = self;

	pmf_device_register(self, pwdog_suspend, pwdog_resume);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, PWDOG_DISABLE, 0);

	sc->sc_smw.smw_name = device_xname(self);
	sc->sc_smw.smw_cookie = sc;
	sc->sc_smw.smw_setmode = pwdog_setmode;
	sc->sc_smw.smw_tickle = pwdog_tickle;
	sc->sc_smw.smw_period = PWDOG_MAX_PERIOD;

	if (sysmon_wdog_register(&sc->sc_smw))
		aprint_error_dev(self, "couldn't register with sysmon\n");
	else
		sc->sc_smw_valid = true;
}

static int
pwdog_detach(device_t self, int flags)
{
	struct pwdog_softc *sc = device_private(self);

	/* XXX check flags & DETACH_FORCE (or DETACH_SHUTDOWN)? */
	if (sc->sc_smw_valid) {
		if ((sc->sc_smw.smw_mode & WDOG_MODE_MASK)
		    != WDOG_MODE_DISARMED)
			return EBUSY;

		sysmon_wdog_unregister(&sc->sc_smw);
		sc->sc_smw_valid = false;
	}

	pmf_device_deregister(self);

	if (sc->sc_iosize)
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_iosize);
	return 0;
}

static bool
pwdog_resume(device_t self, const pmf_qual_t *qual)
{
	struct pwdog_softc *sc = device_private(self);

	if (sc->sc_smw_valid == false)
		return true;

	/*
	 * suspend is inhibited when the watchdog timer was armed,
	 * so when we end up here, the watchdog is disabled; program the
	 * hardware accordingly.
	 */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, PWDOG_DISABLE, 0);
	return true;
}

static bool
pwdog_suspend(device_t self, const pmf_qual_t *qual)
{
	struct pwdog_softc *sc = device_private(self);

	if (sc->sc_smw_valid == false)
		return true;

	if ((sc->sc_smw.smw_mode & WDOG_MODE_MASK) != WDOG_MODE_DISARMED)
		return false;

	return true;
}

static int
pwdog_setmode(struct sysmon_wdog *smw)
{
	struct pwdog_softc *sc = smw->smw_cookie;

	switch (smw->smw_mode & WDOG_MODE_MASK) {
	case WDOG_MODE_DISARMED:
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, PWDOG_DISABLE, 0);
		break;
	default:
		/*
		 * NB: the timer period set by the user is ignored
		 * since the period can only be set in hardware.
		 */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, PWDOG_ACTIVATE, 0);
	}
	return 0;
}

static int
pwdog_tickle(struct sysmon_wdog *smw)
{
	struct pwdog_softc *sc = smw->smw_cookie;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, PWDOG_ACTIVATE, 0);
	return 0;
}

MODULE(MODULE_CLASS_DRIVER, pwdog, "pci,sysmon_wdog");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
pwdog_modcmd(modcmd_t cmd, void *opaque)
{
	int error;

	error = 0;
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_pwdog,
		    cfattach_ioconf_pwdog, cfdata_ioconf_pwdog);
		if (error)
			aprint_error("%s: unable to init component\n",
			    pwdog_cd.cd_name);
#endif
		break;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		config_fini_component(cfdriver_ioconf_pwdog,
		    cfattach_ioconf_pwdog, cfdata_ioconf_pwdog);
#endif
		break;
	default:
		error = ENOTTY;
	}
	return error;
}
