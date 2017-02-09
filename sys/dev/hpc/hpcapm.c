/*	$NetBSD: hpcapm.c,v 1.20 2013/11/09 21:31:56 christos Exp $	*/

/*
 * Copyright (c) 2000 Takemura Shin
 * Copyright (c) 2000-2001 SATO Kazumi
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hpcapm.c,v 1.20 2013/11/09 21:31:56 christos Exp $");

#ifdef _KERNEL_OPT
#include "opt_hpcapm.h"
#endif

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/selinfo.h> /* XXX: for apm_softc that is exposed here */

#include <dev/hpc/apm/apmvar.h>

#include <sys/bus.h>
#include <machine/config_hook.h>
#include <machine/platid.h>
#include <machine/platid_mask.h>

#ifdef HPCAPMDEBUG
#ifndef HPCAPMDEBUG_CONF
#define HPCAPMDEBUG_CONF 1
#endif
int	hpcapm_debug = HPCAPMDEBUG_CONF;
#define	DPRINTF(arg)     do { if (hpcapm_debug) printf arg; } while(0);
#define	DPRINTFN(n, arg) do { if (hpcapm_debug > (n)) printf arg; } while (0);
#else
#define	DPRINTF(arg)     do { } while (0);
#define DPRINTFN(n, arg) do { } while (0);
#endif

/* Definition of the driver for autoconfig. */
static int	hpcapm_match(device_t, cfdata_t, void *);
static void	hpcapm_attach(device_t, device_t, void *);
static int	hpcapm_hook(void *, int, long, void *);

static void	hpcapm_disconnect(void *);
static void	hpcapm_enable(void *, int);
static int	hpcapm_set_powstate(void *, u_int, u_int);
static int	hpcapm_get_powstat(void *, u_int, struct apm_power_info *);
static int	hpcapm_get_event(void *, u_int *, u_int *);
static void	hpcapm_cpu_busy(void *);
static void	hpcapm_cpu_idle(void *);
static void	hpcapm_get_capabilities(void *, u_int *, u_int *);

struct apmhpc_softc {
	void *sc_apmdev;
	volatile unsigned int events;
	volatile int power_state;
	volatile int battery_flags;
	volatile int ac_state;
	config_hook_tag	sc_standby_hook;
	config_hook_tag	sc_suspend_hook;
	config_hook_tag sc_battery_hook;
	config_hook_tag sc_ac_hook;
	int battery_life;
	int minutes_left;
};

CFATTACH_DECL_NEW(hpcapm, sizeof (struct apmhpc_softc),
    hpcapm_match, hpcapm_attach, NULL, NULL);

struct apm_accessops hpcapm_accessops = {
	hpcapm_disconnect,
	hpcapm_enable,
	hpcapm_set_powstate,
	hpcapm_get_powstat,
	hpcapm_get_event,
	hpcapm_cpu_busy,
	hpcapm_cpu_idle,
	hpcapm_get_capabilities,
};

extern struct cfdriver hpcapm_cd;

static int
hpcapm_match(device_t parent, cfdata_t cf, void *aux)
{

	return 1;
}

static void
hpcapm_attach(device_t parent, device_t self, void *aux)
{
	struct apmhpc_softc *sc;
	struct apmdev_attach_args aaa;

	sc = device_private(self);
	printf(": pseudo power management module\n");

	sc->events = 0;
	sc->power_state = APM_SYS_READY;
	sc->battery_flags = APM_BATT_FLAG_UNKNOWN;
	sc->ac_state = APM_AC_UNKNOWN;
	sc->battery_life = APM_BATT_LIFE_UNKNOWN;
	sc->minutes_left = 0;
	sc->sc_standby_hook = config_hook(CONFIG_HOOK_PMEVENT,
					  CONFIG_HOOK_PMEVENT_STANDBYREQ,
					  CONFIG_HOOK_EXCLUSIVE,
					  hpcapm_hook, sc);
	sc->sc_suspend_hook = config_hook(CONFIG_HOOK_PMEVENT,
					  CONFIG_HOOK_PMEVENT_SUSPENDREQ,
					  CONFIG_HOOK_EXCLUSIVE,
					  hpcapm_hook, sc);

	sc->sc_battery_hook = config_hook(CONFIG_HOOK_PMEVENT,
					  CONFIG_HOOK_PMEVENT_BATTERY,
					  CONFIG_HOOK_SHARE,
					  hpcapm_hook, sc);

	sc->sc_ac_hook = config_hook(CONFIG_HOOK_PMEVENT,
				     CONFIG_HOOK_PMEVENT_AC,
				     CONFIG_HOOK_SHARE,
				     hpcapm_hook, sc);

	aaa.accessops = &hpcapm_accessops;
	aaa.accesscookie = sc;
	aaa.apm_detail = 0x0102;

	sc->sc_apmdev = config_found_ia(self, "apmdevif", &aaa, apmprint);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "unable to establish power handler\n");
}

static int
hpcapm_hook(void *ctx, int type, long id, void *msg)
{
	struct apmhpc_softc *sc;
	int s;
	int charge;
	int message;

	sc = ctx;

	if (type != CONFIG_HOOK_PMEVENT)
		return 1;

	if (CONFIG_HOOK_VALUEP(msg))
		message = (int)msg;
	else
		message = *(int *)msg;

	s = splhigh();
	switch (id) {
	case CONFIG_HOOK_PMEVENT_STANDBYREQ:
		if (sc->power_state != APM_SYS_STANDBY) {
			sc->events |= (1 << APM_USER_STANDBY_REQ);
		} else {
			sc->events |= (1 << APM_NORMAL_RESUME);
		}
		break;
	case CONFIG_HOOK_PMEVENT_SUSPENDREQ:
		if (sc->power_state != APM_SYS_SUSPEND) {
			DPRINTF(("hpcapm: suspend request\n"));
			sc->events |= (1 << APM_USER_SUSPEND_REQ);
		} else {
			sc->events |= (1 << APM_NORMAL_RESUME);
		}
		break;
	case CONFIG_HOOK_PMEVENT_BATTERY:
		switch (message) {
		case CONFIG_HOOK_BATT_CRITICAL:
			DPRINTF(("hpcapm: battery state critical\n"));
			charge = sc->battery_flags & APM_BATT_FLAG_CHARGING;
			sc->battery_flags = APM_BATT_FLAG_CRITICAL;
			sc->battery_flags |= charge;
			sc->battery_life = 0;
			break;
		case CONFIG_HOOK_BATT_LOW:
			DPRINTF(("hpcapm: battery state low\n"));
			charge = sc->battery_flags & APM_BATT_FLAG_CHARGING;
			sc->battery_flags = APM_BATT_FLAG_LOW;
			sc->battery_flags |= charge;
			break;
		case CONFIG_HOOK_BATT_HIGH:
			DPRINTF(("hpcapm: battery state high\n"));
			charge = sc->battery_flags & APM_BATT_FLAG_CHARGING;
			sc->battery_flags = APM_BATT_FLAG_HIGH;
			sc->battery_flags |= charge;
			break;
		case CONFIG_HOOK_BATT_10P:
			DPRINTF(("hpcapm: battery life 10%%\n"));
			sc->battery_life = 10;
			break;
		case CONFIG_HOOK_BATT_20P:
			DPRINTF(("hpcapm: battery life 20%%\n"));
			sc->battery_life = 20;
			break;
		case CONFIG_HOOK_BATT_30P:
			DPRINTF(("hpcapm: battery life 30%%\n"));
			sc->battery_life = 30;
			break;
		case CONFIG_HOOK_BATT_40P:
			DPRINTF(("hpcapm: battery life 40%%\n"));
			sc->battery_life = 40;
			break;
		case CONFIG_HOOK_BATT_50P:
			DPRINTF(("hpcapm: battery life 50%%\n"));
			sc->battery_life = 50;
			break;
		case CONFIG_HOOK_BATT_60P:
			DPRINTF(("hpcapm: battery life 60%%\n"));
			sc->battery_life = 60;
			break;
		case CONFIG_HOOK_BATT_70P:
			DPRINTF(("hpcapm: battery life 70%%\n"));
			sc->battery_life = 70;
			break;
		case CONFIG_HOOK_BATT_80P:
			DPRINTF(("hpcapm: battery life 80%%\n"));
			sc->battery_life = 80;
			break;
		case CONFIG_HOOK_BATT_90P:
			DPRINTF(("hpcapm: battery life 90%%\n"));
			sc->battery_life = 90;
			break;
		case CONFIG_HOOK_BATT_100P:
			DPRINTF(("hpcapm: battery life 100%%\n"));
			sc->battery_life = 100;
			break;
		case CONFIG_HOOK_BATT_UNKNOWN:
			DPRINTF(("hpcapm: battery state unknown\n"));
			sc->battery_flags = APM_BATT_FLAG_UNKNOWN;
			sc->battery_life = APM_BATT_LIFE_UNKNOWN;
			break;
		case CONFIG_HOOK_BATT_NO_SYSTEM_BATTERY:
			DPRINTF(("hpcapm: battery state no system battery?\n"));
			sc->battery_flags = APM_BATT_FLAG_NO_SYSTEM_BATTERY;
			sc->battery_life = APM_BATT_LIFE_UNKNOWN;
			break;
		}
		break;
	case CONFIG_HOOK_PMEVENT_AC:
		switch (message) {
		case CONFIG_HOOK_AC_OFF:
			DPRINTF(("hpcapm: ac not connected\n"));
			sc->battery_flags &= ~APM_BATT_FLAG_CHARGING;
			sc->ac_state = APM_AC_OFF;
			break;
		case CONFIG_HOOK_AC_ON_CHARGE:
			DPRINTF(("hpcapm: charging\n"));
			sc->battery_flags |= APM_BATT_FLAG_CHARGING;
			sc->ac_state = APM_AC_ON;
			break;
		case CONFIG_HOOK_AC_ON_NOCHARGE:
			DPRINTF(("hpcapm: ac connected\n"));
			sc->battery_flags &= ~APM_BATT_FLAG_CHARGING;
			sc->ac_state = APM_AC_ON;
			break;
		case CONFIG_HOOK_AC_UNKNOWN:
			sc->ac_state = APM_AC_UNKNOWN;
			break;
		}
		break;
	}
	splx(s);

	return (0);
}

static void
hpcapm_disconnect(void *scx)
{
}

static void
hpcapm_enable(void *scx, int onoff)
{
}

static int
hpcapm_set_powstate(void *scx, u_int devid, u_int powstat)
{
	struct apmhpc_softc *sc;
	int s;

	sc = scx;

	if (devid != APM_DEV_ALLDEVS)
		return APM_ERR_UNRECOG_DEV;

	switch (powstat) {
	case APM_SYS_READY:
		DPRINTF(("hpcapm: set power state READY\n"));
		sc->power_state = APM_SYS_READY;
		break;
	case APM_SYS_STANDBY:
		DPRINTF(("hpcapm: set power state STANDBY\n"));
		s = splhigh();
		config_hook_call(CONFIG_HOOK_PMEVENT,
				 CONFIG_HOOK_PMEVENT_HARDPOWER,
				 (void *)PWR_STANDBY);
		sc->power_state = APM_SYS_STANDBY;
		machine_standby();
		config_hook_call_reverse(CONFIG_HOOK_PMEVENT,
		    CONFIG_HOOK_PMEVENT_HARDPOWER,
		    (void *)PWR_RESUME);
		DPRINTF(("hpcapm: resume\n"));
		splx(s);
		break;
	case APM_SYS_SUSPEND:
		DPRINTF(("hpcapm: set power state SUSPEND...\n"));
		s = splhigh();
		config_hook_call(CONFIG_HOOK_PMEVENT,
		    CONFIG_HOOK_PMEVENT_HARDPOWER,
		    (void *)PWR_SUSPEND);
		sc->power_state = APM_SYS_SUSPEND;
		machine_sleep();
		config_hook_call_reverse(CONFIG_HOOK_PMEVENT,
				 CONFIG_HOOK_PMEVENT_HARDPOWER,
				 (void *)PWR_RESUME);
		DPRINTF(("hpcapm: resume\n"));
		splx(s);
		break;
	case APM_SYS_OFF:
		DPRINTF(("hpcapm: set power state OFF\n"));
		sc->power_state = APM_SYS_OFF;
		break;
	case APM_LASTREQ_INPROG:
		/*DPRINTF(("hpcapm: set power state INPROG\n"));
		 */
		break;
	case APM_LASTREQ_REJECTED:
		DPRINTF(("hpcapm: set power state REJECTED\n"));
		break;
	}

	return (0);
}

static int
hpcapm_get_powstat(void *scx, u_int batteryid, struct apm_power_info *pinfo)
{
	struct apmhpc_softc *sc;
	int val;

	sc = scx;

	pinfo->nbattery = 0;
	pinfo->batteryid = 0;
	pinfo->minutes_valid = 0;
	pinfo->minutes_left = 0;
	pinfo->battery_state = APM_BATT_UNKNOWN; /* XXX: ignored */

	if (config_hook_call(CONFIG_HOOK_GET,
			     CONFIG_HOOK_ACADAPTER, &val) != -1)
		pinfo->ac_state = val;
	else
		pinfo->ac_state = sc->ac_state;

	if (config_hook_call(CONFIG_HOOK_GET,
			     CONFIG_HOOK_CHARGE, &val) != -1)
		pinfo->battery_flags = val;
	else
		pinfo->battery_flags = sc->battery_flags;

	if (config_hook_call(CONFIG_HOOK_GET,
			     CONFIG_HOOK_BATTERYVAL, &val) != -1)
		pinfo->battery_life = val;
	else
		pinfo->battery_life = sc->battery_life;

	return (0);
}

static int
hpcapm_get_event(void *scx, u_int *event_type, u_int *event_info)
{
	struct apmhpc_softc *sc;
	int s, i;

	sc = scx;
	s = splhigh();
	for (i = APM_STANDBY_REQ; i <= APM_CAP_CHANGE; i++) {
		if (sc->events & (1 << i)) {
			sc->events &= ~(1 << i);
			*event_type = i;
			if (*event_type == APM_NORMAL_RESUME ||
			    *event_type == APM_CRIT_RESUME) {
				/* pccard power off in the suspend state */
				*event_info = 1;
				sc->power_state = APM_SYS_READY;
			} else
				*event_info = 0;
			return (0);
		}
	}
	splx(s);

	return APM_ERR_NOEVENTS;
}

static void
hpcapm_cpu_busy(void *scx)
{
}

static void
hpcapm_cpu_idle(void *scx)
{
}

static void
hpcapm_get_capabilities(void *scx, u_int *numbatts, u_int *capflags)
{
	*numbatts = 0;
	*capflags = APM_GLOBAL_STANDBY | APM_GLOBAL_SUSPEND;
}
