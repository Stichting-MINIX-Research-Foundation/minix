/*	$NetBSD: weaselreg.h,v 1.7 2007/12/25 18:33:40 perry Exp $	*/

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
 * Register and firmware communication definitions for the
 * Middle Digital, Inc. PC-Weasel serial console board.
 */

/*
 * Current versions of the PC-Weasel emulate a Monochrome Display
 * Adapter.  The framebuffer is at the standard ISA framebuffer
 * location (0xb0000).  At the end of the viewable framebuffer area
 * is a control register space.
 */

#define	WEASEL_WDT_SEMAPHORE		0x0fa0

#define	WEASEL_CONFIG_BLOCK		0x0fa1

#define WEASEL_WDT_TICKLE		0x0fa2

#define	WEASEL_MISC_COMMAND		0x0fcd

#define	WEASEL_MISC_RESPONSE		0x0fce

/*
 * Layout of the PC-Weasel configuration block.  This is taken
 * more or less right out of the PC-Weasel manual, page 52.
 */
struct weasel_config_block {
	u_int8_t	cfg_version;	/* configuration version */

#define	CFG_VERSION_1_0		0x01
#define	CFG_VERSION_1_1		0x02

	u_int8_t	weasel_attn;	/* Weasel attention character */
	u_int8_t	debug;		/* debug level */
	u_int8_t	reset_pc_on_boot;/* reset PC on Weasel boot */
	u_int8_t	duart_baud;	/* baud rate of DUART */
	u_int8_t	duart_parity;	/* 0 none, 1 even, 2 odd */
	u_int8_t	duart_bits;	/* 7 or 8 */

	/*
	 * Unfortunately, between cfg_version 1 and 2, the semantics
	 * of this variable changed.
	 *
	 *	cfg_version 1:
	 *
	 *		0 == always emulate
	 *		1 == autoswitch
	 *
	 *	cfg_version 2:
	 *
	 *		0 == always emulate
	 *		1 == always serial
	 *		2 == autoswitch
	 */
	u_int8_t	enable_duart_switching;
	u_int8_t	wdt_allow;	/* 0 disable, 1 allow */
	u_int16_t	wdt_msec;	/* watchdog timer period */
	u_int8_t	duart_flow;	/* 1 rts/cts, 0 none */
	u_int8_t	break_passthru;	/* BREAK is passed through */
	u_int8_t	obsolete[30];	/* reserved for future use */
	u_int8_t	cksum;		/* arithmetic sum -> reserved */
} __packed;

/*
 * Commands that can be written to the MISC_COMMAND register.
 */

#define	OS_READY	0x00		/* ready for commands */

#define	OS_UART_CLEAR	0x01		/* clear response for OS_UART_QUERY */

#define	OS_UART_QUERY	0x02		/* query Weasel UART setting */
#define	UART_QUERY_DIS	0x00		/* UART is disabled */
#define	UART_QUERY_3f8	0x01		/* UART at 0x3f8 */
#define	UART_QUERY_2F8	0x02		/* UART at 0x2f8 */
#define	UART_QUERY_3e8	0x03		/* UART at 0x3e8 */
#define	UART_QUERY_2e8	0x04		/* UART at 0x2e8 */

#define	OS_CONFIG_COPY	0x03		/* copy config to offscreen space */
#define OS_WDT_QUERY    0x04		/* query watchdog state. 0=off 1=on */

#define	OS_NOP		0x07

/*
 * The watchdog timer on the PC-Weasel is enabled/disabled (it's a toggle)
 * using the WDT_SEMAPHORE register in the offscreen area.  The semaphore
 * is also used to service the watchdog.
 *
 * To toggle the watchdog:
 *
 *	for (new_state = old_state; new_state == old_state;) {
 *		WDT_SEMAPHORE = 0x22;
 *		delay(1500);
 *		if (WDT_SEMAPHORE == 0xea) {
 *			WDT_SEMAPHORE = 0x2f;
 *			delay(1500);
 *			if (WDT_SEMAPHORE == 0xae) {
 *				WDT_SEMAPHORE = 0x37;
 *				delay(1500);
 *				new_state = WDT_SEMAPHORE;
 *			}
 *		}
 *	}
 *
 * To serivce the watchdog when armed:
 *
 *	tmp = WDT_SEMPAPHORE;
 *	WDT_SEMAPHORE ~= tmp;
 */
#define WDT_ATTENTION   0x22    /* get the attention of the WDT state engine */
#define WDT_OK          0xae    /* we get back an acknowledgement */
#define WDT_ENABLE      0xf1    /* the command to arm to watchdog. */
#define WDT_DISABLE     0xf4    /* the command to disarm the watchdog. */

