/*	$NetBSD: ieee8023_tlv.h,v 1.3 2007/02/21 23:00:06 thorpej Exp $	*/

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

#ifndef _NET_AGR_IEEE8023_TLV_H_
#define	_NET_AGR_IEEE8023_TLV_H_

/*
 * TLV on-wire structure.
 */

struct tlvhdr {
	uint8_t tlv_type;
	uint8_t tlv_length;
	/* uint8_t tlv_value[]; */
} __packed;

/*
 * ... and our implementation.
 */

#define	TLV_SET(tlv, type, length) \
	do { \
		(tlv)->tlv_type = (type); \
		(tlv)->tlv_length = sizeof(*tlv) + (length); \
	} while (/*CONSTCOND*/0)

struct tlv_template {
	uint8_t tmpl_type;
	uint8_t tmpl_length;
};

int tlv_check(const void *, size_t, const struct tlvhdr *,
    const struct tlv_template *, bool);

#endif /* !_NET_AGR_IEEE8023_TLV_H_ */
