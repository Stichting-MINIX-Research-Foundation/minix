/*	$NetBSD: weasel_isa.c,v 1.6 2015/08/20 14:40:18 christos Exp $	*/

/*-
 * Copyright (c) 2000 Zembu Labs, Inc.
 * All rights reserved.
 *
 * Author: Jason R. Thorpe <thorpej@zembu.com>
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
 *	This product includes software developed by Zembu Labs, Inc.
 * 4. Neither the name of Zembu Labs nor the names of its employees may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ZEMBU LABS, INC. ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WAR-
 * RANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DIS-
 * CLAIMED.  IN NO EVENT SHALL ZEMBU LABS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Device driver for the Middle Digital, Inc. PC-Weasel serial
 * console board.
 *
 * We're glued into the MDA display driver (`pcdisplay'), and
 * handle things like the watchdog timer.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: weasel_isa.c,v 1.6 2015/08/20 14:40:18 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/wdog.h>

#include <sys/bus.h>

#include <dev/isa/weaselreg.h>
#include <dev/isa/weaselvar.h>

#include <dev/sysmon/sysmonvar.h>

#include "ioconf.h"

int	weasel_isa_wdog_setmode(struct sysmon_wdog *);
int	weasel_isa_wdog_tickle(struct sysmon_wdog *);
int	weasel_isa_wdog_arm_disarm(struct weasel_handle *, u_int8_t);
int	weasel_isa_wdog_query_state(struct weasel_handle *);


/* ARGSUSED */
void
pcweaselattach(int count)
{

	/* Nothing to do; pseudo-device glue. */
}

void
weasel_isa_init(struct weasel_handle *wh)
{
	struct weasel_config_block cfg;
	int i, j;
	u_int8_t *cp, sum;
	const char *vers, *mode;

	/*
	 * Write a NOP to the command register and see if it
	 * reverts back to READY within 1.5 seconds.
	 */
	bus_space_write_1(wh->wh_st, wh->wh_sh, WEASEL_MISC_COMMAND, OS_NOP);
	for (i = 0; i < 1500; i++) {
		delay(1000);
		sum = bus_space_read_1(wh->wh_st, wh->wh_sh,
		    WEASEL_MISC_COMMAND);
		if (sum == OS_READY)
			break;
	}
	if (sum != OS_READY) {
		/* This is not a Weasel. */
		return;
	}

	/*
	 * It can take a while for the config block to be copied
	 * into the offscreen area, as the Weasel may be busy
	 * sending data to the terminal.  Wait up to 3 seconds,
	 * reading the block each time, and breaking out of the
	 * loop once the checksum passes.
	 */

	bus_space_write_1(wh->wh_st, wh->wh_sh, WEASEL_MISC_COMMAND,
	    OS_CONFIG_COPY);

	/* ...one second to get it started... */
	delay(1000 * 1000);

	/* ...two seconds to let it finish... */
	for (i = 0; i < 2000; i++) {
		delay(1000);
		bus_space_read_region_1(wh->wh_st, wh->wh_sh,
		    WEASEL_CONFIG_BLOCK, &cfg, sizeof(cfg));
		/*
		 * Compute the checksum of the config block.
		 */
		for (cp = (u_int8_t *)&cfg, j = 0, sum = 1;
		     j < (sizeof(cfg) - 1); j++)
			sum += cp[j];
		if (sum == cfg.cksum)
			break;
	}

	if (sum != cfg.cksum) {
		/*
		 * Checksum doesn't match; either it's not a Weasel,
		 * or something is wrong with it.
		 */
		printf("%s: PC-Weasel config block checksum mismatch "
		    "0x%02x != 0x%02x\n", device_xname(wh->wh_parent),
		    sum, cfg.cksum);
		return;
	}

	switch (cfg.cfg_version) {
	case CFG_VERSION_1_0:
		vers = "1.0";
		switch (cfg.enable_duart_switching) {
		case 0:
			mode = "emulation";
			break;

		case 1:
			mode = "autoswitch";
			break;

		default:
			mode = "unknown";
		}
		break;

	case CFG_VERSION_1_1:
		vers = "1.1";
		switch (cfg.enable_duart_switching) {
		case 0:
			mode = "emulation";
			break;

		case 1:
			mode = "serial";
			break;

		case 2:
			mode = "autoswitch";
			break;

		default:
			mode = "unknown";
		}
		break;

	default:
		vers = mode = NULL;
	}

	printf("%s: PC-Weasel, ", device_xname(wh->wh_parent));
	if (vers != NULL)
		printf("version %s, %s mode", vers, mode);
	else
		printf("unknown version 0x%x", cfg.cfg_version);
	printf("\n");

	printf("%s: break passthrough %s", device_xname(wh->wh_parent),
	    cfg.break_passthru ? "enabled" : "disabled");
	if (cfg.wdt_msec == 0) {
		/*
		 * Old firmware -- these Weasels have
		 * a 3000ms watchdog period.
		 */
		cfg.wdt_msec = 3000;
	}

	if ((wh->wh_wdog_armed = weasel_isa_wdog_query_state(wh)) == -1)
		wh->wh_wdog_armed = 0;
	wh->wh_wdog_period = cfg.wdt_msec / 1000;

	printf(", watchdog interval %d sec.\n", wh->wh_wdog_period);

	/*
	 * Always register the Weasel watchdog timer in case user decides
	 * to set 'allow watchdog' to 'YES' after the machine has booted.
	 */
	wh->wh_smw.smw_name = "weasel";
	wh->wh_smw.smw_cookie = wh;
	wh->wh_smw.smw_setmode = weasel_isa_wdog_setmode;
	wh->wh_smw.smw_tickle = weasel_isa_wdog_tickle;
	wh->wh_smw.smw_period = wh->wh_wdog_period;

	if (sysmon_wdog_register(&wh->wh_smw) != 0)
		aprint_error_dev(wh->wh_parent, "unable to register PC-Weasel watchdog "
		    "with sysmon\n");
}

int
weasel_isa_wdog_setmode(struct sysmon_wdog *smw)
{
	struct weasel_handle *wh = smw->smw_cookie;
	int error = 0;

	if ((smw->smw_mode & WDOG_MODE_MASK) == WDOG_MODE_DISARMED) {
		error = weasel_isa_wdog_arm_disarm(wh, WDT_DISABLE);
	} else {
		if (smw->smw_period == WDOG_PERIOD_DEFAULT)
			smw->smw_period = wh->wh_wdog_period;
		else if (smw->smw_period != wh->wh_wdog_period) {
			/* Can't change the period on the Weasel. */
			return (EINVAL);
		}
		error = weasel_isa_wdog_arm_disarm(wh, WDT_ENABLE);
		weasel_isa_wdog_tickle(smw);
	}

	return (error);
}

int
weasel_isa_wdog_tickle(struct sysmon_wdog *smw)
{
	struct weasel_handle *wh = smw->smw_cookie;
	u_int8_t reg;
	int x;
	int s;
	int error = 0;

	s = splhigh();
	/*
	 * first we tickle the watchdog
	 */
	reg = bus_space_read_1(wh->wh_st, wh->wh_sh, WEASEL_WDT_TICKLE);
	bus_space_write_1(wh->wh_st, wh->wh_sh, WEASEL_WDT_TICKLE, ~reg);

	/*
	 * then we check to make sure the weasel is still armed. If someone
	 * has rebooted the weasel for whatever reason (firmware update),
	 * then the watchdog timer would no longer be armed and we'd be
	 * servicing nothing. Let the user know that the machine is no
	 * longer being monitored by the weasel.
	 */
	if((x = weasel_isa_wdog_query_state(wh)) == -1)
		error = EIO;
	if (x == 1) {
		error = 0;
	} else {
		aprint_error_dev(wh->wh_parent, "Watchdog timer disabled on PC/Weasel! Disarming wdog.\n");
		wh->wh_wdog_armed = 0;
		error = 1;
	}
	splx(s);

	return (error);
}

int
weasel_isa_wdog_arm_disarm(struct weasel_handle *wh, u_int8_t mode)
{
	u_int8_t reg;
	int timeout;
	int s, x;
	int error = 0;

	s = splhigh();

	bus_space_write_1(wh->wh_st, wh->wh_sh, WEASEL_WDT_SEMAPHORE,
		WDT_ATTENTION);
	for (timeout = 5000; timeout; timeout--) {
		delay(1500);
		reg = bus_space_read_1(wh->wh_st, wh->wh_sh,
			WEASEL_WDT_SEMAPHORE);
		if (reg == WDT_OK)
			break;
	}
	if (timeout == 0) {
		splx(s);
		return(EIO);
	}
	bus_space_write_1(wh->wh_st, wh->wh_sh, WEASEL_WDT_SEMAPHORE, mode);
	for (timeout = 500 ; timeout; timeout--) {
		delay(1500);
		reg = bus_space_read_1(wh->wh_st, wh->wh_sh,
			WEASEL_WDT_SEMAPHORE);
		if (reg != mode)
			break;
	}
	if (timeout == 0) {
		splx(s);
		return(EIO);
	}
	bus_space_write_1(wh->wh_st, wh->wh_sh, WEASEL_WDT_SEMAPHORE, ~reg);
	for (timeout = 500; timeout; timeout--) {
		delay(1500);
		reg = bus_space_read_1(wh->wh_st, wh->wh_sh,
			WEASEL_WDT_SEMAPHORE);
		if (reg == WDT_OK)
			break;
	}

	/*
	 * Ensure that the Weasel thinks it's in the same mode we want it to
	 * be in.   EIO if not.
	 */
	x = weasel_isa_wdog_query_state(wh);
	switch (x) {
		case -1:
			error = EIO;
			break;
		case 0:
			if (mode == WDT_DISABLE) {
				wh->wh_wdog_armed = 0;
				error = 0;
			} else
				error = EIO;
			break;
		case 1:
			if (mode == WDT_ENABLE) {
				wh->wh_wdog_armed = 1;
				error = 0;
			} else
				error = EIO;
			break;
	}

	splx(s);
	return(error);
}

int
weasel_isa_wdog_query_state(struct weasel_handle *wh)
{
	int timeout, reg;

	bus_space_write_1(wh->wh_st, wh->wh_sh,
		WEASEL_MISC_COMMAND, OS_WDT_QUERY);
	for (timeout = 0; timeout < 1500; timeout++) {
		delay(1000);
		reg = bus_space_read_1(wh->wh_st, wh->wh_sh,
			WEASEL_MISC_COMMAND);
		if (reg == OS_READY)
			break;
	}
	return(bus_space_read_1(wh->wh_st, wh->wh_sh, WEASEL_MISC_RESPONSE));
}
