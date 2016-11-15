/* $NetBSD: emdtvreg.h,v 1.2 2014/06/21 03:44:06 dholland Exp $ */

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

#ifndef _DEV_USB_EMDTVREG_H
#define _DEV_USB_EMDTVREG_H

struct emdtv_eeprom {
	uint32_t	id;	/* 0x9567eb1a */
	uint16_t	vendor;
	uint16_t	product;
	uint16_t	chipcfg;
	uint16_t	boardcfg;
	uint16_t	string[3];
	uint8_t		stringidx;
} __packed;

#define EM28XX_I2C_ADDR_EEPROM	0xa0

#define EM28XX_UR_I2C		0x02

#define EM28XX_REG_I2C_STATUS	0x05

#define EM28XX_I2C_CLK_REG	0x06
#define EM28XX_XCLK_REG		0x0f

#define EM28XX_REG_IR		0x45
#define		EM28XX_IR_RECORD	0x00
#define		EM28XX_IR_PAUSEPLAY	0x01
#define		EM28XX_IR_STOP		0x02
#define		EM28XX_IR_POWER		0x03
#define		EM28XX_IR_PREV		0x04
#define		EM28XX_IR_REWIND	0x05
#define		EM28XX_IR_FORWARD	0x06
#define		EM28XX_IR_NEXT		0x07
#define		EM28XX_IR_EPG		0x08
#define		EM28XX_IR_HOME		0x09
#define		EM28XX_IR_DVDMENU	0x0a
#define		EM28XX_IR_CHANNEL_UP	0x0b
#define		EM28XX_IR_BACK		0x0c
#define		EM28XX_IR_UP		0x0d
#define		EM28XX_IR_INFO		0x0e
#define		EM28XX_IR_CHANNEL_DOWN	0x0f
#define		EM28XX_IR_LEFT		0x10
#define		EM28XX_IR_OK		0x11
#define		EM28XX_IR_RIGHT		0x12
#define		EM28XX_IR_VOLUME_UP	0x13
#define		EM28XX_IR_LAST		0x14
#define		EM28XX_IR_DOWN		0x15
#define		EM28XX_IR_VOLUME_MUTE	0x16
#define		EM28XX_IR_VOLUME_DOWN	0x17

#endif /* !_DEV_USB_EMDTVREG_H */
