/* $NetBSD: hdmireg.h,v 1.1 2015/03/28 14:09:59 jmcneill Exp $ */

/*
 * Copyright (c) 2011 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _HDMIREG_H
#define _HDMIREG_H

struct hdmi_audio_infoframe_header {
	uint8_t		packet_type;
	uint8_t		version;
	uint8_t		length;
} __packed;

#define	HDMI_AI_PACKET_TYPE	0x84
#define	HDMI_AI_VERSION		0x01
#define	HDMI_AI_LENGTH		0x0a

struct hdmi_audio_infoframe {
	struct hdmi_audio_infoframe_header header;
	uint8_t		checksum;
	uint8_t		ct_cc;
	uint8_t		sf_ss;
	uint8_t		ct_ext;
	uint8_t		ca;
	uint8_t		dm_inh_lsv;
	uint8_t		reserved;
} __packed;

#define	HDMI_AI_CHANNEL_COUNT(ai)	((ai)->ct_cc & 0x07)
#define	HDMI_AI_CODING_TYPE(ai)		((ai)->ct_cc & 0xf0)
#define	HDMI_AI_SAMPLE_SIZE(ai)		((ai)->sf_ss & 0x03)
#define	HDMI_AI_SAMPLE_FREQUENCY(ai)	((ai)->sf_ss & 0x18)
#define	HDMI_AI_LSV(ai)			((ai)->dm_inh_lsv & 0x78)
#define	HDMI_AI_DM_INH(ai)		((ai)->dm_inh_lsv & 0x80)

#endif /* !_HDMIREG_H */
