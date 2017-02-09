/*	$NetBSD: if_hippi.h,v 1.13 2008/04/28 20:24:09 martin Exp $	*/

/*
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code contributed to The NetBSD Foundation by Kevin M. Lahey
 * of the Numerical Aerospace Simulation Facility, NASA Ames Research
 * Center.
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

#ifndef _NET_IF_HIPPI_H_
#define _NET_IF_HIPPI_H_

#include <net/if_ether.h>

struct hippi_fp {
	uint8_t  fp_ulp;
	uint8_t  fp_flags;
#define HIPPI_FP_D1_PRESENT	0x80
#define HIPPI_FP_D2_ON_BURST	0x40
	uint16_t fp_offsets;
#define HIPPI_FP_D2_MASK	0x07
	uint32_t fp_d2_len;
} __packed;

struct hippi_le {
	uint32_t le_dest_switch;
	uint32_t le_src_switch;
	uint16_t le_reserved;
	uint8_t  le_dest_addr[6];
	uint16_t le_local_admin;
	uint8_t  le_src_addr[6];
} __packed;

struct hippi_header {
	struct hippi_fp	hi_fp;
	struct hippi_le	hi_le;
} __packed;

#define HIPPI_HDRLEN (sizeof(struct hippi_header))

/* Link-layer ULP identifiers: */

#define HIPPI_ULP_802	     4
#define HIPPI_ULP_IPI3_SLAVE  5
#define HIPPI_ULP_IPI3_MASTER 6
#define HIPPI_ULP_IPI3_PEER   7

/* Need some flags here for the rest of it! */

#define HIPPIMTU 65280	/* Argh, this oughta change! */


#ifdef _KERNEL
void    hippi_ifattach(struct ifnet *, void *);
void    hippi_ip_input(struct ifnet *, struct mbuf *);
#endif /* _KERNEL */
#endif /* !_NET_IF_HIPPI_H_ */
