/* $NetBSD: eldreg.h,v 1.1 2015/03/28 14:09:59 jmcneill Exp $ */

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

#ifndef _ELDREG_H
#define _ELDREG_H

#define	ELD_MAX_NBYTES		80	/* version 2; 15 SAD count */

struct eld_header_block {
	uint8_t flags;			/* ver */
	uint8_t	reserved1;
	uint8_t	baseline_eld_len;	/* dword count */
	uint8_t reserved2;
} __packed;

struct eld_baseline_block {
	struct eld_header_block	header;
	uint8_t flags[4];		/* cea_edid_ver, mnl,
					 * sad_count, conn_type, s_ai, hdcp,
					 * aud_synch_delay,
					 * rlrc, flrc, rc, rlr, fc, lfe, flr
					 */
	uint64_t port_id;
	uint16_t vendor;
	uint16_t product;
	/* monitor name string, CEA SADs, vendor data ... */
} __packed;

#define	ELD_VER(block)		(((block)->header.flags >> 3) & 0x1f)
#define	 ELD_VER_2		 0x02
#define	 ELD_VER_UNCONF		 0x1f
#define ELD_CEA_EDID_VER(block)	(((block)->flags[0] >> 5) & 0x07)
#define	 ELD_CEA_EDID_VER_NONE	 0x0
#define	 ELD_CEA_EDID_VER_861	 0x1
#define	 ELD_CEA_EDID_VER_861A	 0x2
#define	 ELD_CEA_EDID_VER_861BCD 0x3
#define	ELD_MNL(block)		(((block)->flags[0] >> 0) & 0x1f)
#define	ELD_SAD_COUNT(block)	(((block)->flags[1] >> 4) & 0x0f)
#define	ELD_CONN_TYPE(block)	(((block)->flags[1] >> 2) & 0x03)
#define	 ELD_CONN_TYPE_HDMI	 0x0
#define	ELD_S_AI(block)		(((block)->flags[1] >> 1) & 0x01)
#define	ELD_HDCP(block)		(((block)->flags[1] >> 0) & 0x01)
#define	ELD_AUDIO_DELAY(block)	((block)->flags[2])
#define	 ELD_AUDIO_DELAY_NONE	 0x00
#define  ELD_AUDIO_DELAY_MS(x)	 ((x) * 2)
#define	 ELD_AUDIO_DELAY_MS_MAX	 500
#define	ELD_SPEAKER(block) 	((block)->flags[3])
#define	 ELD_SPEAKER_RLRC	 0x40
#define	 ELD_SPEAKER_FLRC	 0x20
#define	 ELD_SPEAKER_RC		 0x10
#define	 ELD_SPEAKER_RLR	 0x08
#define	 ELD_SPEAKER_FC		 0x04
#define	 ELD_SPEAKER_LFE	 0x02
#define	 ELD_SPEAKER_FLR	 0x01

#endif /* !_ELDREG_H */
