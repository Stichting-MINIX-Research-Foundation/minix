/*	$NetBSD: synapticsvar.h,v 1.6 2014/05/23 01:11:29 christos Exp $	*/

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
 *   * Neither the name of the Kentaro A. Kurahone nor the names of its
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

#ifndef _DEV_PCKBCPORT_SYNAPTICSVAR_H_
#define _DEV_PCKBCPORT_SYNAPTICSVAR_H_

struct synaptics_softc {
	int	caps;

	int	flags;
#define	SYN_FLAG_HAS_MIDDLE_BUTTON		(1 << 0)
#define	SYN_FLAG_HAS_BUTTONS_4_5		(1 << 1)
#define	SYN_FLAG_HAS_UP_DOWN_BUTTONS		(1 << 2)
#define	SYN_FLAG_HAS_PASSTHROUGH		(1 << 3)
#define	SYN_FLAG_HAS_PALM_DETECT		(1 << 4)
#define	SYN_FLAG_HAS_MULTI_FINGER		(1 << 5)
#define	SYN_FLAG_HAS_MULTI_FINGER_REPORT	(1 << 6)
#define	SYN_FLAG_HAS_VERTICAL_SCROLL		(1 << 7)
#define	SYN_FLAG_HAS_HORIZONTAL_SCROLL		(1 << 8)
#define	SYN_FLAG_HAS_ONE_BUTTON_CLICKPAD	(1 << 9)
#define	SYN_FLAG_HAS_TWO_BUTTON_CLICKPAD	(1 << 10)
#define	SYN_FLAG_HAS_EXTENDED_WMODE		(1 << 11)

	u_int	total_packets;		/* Total number of packets received */
#define	SYN_TIME(sc,c)	(((sc)->total_packets >= (c)) ?		\
			    ((sc)->total_packets - (c)) :	\
			    ((c) - (sc)->total_packets))

	int	up_down;
	int	prev_fingers;

	int	gesture_start_x, gesture_start_y;
	int	gesture_move_x, gesture_move_y;
	u_int	gesture_start_packet;
	u_int	gesture_tap_packet;

	int	gesture_buttons;
	int	gesture_type;
#define	SYN_GESTURE_SINGLE	0x01
#define	SYN_GESTURE_DOUBLE	0x02
#define	SYN_GESTURE_DRAG	0x04
#define	SYN_IS_SINGLE_TAP(t)	((t) & SYN_GESTURE_SINGLE)
#define	SYN_IS_DOUBLE_TAP(t)	((t) & SYN_GESTURE_DOUBLE)
#define	SYN_IS_DRAG(t)		((t) & SYN_GESTURE_DRAG)

#define	SYN_HIST_SIZE	4
	int	rem_x, rem_y;
	u_int	movement_history;
	int	history_x[SYN_HIST_SIZE], history_y[SYN_HIST_SIZE];
};

int pms_synaptics_probe_init(void *vsc);
void pms_synaptics_enable(void *vsc);
void pms_synaptics_resume(void *vsc);

#endif
