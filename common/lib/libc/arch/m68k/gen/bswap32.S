/*	$NetBSD: bswap32.S,v 1.6 2014/03/18 18:20:35 riastradh Exp $	*/

/*
 * Copyright (C) 1996 Scott Reynolds.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>

#if defined(_KERNEL) || defined(_STANDALONE)
#define BSWAP32 _C_LABEL(bswap32)
#else /* defined(_KERNEL) || defined(_STANDALONE) */
#define BSWAP32 _C_LABEL(__bswap32)
#endif /* defined(_KERNEL) || defined(_STANDALONE) */

_ENTRY(BSWAP32)
	movl	4(%sp),%d0
#ifdef __mcfisac__
	byterev	%d0
#elif defined(__mcoldfire__)
	movl	%d0,%d1
	lsrl	#8,%d0		| ABCD -> 0ABC
	andl	#0x00ff00ff,%d0	| 0ABC -> 0A0C
	andl	#0x00ff00ff,%d1	| ABCD -> 0B0D
	lsll	#8,%d1		| 0B0D -> B0D0
	orl	%d1,%d0		| 0A0C | B0D0 -> BADC
	swap	%d0		| BADC -> DCBA
#else
 	rolw	#8,%d0
 	swap	%d0
	rolw	#8,%d0
#endif
	rts
END(BSWAP32)
