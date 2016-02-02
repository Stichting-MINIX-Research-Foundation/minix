/*	$NetBSD: bswap16.S,v 1.6 2014/03/18 18:20:35 riastradh Exp $	*/

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
#define BSWAP16 _C_LABEL(bswap16)
#else /* defined(_KERNEL) || defined(_STANDALONE) */
#define BSWAP16 _C_LABEL(__bswap16)
#endif /* defined(_KERNEL) || defined(_STANDALONE) */

_ENTRY(BSWAP16)
#ifdef __mcfisac__
	mvzw	6(%sp),%d0
	swap	%d0
	byterev	%d0
#elif defined(__mcoldfire__)
	mvzw	6(%sp),%d0
	movl	%d0,%d1
	mvzb	%d0,%d0
	lsll	#8,%d0
	lsrl	#8,%d1
	orl	%d1,%d0
#else
	movl	4(%sp),%d0
	rolw	#8,%d0
#endif
	rts
END(BSWAP16)
