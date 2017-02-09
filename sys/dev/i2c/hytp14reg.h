/* $NetBSD: hytp14reg.h,v 1.2 2014/06/29 09:06:05 kardel Exp $ */

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank Kardel.
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
 * IST-AG P14 calibrated Hygro-/Temperature sensor module
 * Devices: HYT-271, HYT-221 and HYT-939 
 *
 * see:
 * http://www.ist-ag.com/eh/ist-ag/resource.nsf/imgref/Download_AHHYTM_E2.1.pdf/
 *      $FILE/AHHYTM_E2.1.pdf
 */ 
#ifndef _DEV_I2C_HYTP14REG_H_
#define _DEV_I2C_HYTP14REG_H_

#define HYTP14_DEFAULT_ADDR	0x28

#define HYTP14_CMD_START_NOM	0x80 /* end command mode (or power-off)  */
#define HYTP14_CMD_START_CM	0xA0 /* start command mode (within 10ms after power-up) */
#define HYTP14_CMD_GET_REV	0xB0 /* get revison */

#define HYTP14_NUM_WORDS	32
#define HYTP14_READ_OFFSET	0x00 /* command offset to read EEPROM words */
#define HYTP14_WRITE_OFFSET	0x40 /* command offset to write EEPROM words */

#define HYTP14_READ_EEPROM(_X_)	(HYTP14_READ_OFFSET + ((_X_) & (HYTP14_NUM_WORDS - 1)))
#define HYTP14_WRITE_EEPROM(_X_) (HYTP14_WRITE_OFFSET + ((_X_) & (HYTP14_NUM_WORDS - 1)))

#define HYTP14_EEADDR_I2CADDR	0x1C /* I2C address EEPROM word address */

#define HYTP14_RESP_CMDMODE	0x80 /* command mode response */
#define HYTP14_RESP_STALE	0x40 /* stale measurement data */

#define HYT_STATUS_FMT "\177\20b\7CM\0b\6STALE\0b\5ERR_CFG\0b\4ERR_RAM\0b\3ERR_UNCEEP\0"\
                       "b\2ERR_COREEP\0f\0\2RESP\0=\0BSY\0=\1ACK\0=\2NAK\0=\3INV\0\0"

#define HYTP14_DIAG_ERR_CFG	0x20 /* configuration error */
#define HYTP14_DIAG_ERR_RAMPRTY	0x10 /* RAM parity error */
#define HYTP14_DIAG_ERR_UNCEEP	0x08 /* uncorrectable EEPROM error */
#define HYTP14_DIAG_ERR_COREEP	0x04 /* correctable EEPROM error */

#define HYTP14_RESP_MASK	0x03
#define HYTP14_RESP_BUSY	0x00 /* device is busy */
#define HYTP14_RESP_ACK		0x01 /* positive acknowlege */
#define HYTP14_RESP_NACK	0x02 /* negative acknowlege */

#define HYTP14_ST_CMDMODE	0x8000 /* command mode */
#define HYTP14_ST_STALE		0x4000 /* stale measurement data */
#define HYTP14_HYG_RAWVAL(_X_)	((_X_) & 0x3FFF)
#define HYTP14_HYG_SCALE	(1<<14)
#define HYTP14_TEMP_RAWVAL(_X_)	(((_X_) >> 2) & 0x3FFF)
#define HYTP14_TEMP_SCALE	(1<<14)
#define HYTP14_TEMP_FACTOR	165
#define HYTP14_TEMP_OFFSET	(-40)

#endif
