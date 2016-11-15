/* $NetBSD: elantech.c,v 1.6 2014/02/25 18:30:10 pooka Exp $ */

/*-
 * Copyright (c) 2008 Jared D. McNeill <jmcneill@invisible.ca>
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

#include "opt_pms.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: elantech.c,v 1.6 2014/02/25 18:30:10 pooka Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/bus.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#include <dev/pckbport/pckbportvar.h>
#include <dev/pckbport/elantechreg.h>
#include <dev/pckbport/elantechvar.h>
#include <dev/pckbport/pmsreg.h>
#include <dev/pckbport/pmsvar.h>

/* #define ELANTECH_DEBUG */

static int elantech_xy_unprecision_nodenum;
static int elantech_z_unprecision_nodenum;

static int elantech_xy_unprecision = 2;
static int elantech_z_unprecision = 3;

struct elantech_packet {
	int16_t		ep_x, ep_y, ep_z;
	int8_t		ep_buttons;
	uint8_t		ep_nfingers;
};

static int
pms_sysctl_elantech_verify(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;

	node = *rnode;
	t = *(int *)rnode->sysctl_data;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (node.sysctl_num == elantech_xy_unprecision_nodenum ||
	    node.sysctl_num == elantech_z_unprecision_nodenum) {
		if (t < 0 || t > 7)
			return EINVAL;
	} else
		return EINVAL;

	*(int *)rnode->sysctl_data = t;

	return 0;
}

static void
pms_sysctl_elantech(struct sysctllog **clog)
{
	const struct sysctlnode *node;
	int rc, root_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "elantech",
	    SYSCTL_DESCR("Elantech touchpad controls"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	root_num = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "xy_precision_shift",
	    SYSCTL_DESCR("X/Y-axis precision shift value"),
	    pms_sysctl_elantech_verify, 0,
	    &elantech_xy_unprecision,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	elantech_xy_unprecision_nodenum = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "z_precision_shift",
	    SYSCTL_DESCR("Z-axis precision shift value"),
	    pms_sysctl_elantech_verify, 0,
	    &elantech_z_unprecision,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	elantech_z_unprecision_nodenum = node->sysctl_num;
	return;

err:
	aprint_error("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}

static int
pms_elantech_read_1(pckbport_tag_t tag, pckbport_slot_t slot, uint8_t reg,
    uint8_t *val)
{
	int res;
	uint8_t cmd;
	uint8_t resp[3];

	cmd = ELANTECH_CUSTOM_CMD;
	res = pckbport_poll_cmd(tag, slot, &cmd, 1, 0, NULL, 0);
	cmd = ELANTECH_REG_READ;
	res |= pckbport_poll_cmd(tag, slot, &cmd, 1, 0, NULL, 0);
	cmd = ELANTECH_CUSTOM_CMD;
	res |= pckbport_poll_cmd(tag, slot, &cmd, 1, 0, NULL, 0);
	cmd = reg;
	res |= pckbport_poll_cmd(tag, slot, &cmd, 1, 0, NULL, 0);
	cmd = PMS_SEND_DEV_STATUS;
	res |= pckbport_poll_cmd(tag, slot, &cmd, 1, 3, resp, 0);

	if (res == 0)
		*val = resp[0];

	return res;
}

static int
pms_elantech_write_1(pckbport_tag_t tag, pckbport_slot_t slot, uint8_t reg,
    uint8_t val)
{
	int res;
	uint8_t cmd;

	cmd = ELANTECH_CUSTOM_CMD;
	res = pckbport_poll_cmd(tag, slot, &cmd, 1, 0, NULL, 0);
	cmd = ELANTECH_REG_WRITE;
	res |= pckbport_poll_cmd(tag, slot, &cmd, 1, 0, NULL, 0);
	cmd = ELANTECH_CUSTOM_CMD;
	res |= pckbport_poll_cmd(tag, slot, &cmd, 1, 0, NULL, 0);
	cmd = reg;
	res |= pckbport_poll_cmd(tag, slot, &cmd, 1, 0, NULL, 0);
	cmd = ELANTECH_CUSTOM_CMD;
	res |= pckbport_poll_cmd(tag, slot, &cmd, 1, 0, NULL, 0);
	cmd = val;
	res |= pckbport_poll_cmd(tag, slot, &cmd, 1, 0, NULL, 0);
	cmd = PMS_SET_SCALE11;
	res |= pckbport_poll_cmd(tag, slot, &cmd, 1, 0, NULL, 0);

	return res;
}

static int
pms_elantech_init(struct pms_softc *psc)
{
	uint8_t val;
	int res;

	/* set absolute mode */
	res = pms_elantech_write_1(psc->sc_kbctag, psc->sc_kbcslot, 0x10, 0x54);
	if (res)
		return res;
	res = pms_elantech_write_1(psc->sc_kbctag, psc->sc_kbcslot, 0x11, 0x88);
	if (res)
		return res;
	res = pms_elantech_write_1(psc->sc_kbctag, psc->sc_kbcslot, 0x21, 0x60);
	if (res)
		return res;

	res = pms_elantech_read_1(psc->sc_kbctag, psc->sc_kbcslot, 0x10, &val);

	if (res)
		aprint_error_dev(psc->sc_dev, "couldn't set absolute mode\n");

	return res;
}

static void
pms_elantech_input(void *opaque, int data)
{
	struct pms_softc *psc = opaque;
	struct elantech_softc *sc = &psc->u.elantech;
	struct elantech_packet ep;
	int s;

	if (!psc->sc_enabled)
		return;

	if (sc->version >= 0x020800) {
		if ((psc->inputstate == 0 && (data & 0x0c) != 0x04) ||
		    (psc->inputstate == 3 && (data & 0x0f) != 0x02)) {
			aprint_debug_dev(psc->sc_dev, "waiting for sync..\n");
			psc->inputstate = 0;
			return;
		}
	} else {
		if ((psc->inputstate == 0 && (data & 0x0c) != 0x0c) ||
		    (psc->inputstate == 3 && (data & 0x0e) != 0x08)) {
			aprint_debug_dev(psc->sc_dev, "waiting for sync..\n");
			psc->inputstate = 0;
			return;
		}
	}

	psc->packet[psc->inputstate++] = data & 0xff;
	if (psc->inputstate != 6)
		return;

	psc->inputstate = 0;

	ep.ep_nfingers = (psc->packet[0] & 0xc0) >> 6;
	ep.ep_buttons = 0;
	ep.ep_buttons = psc->packet[0] & 1;		/* left button */
	ep.ep_buttons |= (psc->packet[0] & 2) << 1;	/* right button */

	if (ep.ep_nfingers == 0 || ep.ep_nfingers != sc->last_nfingers)
		sc->initializing = true;

	switch (ep.ep_nfingers) {
	case 0:
		/* FALLTHROUGH */
	case 1:
		ep.ep_x = ((int16_t)(psc->packet[1] & 0xf) << 8) | psc->packet[2];
		ep.ep_y = ((int16_t)(psc->packet[4] & 0xf) << 8) | psc->packet[5];

		aprint_debug_dev(psc->sc_dev,
		    "%d finger detected in elantech mode:\n", ep.ep_nfingers);
		aprint_debug_dev(psc->sc_dev,
		    "  X=%d Y=%d\n", ep.ep_x, ep.ep_y);
		aprint_debug_dev(psc->sc_dev,
		    "  %02x %02x %02x %02x %02x %02x\n",
		    psc->packet[0], psc->packet[1], psc->packet[2],
		    psc->packet[3], psc->packet[4], psc->packet[5]);

		s = spltty();
		wsmouse_input(psc->sc_wsmousedev, ep.ep_buttons,
		    sc->initializing ?
		      0 : (ep.ep_x - sc->last_x) >> elantech_xy_unprecision,
		    sc->initializing ?
		      0 : (ep.ep_y - sc->last_y) >> elantech_xy_unprecision,
		    0, 0,
		    WSMOUSE_INPUT_DELTA);
		splx(s);

		if (sc->initializing == true ||
		    ((ep.ep_x - sc->last_x) >> elantech_xy_unprecision) != 0)
			sc->last_x = ep.ep_x;
		if (sc->initializing == true ||
		    ((ep.ep_y - sc->last_y) >> elantech_xy_unprecision) != 0)
			sc->last_y = ep.ep_y;
		break;
	case 2:
		/* emulate z axis */
		ep.ep_z = psc->packet[2];
		aprint_debug_dev(psc->sc_dev,
		    "2 fingers detected in elantech mode:\n");
		aprint_debug_dev(psc->sc_dev,
		    "  %02x %02x %02x %02x %02x %02x\n",
		    psc->packet[0], psc->packet[1], psc->packet[2],
		    psc->packet[3], psc->packet[4], psc->packet[5]);

		s = spltty();
		wsmouse_input(psc->sc_wsmousedev, 0,
		    0, 0,
		    sc->initializing ?
		      0 : (sc->last_z - ep.ep_z) >> elantech_z_unprecision,
		    0,
		    WSMOUSE_INPUT_DELTA);
		splx(s);

		if (sc->initializing == true ||
		    ((sc->last_z - ep.ep_z) >> elantech_z_unprecision) != 0)
			sc->last_z = ep.ep_z;
		break;
	default:
		aprint_debug_dev(psc->sc_dev, "that's a lot of fingers!\n");
		return;
	}

	if (ep.ep_nfingers > 0)
		sc->initializing = false;
	sc->last_nfingers = ep.ep_nfingers;
}

int
pms_elantech_probe_init(void *opaque)
{
	struct pms_softc *psc = opaque;
	struct elantech_softc *sc = &psc->u.elantech;
	struct sysctllog *clog = NULL;
	u_char cmd[1], resp[3];
	uint16_t fwversion;
	int res;

	pckbport_flush(psc->sc_kbctag, psc->sc_kbcslot);

	cmd[0] = PMS_SET_SCALE11;
	if ((res = pckbport_poll_cmd(psc->sc_kbctag, psc->sc_kbcslot,
	    cmd, 1, 0, NULL, 0)) != 0)
		goto doreset;
	cmd[0] = PMS_SET_SCALE11;
	if ((res = pckbport_poll_cmd(psc->sc_kbctag, psc->sc_kbcslot,
	    cmd, 1, 0, NULL, 0)) != 0)
		goto doreset;
	cmd[0] = PMS_SET_SCALE11;
	if ((res = pckbport_poll_cmd(psc->sc_kbctag, psc->sc_kbcslot,
	    cmd, 1, 0, NULL, 0)) != 0)
		goto doreset;

	cmd[0] = PMS_SEND_DEV_STATUS;
	if ((res = pckbport_poll_cmd(psc->sc_kbctag, psc->sc_kbcslot,
	    cmd, 1, 3, resp, 0)) != 0)
		goto doreset;

	if (!ELANTECH_MAGIC(resp)) {
#ifdef ELANTECH_DEBUG
		aprint_error_dev(psc->sc_dev,
		    "bad elantech magic (%X %X %X)\n",
		    resp[0], resp[1], resp[2]);
#endif
		res = 1;
		goto doreset;
	}

	res = pms_sliced_command(psc->sc_kbctag, psc->sc_kbcslot,
	    ELANTECH_FW_VERSION);
	cmd[0] = PMS_SEND_DEV_STATUS;
	res |= pckbport_poll_cmd(psc->sc_kbctag, psc->sc_kbcslot,
	    cmd, 1, 3, resp, 0);
	if (res) {
		aprint_error_dev(psc->sc_dev,
		    "unable to query elantech firmware version\n");
		goto doreset;
	}

	fwversion = (resp[0] << 8) | resp[2];
	if (fwversion < ELANTECH_MIN_VERSION) {
		aprint_error_dev(psc->sc_dev,
		    "unsupported Elantech version %d.%d (%X %X %X)\n",
		    resp[0], resp[2], resp[0], resp[1], resp[2]);
		goto doreset;
	}
	sc->version = (resp[0] << 16) | (resp[1] << 8) | resp[2];
	aprint_normal_dev(psc->sc_dev, "Elantech touchpad version %d.%d (%06x)\n",
	    resp[0], resp[2], sc->version);

	res = pms_elantech_init(psc);
	if (res) {
		aprint_error_dev(psc->sc_dev,
		    "couldn't initialize elantech touchpad\n");
		goto doreset;
	}

	pms_sysctl_elantech(&clog);
	pckbport_set_inputhandler(psc->sc_kbctag, psc->sc_kbcslot,
	    pms_elantech_input, psc, device_xname(psc->sc_dev));

	return 0;

doreset:
	cmd[0] = PMS_RESET;
	(void)pckbport_poll_cmd(psc->sc_kbctag, psc->sc_kbcslot, cmd,
	    1, 2, resp, 1);
	return res;
}

void
pms_elantech_enable(void *opaque)
{
	struct pms_softc *psc = opaque;
	struct elantech_softc *sc = &psc->u.elantech;

	sc->initializing = true;
}

void
pms_elantech_resume(void *opaque)
{
	struct pms_softc *psc = opaque;
	uint8_t cmd, resp[2];
	int res;

	cmd = PMS_RESET;
	res = pckbport_poll_cmd(psc->sc_kbctag, psc->sc_kbcslot, &cmd,
	    1, 2, resp, 1);
	if (res)
		aprint_error_dev(psc->sc_dev,
		    "elantech reset on resume failed\n");
	else {
		pms_elantech_init(psc);
		pms_elantech_enable(psc);
	}
}
