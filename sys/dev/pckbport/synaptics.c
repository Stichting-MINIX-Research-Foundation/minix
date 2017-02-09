/*	$NetBSD: synaptics.c,v 1.33 2015/03/04 22:58:35 christos Exp $	*/

/*
 * Copyright (c) 2005, Steve C. Woodford
 * Copyright (c) 2004, Ales Krenek
 * Copyright (c) 2004, Kentaro A. Kurahone
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the authors nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * TODO:
 *	- Make the sysctl values per-instance instead of global.
 *	- Consider setting initial scaling factors at runtime according
 *	  to the values returned by the 'Read Resolutions' command.
 *	- Support the serial protocol (we only support PS/2 for now)
 *	- Support auto-repeat for up/down button Z-axis emulation.
 *	- Maybe add some more gestures (can we use Palm support somehow?)
 */

#include "opt_pms.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: synaptics.c,v 1.33 2015/03/04 22:58:35 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>

#include <sys/bus.h>

#include <dev/pckbport/pckbportvar.h>

#include <dev/pckbport/synapticsreg.h>
#include <dev/pckbport/synapticsvar.h>

#include <dev/pckbport/pmsreg.h>
#include <dev/pckbport/pmsvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

/*
 * Absolute-mode packets are decoded and passed around using
 * the following structure.
 */
struct synaptics_packet {
	signed short	sp_x;	/* Unscaled absolute X/Y coordinates */
	signed short	sp_y;
	u_char	sp_z;		/* Z (pressure) */
	u_char	sp_w;		/* W (contact patch width) */
	char	sp_left;	/* Left mouse button status */
	char	sp_right;	/* Right mouse button status */
	char	sp_middle;	/* Middle button status (possibly emulated) */
	char	sp_up;		/* Up button status */
	char	sp_down;	/* Down button status */
};

static void pms_synaptics_input(void *, int);
static void pms_synaptics_process_packet(struct pms_softc *,
		struct synaptics_packet *);
static void pms_sysctl_synaptics(struct sysctllog **);
static int pms_sysctl_synaptics_verify(SYSCTLFN_ARGS);

/* Controlled by sysctl. */
static int synaptics_up_down_emul = 2;
static int synaptics_up_down_motion_delta = 1;
static int synaptics_gesture_move = 200;
static int synaptics_gesture_length = 20;
static int synaptics_edge_left = SYNAPTICS_EDGE_LEFT;
static int synaptics_edge_right = SYNAPTICS_EDGE_RIGHT;
static int synaptics_edge_top = SYNAPTICS_EDGE_TOP;
static int synaptics_edge_bottom = SYNAPTICS_EDGE_BOTTOM;
static int synaptics_edge_motion_delta = 32;
static u_int synaptics_finger_high = SYNAPTICS_FINGER_LIGHT + 5;
static u_int synaptics_finger_low = SYNAPTICS_FINGER_LIGHT - 10;
static int synaptics_two_fingers_emul = 0;
static int synaptics_scale_x = 16;
static int synaptics_scale_y = 16;
static int synaptics_max_speed_x = 32;
static int synaptics_max_speed_y = 32;
static int synaptics_movement_threshold = 4;

/* Sysctl nodes. */
static int synaptics_up_down_emul_nodenum;
static int synaptics_up_down_motion_delta_nodenum;
static int synaptics_gesture_move_nodenum;
static int synaptics_gesture_length_nodenum;
static int synaptics_edge_left_nodenum;
static int synaptics_edge_right_nodenum;
static int synaptics_edge_top_nodenum;
static int synaptics_edge_bottom_nodenum;
static int synaptics_edge_motion_delta_nodenum;
static int synaptics_finger_high_nodenum;
static int synaptics_finger_low_nodenum;
static int synaptics_two_fingers_emul_nodenum;
static int synaptics_scale_x_nodenum;
static int synaptics_scale_y_nodenum;
static int synaptics_max_speed_x_nodenum;
static int synaptics_max_speed_y_nodenum;
static int synaptics_movement_threshold_nodenum;

static void
pms_synaptics_probe_extended(struct pms_softc *psc)
{
	struct synaptics_softc *sc = &psc->u.synaptics;
	u_char cmd[1], resp[3];
	int res;

	aprint_debug_dev(psc->sc_dev,
	    "synaptics_probe: Capabilities 0x%04x.\n", sc->caps);
	if (sc->caps & SYNAPTICS_CAP_PASSTHROUGH)
		sc->flags |= SYN_FLAG_HAS_PASSTHROUGH;

	if (sc->caps & SYNAPTICS_CAP_PALMDETECT)
		sc->flags |= SYN_FLAG_HAS_PALM_DETECT;

	if (sc->caps & SYNAPTICS_CAP_MULTIDETECT)
		sc->flags |= SYN_FLAG_HAS_MULTI_FINGER;

	if (sc->caps & SYNAPTICS_CAP_MULTIFINGERREPORT)
		sc->flags |= SYN_FLAG_HAS_MULTI_FINGER_REPORT;

	/* Ask about extra buttons to detect up/down. */
	if (((sc->caps & SYNAPTICS_CAP_EXTNUM) + 0x08)
	    >= SYNAPTICS_EXTENDED_QUERY)
	{
		res = pms_sliced_command(psc->sc_kbctag,
		    psc->sc_kbcslot, SYNAPTICS_EXTENDED_QUERY);
		cmd[0] = PMS_SEND_DEV_STATUS;
		res |= pckbport_poll_cmd(psc->sc_kbctag,
		    psc->sc_kbcslot, cmd, 1, 3, resp, 0);
		if (res == 0) {
			int buttons = (resp[1] >> 4);
			aprint_debug_dev(psc->sc_dev,
			    "%s: Extended Buttons: %d.\n", __func__, buttons);

			aprint_debug_dev(psc->sc_dev, "%s: Extended "
			    "Capabilities: 0x%02x 0x%02x 0x%02x.\n", __func__,
			    resp[0], resp[1], resp[2]);
			if (buttons >= 2) {
				/* Yes. */
				sc->flags |= SYN_FLAG_HAS_UP_DOWN_BUTTONS;
			}
			if (resp[0] & 0x1) {
				/* Vertical scroll area */
				sc->flags |= SYN_FLAG_HAS_VERTICAL_SCROLL;
			}
			if (resp[0] & 0x2) {
				/* Horizontal scroll area */
				sc->flags |= SYN_FLAG_HAS_HORIZONTAL_SCROLL;
			}
			if (resp[0] & 0x4) {
				/* Extended W-Mode */
				sc->flags |= SYN_FLAG_HAS_EXTENDED_WMODE;
			}
		}
	}

	/* Ask about click pad */
	if (((sc->caps & SYNAPTICS_CAP_EXTNUM) + 0x08) >=
	    SYNAPTICS_CONTINUED_CAPABILITIES)
	{
		res = pms_sliced_command(psc->sc_kbctag,
		    psc->sc_kbcslot, SYNAPTICS_CONTINUED_CAPABILITIES);
		cmd[0] = PMS_SEND_DEV_STATUS;
		res |= pckbport_poll_cmd(psc->sc_kbctag,
		    psc->sc_kbcslot, cmd, 1, 3, resp, 0);
/*
 * The following describes response for the
 * SYNAPTICS_CONTINUED_CAPABILITIES query.
 *
 * byte	mask	name			meaning
 * ----	----	-------			------------
 * 0	0x01	adjustable threshold	capacitive button sensitivity
 *					can be adjusted
 * 0	0x02	report max		query 0x0d gives max coord reported
 * 0	0x04	clearpad		sensor is ClearPad product
 * 0	0x08	advanced gesture	not particularly meaningful
 * 0	0x10	clickpad bit 0		1-button ClickPad
 * 0	0x60	multifinger mode	identifies firmware finger counting
 *					(not reporting!) algorithm.
 *					Not particularly meaningful
 * 0	0x80	covered pad		W clipped to 14, 15 == pad mostly covered
 * 1	0x01	clickpad bit 1		2-button ClickPad
 * 1	0x02	deluxe LED controls	touchpad support LED commands
 *					ala multimedia control bar
 * 1	0x04	reduced filtering	firmware does less filtering on
 *					position data, driver should watch
 *					for noise.
 * 1	0x08	image sensor		image sensor tracks 5 fingers, but only
 *					reports 2.
 * 1	0x01	uniform clickpad	whole clickpad moves instead of being
 *					hinged at the top.
 * 1	0x20	report min		query 0x0f gives min coord reported
 */
		if (res == 0) {
			u_char clickpad_type = (resp[0] & 0x10);
			clickpad_type |=       (resp[1] & 0x01);

			aprint_debug_dev(psc->sc_dev, "%s: Continued "
			    "Capabilities 0x%02x 0x%02x 0x%02x.\n", __func__,
			    resp[0], resp[1], resp[2]);
			switch (clickpad_type) {
			case 0x10:
				sc->flags |= SYN_FLAG_HAS_ONE_BUTTON_CLICKPAD;
				break;
			case 0x01:
				sc->flags |= SYN_FLAG_HAS_TWO_BUTTON_CLICKPAD;
				break;
			default:
				break;
			}
		}
	}
}

int
pms_synaptics_probe_init(void *vsc)
{
	struct pms_softc *psc = vsc;
	struct synaptics_softc *sc = &psc->u.synaptics;
	u_char cmd[1], resp[3];
	int res, ver_minor, ver_major;
	struct sysctllog *clog = NULL;

	res = pms_sliced_command(psc->sc_kbctag, psc->sc_kbcslot,
	    SYNAPTICS_IDENTIFY_TOUCHPAD);
	cmd[0] = PMS_SEND_DEV_STATUS;
	res |= pckbport_poll_cmd(psc->sc_kbctag, psc->sc_kbcslot, cmd, 1, 3,
	    resp, 0);
	if (res) {
		aprint_debug_dev(psc->sc_dev,
		    "synaptics_probe: Identify Touchpad error.\n");
		/*
		 * Reset device in case the probe confused it.
		 */
 doreset:
		cmd[0] = PMS_RESET;
		(void) pckbport_poll_cmd(psc->sc_kbctag, psc->sc_kbcslot, cmd,
		    1, 2, resp, 1);
		return (res);
	}

	if (resp[1] != SYNAPTICS_MAGIC_BYTE) {
		aprint_debug_dev(psc->sc_dev,
		    "synaptics_probe: Not synaptics.\n");
		res = 1;
		goto doreset;
	}

	sc->flags = 0;

	/* Check for minimum version and print a nice message. */
	ver_major = resp[2] & 0x0f;
	ver_minor = resp[0];
	aprint_normal_dev(psc->sc_dev, "Synaptics touchpad version %d.%d\n",
	    ver_major, ver_minor);
	if (ver_major * 10 + ver_minor < SYNAPTICS_MIN_VERSION) {
		/* No capability query support. */
		sc->caps = 0;
		goto done;
	}

	/* Query the hardware capabilities. */
	res = pms_sliced_command(psc->sc_kbctag, psc->sc_kbcslot,
	    SYNAPTICS_READ_CAPABILITIES);
	cmd[0] = PMS_SEND_DEV_STATUS;
	res |= pckbport_poll_cmd(psc->sc_kbctag, psc->sc_kbcslot, cmd, 1, 3,
	    resp, 0);
	if (res) {
		/* Hmm, failed to get capabilites. */
		aprint_error_dev(psc->sc_dev,
		    "synaptics_probe: Failed to query capabilities.\n");
		goto doreset;
	}

	sc->caps = (resp[0] << 8) | resp[2];

	if (sc->caps & SYNAPTICS_CAP_MBUTTON)
		sc->flags |= SYN_FLAG_HAS_MIDDLE_BUTTON;

	if (sc->caps & SYNAPTICS_CAP_4BUTTON)
		sc->flags |= SYN_FLAG_HAS_BUTTONS_4_5;

	if (sc->caps & SYNAPTICS_CAP_EXTENDED) {
		pms_synaptics_probe_extended(psc);
	}

	if (sc->flags) {
		const char comma[] = ", ";
		const char *sep = "";
		aprint_normal_dev(psc->sc_dev, "");
		if (sc->flags & SYN_FLAG_HAS_EXTENDED_WMODE) {
			aprint_normal("%sExtended W mode", sep);
			sep = comma;
		}
		if (sc->flags & SYN_FLAG_HAS_PASSTHROUGH) {
			aprint_normal("%sPassthrough", sep);
			sep = comma;
		}
		if (sc->flags & SYN_FLAG_HAS_MIDDLE_BUTTON) {
			aprint_normal("%sMiddle button", sep);
			sep = comma;
		}
		if (sc->flags & SYN_FLAG_HAS_BUTTONS_4_5) {
			aprint_normal("%sButtons 4/5", sep);
			sep = comma;
		}
		if (sc->flags & SYN_FLAG_HAS_UP_DOWN_BUTTONS) {
			aprint_normal("%sUp/down buttons", sep);
			sep = comma;
		}
		if (sc->flags & SYN_FLAG_HAS_PALM_DETECT) {
			aprint_normal("%sPalm detect", sep);
			sep = comma;
		}
		if (sc->flags & SYN_FLAG_HAS_ONE_BUTTON_CLICKPAD) {
			aprint_normal("%sOne button click pad", sep);
			sep = comma;
		}
		if (sc->flags & SYN_FLAG_HAS_TWO_BUTTON_CLICKPAD) {
			aprint_normal("%sTwo button click pad", sep);
			sep = comma;
		}
		if (sc->flags & SYN_FLAG_HAS_VERTICAL_SCROLL) {
			aprint_normal("%sVertical scroll", sep);
			sep = comma;
		}
		if (sc->flags & SYN_FLAG_HAS_HORIZONTAL_SCROLL) {
			aprint_normal("%sHorizontal scroll", sep);
			sep = comma;
		}
		if (sc->flags & SYN_FLAG_HAS_MULTI_FINGER_REPORT) {
			aprint_normal("%sMulti-finger Report", sep);
			sep = comma;
		}
		if (sc->flags & SYN_FLAG_HAS_MULTI_FINGER)
			aprint_normal("%sMulti-finger", sep);

		aprint_normal("\n");
	}

done:
	pms_sysctl_synaptics(&clog);
	pckbport_set_inputhandler(psc->sc_kbctag, psc->sc_kbcslot,
	    pms_synaptics_input, psc, device_xname(psc->sc_dev));

	return (0);
}

void
pms_synaptics_enable(void *vsc)
{
	struct pms_softc *psc = vsc;
	struct synaptics_softc *sc = &psc->u.synaptics;
	u_char cmd[2], resp[2];
	int res;

	if (sc->flags & SYN_FLAG_HAS_PASSTHROUGH) {
		/*
		 * Extended capability probes can confuse the passthrough device;
		 * reset the touchpad now to cure that.
		 */
		cmd[0] = PMS_RESET;
		res = pckbport_poll_cmd(psc->sc_kbctag, psc->sc_kbcslot, cmd,
		    1, 2, resp, 1);
	}

	/*
	 * Enable Absolute mode with W (width) reporting, and set
	 * the packet rate to maximum (80 packets per second).
	 */
	res = pms_sliced_command(psc->sc_kbctag, psc->sc_kbcslot,
	    SYNAPTICS_MODE_ABSOLUTE | SYNAPTICS_MODE_W | SYNAPTICS_MODE_RATE);
	cmd[0] = PMS_SET_SAMPLE;
	cmd[1] = SYNAPTICS_CMD_SET_MODE2;
	res |= pckbport_enqueue_cmd(psc->sc_kbctag, psc->sc_kbcslot, cmd, 2, 0,
	    1, NULL);
	sc->up_down = 0;
	sc->prev_fingers = 0;
	sc->gesture_start_x = sc->gesture_start_y = 0;
	sc->gesture_start_packet = 0;
	sc->gesture_tap_packet = 0;
	sc->gesture_type = 0;
	sc->gesture_buttons = 0;
	sc->rem_x = sc->rem_y = 0;
	sc->movement_history = 0;
	if (res) {
		aprint_error_dev(psc->sc_dev,
		    "synaptics_enable: Error enabling device.\n");
	}
}

void
pms_synaptics_resume(void *vsc)
{
	struct pms_softc *psc = vsc;
	unsigned char cmd[1],resp[2] = { 0,0 };
	int res;

	cmd[0] = PMS_RESET;
	res = pckbport_poll_cmd(psc->sc_kbctag, psc->sc_kbcslot, cmd, 1, 2,
	    resp, 1);
	aprint_debug_dev(psc->sc_dev,
	    "pms_synaptics_resume: reset on resume %d 0x%02x 0x%02x\n",
	    res, resp[0], resp[1]);
}

static void
pms_sysctl_synaptics(struct sysctllog **clog)
{
	int rc, root_num;
	const struct sysctlnode *node;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "synaptics",
	    SYSCTL_DESCR("Synaptics touchpad controls"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
	    goto err;

	root_num = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "up_down_emulation",
	    SYSCTL_DESCR("Middle button/Z-axis emulation with up/down buttons"),
	    pms_sysctl_synaptics_verify, 0,
	    &synaptics_up_down_emul,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	synaptics_up_down_emul_nodenum = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "up_down_motion_delta",
	    SYSCTL_DESCR("Up/down button Z-axis emulation rate"),
	    pms_sysctl_synaptics_verify, 0,
	    &synaptics_up_down_motion_delta,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	synaptics_up_down_motion_delta_nodenum = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "gesture_move",
	    SYSCTL_DESCR("Movement greater than this between taps cancels gesture"),
	    pms_sysctl_synaptics_verify, 0,
	    &synaptics_gesture_move,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	synaptics_gesture_move_nodenum = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "gesture_length",
	    SYSCTL_DESCR("Time period in which tap is recognised as a gesture"),
	    pms_sysctl_synaptics_verify, 0,
	    &synaptics_gesture_length,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	synaptics_gesture_length_nodenum = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "edge_left",
	    SYSCTL_DESCR("Define left edge of touchpad"),
	    pms_sysctl_synaptics_verify, 0,
	    &synaptics_edge_left,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	synaptics_edge_left_nodenum = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "edge_right",
	    SYSCTL_DESCR("Define right edge of touchpad"),
	    pms_sysctl_synaptics_verify, 0,
	    &synaptics_edge_right,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	synaptics_edge_right_nodenum = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "edge_top",
	    SYSCTL_DESCR("Define top edge of touchpad"),
	    pms_sysctl_synaptics_verify, 0,
	    &synaptics_edge_top,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	synaptics_edge_top_nodenum = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "edge_bottom",
	    SYSCTL_DESCR("Define bottom edge of touchpad"),
	    pms_sysctl_synaptics_verify, 0,
	    &synaptics_edge_bottom,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	synaptics_edge_bottom_nodenum = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "edge_motion_delta",
	    SYSCTL_DESCR("Define edge motion rate"),
	    pms_sysctl_synaptics_verify, 0,
	    &synaptics_edge_motion_delta,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	synaptics_edge_motion_delta_nodenum = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "finger_high",
	    SYSCTL_DESCR("Define finger applied pressure threshold"),
	    pms_sysctl_synaptics_verify, 0,
	    &synaptics_finger_high,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	synaptics_finger_high_nodenum = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "finger_low",
	    SYSCTL_DESCR("Define finger removed pressure threshold"),
	    pms_sysctl_synaptics_verify, 0,
	    &synaptics_finger_low,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	synaptics_finger_low_nodenum = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "two_fingers_emulation",
	    SYSCTL_DESCR("Map two fingers to middle button"),
	    pms_sysctl_synaptics_verify, 0,
	    &synaptics_two_fingers_emul,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	synaptics_two_fingers_emul_nodenum = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "scale_x",
	    SYSCTL_DESCR("Horizontal movement scale factor"),
	    pms_sysctl_synaptics_verify, 0,
	    &synaptics_scale_x,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	synaptics_scale_x_nodenum = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "scale_y",
	    SYSCTL_DESCR("Vertical movement scale factor"),
	    pms_sysctl_synaptics_verify, 0,
	    &synaptics_scale_y,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	synaptics_scale_y_nodenum = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "max_speed_x",
	    SYSCTL_DESCR("Horizontal movement maximum speed"),
	    pms_sysctl_synaptics_verify, 0,
	    &synaptics_max_speed_x,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	synaptics_max_speed_x_nodenum = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "max_speed_y",
	    SYSCTL_DESCR("Vertical movement maximum speed"),
	    pms_sysctl_synaptics_verify, 0,
	    &synaptics_max_speed_y,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	synaptics_max_speed_y_nodenum = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "movement_threshold",
	    SYSCTL_DESCR("Minimum reported movement threshold"),
	    pms_sysctl_synaptics_verify, 0,
	    &synaptics_movement_threshold,
	    0, CTL_HW, root_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	synaptics_movement_threshold_nodenum = node->sysctl_num;
	return;

err:
	aprint_error("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}

static int
pms_sysctl_synaptics_verify(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;

	node = *rnode;
	t = *(int *)rnode->sysctl_data;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	/* Sanity check the params. */
	if (node.sysctl_num == synaptics_up_down_emul_nodenum ||
	    node.sysctl_num == synaptics_two_fingers_emul_nodenum) {
		if (t < 0 || t > 2)
			return (EINVAL);
	} else
	if (node.sysctl_num == synaptics_gesture_length_nodenum ||
	    node.sysctl_num == synaptics_edge_motion_delta_nodenum ||
	    node.sysctl_num == synaptics_up_down_motion_delta_nodenum) {
		if (t < 0)
			return (EINVAL);
	} else
	if (node.sysctl_num == synaptics_edge_left_nodenum ||
	    node.sysctl_num == synaptics_edge_bottom_nodenum) {
		if (t < 0 || t > (SYNAPTICS_EDGE_MAX / 2))
			return (EINVAL);
	} else
	if (node.sysctl_num == synaptics_edge_right_nodenum ||
	    node.sysctl_num == synaptics_edge_top_nodenum) {
		if (t < (SYNAPTICS_EDGE_MAX / 2))
			return (EINVAL);
	} else
	if (node.sysctl_num == synaptics_scale_x_nodenum ||
	    node.sysctl_num == synaptics_scale_y_nodenum) {
		if (t < 1 || t > (SYNAPTICS_EDGE_MAX / 4))
			return (EINVAL);
	} else
	if (node.sysctl_num == synaptics_finger_high_nodenum) {
		if (t < 0 || t > SYNAPTICS_FINGER_PALM ||
		    t < synaptics_finger_low)
			return (EINVAL);
	} else
	if (node.sysctl_num == synaptics_finger_low_nodenum) {
		if (t < 0 || t > SYNAPTICS_FINGER_PALM ||
		    t > synaptics_finger_high)
			return (EINVAL);
	} else
	if (node.sysctl_num == synaptics_gesture_move_nodenum ||
	    node.sysctl_num == synaptics_movement_threshold_nodenum) {
		if (t < 0 || t > (SYNAPTICS_EDGE_MAX / 4))
			return (EINVAL);
	} else
		return (EINVAL);

	*(int *)rnode->sysctl_data = t;

	return (0);
}

/* Masks for the first byte of a packet */
#define PMS_LBUTMASK 0x01
#define PMS_RBUTMASK 0x02
#define PMS_MBUTMASK 0x04

static void
pms_synaptics_parse(struct pms_softc *psc)
{
	struct synaptics_softc *sc = &psc->u.synaptics;
	struct synaptics_packet sp;

	memset(&sp, 0, sizeof(sp));

	/* Absolute X/Y coordinates of finger */
	sp.sp_x = psc->packet[4] + ((psc->packet[1] & 0x0f) << 8) +
	   ((psc->packet[3] & 0x10) << 8);
	sp.sp_y = psc->packet[5] + ((psc->packet[1] & 0xf0) << 4) +
	   ((psc->packet[3] & 0x20) << 7);

	/* Pressure */
	sp.sp_z = psc->packet[2];

	/* Width of finger */
	sp.sp_w = ((psc->packet[0] & 0x30) >> 2) +
	   ((psc->packet[0] & 0x04) >> 1) +
	   ((psc->packet[3] & 0x04) >> 2);

	/* Left/Right button handling. */
	sp.sp_left = psc->packet[0] & PMS_LBUTMASK;
	sp.sp_right = psc->packet[0] & PMS_RBUTMASK;

	/* Up/Down buttons. */
	if (sc->flags & SYN_FLAG_HAS_BUTTONS_4_5) {
		/* Old up/down buttons. */
		sp.sp_up = sp.sp_left ^
		    (psc->packet[3] & PMS_LBUTMASK);
		sp.sp_down = sp.sp_right ^
		    (psc->packet[3] & PMS_RBUTMASK);
	} else
	if (sc->flags & SYN_FLAG_HAS_UP_DOWN_BUTTONS &&
	   ((psc->packet[0] & PMS_RBUTMASK) ^
	   (psc->packet[3] & PMS_RBUTMASK))) {
		/* New up/down button. */
		sp.sp_up = psc->packet[4] & SYN_1BUTMASK;
		sp.sp_down = psc->packet[5] & SYN_2BUTMASK;
	} else {
		sp.sp_up = 0;
		sp.sp_down = 0;
	}

	if(sc->flags & SYN_FLAG_HAS_ONE_BUTTON_CLICKPAD) {
		/* This is not correctly specified. Read this button press
		 * from L/U bit.
		 */
		sp.sp_left = ((psc->packet[0] ^ psc->packet[3]) & 0x01) ? 1 : 0;
	} else
	/* Middle button. */
	if (sc->flags & SYN_FLAG_HAS_MIDDLE_BUTTON) {
		/* Old style Middle Button. */
		sp.sp_middle = (psc->packet[0] & PMS_LBUTMASK) ^
		    (psc->packet[3] & PMS_LBUTMASK);
	} else
	if (synaptics_up_down_emul == 1) {
		/* Do middle button emulation using up/down buttons */
		sp.sp_middle = sp.sp_up | sp.sp_down;
		sp.sp_up = sp.sp_down = 0;
	} else
		sp.sp_middle = 0;

	pms_synaptics_process_packet(psc, &sp);
}

static void
pms_synaptics_passthrough(struct pms_softc *psc)
{
	int dx, dy, dz;
	int buttons, changed;
	int s;

	buttons = ((psc->packet[1] & PMS_LBUTMASK) ? 0x20 : 0) |
		((psc->packet[1] & PMS_MBUTMASK) ? 0x40 : 0) |
		((psc->packet[1] & PMS_RBUTMASK) ? 0x80 : 0);

	dx = psc->packet[4];
	if (dx >= 128)
		dx -= 256;
	if (dx == -128)
		dx = -127;

	dy = psc->packet[5];
	if (dy >= 128)
		dy -= 256;
	if (dy == -128)
		dy = -127;

	dz = 0;

	changed = buttons ^ (psc->buttons & 0xe0);
	psc->buttons ^= changed;

	if (dx || dy || dz || changed) {
		buttons = (psc->buttons & 0x1f) | ((psc->buttons >> 5) & 0x7);
		s = spltty();
		wsmouse_input(psc->sc_wsmousedev,
			buttons, dx, dy, dz, 0,
			WSMOUSE_INPUT_DELTA);
		splx(s);
	}
}

static void
pms_synaptics_input(void *vsc, int data)
{
	struct pms_softc *psc = vsc;
	struct timeval diff;

	if (!psc->sc_enabled) {
		/* Interrupts are not expected. Discard the byte. */
		return;
	}

	getmicrouptime(&psc->current);

	if (psc->inputstate > 0) {
		timersub(&psc->current, &psc->last, &diff);
		if (diff.tv_sec > 0 || diff.tv_usec >= 40000) {
			aprint_debug_dev(psc->sc_dev,
			    "pms_input: unusual delay (%ld.%06ld s), "
			    "scheduling reset\n",
			    (long)diff.tv_sec, (long)diff.tv_usec);
			psc->inputstate = 0;
			psc->sc_enabled = 0;
			wakeup(&psc->sc_enabled);
			return;
		}
	}
	psc->last = psc->current;

	switch (psc->inputstate) {
	case 0:
		if ((data & 0xc8) != 0x80) {
			aprint_debug_dev(psc->sc_dev,
			    "pms_input: 0x%02x out of sync\n", data);
			return;	/* not in sync yet, discard input */
		}
		/*FALLTHROUGH*/

	case 3:
		if ((data & 8) == 8) {
			aprint_debug_dev(psc->sc_dev,
			    "pms_input: dropped in relative mode, reset\n");
			psc->inputstate = 0;
			psc->sc_enabled = 0;
			wakeup(&psc->sc_enabled);
			return;
		}
	}

	psc->packet[psc->inputstate++] = data & 0xff;
	if (psc->inputstate == 6) {
		/*
		 * We have a complete packet.
		 * Extract the pertinent details.
		 */
		psc->inputstate = 0;

		if ((psc->packet[0] & 0xfc) == 0x84 &&
		    (psc->packet[3] & 0xcc) == 0xc4) {
			/* W = SYNAPTICS_WIDTH_PASSTHROUGH, PS/2 passthrough */
			pms_synaptics_passthrough(psc);
		} else {
			pms_synaptics_parse(psc);
		}
	}
}

static inline int
synaptics_finger_detect(struct synaptics_softc *sc, struct synaptics_packet *sp,
    int *palmp)
{
	int fingers;

	/* Assume no palm */
	*palmp = 0;

	/*
	 * Apply some hysteresis when checking for a finger.
	 * When the finger is first applied, we ignore it until the
	 * pressure exceeds the 'high' threshold. The finger is considered
	 * removed only when pressure falls beneath the 'low' threshold.
	 */
	if ((sc->prev_fingers == 0 && sp->sp_z > synaptics_finger_high) ||
	    (sc->prev_fingers != 0 && sp->sp_z > synaptics_finger_low))
		fingers = 1;
	else
		fingers = 0;

	/*
	 * If the pad can't do palm detection, skip the rest.
	 */
	if (fingers == 0 || (sc->flags & SYN_FLAG_HAS_PALM_DETECT) == 0)
		return (fingers);

	/*
	 * Palm detection
	 */
	if (sp->sp_z > SYNAPTICS_FINGER_FLAT &&
	    sp->sp_w >= SYNAPTICS_WIDTH_PALM_MIN)
		*palmp = 1;

	if (sc->prev_fingers == 0 &&
	    (sp->sp_z > SYNAPTICS_FINGER_FLAT ||
	     sp->sp_w >= SYNAPTICS_WIDTH_PALM_MIN)) {
		/*
		 * Contact area or pressure is too great to be a finger.
		 * Just ignore it for now.
		 */
		return (0);
	}

	/*
	 * Detect 2 and 3 fingers if supported, but only if multiple
	 * fingers appear within the tap gesture time period.
	 */
	if (sc->flags & SYN_FLAG_HAS_MULTI_FINGER &&
	    SYN_TIME(sc, sc->gesture_start_packet) < synaptics_gesture_length) {
		switch (sp->sp_w) {
		case SYNAPTICS_WIDTH_TWO_FINGERS:
			fingers = 2;
			break;

		case SYNAPTICS_WIDTH_THREE_OR_MORE:
			fingers = 3;
			break;

		case SYNAPTICS_WIDTH_PEN:
			fingers = 1;
			break;

		default:
			/*
			 * The width value can report spurious single-finger
			 * events after a multi-finger event.
			 */
			if (sc->prev_fingers > 1)
				fingers = sc->prev_fingers;
			else
				fingers = 1;
			break;
		}
	}

	return (fingers);
}

static inline void
synaptics_gesture_detect(struct synaptics_softc *sc,
    struct synaptics_packet *sp, int fingers)
{
	int gesture_len, gesture_buttons;
	int set_buttons;

	gesture_len = SYN_TIME(sc, sc->gesture_start_packet);
	gesture_buttons = sc->gesture_buttons;

	if (fingers > 0 && (fingers == sc->prev_fingers)) {
		/* Finger is still present */
		sc->gesture_move_x = abs(sc->gesture_start_x - sp->sp_x);
		sc->gesture_move_y = abs(sc->gesture_start_y - sp->sp_y);
	} else
	if (fingers && sc->prev_fingers == 0) {
		/*
		 * Finger was just applied.
		 * If the previous gesture was a single-click, set things
		 * up to deal with a possible drag or double-click gesture.
		 * Basically, if the finger is removed again within
		 * 'synaptics_gesture_length' packets, this is treated
		 * as a double-click. Otherwise we will emulate holding
		 * the left button down whilst dragging the mouse.
		 */
		if (SYN_IS_SINGLE_TAP(sc->gesture_type))
			sc->gesture_type |= SYN_GESTURE_DRAG;

		sc->gesture_start_x = abs(sp->sp_x);
		sc->gesture_start_y = abs(sp->sp_y);
		sc->gesture_move_x = 0;
		sc->gesture_move_y = 0;
		sc->gesture_start_packet = sc->total_packets;

#ifdef DIAGNOSTIC
		aprint_debug("Finger applied: gesture_start_x: %d gesture_start_y: %d\n",
			sc->gesture_start_x, sc->gesture_start_y);
#endif
	} else
	if (fingers == 0 && sc->prev_fingers != 0) {
		/*
		 * Finger was just removed.
		 * Check if the contact time and finger movement were
		 * small enough to qualify as a gesture.
		 * Ignore finger movement if multiple fingers were
		 * detected (the pad may report coordinates for any
		 * of the fingers).
		 */

#ifdef DIAGNOSTIC
		aprint_debug("Finger removed: gesture_len: %d (%d)\n",
			gesture_len, synaptics_gesture_length);
		aprint_debug("gesture_move_x: %d (%d) sp_x: %d\n",
			sc->gesture_move_x, synaptics_gesture_move, abs(sp->sp_x));
		aprint_debug("gesture_move_y: %d (%d) sp_y: %d\n",
			sc->gesture_move_y, synaptics_gesture_move, abs(sp->sp_y));
#endif

		if (gesture_len < synaptics_gesture_length &&
		    ((sc->gesture_move_x < synaptics_gesture_move &&
		     sc->gesture_move_y < synaptics_gesture_move))) {
			/*
			 * Looking good so far.
			 */
			if (SYN_IS_DRAG(sc->gesture_type)) {
				/*
				 * Promote this gesture to double-click.
				 */
				sc->gesture_type |= SYN_GESTURE_DOUBLE;
				sc->gesture_type &= ~SYN_GESTURE_SINGLE;
			} else {
				/*
				 * Single tap gesture. Set the tap length timer
				 * and flag a single-click.
				 */
				sc->gesture_tap_packet = sc->total_packets;
				sc->gesture_type |= SYN_GESTURE_SINGLE;

				/*
				 * The gesture can be modified depending on
				 * the number of fingers detected.
				 *
				 * 1: Normal left button emulation.
				 * 2: Either middle button or right button
				 *    depending on the value of the two_fingers
				 *    sysctl variable.
				 * 3: Right button.
				 */
				switch (sc->prev_fingers) {
				case 2:
					if (synaptics_two_fingers_emul == 1)
						gesture_buttons |= PMS_RBUTMASK;
					else
					if (synaptics_two_fingers_emul == 2)
						gesture_buttons |= PMS_MBUTMASK;
					break;
				case 3:
					gesture_buttons |= PMS_RBUTMASK;
					break;
				default:
					gesture_buttons |= PMS_LBUTMASK;
					break;
				}
			}
		}

		/*
		 * Always clear drag state when the finger is removed.
		 */
		sc->gesture_type &= ~SYN_GESTURE_DRAG;
	}

	if (sc->gesture_type == 0) {
		/*
		 * There is no gesture in progress.
		 * Clear emulated button state.
		 */
		sc->gesture_buttons = 0;
		return;
	}

	/*
	 * A gesture is in progress.
	 */
	set_buttons = 0;

	if (SYN_IS_SINGLE_TAP(sc->gesture_type)) {
		/*
		 * Single-click.
		 * Activate the relevant button(s) until the
		 * gesture tap timer has expired.
		 */
		if (SYN_TIME(sc, sc->gesture_tap_packet) <
		    synaptics_gesture_length)
			set_buttons = 1;
		else
			sc->gesture_type &= ~SYN_GESTURE_SINGLE;
	} else
	if (SYN_IS_DOUBLE_TAP(sc->gesture_type) && sc->prev_fingers == 0) {
		/*
		 * Double-click.
		 * Activate the relevant button(s) once.
		 */
		set_buttons = 1;
		sc->gesture_type &= ~SYN_GESTURE_DOUBLE;
	}

	if (set_buttons || SYN_IS_DRAG(sc->gesture_type)) {
		/*
		 * Single-click and drag.
		 * Maintain button state until the finger is removed.
		 */
		sp->sp_left |= gesture_buttons & PMS_LBUTMASK;
		sp->sp_right |= gesture_buttons & PMS_RBUTMASK;
		sp->sp_middle |= gesture_buttons & PMS_MBUTMASK;
	}

	sc->gesture_buttons = gesture_buttons;
}

static inline int
synaptics_filter_policy(struct synaptics_softc *sc, int *history, int value)
{
	int a, b, rv, count;

	count = sc->total_packets;

	/*
	 * Once we've accumulated at least SYN_HIST_SIZE values, combine
	 * each new value with the previous two and return the average.
	 *
	 * This is necessary when the touchpad is operating in 80 packets
	 * per second mode, as it performs little internal filtering on
	 * reported values.
	 *
	 * Using a rolling average helps to filter out jitter caused by
	 * tiny finger movements.
	 */
	if (sc->movement_history >= SYN_HIST_SIZE) {
		a = (history[(count + 0) % SYN_HIST_SIZE] +
		    history[(count + 1) % SYN_HIST_SIZE]) / 2;

		b = (value + history[(count + 0) % SYN_HIST_SIZE]) / 2;

		rv = b - a;

		/*
		 * Don't report the movement if it's below a certain
		 * threshold.
		 */
		if (abs(rv) < synaptics_movement_threshold)
			rv = 0;
	} else
		rv = 0;

	/*
	 * Add the new value to the history buffer.
	 */
	history[(count + 1) % SYN_HIST_SIZE] = value;

	return (rv);
}

/* Edge detection */
#define	SYN_EDGE_TOP		1
#define	SYN_EDGE_BOTTOM		2
#define	SYN_EDGE_LEFT		4
#define	SYN_EDGE_RIGHT		8

static inline int
synaptics_check_edge(int x, int y)
{
	int rv = 0;

	if (x < synaptics_edge_left)
		rv |= SYN_EDGE_LEFT;
	else
	if (x > synaptics_edge_right)
		rv |= SYN_EDGE_RIGHT;

	if (y < synaptics_edge_bottom)
		rv |= SYN_EDGE_BOTTOM;
	else
	if (y > synaptics_edge_top)
		rv |= SYN_EDGE_TOP;

	return (rv);
}

static inline int
synaptics_edge_motion(struct synaptics_softc *sc, int delta, int dir)
{

	/*
	 * When edge motion is enabled, synaptics_edge_motion_delta is
	 * combined with the current delta, together with the direction
	 * in which to simulate the motion. The result is added to
	 * the delta derived from finger movement. This provides a smooth
	 * transition from finger movement to edge motion.
	 */
	delta = synaptics_edge_motion_delta + (dir * delta);
	if (delta < 0)
		return (0);
	if (delta > synaptics_edge_motion_delta)
		return (synaptics_edge_motion_delta);
	return (delta);
}

static inline int
synaptics_scale(int delta, int scale, int *remp)
{
	int rv;

	/*
	 * Scale the raw delta in Synaptics coordinates (0-6143) into
	 * something more reasonable by dividing the raw delta by a
	 * scale factor. Any remainder from the previous scale result
	 * is added to the current delta before scaling.
	 * This prevents loss of resolution for very small/slow
	 * movements of the finger.
	 */
	delta += *remp;
	rv = delta / scale;
	*remp = delta % scale;

	return (rv);
}

static inline void
synaptics_movement(struct synaptics_softc *sc, struct synaptics_packet *sp,
    int *dxp, int *dyp)
{
	int dx, dy, edge;

	/*
	 * Compute the next values of dx and dy
	 */
	dx = synaptics_filter_policy(sc, sc->history_x, sp->sp_x);
	dy = synaptics_filter_policy(sc, sc->history_y, sp->sp_y);

	/*
	 * If we're dealing with a drag gesture, and the finger moves to
	 * the edge of the touchpad, apply edge motion emulation if it
	 * is enabled.
	 */
	if (synaptics_edge_motion_delta && SYN_IS_DRAG(sc->gesture_type)) {
		edge = synaptics_check_edge(sp->sp_x, sp->sp_y);

		if (edge & SYN_EDGE_LEFT)
			dx -= synaptics_edge_motion(sc, dx, 1);
		if (edge & SYN_EDGE_RIGHT)
			dx += synaptics_edge_motion(sc, dx, -1);
		if (edge & SYN_EDGE_BOTTOM)
			dy -= synaptics_edge_motion(sc, dy, 1);
		if (edge & SYN_EDGE_TOP)
			dy += synaptics_edge_motion(sc, dy, -1);
	}

	/*
	 * Apply scaling to both deltas
	 */
	dx = synaptics_scale(dx, synaptics_scale_x, &sc->rem_x);
	dy = synaptics_scale(dy, synaptics_scale_y, &sc->rem_y);

	/*
	 * Clamp deltas to specified maximums.
	 */
	if (dx > synaptics_max_speed_x)
		dx = synaptics_max_speed_x;
	if (dy > synaptics_max_speed_y)
		dy = synaptics_max_speed_y;

	*dxp = dx;
	*dyp = dy;

	sc->movement_history++;
}

static void
pms_synaptics_process_packet(struct pms_softc *psc, struct synaptics_packet *sp)
{
	struct synaptics_softc *sc = &psc->u.synaptics;
	int dx, dy, dz;
	int fingers, palm, buttons, changed;
	int s;

	/*
	 * Do Z-axis emulation using up/down buttons if required.
	 * Note that the pad will send a one second burst of packets
	 * when an up/down button is pressed and held. At the moment
	 * we don't deal with auto-repeat, so convert the burst into
	 * a one-shot.
	 */
	dz = 0;
	if (synaptics_up_down_emul == 2) {
		if (sc->up_down == 0) {
			if (sp->sp_up && sp->sp_down) {
				/*
				 * Most up/down buttons will be actuated using
				 * a rocker switch, so we should never see
				 * them both simultaneously. But just in case,
				 * treat this situation as a middle button
				 * event.
				 */
				sp->sp_middle = 1;
			} else
			if (sp->sp_up)
				dz = -synaptics_up_down_motion_delta;
			else
			if (sp->sp_down)
				dz = synaptics_up_down_motion_delta;
		}

		sc->up_down = sp->sp_up | sp->sp_down;
		sp->sp_up = sp->sp_down = 0;
	}

	/*
	 * Determine whether or not a finger is on the pad.
	 * On some pads, this will return the number of fingers
	 * detected.
	 */
	fingers = synaptics_finger_detect(sc, sp, &palm);

	/*
	 * Do gesture processing only if we didn't detect a palm.
	 */
	if (palm == 0)
		synaptics_gesture_detect(sc, sp, fingers);
	else
		sc->gesture_type = sc->gesture_buttons = 0;

	/*
	 * Determine what buttons to report
	 */
	buttons = (sp->sp_left ? 0x1 : 0) |
	    (sp->sp_middle ? 0x2 : 0) |
	    (sp->sp_right ? 0x4 : 0) |
	    (sp->sp_up ? 0x8 : 0) |
	    (sp->sp_down ? 0x10 : 0);
	changed = buttons ^ (psc->buttons & 0x1f);
	psc->buttons ^= changed;

	sc->prev_fingers = fingers;
	sc->total_packets++;

	/*
	 * Do movement processing IFF we have a single finger and no palm.
	 */
	if (fingers == 1 && palm == 0)
		synaptics_movement(sc, sp, &dx, &dy);
	else {
		/*
		 * No valid finger. Therefore no movement.
		 */
		sc->movement_history = 0;
		sc->rem_x = sc->rem_y = 0;
		dx = dy = 0;
	}

	/*
	 * Pass the final results up to wsmouse_input() if necessary.
	 */
	if (dx || dy || dz || changed) {
		buttons = (psc->buttons & 0x1f) | ((psc->buttons >> 5) & 0x7);
		s = spltty();
		wsmouse_input(psc->sc_wsmousedev,
				buttons,
				dx, dy, dz, 0,
		    		WSMOUSE_INPUT_DELTA);
		splx(s);
	}
}
