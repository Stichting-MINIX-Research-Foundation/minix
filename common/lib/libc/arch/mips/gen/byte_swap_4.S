/*	$NetBSD: byte_swap_4.S,v 1.3 2009/12/14 00:39:00 matt Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <mips/asm.h>

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	RCSID("from: @(#)htonl.s	8.1 (Berkeley) 6/4/93")
#else
	RCSID("$NetBSD: byte_swap_4.S,v 1.3 2009/12/14 00:39:00 matt Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

#undef _LOCORE
#define _LOCORE		/* XXX not really, just assembly-code source */
#include <machine/endian.h>

#if defined(_KERNEL) || defined(_STANDALONE)
#define	BSWAP32_NAME	bswap32
#else
#define	BSWAP32_NAME	__bswap32
#endif

NLEAF(BSWAP32_NAME)			# a0 = 0x11223344, return 0x44332211
#if BYTE_ORDER == LITTLE_ENDIAN
ALEAF(htonl)				# a0 = 0x11223344, return 0x44332211
ALEAF(ntohl)
#endif
#if (__mips == 32 || __mips == 64) && __mips_isa_rev == 2
	/*
	 * If we are on MIPS32R2 or MIPS64R2 it's much easier
	 */
	wsbh	a0, a0			# word swap bytes within halfwords
	rotr	v0, a0, 16		# rotate word 16bits
	j	ra
#else
	srl	v1, a0, 24		# v1 = 0x00000011
	sll	v0, a0, 24		# v0 = 0x44000000
	or	v0, v0, v1
	and	v1, a0, 0xff00
	sll	v1, v1, 8		# v1 = 0x00330000
	or	v0, v0, v1
	srl	v1, a0, 8
	and	v1, v1, 0xff00		# v1 = 0x00002200
	or	v0, v0, v1
	j	ra
#endif
END(BSWAP32_NAME)

#if BYTE_ORDER == BIG_ENDIAN
NLEAF(htonl)				# a0 = 0x11223344, return 0x44332211
ALEAF(ntohl)
	move	v0, a0
	j	ra
END(htonl)
#endif
