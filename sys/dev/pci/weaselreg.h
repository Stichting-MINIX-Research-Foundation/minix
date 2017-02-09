/*	$NetBSD: weaselreg.h,v 1.2 2008/04/28 20:23:55 martin Exp $	*/

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


#ifndef _DEV_PCI_WEASELREG_H_
#define	_DEV_PCI_WEASELREG_H_

/*
 * PCI-Weasel configuration block.
 */
#define	CFG_VERSION_2		0x02
struct weasel_config_block {
	uint8_t	cfg_version;		/* version of config */
	uint8_t	weasel_attn;		/* attention character */
	uint8_t debug;			/* debug level */
	uint8_t reset_pc_on_boot;	/* reset PC Weasel on boot */
	uint8_t duart_baud;		/* UART baud rate */
	uint8_t duart_parity;		/* 0=none, 1=even, 2=odd */
	uint8_t duart_bits;		/* 7 or 8 */
	uint8_t	enable_duart_switching;	/* UART switching mode */
	uint8_t	wdt_allow;		/* WDOG is allowed */
	uint16_t	wdt_msec;	/* WDOG period in msec. */
	uint8_t	duart_flow;		/* RTS/CTS enabled */
	uint8_t	break_passthru;		/* BRK passed through to 16550 */
	uint8_t	serial_number[10];	/* board serial number */
	uint8_t	default_partition;	/* default Weasel boot image */
	uint8_t	color;			/* use color */
	uint8_t	future_use[229];	/* reserved for future use */
	uint8_t cksum;			/* always last element. */
};

#define	WEASEL_DATA_RD		0x00
#define		OS_RET_WDT_ACTIVE	0x01
#define		OS_RET_WDT_INACTIVE	0x02
#define		OS_RET_PONG		0x03

#define	WEASEL_DATA_WR		0x01
#define		OS_CMD_WDT_ENABLE	0x01
#define		OS_CMD_WDT_DISABLE	0x02
#define		OS_CMD_WDT_QUERY	0x03
#define		OS_CMD_SHOW_CONFIG	0x04
#define		OS_CMD_QUERY_SW_VER	0x05
#define		OS_CMD_QUERY_HW_VER	0x06
#define		OS_CMD_QUERY_L_VER	0x07
#define		OS_CMD_PING		0x08
#define		OS_CMD_QUERY_VB_VER	0x09
#define		OS_CMD_QUERT_BR_VER	0x0a
#define		OS_CMD_RESET_SM		0xff

#define	WEASEL_STATUS		0x02
#define		OS_WS_HOST_WRITE	0x00
#define		OS_WS_HOST_READ		0x01
#define		OS_WS_WDT_RDY		0x02
#define		OS_WS_BUSY		0xff

#define	WEASEL_HOST_STATUS	0x03
#define		OS_HS_WEASEL_WRITE	0x00
#define		OS_HS_WEASEL_READ	0x01
#define		OS_HS_WDT_RDY		0x02

#define	WEASEL_CHALLENGE	0x04

#define	WEASEL_RESPONSE		0x05

#endif /* _DEV_PCI_WEASELREG_H_ */
