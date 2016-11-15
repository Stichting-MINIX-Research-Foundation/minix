/* $NetBSD: hd44780reg.h,v 1.4 2009/08/30 02:07:05 tsutsui Exp $ */

/*
 * Copyright (c) 2002 Dennis I. Chernoivanov
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#ifndef _DEV_IC_HD44780REG_H_
#define _DEV_IC_HD44780REG_H_

/*
 * Register definitions for Hitachi HD44870 style displays
 */

#define HD_MAX_CHARS	80

#define HD_ROW1_ADDR	0x00
#define HD_ROW2_ADDR	0x40

#define HD_TIMEOUT_LONG	5000
#define HD_TIMEOUT_SHORT	100
#define HD_TIMEOUT_NORMAL	200

#define	BUSY_FLAG	0x80

/* Bit set helper */
#define bset(cond, bit)	((cond) ? (bit) : 0x00)

/* Get 4 most/least significant bits */
#define hi_bits(byte)	(((byte) & 0xf0) >> 4)
#define lo_bits(byte)	((byte) & 0x0f)

/*
 * 'Initialize by instruction'	8bit=1/0 8-bit/4-bit operation
 */
#define cmd_init(mode)	((uint8_t)(mode ? 0x3f : 0x03))

/*
 * 'Clear display'
 */
#define cmd_clear()	((uint8_t)0x01)

/*
 * 'Return home'
 */
#define cmd_rethome()	((uint8_t)0x03)

/*
 * 'Entry mode set'		id=1/0 increment/decrement
 * 				s=1  display shift
 */
#define cmd_modset(id, s) \
	((uint8_t)(0x04 | bset(id, 0x2) | bset(s, 0x1)))

/*
 * 'Display on/off control'	d=1/0 display on/off
 * 				c=1/0 cursor on/off
 * 				b=1/0 blinking of cursor position on/off
 */
#define cmd_dispctl(d, c, b) \
	((uint8_t)(0x08 | bset(d, 0x04) | bset(c, 0x02) | bset(b, 0x01)))

/*
 * 'Cursor or display shift'	sc=1/0 display shift/cursor move
 * 				rl=1/0 shift to the right/left
 */
#define cmd_shift(sc, rl) \
	((uint8_t)(0x13 | bset(sc, 0x08) | bset(rl, 0x04)))

/*
 * 'Function set'		dl=1/0 8 bits/4 bits operation
 * 				n=1/0  2 lines/1 line
 * 				f=1/0  5x10/5x8 dots font
 */
#define cmd_funcset(dl, n, f) \
	 ((uint8_t)(0x23 | bset(dl, 0x10) | bset(n, 0x08) | bset(f, 0x04)))

/*
 * 'Set CGRAM address'
 */
#define cmd_cgramset(acg) \
	((uint8_t)(0x40 | ((acg) & 0x3f)))

/*
 * 'Set DDRAM address'
 */
#define cmd_ddramset(add) \
	((uint8_t)(0x80 | ((add) & 0x7f)))

#endif /* _DEV_IC_HD44780REG_H_ */
