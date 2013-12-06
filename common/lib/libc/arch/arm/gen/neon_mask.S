/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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

#include <machine/asm.h>

#ifdef _ARM_ARCH_7

RCSID("$NetBSD: neon_mask.S,v 1.2 2012/12/18 06:14:23 matt Exp $")

/*
 * __neon_loading_qword_bitmask(size_t len);
 *  IN	r0 = length of mask in bits
 * OUT	q0 = mask
 */
ENTRY(__neon_leading_qword_bitmask)
	cmp		r0, #64		/* which dword is partial? */
#ifdef __ARMEL__
	sublt		r0, r0, #64	/* 1st dword needs MSBs cleared */
	subge		r1, r0, #128	/* 2nd dword needs MSBs cleared */
#else
	rsblt		r0, r0, #64	/* 1st dword needs LSBs cleared */
	rsbge		r1, r0, #128	/* 2nd dword needs LSBs cleared */
#endif
	movge		r0, #0		/* 1st dword needs to left alone */
	movlt		r1, #64		/* 2st dword needs to be cleared */
	vmov		d2, r0, r1	/* move dword shifts to SIMD */
	vmovl.u32	q1, d2		/* 2 U32 -> 2 U64 */
	vmvn.u64	q0, #0		/* create a mask */
	vshl.u64	q0, q0, q1	/* shift out unneeded bytes */
	RET
END(__neon_leading_qword_bitmask)

#endif	/* _ARM_ARCH_7 */
