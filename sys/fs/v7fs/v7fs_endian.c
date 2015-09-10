/*	$NetBSD: v7fs_endian.c,v 1.2 2011/07/18 21:51:49 apb Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: v7fs_endian.c,v 1.2 2011/07/18 21:51:49 apb Exp $");
#if defined _KERNEL_OPT
#include "opt_v7fs.h"
#endif

#include "v7fs.h"
#include "v7fs_endian.h"
#include "v7fs_impl.h"

#ifndef BYTE_ORDER
#error
#endif

/* PDP to Little */
#define	bswap32pdp_le(x)			\
	((uint32_t)				\
	    ((((x) & 0xffff0000) >> 16) |	\
		(((x) & 0x0000ffff) << 16)))
/* PDP to Big */
#define	bswap32pdp_be(x)			\
	((uint32_t)				\
	    ((((x) & 0xff00ff00) >> 8) |	\
		(((x) & 0x00ff00ff) <<  8)))
#ifdef V7FS_EI
static uint32_t val32_normal_order(uint32_t);
static uint32_t val32_reverse_order(uint32_t);
#if BYTE_ORDER == LITTLE_ENDIAN
static uint32_t val32_pdp_to_little(uint32_t);
#else
static uint32_t val32_pdp_to_big(uint32_t);
#endif
static uint16_t val16_normal_order(uint16_t);
static uint16_t val16_reverse_order(uint16_t);
static v7fs_daddr_t val24_reverse_order_read(uint8_t *);
static void val24_reverse_order_write(v7fs_daddr_t, uint8_t *);
static v7fs_daddr_t val24_pdp_read(uint8_t *);
static void val24_pdp_write(v7fs_daddr_t, uint8_t *);

static uint32_t
val32_normal_order(uint32_t v)
{

	return v;
}

static uint32_t
val32_reverse_order(uint32_t v)
{

	return bswap32(v);
}
#if BYTE_ORDER == LITTLE_ENDIAN
static uint32_t
val32_pdp_to_little(uint32_t v)
{

	return bswap32pdp_le(v);
}
#else
static uint32_t
val32_pdp_to_big(uint32_t v)
{

	return bswap32pdp_be(v);
}
#endif
static uint16_t
val16_normal_order(uint16_t v)
{

	return v;
}

static uint16_t
val16_reverse_order(uint16_t v)
{

	return bswap16(v);
}

static v7fs_daddr_t
val24_reverse_order_read(uint8_t *a)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	return (a[0] << 16) | (a[1] << 8) | a[2];
#else
	return (a[2] << 16) | (a[1] << 8) | a[0];
#endif
}

static void
val24_reverse_order_write(v7fs_daddr_t addr, uint8_t *a)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	a[0] = (addr >> 16) & 0xff;
	a[1] = (addr >> 8) & 0xff;
	a[2] = addr & 0xff;
#else
	a[0] = addr & 0xff;
	a[1] = (addr >> 8) & 0xff;
	a[2] = (addr >> 16) & 0xff;
#endif
}

static v7fs_daddr_t
val24_pdp_read(uint8_t *a)
{

	return (a[0] << 16) | a[1] | (a[2] << 8);
}

static void
val24_pdp_write(v7fs_daddr_t addr, uint8_t *a)
{

	a[0] = (addr >> 16) & 0xff;
	a[1] = addr & 0xff;
	a[2] = (addr >> 8) & 0xff;
}

void
v7fs_endian_init(struct v7fs_self *fs)
{
	struct endian_conversion_ops *ops = &fs->val;

	switch (fs->endian)
	{
#if BYTE_ORDER == LITTLE_ENDIAN
	case LITTLE_ENDIAN:
		ops->conv32 = val32_normal_order;
		ops->conv16 = val16_normal_order;
		ops->conv24read = val24_normal_order_read;
		ops->conv24write = val24_normal_order_write;
		break;
	case BIG_ENDIAN:
		ops->conv32 = val32_reverse_order;
		ops->conv16 = val16_reverse_order;
		ops->conv24read = val24_reverse_order_read;
		ops->conv24write = val24_reverse_order_write;
		break;
	case PDP_ENDIAN:
		ops->conv32 = val32_pdp_to_little;
		ops->conv16 = val16_normal_order;
		ops->conv24read = val24_pdp_read;
		ops->conv24write = val24_pdp_write;
		break;
#else /* BIG_ENDIAN */
	case LITTLE_ENDIAN:
		ops->conv32 = val32_reverse_order;
		ops->conv16 = val16_reverse_order;
		ops->conv24read = val24_reverse_order_read;
		ops->conv24write = val24_reverse_order_write;
		break;
	case BIG_ENDIAN:
		ops->conv32 = val32_normal_order;
		ops->conv16 = val16_normal_order;
		ops->conv24read = val24_normal_order_read;
		ops->conv24write = val24_normal_order_write;
		break;
	case PDP_ENDIAN:
		ops->conv32 = val32_pdp_to_big;
		ops->conv16 = val16_reverse_order;
		ops->conv24read = val24_pdp_read;
		ops->conv24write = val24_pdp_write;
		break;
#endif
	}
}
#endif	/* V7FS_EI */
v7fs_daddr_t
val24_normal_order_read(uint8_t *a)
{
	/*(v7fs_daddr_t)cast is required for int 16bit system. */
#if BYTE_ORDER == LITTLE_ENDIAN
	return ((v7fs_daddr_t)a[2] << 16) | ((v7fs_daddr_t)a[1] << 8) |
	    (v7fs_daddr_t)a[0];
#else
	return ((v7fs_daddr_t)a[0] << 16) | ((v7fs_daddr_t)a[1] << 8) |
	    (v7fs_daddr_t)a[2];
#endif
}

void
val24_normal_order_write(v7fs_daddr_t addr, uint8_t *a)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	a[0] = addr & 0xff;
	a[1] = (addr >> 8) & 0xff;
	a[2] = (addr >> 16) & 0xff;
#else
	a[0] = (addr >> 16) & 0xff;
	a[1] = (addr >> 8) & 0xff;
	a[2] = addr & 0xff;
#endif
}
