/*	$NetBSD: weasel_pci.c,v 1.15 2013/10/17 21:06:15 christos Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Herb Peyerl and Jason Thorpe.
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
 * Device driver for the control space on the Middle Digital, Inc.
 * PCI-Weasel serial console board.
 *
 * Since the other functions of the PCI-Weasel already appear in
 * PCI configuration space, we just need to hook up the watchdog
 * timer.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: weasel_pci.c,v 1.15 2013/10/17 21:06:15 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/wdog.h>
#include <sys/endian.h>

#include <sys/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/weaselreg.h>

#include <dev/sysmon/sysmonvar.h>

struct weasel_softc {
	device_t sc_dev;
	bus_space_tag_t sc_st;
	bus_space_handle_t sc_sh;

	struct sysmon_wdog sc_smw;

	int sc_wdog_armed;
	int sc_wdog_period;
};

/* XXX */
extern int	sysmon_wdog_setmode(struct sysmon_wdog *, int, u_int);

static int	weasel_pci_wdog_setmode(struct sysmon_wdog *);
static int	weasel_pci_wdog_tickle(struct sysmon_wdog *);

static int	weasel_wait_response(struct weasel_softc *);
static int	weasel_issue_command(struct weasel_softc *, uint8_t cmd);

static int	weasel_pci_wdog_arm(struct weasel_softc *);
static int	weasel_pci_wdog_disarm(struct weasel_softc *);

static int	weasel_pci_wdog_query_state(struct weasel_softc *);

static int
weasel_pci_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_MIDDLE_DIGITAL &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_MIDDLE_DIGITAL_WEASEL_CONTROL)
		return (1);

	return (0);
}

static void
weasel_pci_attach(device_t parent, device_t self, void *aux)
{
	struct weasel_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	struct weasel_config_block cfg;
	const char *vers, *mode;
	uint8_t v, *cp;
	uint16_t cfg_size;
	uint8_t buf[8];

	sc->sc_dev = self;

	printf(": PCI-Weasel watchdog timer\n");

	if (pci_mapreg_map(pa, PCI_MAPREG_START,
	    PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &sc->sc_st, &sc->sc_sh, NULL, NULL) != 0) {
		aprint_error_dev(self, "unable to map device registers\n");
		return;
	}

	/* Ping the Weasel to see if it's alive. */
	if (weasel_issue_command(sc, OS_CMD_PING)) {
		aprint_error_dev(self, "Weasel didn't respond to PING\n");
		return;
	}
	bus_space_write_1(sc->sc_st, sc->sc_sh, WEASEL_STATUS, 0);
	if ((v = bus_space_read_1(sc->sc_st, sc->sc_sh, WEASEL_DATA_RD)) !=
	    OS_RET_PONG) {
		aprint_error_dev(self, "unexpected PING response from Weasel: 0x%02x\n", v);
		return;
	}

	/* Read the config block. */
	if (weasel_issue_command(sc, OS_CMD_SHOW_CONFIG)) {
		aprint_error_dev(self, "Weasel didn't respond to SHOW_CONFIG\n");
		return;
	}
	cfg_size = bus_space_read_1(sc->sc_st, sc->sc_sh, WEASEL_DATA_RD);
	bus_space_write_1(sc->sc_st, sc->sc_sh, WEASEL_STATUS, 0);

	if (++cfg_size != sizeof(cfg)) {
		aprint_error_dev(self, "weird config block size from Weasel: 0x%03x\n", cfg_size);
		return;
	}

	for (cp = (uint8_t *) &cfg; cfg_size != 0; cfg_size--) {
		if (weasel_wait_response(sc)) {
			aprint_error_dev(self, "Weasel stopped providing config block(%d)\n", cfg_size);
			return;
		}
		*cp++ = bus_space_read_1(sc->sc_st, sc->sc_sh, WEASEL_DATA_RD);
		bus_space_write_1(sc->sc_st, sc->sc_sh, WEASEL_STATUS, 0);
	}

	switch (cfg.cfg_version) {
	case CFG_VERSION_2:
		vers="2";
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

	if (vers != NULL)
		printf("%s: %s mode\n", device_xname(self), mode);
	else
		printf("%s: unknown config version 0x%02x\n", device_xname(self),
		    cfg.cfg_version);

	/*
	 * Fetch sw version.
	 */
	if (weasel_issue_command(sc, OS_CMD_QUERY_SW_VER)) {
		aprint_error_dev(self, "didn't reply to software version query.\n");
	}
	else {
		v = bus_space_read_1(sc->sc_st, sc->sc_sh, WEASEL_DATA_RD);
		bus_space_write_1(sc->sc_st, sc->sc_sh, WEASEL_STATUS, 0);
		if (v>7)
			printf("%s: weird length for version string(%d).\n",
			    device_xname(self), v);
		memset(buf, 0, sizeof(buf));
		for (cp = buf; v != 0; v--) {
			if (weasel_wait_response(sc)) {
				printf("%s: Weasel stopped providing version\n",
				    device_xname(self));
			}
			*cp++ = bus_space_read_1(sc->sc_st, sc->sc_sh, WEASEL_DATA_RD);
			bus_space_write_1(sc->sc_st, sc->sc_sh, WEASEL_STATUS, 0);
		}
		printf("%s: sw: %s", device_xname(self), buf);
	}
	/*
	 * Fetch logic version.
	 */
	if (weasel_issue_command(sc, OS_CMD_QUERY_L_VER)) {
		aprint_normal("\n");
		aprint_error_dev(self, "didn't reply to logic version query.\n");
	}
	bus_space_write_1(sc->sc_st, sc->sc_sh, WEASEL_STATUS, 0);
	v = bus_space_read_1(sc->sc_st, sc->sc_sh, WEASEL_DATA_RD);
	printf(" logic: %03d", v);
	/*
	 * Fetch vga bios version.
	 */
	if (weasel_issue_command(sc, OS_CMD_QUERY_VB_VER)) {
		aprint_normal("\n");
		aprint_error_dev(self, "didn't reply to vga bios version query.\n");
	}
	v = bus_space_read_1(sc->sc_st, sc->sc_sh, WEASEL_DATA_RD);
	bus_space_write_1(sc->sc_st, sc->sc_sh, WEASEL_STATUS, 0);
	printf(" vga bios: %d.%d", (v>>4), (v&0x0f));
	/*
	 * Fetch hw version.
	 */
	if (weasel_issue_command(sc, OS_CMD_QUERY_HW_VER)) {
		aprint_normal("\n");
		aprint_error_dev(self, "didn't reply to hardware version query.\n");
	}
	v = bus_space_read_1(sc->sc_st, sc->sc_sh, WEASEL_DATA_RD);
	bus_space_write_1(sc->sc_st, sc->sc_sh, WEASEL_STATUS, 0);
	printf(" hw: %d.%d", (v>>4), (v&0x0f));

	printf("\n%s: break passthrough %s", device_xname(self),
	    cfg.break_passthru ? "enabled" : "disabled");

	if ((sc->sc_wdog_armed = weasel_pci_wdog_query_state(sc)) == -1)
		sc->sc_wdog_armed = 0;

	/* Weasel is big-endian */
	sc->sc_wdog_period = be16toh(cfg.wdt_msec) / 1000;

	printf(", watchdog timer %d sec.\n", sc->sc_wdog_period);
	sc->sc_smw.smw_name = "weasel";
	sc->sc_smw.smw_cookie = sc;
	sc->sc_smw.smw_setmode = weasel_pci_wdog_setmode;
	sc->sc_smw.smw_tickle = weasel_pci_wdog_tickle;
	sc->sc_smw.smw_period = sc->sc_wdog_period;

	if (sysmon_wdog_register(&sc->sc_smw) != 0)
		aprint_error_dev(self, "unable to register PC-Weasel watchdog "
		    "with sysmon\n");
}

CFATTACH_DECL_NEW(weasel_pci, sizeof(struct weasel_softc),
    weasel_pci_match, weasel_pci_attach, NULL, NULL);

static int
weasel_wait_response(struct weasel_softc *sc)
{
	int i;

	for (i = 10000; i ; i--) {
		delay(100);
		if (bus_space_read_1(sc->sc_st, sc->sc_sh, WEASEL_STATUS) ==
		    OS_WS_HOST_READ)
			return(0);
	}
	return (1);
}

static int
weasel_issue_command(struct weasel_softc *sc, uint8_t cmd)
{
	bus_space_write_1(sc->sc_st, sc->sc_sh, WEASEL_DATA_WR, cmd);
	bus_space_write_1(sc->sc_st, sc->sc_sh, WEASEL_HOST_STATUS, OS_HS_WEASEL_READ);
	bus_space_write_1(sc->sc_st, sc->sc_sh, WEASEL_STATUS, 0);
	return (weasel_wait_response(sc));
}

static int
weasel_pci_wdog_setmode(struct sysmon_wdog *smw)
{
	struct weasel_softc *sc = smw->smw_cookie;
	int error = 0;

	if ((smw->smw_mode & WDOG_MODE_MASK) == WDOG_MODE_DISARMED) {
		error = weasel_pci_wdog_disarm(sc);
	} else {
		if (smw->smw_period == WDOG_PERIOD_DEFAULT)
			smw->smw_period = sc->sc_wdog_period;
		else if (smw->smw_period != sc->sc_wdog_period) {
			/* Can't change the period on the Weasel. */
			return (EINVAL);
		}
		error = weasel_pci_wdog_arm(sc);
		weasel_pci_wdog_tickle(smw);
	}

	return (error);
}

static int
weasel_pci_wdog_tickle(struct sysmon_wdog *smw)
{
	struct weasel_softc *sc = smw->smw_cookie;
	u_int8_t reg;
	int x;
	int s;
	int error = 0;

	s = splhigh();
	/*
	 * first we tickle the watchdog
	 */
	reg = bus_space_read_1(sc->sc_st, sc->sc_sh, WEASEL_CHALLENGE);
	bus_space_write_1(sc->sc_st, sc->sc_sh, WEASEL_RESPONSE, ~reg);

	/*
	 * then we check to make sure the weasel is still armed. If someone
	 * has rebooted the weasel for whatever reason (firmware update),
	 * then the watchdog timer would no longer be armed and we'd be
	 * servicing nothing. Let the user know that the machine is no
	 * longer being monitored by the weasel.
	 */
	if((x = weasel_pci_wdog_query_state(sc)) == -1)
		error = EIO;
	if (x == 1) {
		error = 0;
	} else {
		printf("%s: Watchdog timer disabled on PC/Weasel! Disarming wdog.\n",
			device_xname(sc->sc_dev));
		sc->sc_wdog_armed = 0;
		sysmon_wdog_setmode(smw, WDOG_MODE_DISARMED, 0);
		error = 1;
	}
	splx(s);

	return (error);
}

static int
weasel_pci_wdog_arm(struct weasel_softc *sc)
{
	int x;
	int s;
	int error = 0;

	s = splhigh();
	if (weasel_issue_command(sc, OS_CMD_WDT_ENABLE)) {
		printf("%s: no reply to watchdog enable. Check Weasel \"Allow Watchdog\" setting.\n",
			device_xname(sc->sc_dev));
		error = EIO;
	}
	(void)bus_space_read_1(sc->sc_st, sc->sc_sh, WEASEL_DATA_RD);
	bus_space_write_1(sc->sc_st, sc->sc_sh, WEASEL_STATUS, 0);

	/*
	 * Ensure that the Weasel thinks it's in the same mode we want it to
	 * be in.   EIO if not.
	 */
	x = weasel_pci_wdog_query_state(sc);
	switch (x) {
		case -1:
			error = EIO;
			break;
		case 0:
			sc->sc_wdog_armed = 0;
			error = EIO;
			break;
		case 1:
			sc->sc_wdog_armed = 1;
			error = 0;
			break;
	}

	splx(s);
	return(error);
}


static int
weasel_pci_wdog_disarm(struct weasel_softc *sc)
{
	int x;
	int s;
	int error = 0;

	s = splhigh();

	if (weasel_issue_command(sc, OS_CMD_WDT_DISABLE)) {
		printf("%s: didn't reply to watchdog disable.\n",
			device_xname(sc->sc_dev));
		error = EIO;
	}
	(void)bus_space_read_1(sc->sc_st, sc->sc_sh, WEASEL_DATA_RD);
	bus_space_write_1(sc->sc_st, sc->sc_sh, WEASEL_STATUS, 0);

	/*
	 * Ensure that the Weasel thinks it's in the same mode we want it to
	 * be in.   EIO if not.
	 */
	x = weasel_pci_wdog_query_state(sc);
	switch (x) {
		case -1:
			error = EIO;
			break;
		case 0:
			sc->sc_wdog_armed = 0;
			error = 0;
			break;
		case 1:
			sc->sc_wdog_armed = 1;
			error = EIO;
			break;
	}

	splx(s);
	return(error);
}

static int
weasel_pci_wdog_query_state(struct weasel_softc *sc)
{

	u_int8_t v;

	if (weasel_issue_command(sc, OS_CMD_WDT_QUERY)) {
		printf("%s: didn't reply to watchdog state query.\n",
			device_xname(sc->sc_dev));
		bus_space_write_1(sc->sc_st, sc->sc_sh, WEASEL_STATUS, 0);
		return(-1);
	}
	v = bus_space_read_1(sc->sc_st, sc->sc_sh, WEASEL_DATA_RD);
	bus_space_write_1(sc->sc_st, sc->sc_sh, WEASEL_STATUS, 0);
	return(v);
}
