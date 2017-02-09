/*	$NetBSD: synapticsreg.h,v 1.8 2014/05/23 01:11:29 christos Exp $	*/

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

#ifndef	_DEV_PCKBCPORT_SYNAPTICSREG_H_
#define	_DEV_PCKBCPORT_SYNAPTICSREG_H_

/* Synaptics information queries. */
#define	SYNAPTICS_IDENTIFY_TOUCHPAD	0x0
#define	SYNAPTICS_READ_MODE		0x1
#define	SYNAPTICS_READ_CAPABILITIES	0x2
#define	SYNAPTICS_READ_MODEL_ID		0x3
#define	SYNAPTICS_EXTENDED_QUERY	0x9
#define	SYNAPTICS_CONTINUED_CAPABILITIES 0x0c

/* Synaptics special commands */
#define	SYNAPTICS_CMD_SET_MODE2		0x14
#define	SYNAPTICS_CMD_CLIENT_CMD	0x28

/* Magic numbers. */
#define	SYNAPTICS_MIN_VERSION		45 /* 4.5 */
#define	SYNAPTICS_MAGIC_BYTE		0x47

/* Capability bits. */
#define	SYNAPTICS_CAP_EXTENDED		(1 << 15)
#define	SYNAPTICS_CAP_EXTNUM		(1 << 14 | 1 << 13 | 1 << 12)
#define	SYNAPTICS_CAP_MBUTTON		(1 << 10)
#define	SYNAPTICS_CAP_PASSTHROUGH	(1 << 7)
#define	SYNAPTICS_CAP_MULTIFINGERREPORT (1 << 5)
#define	SYNAPTICS_CAP_SLEEP		(1 << 4)
#define	SYNAPTICS_CAP_4BUTTON		(1 << 3)
#define	SYNAPTICS_CAP_MULTIDETECT	(1 << 1)
#define	SYNAPTICS_CAP_PALMDETECT	(1 << 0)

/* Mode bits. */
#define	SYNAPTICS_MODE_ABSOLUTE		(1 << 7)
#define	SYNAPTICS_MODE_RATE		(1 << 6)
#define	SYNAPTICS_MODE_SLEEP		(1 << 3)
#define	SYNAPTICS_MODE_GEST		(1 << 2)
#define	SYNAPTICS_MODE_4BYTE_CLIENT	(1 << 1)
#define	SYNAPTICS_MODE_W		(1)

/* Extended mode button masks. */
#define	SYN_1BUTMASK			0x1
#define	SYN_2BUTMASK			0x1

/* Touchpad edge boundaries (Recommended values from Synaptics documentation) */
#define	SYNAPTICS_EDGE_LEFT		1632
#define	SYNAPTICS_EDGE_RIGHT		5312
#define	SYNAPTICS_EDGE_TOP		4288
#define	SYNAPTICS_EDGE_BOTTOM		1568
#define	SYNAPTICS_EDGE_MAX		6143

/* Finger pressures */
#define	SYNAPTICS_FINGER_NONE		0
#define	SYNAPTICS_FINGER_HOVER		10
#define	SYNAPTICS_FINGER_LIGHT		30
#define	SYNAPTICS_FINGER_NORMAL		80
#define	SYNAPTICS_FINGER_HEAVY		110
#define	SYNAPTICS_FINGER_FLAT		200
#define	SYNAPTICS_FINGER_PALM		255

/* Width values */
#define	SYNAPTICS_WIDTH_TWO_FINGERS	0
#define	SYNAPTICS_WIDTH_THREE_OR_MORE	1
#define	SYNAPTICS_WIDTH_PEN		2
#define	SYNAPTICS_WIDTH_ADVANCEDGESTURE	2
#define	SYNAPTICS_WIDTH_PASSTHROUGH	3
#define	SYNAPTICS_WIDTH_FINGER_MIN	4
#define	SYNAPTICS_WIDTH_FINGER_NORMAL	5
#define	SYNAPTICS_WIDTH_FINGER_MAX	7
#define	SYNAPTICS_WIDTH_PALM_MIN	8
#define	SYNAPTICS_WIDTH_PALM_MAX	14
#define	SYNAPTICS_WIDTH_MAX		15

#endif	/* _DEV_PCKBCPORT_SYNAPTICSREG_H_ */
