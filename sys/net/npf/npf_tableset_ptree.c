/*	$NetBSD: npf_tableset_ptree.c,v 1.1 2012/07/15 00:23:01 rmind Exp $	*/

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas <matt@3am-software.com>.
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
 * Patricia/RADIX tree comparators for NPF tables.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: npf_tableset_ptree.c,v 1.1 2012/07/15 00:23:01 rmind Exp $");

#include <sys/param.h>
#include <sys/types.h>

#include <sys/bitops.h>
#include <sys/ptree.h>

#include "npf_impl.h"

#define	DIV32		5

static pt_slot_t
npf_ptree_testkey(const void *vkey, pt_bitoff_t bitoff,
    pt_bitlen_t bitlen, void *ctx)
{
	const uint32_t *addr = (const uint32_t *)vkey;

	KASSERT(bitoff < 32 * (const uintptr_t)ctx);
	KASSERT(bitlen == 1);

	const uint32_t bits = ntohl(addr[bitoff >> DIV32]);
	const uint32_t mask = 0x80000000U >> (bitoff & 31);
	return (bits & mask) ? PT_SLOT_RIGHT : PT_SLOT_LEFT;
}

static bool
npf_ptree_matchkey(const void *vleft, const void *vright,
    pt_bitoff_t bitoff, pt_bitlen_t bitlen, void *ctx)
{
	const uint32_t *left = (const uint32_t *)vleft;
	const uint32_t *right = (const uint32_t *)vright;
	const u_int nwords = (const uintptr_t)ctx;
	size_t i = bitoff >> DIV32;

	/* Constrain bitlen to a reasonable value. */
	if (nwords * 32 < bitoff + bitlen || nwords * 32 < bitlen) {
		bitlen = nwords * 32 - bitoff;
	}

	/* Find the first word from the offset. */
	left += i;
	right += i;
	bitoff -= i * 32;

	for (; i < nwords; i++, left++, right++, bitoff = 0) {
		const uint32_t bits = ntohl(*left ^ *right);
		const signed int xbitlen = 32 - (bitoff + bitlen);
		uint32_t mask = UINT32_MAX >> bitoff;

		/*
		 * We have the mask up to the lowest bit.  Overlap with the
		 * mask up to required lowest bit, if extacting the middle.
		 */
		KASSERT((size_t)bitoff < 32);
		if (xbitlen > 0) {
			mask &= UINT32_MAX << xbitlen;
		}

		/* Compare the masked part of the word. */
		if (bits & mask) {
			return false;
		}

		/* Done if extracting the middle or exactly up to the LSB. */
		if (xbitlen >= 0) {
			break;
		}
		bitlen = -xbitlen;
	}

	return true;
}

static bool
npf_ptree_matchnode(const void *vleft, const void *vright,
    pt_bitoff_t maxbitoff, pt_bitoff_t *bitoffp, pt_bitoff_t *slotp, void *ctx)
{
	static const uint32_t zeroes[4] = { 0, 0, 0, 0 };
	const uint32_t *left = (const uint32_t *)vleft;
	const uint32_t *right = vright ? (const uint32_t *)vright : zeroes;
	pt_bitoff_t bitoff = *bitoffp;
	const u_int nwords = (const uintptr_t)ctx;
	size_t i = bitoff >> DIV32;

	/* Constrain maxbitoff & bitlen to reasonable value. */
	if (maxbitoff > nwords * 32) {
		maxbitoff = nwords * 32;
	}
	pt_bitoff_t bitlen = maxbitoff - bitoff;

	/* Find the first word from the offset. */
	*slotp = PT_SLOT_LEFT;
	left += i;
	right += i;
	bitoff -= i * 32;

	for (; i < nwords; i++, left++, right++, bitoff = 0) {
		const signed int xbitlen = 32 - (bitoff + bitlen);
		uint32_t bits = ntohl(*left ^ *right);
		uint32_t mask = UINT32_MAX >> bitoff;

		KASSERT((size_t)bitoff < 32);
		if (xbitlen > 0) {
			mask &= UINT32_MAX << xbitlen;
		}

		/* Compare the masked part of the word. */
		bits &= mask;
		if (bits) {
			/*
			 * Did not match.  Find the bit where the difference
			 * occured.  Also, determine the slot.
			 */
			bitoff = 32 * i + (32 - fls32(bits));

			KASSERT(bitoff < nwords * 32);
			KASSERT(bitoff >= *bitoffp);
			KASSERT(bitoff <= maxbitoff);

			*bitoffp = bitoff;
			if ((ntohl(*left) >> (31 - bitoff)) & 1) {
				*slotp = PT_SLOT_RIGHT;
			}

			KASSERT(npf_ptree_testkey(vleft, bitoff, 1, ctx)
			    == *slotp);
			return false;
		}
		if (xbitlen >= 0) {
			i++;
			break;
		}
		bitlen = -xbitlen;
	}

	bitoff = 32 * i;
	*bitoffp = bitoff < maxbitoff ? bitoff : maxbitoff;
	return true;
}

const pt_tree_ops_t npf_table_ptree_ops = {
	.ptto_matchnode	= npf_ptree_matchnode,
	.ptto_matchkey	= npf_ptree_matchkey,
	.ptto_testnode	= npf_ptree_testkey,
	.ptto_testkey	= npf_ptree_testkey,
};
