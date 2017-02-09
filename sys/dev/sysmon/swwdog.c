/*	$NetBSD: swwdog.c,v 1.19 2015/05/12 10:20:14 pgoyette Exp $	*/

/*
 * Copyright (c) 2004, 2005 Steven M. Bellovin
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Steven M. Bellovin
 * 4. The name of the author contributors may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: swwdog.c,v 1.19 2015/05/12 10:20:14 pgoyette Exp $");

/*
 *
 * Software watchdog timer
 *
 */
#include <sys/param.h>
#include <sys/device.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/wdog.h>
#include <sys/workqueue.h>
#include <sys/module.h>
#include <dev/sysmon/sysmonvar.h>

#ifndef _MODULE
#include "opt_modular.h"
#endif

struct swwdog_softc {
	device_t		sc_dev;
	struct sysmon_wdog	sc_smw;
	struct callout		sc_c;
	int			sc_armed;
};

bool	swwdog_reboot = false;	/* false --> panic , true  --> reboot */

static struct workqueue *wq;
static device_t		swwdog_dev;

MODULE(MODULE_CLASS_DRIVER, swwdog, "sysmon_wdog");

#ifdef _MODULE
CFDRIVER_DECL(swwdog, DV_DULL, NULL);
#endif

int swwdogattach(int);

static int	swwdog_setmode(struct sysmon_wdog *);
static int	swwdog_tickle(struct sysmon_wdog *);
static bool	swwdog_suspend(device_t, const pmf_qual_t *);
static int	swwdog_arm(struct swwdog_softc *);
static int	swwdog_disarm(struct swwdog_softc *);

static void	swwdog_panic(void *);

static void	swwdog_sysctl_setup(void);
static struct sysctllog *swwdog_sysctllog = NULL;

static int	swwdog_match(device_t, cfdata_t, void *);
static void	swwdog_attach(device_t, device_t, void *);
static int	swwdog_detach(device_t, int);
static bool	swwdog_suspend(device_t, const pmf_qual_t *);

static int	swwdog_init(void *);
static int	swwdog_fini(void *);
static int	swwdog_modcmd(modcmd_t, void *);

CFATTACH_DECL_NEW(swwdog, sizeof(struct swwdog_softc),
	swwdog_match, swwdog_attach, swwdog_detach, NULL);
extern struct cfdriver swwdog_cd;

#define	SWDOG_DEFAULT	60		/* 60-second default period */

static void
doreboot(struct work *wrkwrkwrk, void *p)
{

	cpu_reboot(0, NULL);
}

int
swwdogattach(int n __unused)
{
	int error;
	static struct cfdata cf;

	if (workqueue_create(&wq, "swwreboot", doreboot, NULL,
	    PRI_NONE, IPL_NONE, 0) != 0) {
		aprint_error("failed to create swwdog reboot wq");
		return 1;
	}

	error = config_cfattach_attach(swwdog_cd.cd_name, &swwdog_ca);
	if (error) {
		aprint_error("%s: unable to attach cfattach\n",
		    swwdog_cd.cd_name);
		workqueue_destroy(wq);
		return error;
	}

	cf.cf_name = swwdog_cd.cd_name;
	cf.cf_atname = swwdog_cd.cd_name;
	cf.cf_unit = 0;
	cf.cf_fstate = FSTATE_STAR;
	cf.cf_pspec = NULL;
	cf.cf_loc = NULL;
	cf.cf_flags = 0;

	swwdog_dev = config_attach_pseudo(&cf);

	if (swwdog_dev == NULL) {
		config_cfattach_detach(swwdog_cd.cd_name, &swwdog_ca);
		workqueue_destroy(wq);
		return 1;
	}
	return 0;
}

static int
swwdog_match(device_t parent, cfdata_t data, void *aux)
{

	return 1;
}

static void
swwdog_attach(device_t parent, device_t self, void *aux)
{
	struct swwdog_softc *sc = device_private(self);

	if (workqueue_create(&wq, "swwreboot", doreboot, NULL,
	    PRI_NONE, IPL_NONE, 0) != 0) {
		aprint_error_dev(self, "failed to create reboot workqueue");
	}

	sc->sc_dev = self;
	sc->sc_smw.smw_name = device_xname(self);
	sc->sc_smw.smw_cookie = sc;
	sc->sc_smw.smw_setmode = swwdog_setmode;
	sc->sc_smw.smw_tickle = swwdog_tickle;
	sc->sc_smw.smw_period = SWDOG_DEFAULT;

	callout_init(&sc->sc_c, 0);
	callout_setfunc(&sc->sc_c, swwdog_panic, sc);

	if (sysmon_wdog_register(&sc->sc_smw) == 0)
		aprint_normal_dev(self, "software watchdog initialized\n");
	else {
		aprint_error_dev(self, "unable to register software "
		    "watchdog with sysmon\n");
		callout_destroy(&sc->sc_c);
		workqueue_destroy(wq);
		return;
	}

	if (!pmf_device_register(self, swwdog_suspend, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	swwdog_sysctl_setup();
}

static int
swwdog_detach(device_t self, int flags)
{
	struct swwdog_softc *sc = device_private(self);

	pmf_device_deregister(self);
	swwdog_disarm(sc);
	sysctl_teardown(&swwdog_sysctllog);
	sysmon_wdog_unregister(&sc->sc_smw);
	callout_destroy(&sc->sc_c);
	workqueue_destroy(wq);

	return 0;
}

static bool
swwdog_suspend(device_t dev, const pmf_qual_t *qual)
{
	struct swwdog_softc *sc = device_private(dev);

	/* Don't allow suspend if watchdog is armed */
	if ((sc->sc_smw.smw_mode & WDOG_MODE_MASK) != WDOG_MODE_DISARMED)
		return false;
	return true;
}

static int
swwdog_setmode(struct sysmon_wdog *smw)
{
	struct swwdog_softc *sc = smw->smw_cookie;
	int error = 0;

	if ((smw->smw_mode & WDOG_MODE_MASK) == WDOG_MODE_DISARMED) {
		error = swwdog_disarm(sc);
	} else {
		if (smw->smw_period == 0)
			return EINVAL;
		else if (smw->smw_period == WDOG_PERIOD_DEFAULT)
			sc->sc_smw.smw_period = SWDOG_DEFAULT;
		else
			sc->sc_smw.smw_period = smw->smw_period;
		error = swwdog_arm(sc);
	}
	return error;
}

static int
swwdog_tickle(struct sysmon_wdog *smw)
{

	return swwdog_arm(smw->smw_cookie);
}

static int
swwdog_arm(struct swwdog_softc *sc)
{

	callout_schedule(&sc->sc_c, sc->sc_smw.smw_period * hz);
	return 0;
}

static int
swwdog_disarm(struct swwdog_softc *sc)
{

	callout_stop(&sc->sc_c);
	return 0;
}

static void
swwdog_panic(void *vsc)
{
	struct swwdog_softc *sc = vsc;
	static struct work wk; /* we'll need it max once */
	bool do_panic;

	do_panic = !swwdog_reboot;
	swwdog_reboot = false;
	callout_schedule(&sc->sc_c, 60 * hz);	/* deliberate double-panic */

	printf("%s: %d second timer expired\n", "swwdog",
	    sc->sc_smw.smw_period);

	if (do_panic)
		panic("watchdog timer expired");
	else
		workqueue_enqueue(wq, &wk, NULL);
}

static void
swwdog_sysctl_setup(void)
{
	const struct sysctlnode *me;

	KASSERT(swwdog_sysctllog == NULL);

	sysctl_createv(&swwdog_sysctllog, 0, NULL, &me, CTLFLAG_READWRITE,
	    CTLTYPE_NODE, "swwdog", NULL,
	    NULL, 0, NULL, 0,
	    CTL_HW, CTL_CREATE, CTL_EOL);
	sysctl_createv(&swwdog_sysctllog, 0, NULL, NULL, CTLFLAG_READWRITE,
	    CTLTYPE_BOOL, "reboot", "reboot if timer expires",
	    NULL, 0, &swwdog_reboot, sizeof(bool),
	    CTL_HW, me->sysctl_num, CTL_CREATE, CTL_EOL);
}

/*
 * Module management
 */

static
int
swwdog_init(void *arg)
{
	/*
	 * Merge the driver info into the kernel tables and attach the
	 * pseudo-device
	 */
	int error = 0;


#ifdef _MODULE
	error = config_cfdriver_attach(&swwdog_cd);
	if (error) {
		aprint_error("%s: unable to attach cfdriver\n",
		    swwdog_cd.cd_name);
		return error;
	}
	error = swwdogattach(1);
	if (error) {
		aprint_error("%s: device attach failed\n", swwdog_cd.cd_name);
		config_cfdriver_detach(&swwdog_cd);
	}
#endif

	return error;
}

static
int
swwdog_fini(void *arg)
{
	int error;

	error = config_detach(swwdog_dev, 0);

#ifdef _MODULE
	error = config_cfattach_detach(swwdog_cd.cd_name, &swwdog_ca);
	if (error)
		aprint_error("%s: error detaching cfattach: %d\n",
		    swwdog_cd.cd_name, error);

	error = config_cfdriver_detach(&swwdog_cd);
	if (error)
		aprint_error("%s: error detaching cfdriver: %d\n",
		    swwdog_cd.cd_name, error);
#endif

        return error;
}

static
int
swwdog_modcmd(modcmd_t cmd, void *arg)
{
	int ret;
 
	switch (cmd) {
	case MODULE_CMD_INIT:
		ret = swwdog_init(arg);
		break;
 
	case MODULE_CMD_FINI:
		ret = swwdog_fini(arg);
		break;
 
	case MODULE_CMD_STAT:
	default:
		ret = ENOTTY;
	}
 
	return ret;
}

