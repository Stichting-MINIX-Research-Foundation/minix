/*	$NetBSD: ieee8023ad_marker.h,v 1.2 2005/12/10 23:21:39 elad Exp $	*/

/*-
 * Copyright (c)2005 YAMAMOTO Takashi,
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _NET_AGR_IEEE8023AD_MARKER_H_
#define	_NET_AGR_IEEE8023AD_MARKER_H_

/*
 * IEEE802.3ad marker protocol
 *
 * protocol (on-wire) definitions.
 *
 * XXX should be elsewhere.
 */

struct markerdu {
	struct ether_header mdu_eh;
	struct slowprothdr mdu_sph;

	struct tlvhdr mdu_tlv;
	uint16_t mdu_rq_port;
	uint8_t mdu_rq_system[6];
	uint8_t mdu_rq_xid[4];
	uint8_t mdu_pad[2];

	struct tlvhdr mdu_tlv_term;
	uint8_t mdu_resv[90];
} __packed;

#define	MARKER_TYPE_INFO	1
#define	MARKER_TYPE_RESPONSE	2

#endif /* !_NET_AGR_IEEE8023AD_MARKER_H_ */
