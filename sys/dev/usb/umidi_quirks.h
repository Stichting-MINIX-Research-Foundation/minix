/*	$NetBSD: umidi_quirks.h,v 1.8 2008/07/08 11:34:43 gmcgarry Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Takuya SHIOZAKI (tshiozak@NetBSD.org).
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
 * quirk code for UMIDI
 */

#ifndef _DEV_USB_UMIDI_QUIRKS_H_
#define _DEV_USB_UMIDI_QUIRKS_H_

struct umq_data {
	int		type;
#define UMQ_TYPE_NONE		0
#define UMQ_TYPE_FIXED_EP	1
#define UMQ_TYPE_YAMAHA		2
#define UMQ_TYPE_MIDIMAN_GARBLE	3
#define UMQ_TYPE_CN_SEQ_PER_EP  4 /* should be default behavior, but isn't */
#define UMQ_TYPE_CN_SEQ_GLOBAL	5 /* shouldn't be default behavior, but is */
#define UMQ_TYPE_CN_FIXED       6 /* should be a joke, but isn't funny */
#define UMQ_TYPE_MD_FIXED       7 /* in case CN_FIXED gives a weird order */
	const void	*data;
};

struct umidi_quirk {
	int			vendor;
	int			product;
	int			iface;
	const struct umq_data	*quirks;
        u_int32_t		type_mask;
};
#define UMQ_ISTYPE(q, type) \
	((q)->sc_quirk && ((q)->sc_quirk->type_mask & (1<<((type)-1))))

#define UMQ_TERMINATOR { .type = UMQ_TYPE_NONE, },
#define UMQ_DEF(v, p, i)					\
static const struct umq_data	umq_##v##_##p##_##i[]
#define UMQ_REG(v, p, i)					\
	{ USB_VENDOR_##v, USB_PRODUCT_##p, i,			\
	  umq_##v##_##p##_##i, 0 }
#define ANYIFACE			-1
#define ANYVENDOR			-1
#define USB_VENDOR_ANYVENDOR		ANYVENDOR
#define ANYPRODUCT			-1
#define USB_PRODUCT_ANYPRODUCT		ANYPRODUCT

/*
 * quirk - fixed port. By the way, the ep field contains not the
 * endpoint address, but the index of the endpoint descriptor.
 */

struct umq_fixed_ep_endpoint {
	int	ep;
	int	num_jacks;
};
struct umq_fixed_ep_desc {
	int					num_out_ep;
	int					num_in_ep;
	const struct umq_fixed_ep_endpoint	*out_ep;
	const struct umq_fixed_ep_endpoint	*in_ep;
};

#define UMQ_FIXED_EP_DATA_DEF(v, p, i, noep, niep)			\
static const struct umq_fixed_ep_endpoint				\
umq_##v##_##p##_##i##_fixed_ep_endpoints[noep+niep]

#define UMQ_FIXED_EP_DEF(v, p, i, noep, niep)				\
static const struct umq_fixed_ep_desc					\
umq_##v##_##p##_##i##_fixed_ep_desc = {					\
	noep, niep,							\
	&umq_##v##_##p##_##i##_fixed_ep_endpoints[0],			\
	&umq_##v##_##p##_##i##_fixed_ep_endpoints[noep],		\
};									\

#define UMQ_FIXED_EP_REG(v, p, i)					\
{ UMQ_TYPE_FIXED_EP, &umq_##v##_##p##_##i##_fixed_ep_desc }

/*
 * quirk - fixed cable numbers. Supply as many values as there are jacks,
 * in the same jack order implied by the FIXED_EP_DEF. Each value becomes
 * the cable number of the corresponding jack.
 */
#define UMQ_FIXED_CN_DEF(v, p, i)					\
static const unsigned char umq_##v##_##p##_##i##_fixed_cn_desc[]
#define UMQ_FIXED_CN_REG(v, p, i)					\
{ UMQ_TYPE_CN_FIXED, &umq_##v##_##p##_##i##_fixed_cn_desc }

/*
 * quirk - fixed mididev assignment. Supply pairs of numbers out, in, as
 * many pairs as mididevs (that is, max(num_out_jack,num_in_jack)). The
 * pairs, in order, correspond to the mididevs that will be created; in
 * each pair, out is the index of the out_jack to bind and in is the
 * index of the in_jack, both in the order implied by the FIXED_EP_DEF.
 * Either out or in can be -1 to bind no out jack or in jack, respectively,
 * to the corresponding mididev.
 */
#define UMQ_FIXED_MD_DEF(v, p, i)					\
static const unsigned char umq_##v##_##p##_##i##_fixed_md_desc[]
#define UMQ_FIXED_MD_REG(v, p, i)					\
{ UMQ_TYPE_MD_FIXED, &umq_##v##_##p##_##i##_fixed_md_desc }

/*
 * generic boolean quirk, no data
 */
#define UMQ_TYPE(t)							\
{ UMQ_TYPE_##t, NULL }

/*
 * quirk - yamaha style midi I/F
 */
#define UMQ_YAMAHA_REG(v, p, i)						\
UMQ_TYPE(YAMAHA)


/* extern const struct umidi_quirk umidi_quirklist[]; */
const struct umidi_quirk *umidi_search_quirk(int, int, int);
void umidi_print_quirk(const struct umidi_quirk *);
const void *umidi_get_quirk_data_from_type(const struct umidi_quirk *, u_int32_t);

#endif
