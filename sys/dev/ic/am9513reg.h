/*	$NetBSD: am9513reg.h,v 1.4 2008/04/28 20:23:49 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matthew Fredette.
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

#ifndef	_AM9513REG_H
#define	_AM9513REG_H

/*
 * Driver support for the Am9513 timer chip.  See
 * http://www.amd.com/products/cpg/techdocs/misc/03402/03402.html
 * for data sheets.
 */

struct am9513 {
	u_int16_t	am9513_clk_data;	/* data register */
	u_int16_t	am9513_clk_cmd;		/* command register */
};

#define	AM9513_CLK_DATA		0
#define	AM9513_CLK_CMD		2

/*
 * All of the below macros are for use in bitfields in the Counter
 * Mode (CM) Register.
 */

/* Gating control, CM[13:15]: */
/*
 * Legend: A[HL][LE]?
 *         | |    `-> L = level, E = edge
 *         | `-> H = high, L = low
 *         `-> active
 *
 *         P1 = plus one, M1 = minus one
 */
#define	AM9513_CM_GATING_NONE		(0x0 << 13)
#define	AM9513_CM_GATING_AH_TCN_M1	(0x1 << 13)
#define	AM9513_CM_GATING_AHL_GATEN_P1	(0x2 << 13)
#define	AM9513_CM_GATING_AHL_GATEN_M1	(0x3 << 13)
#define	AM9513_CM_GATING_AHL_GATEN	(0x4 << 13)
#define	AM9513_CM_GATING_ALL_GATEN	(0x5 << 13)
#define	AM9513_CM_GATING_AHE_GATEN	(0x6 << 13)
#define	AM9513_CM_GATING_ALE_GATEN	(0x7 << 13)

/* Source edge, CM[12]: */
#define	AM9513_CM_SOURCE_EDGE_RISING  (0 << 12)
#define	AM9513_CM_SOURCE_EDGE_FALLING (1 << 12)

/* Count sources, CM[8:11]: */
#define	AM9513_CM_SOURCE_TCN1	(0x0 << 8)
#define	AM9513_CM_SOURCE_SRC1	(0x1 << 8)
#define	AM9513_CM_SOURCE_SRC2	(0x2 << 8)
#define	AM9513_CM_SOURCE_SRC3	(0x3 << 8)
#define	AM9513_CM_SOURCE_SRC4	(0x4 << 8)
#define	AM9513_CM_SOURCE_SRC5	(0x5 << 8)
#define	AM9513_CM_SOURCE_GATE1	(0x6 << 8)
#define	AM9513_CM_SOURCE_GATE2	(0x7 << 8)
#define	AM9513_CM_SOURCE_GATE3	(0x8 << 8)
#define	AM9513_CM_SOURCE_GATE4	(0x9 << 8)
#define	AM9513_CM_SOURCE_GATE5	(0xA << 8)
#define	AM9513_CM_SOURCE_F1	(0xB << 8)
#define	AM9513_CM_SOURCE_F2	(0xC << 8)
#define	AM9513_CM_SOURCE_F3	(0xD << 8)
#define	AM9513_CM_SOURCE_F4	(0xE << 8)
#define	AM9513_CM_SOURCE_F5	(0xF << 8)

/* Count control, CM[7]: */
#define	AM9513_CM_SPECIAL_GATE_ENA	(1 << 7)
#define	AM9513_CM_SPECIAL_GATE_DIS	(0 << 7)

/* Counter reload source, CM[6]: */
#define	AM9513_CM_RELOAD_SOURCE_LOAD		(0 << 6)
#define	AM9513_CM_RELOAD_SOURCE_LOAD_OR_HOLD	(1 << 6)

/* Counter repeat control, CM[5]: */
#define	AM9513_CM_REPEAT_DIS	(0 << 5)
#define	AM9513_CM_REPEAT_ENA	(1 << 5)

/* Counter type, CM[4]: */
#define	AM9513_CM_TYPE_BIN	(0 << 4)
#define	AM9513_CM_TYPE_BCD	(1 << 4)

/* Counter direction, CM[3]: */
#define	AM9513_CM_DIR_DOWN	(0 << 3)
#define	AM9513_CM_DIR_UP	(1 << 3)

/* Output control, CM[0:2]: */
#define	AM9513_CM_OUTPUT_OL		(0x0 << 0)	/* inactive, output low */
#define	AM9513_CM_OUTPUT_AHTCP		(0x1 << 0)	/* active high terminal count pulse */
#define	AM9513_CM_OUTPUT_TC_TOGGLED	(0x2 << 0)	/* TC toggled */
#define	AM9513_CM_OUTPUT_OHI		(0x4 << 0)	/* inactive, output high impedance */
#define	AM9513_CM_OUTPUT_ALTCP		(0x5 << 0)	/* active low terminal count pulse */

/* The modes are various combinations of the above: */

/* Mode A: */
#define	AM9513_CM_MODE_A (AM9513_CM_GATING_NONE \
			  | AM9513_CM_SPECIAL_GATE_DIS \
			  | AM9513_CM_RELOAD_SOURCE_LOAD \
			  | AM9513_CM_REPEAT_DIS)

/*
 * Mode B is just like mode A, except you OR in the
 * AM9513_CM_GATING_ value for the *level* you want:
 */
#define	AM9513_CM_MODE_B AM9513_CM_MODE_A

/*
 * Mode C is just like mode A, except you OR in the
 * AM9513_CM_GATING_ value for the *edge* you want:
 */
#define	AM9513_CM_MODE_C AM9513_CM_MODE_A

/* Mode D: */
#define	AM9513_CM_MODE_D (AM9513_CM_GATING_NONE \
			  | AM9513_CM_SPECIAL_GATE_DIS \
			  | AM9513_CM_RELOAD_SOURCE_LOAD \
			  | AM9513_CM_REPEAT_ENA)

/*
 * Mode E is just like mode D, except you OR in the
 * AM9513_CM_GATING_ value for the *level* you want:
 */
#define	AM9513_CM_MODE_E AM9513_CM_MODE_D

/*
 * Mode F is just like mode D, except you OR in the
 * AM9513_CM_GATING_ value for the *edge* you want:
 */
#define	AM9513_CM_MODE_F AM9513_CM_MODE_D

/* Mode G: */
#define	AM9513_CM_MODE_G (AM9513_CM_GATING_NONE \
			  | AM9513_CM_SPECIAL_GATE_DIS \
			  | AM9513_CM_RELOAD_SOURCE_LOAD_OR_HOLD \
			  | AM9513_CM_REPEAT_DIS)

/*
 * Mode H is just like mode G, except you OR in the
 * AM9513_CM_GATING_ value for the *level* you want:
 */
#define	AM9513_CM_MODE_H AM9513_CM_MODE_G

/*
 * Mode I is just like mode G, except you OR in the
 * AM9513_CM_GATING_ value for the *edge* you want:
 */
#define	AM9513_CM_MODE_I AM9513_CM_MODE_G

/* Mode J: */
#define	AM9513_CM_MODE_J (AM9513_CM_GATING_NONE \
			  | AM9513_CM_SPECIAL_GATE_DIS \
			  | AM9513_CM_RELOAD_SOURCE_LOAD_OR_HOLD \
			  | AM9513_CM_REPEAT_ENA)

/*
 * Mode K is just like mode J, except you OR in the
 * AM9513_CM_GATING_ value for the *level* you want:
 */
#define	AM9513_CM_MODE_K AM9513_CM_MODE_J

/*
 * Mode L is just like mode J, except you OR in the
 * AM9513_CM_GATING_ value for the *edge* you want:
 */
#define	AM9513_CM_MODE_L AM9513_CM_MODE_J

/* Mode N: */
#define	AM9513_CM_MODE_N (AM9513_CM_GATING_NONE \
			  | AM9513_CM_SPECIAL_GATE_ENA \
			  | AM9513_CM_RELOAD_SOURCE_LOAD \
			  | AM9513_CM_REPEAT_DIS)

/*
 * Mode O is just like mode N, except you OR in the
 * AM9513_CM_GATING_ value for the *edge* you want:
 */
#define	AM9513_CM_MODE_O AM9513_CM_MODE_N

/* Mode Q; OR in the AM9513_CM_GATING_ value for the *level* you want: */
#define	AM9513_CM_MODE_Q (AM9513_CM_GATING_NONE \
			  | AM9513_CM_SPECIAL_GATE_ENA \
			  | AM9513_CM_RELOAD_SOURCE_LOAD \
			  | AM9513_CM_REPEAT_ENA)

/*
 * Mode R is just like mode N, except you OR in the
 * AM9513_CM_GATING_ value for the *edge* you want:
 */
#define	AM9513_CM_MODE_R AM9513_CM_MODE_Q

/* Mode S: */
#define	AM9513_CM_MODE_S (AM9513_CM_GATING_NONE \
			  | AM9513_CM_SPECIAL_GATE_ENA \
			  | AM9513_CM_RELOAD_SOURCE_LOAD_OR_HOLD \
			  | AM9513_CM_REPEAT_DIS)

/* Mode V: */
#define	AM9513_CM_MODE_V (AM9513_CM_GATING_NONE \
			  | AM9513_CM_SPECIAL_GATE_ENA \
			  | AM9513_CM_RELOAD_SOURCE_LOAD_OR_HOLD \
			  | AM9513_CM_REPEAT_ENA)

/* Given an AM9513_CM_SOURCE_F? value, return the divisor: */
#define	AM9513_CM_SOURCE_Fn_DIV(func) (1 << (((func) - AM9513_CM_SOURCE_F1) >> 6))

/*
 * Given a basic frequency and an AM9513_CM_SOURCE_F? value, returns
 * the number of clock ticks for a certain frequency:
 */
#define	AM9513_TICKS(basic, func, hz) (((basic) / AM9513_CM_SOURCE_Fn_DIV(func)) / (hz))

/* These are the timer numbers: */
#define	AM9513_TIMER1	(1)
#define	AM9513_TIMER2	(2)
#define	AM9513_TIMER3	(3)
#define	AM9513_TIMER4	(4)
#define	AM9513_TIMER5	(5)

/*
 * This macro is used to compose a bitmask of times for those
 * commands that take a bitmask:
 */
#define	AM9513_TIMER_BIT(timer)	(1 << ((timer) - 1))

/* When in 16-bit mode, the high 8 bits of every command must be ones: */
#define	_AM9513_CMD(x) (0xFF00 | (x))

/* Commands: */
#define	AM9513_CMD_LOAD_MODE(timer)		_AM9513_CMD(0x00 | (timer))
#define	AM9513_CMD_LOAD_LOAD(timer)		_AM9513_CMD(0x08 | (timer))
#define	AM9513_CMD_LOAD_HOLD(timer)		_AM9513_CMD(0x10 | (timer))
#define	AM9513_CMD_ARM(timers)			_AM9513_CMD(0x20 | (timers))
#define	AM9513_CMD_LOAD(timers)			_AM9513_CMD(0x40 | (timers))
#define	AM9513_CMD_LOAD_ARM(timers)		_AM9513_CMD(0x60 | (timers))
#define	AM9513_CMD_SAVE(timers)			_AM9513_CMD(0xA0 | (timers))
#define	AM9513_CMD_DISARM(timers)		_AM9513_CMD(0xC0 | (timers))
#define	AM9513_CMD_CLEAR_OUTPUT(timer)		_AM9513_CMD(0xE0 | (timer))
#define	AM9513_CMD_RESET			_AM9513_CMD(0xFF)

/* These are mode register values for various uses */
#define	AM9513_TICK_MODE	(AM9513_F2+0x22)	/* F2 + Operating mode D */
#define	AM9513_UART_MODE	(AM9513_F1+0x22)	/* F1 + Operating mode D */
#define	AM9513_FAST_LO_MODE	(AM9513_F3+0x28)	/* F3, repeat, count up */
#define	AM9513_FAST_HI_MODE	(0x0028)		/* TC of LO, repeat, count up */

#endif	/* _AM9513REG_H */
