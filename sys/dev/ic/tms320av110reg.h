/*	$NetBSD: tms320av110reg.h,v 1.4 2008/04/28 20:23:51 martin Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ignatios Souvatzis.
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
 * Definitions for access to the TMS320AV110AV mpeg audio decoder.
 * Based on the TMS320AV110 data sheet.
 *
 * Currently, only minimum support for audio output. For audio/video
 * synchronization, more is needed.
 */

#ifndef _TMS320AV110_REG_H_
#define _TMS320AV110_REG_H_

/* symbolic registers and values */

#define TAV_ANC				0x06	/* RO, 4 bytes */
#define TAV_ANC_AV			0x6c	/* RO */
#define TAV_ATTEN_L			0x1e
#define TAV_ATTEN_R			0x20
#define TAV_AUD_ID			0x22
#define TAV_AUD_ID_EN			0x24

#define TAV_BALE_LIM			0x68	/* 2 bytes */
#define TAV_BALF_LIM			0x6A	/* 2 bytes */
#define TAV_BUFF			0x12	/* RO, 2 bytes */

#define	TAV_CRC_ECM			0x2a
#define TAV_ECM_IGNORE				0	/* same for SYNC */
#define TAV_ECM_MUTE				1
#define TAV_ECM_REPEAT				2
#define TAV_ECM_SKIP				3

#define TAV_DATAIN			0x18	/* WO */
#define TAV_DIF				0x6f

#define TAV_DMPH			0x46
#define TAV_DRAM_EXT			0x3e	/* RO */
#define TAV_DRAM_SIZE(ext) ((ext) ? 131072 : 256)
#define TAV_DRAM_HSIZE(ext) ((ext) ? 65536 : 128)

#define TAV_FREE_FORM			0x14	/* RW, 11 bit */

#define TAV_HEADER			0x5e	/* RO, 4 bytes */

#define TAV_INTR			0x1a	/* RO, 2 bytes */
#define TAV_INTR_EN			0x1c	/* RW, 2 bytes */
#define TAV_INTR_SYNCCHANGE			0x0001
#define TAV_INTR_HEADERVALID			0x0002
#define TAV_INTR_PTSVALID			0x0004
#define TAV_INTR_LOWWATER			0x0008
#define TAV_INTR_HIGHWATER			0x0010
#define TAV_INTR_CRCERROR			0x0020
#define TAV_INTR_ANCILLARY_VALID		0x0040
#define TAV_INTR_ANCILLARY_FULL			0x0080
#define TAV_INTR_PCM_OUTPUT_UNDERFLOW		0x0100
#define TAV_INTR_SAMPLING_FREQ_CHANGE		0x0200
#define TAV_INTR_DEEMPH_CHANGE			0x0400
#define TAV_INTR_SRC_DETECT			0x0800

#define TAV_IRC				0x78	/* RO, 33 bit */
#define TAV_IRC_CNT			0x54	/* RO, 33 bit */
#define TAV_IRC_LOAD			0x7e

#define TAV_LATENCY			0x3c
#define TAV_MUTE			0x30

#define TAV_PCM_DIV			0x6e
#define TAV_PCM_18			0x16
#define TAV_PCM_FS			0x44	/* RO */
#define TAV_PCM_ORD			0x38

#define TAV_PLAY			0x2e
#define TAV_PTS				0x62	/* RO, 33 bits */
#define TAV_REPEAT			0x34
#define TAV_RESET			0x40
#define TAV_RESTART			0x42

#define TAV_SRC				0x72	/* RO, 33 bits */
#define TAV_SIN_EN			0x70
#define TAV_SKIP			0x32

#define TAV_STR_SEL			0x36
#define TAV_STR_SEL_MPEG_AUDIO_STREAM		0
#define TAV_STR_SEL_MPEG_AUDIO_PACKETS		1
#define TAV_STR_SEL_MPEG_SYSTEM_STREAM		2
#define TAV_STR_SEL_AUDIO_BYPASS		3

#define TAV_SYNC_ECM			0x2c	/* see CRC_ECM */

#define	TAV_SYNC_ST			0x28	/* 0..3 */
#define	TAV_SYNC_ST_UNLOCKED			0
#define	TAV_SYNC_ST_ATTEMPTING			2
#define	TAV_SYNC_ST_LOCKED			3

#define	TAV_VERSION			0x6d

#endif /* _TMS320AV110_REG_H_ */
