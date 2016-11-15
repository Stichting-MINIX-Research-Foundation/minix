/*	$NetBSD: ath_netbsd.c,v 1.22 2014/02/25 18:30:09 pooka Exp $ */

/*-
 * Copyright (c) 2003, 2004 David Young
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ath_netbsd.c,v 1.22 2014/02/25 18:30:09 pooka Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/callout.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/device.h>
#include <sys/module.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/if_ether.h>
#include <net/if_llc.h>

#include <net80211/ieee80211_netbsd.h>
#include <net80211/ieee80211_var.h>
#include <dev/ic/ath_netbsd.h>
#include <dev/ic/athvar.h>

/*
 * Setup sysctl(3) MIB, hw.ath.*.
 *
 * TBD condition CTLFLAG_PERMANENT on being a module or not
 */
SYSCTL_SETUP(sysctl_ath, "sysctl ath subtree setup")
{
	int rc;
	const struct sysctlnode *cnode, *rnode;

	if ((rnode = ath_sysctl_treetop(clog)) == NULL)
		return;

	if ((rc = SYSCTL_GLOBAL_INT(CTLFLAG_READWRITE, "dwell",
	    "channel dwell time (ms) for AP/station scanning",
	    dwelltime)) != 0)
		goto err;

	if ((rc = SYSCTL_GLOBAL_INT(CTLFLAG_READWRITE, "calibrate",
	    "chip calibration interval (secs)", calinterval)) != 0)
		goto err;

	if ((rc = SYSCTL_GLOBAL_INT(CTLFLAG_READWRITE, "outdoor",
	    "outdoor operation", outdoor)) != 0)
		goto err;

	/* country code */
	if ((rc = SYSCTL_GLOBAL_INT(CTLFLAG_READWRITE,
	    "countrycode", "country code", countrycode)) != 0)
		goto err;

	/* regulatory domain */
	if ((rc = SYSCTL_GLOBAL_INT(CTLFLAG_READWRITE, "regdomain",
	    "EEPROM regdomain code", regdomain)) != 0)
		goto err;

	if ((rc = SYSCTL_GLOBAL_INT(CTLFLAG_READWRITE, "debug",
	    "control debugging printfs", debug)) != 0)
		goto err;

	if ((rc = SYSCTL_GLOBAL_INT(CTLFLAG_READONLY, "rxbuf",
	    "rx buffers allocated", rxbuf)) != 0)
		goto err;
	if ((rc = SYSCTL_GLOBAL_INT(CTLFLAG_READONLY, "txbuf",
	    "tx buffers allocated", txbuf)) != 0)
		goto err;

	return;
err:
	printf("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}

static int
ath_sysctl_slottime(SYSCTLFN_ARGS)
{
	struct ath_softc *sc;
	struct sysctlnode node;
	u_int slottime;
	int error;

	node = *rnode;
	sc = (struct ath_softc *)node.sysctl_data;
	slottime = ath_hal_getslottime(sc->sc_ah);
	node.sysctl_data = &slottime;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	return !ath_hal_setslottime(sc->sc_ah, slottime) ? EINVAL : 0;
}

static int
ath_sysctl_acktimeout(SYSCTLFN_ARGS)
{
	struct ath_softc *sc;
	struct sysctlnode node;
	u_int acktimeout;
	int error;

	node = *rnode;
	sc = (struct ath_softc *)node.sysctl_data;
	acktimeout = ath_hal_getacktimeout(sc->sc_ah);
	node.sysctl_data = &acktimeout;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	return !ath_hal_setacktimeout(sc->sc_ah, acktimeout) ? EINVAL : 0;
}

static int
ath_sysctl_ctstimeout(SYSCTLFN_ARGS)
{
	struct ath_softc *sc;
	struct sysctlnode node;
	u_int ctstimeout;
	int error;

	node = *rnode;
	sc = (struct ath_softc *)node.sysctl_data;
	ctstimeout = ath_hal_getctstimeout(sc->sc_ah);
	node.sysctl_data = &ctstimeout;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	return !ath_hal_setctstimeout(sc->sc_ah, ctstimeout) ? EINVAL : 0;
}

static int
ath_sysctl_softled(SYSCTLFN_ARGS)
{
	struct ath_softc *sc;
	struct sysctlnode node;
	int softled;
	int error;

	node = *rnode;
	sc = (struct ath_softc *)node.sysctl_data;
	softled = sc->sc_softled;
	node.sysctl_data = &softled;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	softled = (softled != 0);
	if (softled != sc->sc_softled) {
		if (softled) {
			/* NB: handle any sc_ledpin change */
			ath_hal_gpioCfgOutput(sc->sc_ah, sc->sc_ledpin,
			    HAL_GPIO_MUX_MAC_NETWORK_LED);
			ath_hal_gpioset(sc->sc_ah, sc->sc_ledpin,
				!sc->sc_ledon);
		}
		sc->sc_softled = softled;
	}
	return 0;
}

static int
ath_sysctl_rxantenna(SYSCTLFN_ARGS)
{
	struct ath_softc *sc;
	struct sysctlnode node;
	u_int defantenna;
	int error;

	node = *rnode;
	sc = (struct ath_softc *)node.sysctl_data;
	defantenna = ath_hal_getdefantenna(sc->sc_ah);
	node.sysctl_data = &defantenna;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	ath_hal_setdefantenna(sc->sc_ah, defantenna);
	return 0;
}

static int
ath_sysctl_diversity(SYSCTLFN_ARGS)
{
	struct ath_softc *sc;
	struct sysctlnode node;
	u_int diversity;
	int error;

	node = *rnode;
	sc = (struct ath_softc *)node.sysctl_data;
	diversity = sc->sc_diversity;
	node.sysctl_data = &diversity;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	if (!ath_hal_setdiversity(sc->sc_ah, diversity))
		return EINVAL;
	sc->sc_diversity = diversity;
	return 0;
}

static int
ath_sysctl_diag(SYSCTLFN_ARGS)
{
	struct ath_softc *sc;
	struct sysctlnode node;
	u_int32_t diag;
	int error;

	node = *rnode;
	sc = (struct ath_softc *)node.sysctl_data;
	if (!ath_hal_getdiag(sc->sc_ah, &diag))
		return EINVAL;
	node.sysctl_data = &diag;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	return !ath_hal_setdiag(sc->sc_ah, diag) ? EINVAL : 0;
}

static int
ath_sysctl_tpscale(SYSCTLFN_ARGS)
{
	struct ath_softc *sc;
	struct sysctlnode node;
	u_int32_t scale;
	int error;

	node = *rnode;
	sc = (struct ath_softc *)node.sysctl_data;
	(void)ath_hal_gettpscale(sc->sc_ah, &scale);
	node.sysctl_data = &scale;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	return !ath_hal_settpscale(sc->sc_ah, scale)
	    ? EINVAL
	    : ath_reset(&sc->sc_if);
}

static int
ath_sysctl_tpc(SYSCTLFN_ARGS)
{
	struct ath_softc *sc;
	struct sysctlnode node;
	u_int tpc;
	int error;

	node = *rnode;
	sc = (struct ath_softc *)node.sysctl_data;
	tpc = ath_hal_gettpc(sc->sc_ah);
	node.sysctl_data = &tpc;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	return !ath_hal_settpc(sc->sc_ah, tpc) ? EINVAL : 0;
}

static int
ath_sysctl_rfkill(SYSCTLFN_ARGS)
{
	struct ath_softc *sc;
	struct sysctlnode node;
	u_int rfkill;
	int error;

	node = *rnode;
	sc = (struct ath_softc *)node.sysctl_data;
	rfkill = ath_hal_getrfkill(sc->sc_ah);
	node.sysctl_data = &rfkill;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	return !ath_hal_setrfkill(sc->sc_ah, rfkill) ? EINVAL : 0;
}

static int
ath_sysctl_rfsilent(SYSCTLFN_ARGS)
{
	struct ath_softc *sc;
	struct sysctlnode node;
	u_int rfsilent;
	int error;

	node = *rnode;
	sc = (struct ath_softc *)node.sysctl_data;
	(void)ath_hal_getrfsilent(sc->sc_ah, &rfsilent);
	node.sysctl_data = &rfsilent;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	return !ath_hal_setrfsilent(sc->sc_ah, rfsilent) ? EINVAL : 0;
}

static int
ath_sysctl_regdomain(SYSCTLFN_ARGS)
{
	struct ath_softc *sc;
	struct sysctlnode node;
	u_int32_t rd;
	int error;

	node = *rnode;
	sc = (struct ath_softc *)node.sysctl_data;
	if (!ath_hal_getregdomain(sc->sc_ah, &rd))
		return EINVAL;
	node.sysctl_data = &rd;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	return !ath_hal_setregdomain(sc->sc_ah, rd) ? EINVAL : 0;
}

static int
ath_sysctl_tpack(SYSCTLFN_ARGS)
{
	struct ath_softc *sc;
	struct sysctlnode node;
	u_int32_t tpack;
	int error;

	node = *rnode;
	sc = (struct ath_softc *)node.sysctl_data;
	(void)ath_hal_gettpack(sc->sc_ah, &tpack);
	node.sysctl_data = &tpack;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	return !ath_hal_settpack(sc->sc_ah, tpack) ? EINVAL : 0;
}

static int
ath_sysctl_tpcts(SYSCTLFN_ARGS)
{
	struct ath_softc *sc;
	struct sysctlnode node;
	u_int32_t tpcts;
	int error;

	node = *rnode;
	sc = (struct ath_softc *)node.sysctl_data;
	(void)ath_hal_gettpcts(sc->sc_ah, &tpcts);
	node.sysctl_data = &tpcts;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	return !ath_hal_settpcts(sc->sc_ah, tpcts) ? EINVAL : 0;
}

const struct sysctlnode *
ath_sysctl_instance(const char *dvname, struct sysctllog **log)
{
	int rc;
	const struct sysctlnode *rnode;

	if ((rc = sysctl_createv(log, 0, NULL, &rnode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, dvname,
	    SYSCTL_DESCR("ath information and options"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	return rnode;
err:
	printf("%s: sysctl_createv failed, rc = %d\n", __func__, rc);
	return NULL;
}

const struct sysctlnode *
ath_sysctl_treetop(struct sysctllog **log)
{
	int rc;
	const struct sysctlnode *rnode;

	if ((rc = sysctl_createv(log, 0, NULL, &rnode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "ath",
	    SYSCTL_DESCR("ath information and options"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	return rnode;
err:
	printf("%s: sysctl_createv failed, rc = %d\n", __func__, rc);
	return NULL;
}

void
ath_sysctlattach(struct ath_softc *sc)
{
	int rc;
	struct sysctllog **log = &sc->sc_sysctllog;
	const struct sysctlnode *cnode, *rnode;

	ath_hal_getcountrycode(sc->sc_ah, &sc->sc_countrycode);
	(void)ath_hal_getregdomain(sc->sc_ah, &sc->sc_regdomain);
	sc->sc_debug = ath_debug;
	sc->sc_txintrperiod = ATH_TXINTR_PERIOD;

	if ((rnode = ath_sysctl_instance(device_xname(sc->sc_dev), log)) == NULL)
		return;

	if ((rc = SYSCTL_INT(0, countrycode, "EEPROM country code")) != 0)
		goto err;

	if ((rc = SYSCTL_INT(CTLFLAG_READWRITE, debug,
	    "control debugging printfs")) != 0)
		goto err;

#if 0
	/* channel dwell time (ms) for AP/station scanning */
	if ((rc = SYSCTL_INT(CTLFLAG_READWRITE, dwell,
	    "Channel dwell time (ms) for scanning")) != 0)
		goto err;
#endif

	if ((rc = SYSCTL_INT_SUBR(CTLFLAG_READWRITE, slottime,
	    "802.11 slot time (us)")) != 0)
		goto err;
	if ((rc = SYSCTL_INT_SUBR(CTLFLAG_READWRITE, acktimeout,
	    "802.11 ACK timeout (us)")) != 0)
		goto err;
	if ((rc = SYSCTL_INT_SUBR(CTLFLAG_READWRITE, ctstimeout,
	    "802.11 CTS timeout (us)")) != 0)
		goto err;
	if ((rc = SYSCTL_INT_SUBR(CTLFLAG_READWRITE, softled,
	    "enable/disable software LED support")) != 0)
		goto err;
	if ((rc = SYSCTL_INT(CTLFLAG_READWRITE, ledpin,
	    "GPIO pin connected to LED")) != 0)
		goto err;
	if ((rc = SYSCTL_INT(CTLFLAG_READWRITE, ledon,
	    "setting to turn LED on")) != 0)
		goto err;
	if ((rc = SYSCTL_INT(CTLFLAG_READWRITE, ledidle,
	    "idle time for inactivity LED (ticks)")) != 0)
		goto err;
	if ((rc = SYSCTL_INT(CTLFLAG_READWRITE, txantenna,
	    "tx antenna (0=auto)")) != 0)
		goto err;
	if ((rc = SYSCTL_INT_SUBR(CTLFLAG_READWRITE, rxantenna,
	    "default/rx antenna")) != 0)
		goto err;
	if (ath_hal_hasdiversity(sc->sc_ah)) {
		if ((rc = SYSCTL_INT_SUBR(CTLFLAG_READWRITE, diversity,
		    "antenna diversity")) != 0)
			goto err;
	}
	if ((rc = SYSCTL_INT(CTLFLAG_READWRITE, txintrperiod,
	    "tx descriptor batching")) != 0)
		goto err;
	if ((rc = SYSCTL_INT_SUBR(CTLFLAG_READWRITE, diag,
	    "h/w diagnostic control")) != 0)
		goto err;
	if ((rc = SYSCTL_INT_SUBR(CTLFLAG_READWRITE, tpscale,
	    "tx power scaling")) != 0)
		goto err;
	if (ath_hal_hastpc(sc->sc_ah)) {
		if ((rc = SYSCTL_INT_SUBR(CTLFLAG_READWRITE, tpc,
		    "enable/disable per-packet TPC")) != 0)
			goto err;
		if ((rc = SYSCTL_INT_SUBR(CTLFLAG_READWRITE, tpack,
		    "tx power for ack frames")) != 0)
			goto err;
		if ((rc = SYSCTL_INT_SUBR(CTLFLAG_READWRITE, tpcts,
		    "tx power for cts frames")) != 0)
			goto err;
	}
	if (ath_hal_hasrfsilent(sc->sc_ah)) {
		if ((rc = SYSCTL_INT_SUBR(CTLFLAG_READWRITE, rfsilent,
		    "h/w RF silent config")) != 0)
			goto err;
		if ((rc = SYSCTL_INT_SUBR(CTLFLAG_READWRITE, rfkill,
		    "enable/disable RF kill switch")) != 0)
			goto err;
	}
	if ((rc = SYSCTL_INT_SUBR(CTLFLAG_READWRITE, regdomain,
	    "EEPROM regdomain code")) != 0)
		goto err;
	return;
err:
	printf("%s: sysctl_createv failed, rc = %d\n", __func__, rc);
}

MODULE(MODULE_CLASS_MISC, ath, "ath_hal");

static int
ath_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
	case MODULE_CMD_FINI:
		return 0;
	default:
		return ENOTTY;
	}
}
