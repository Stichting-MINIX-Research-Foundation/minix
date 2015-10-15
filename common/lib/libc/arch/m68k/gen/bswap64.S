/*	$NetBSD: bswap64.S,v 1.5 2014/03/18 18:20:35 riastradh Exp $	*/

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

ENTRY(bswap64)
	movl	4(%sp),%d1
	movl	8(%sp),%d0
#ifdef __mcfisac__
	byterev	%d0
	byterev	%d1
#elif defined(__mcoldfire__)
	movl	%d3,-(%sp)
	movl	%d2,-(%sp)
	movl	#0x00ff00ff, %d3
	movl	%d0,%d2
	andl	%d3,%d2
	lsll	#8,%d2
	lsrl	#8,%d0
	andl	%d3,%d0
	orl	%d2,%d0
	movl	%d1,%d2
	andl	%d3,%d2
	lsll	#8,%d2
	lsrl	#8,%d1
	andl	%d3,%d1
	orl	%d2,%d1
	movl	(%sp)+,%d2
	movl	(%sp)+,%d3
#else
	rolw	#8,%d1
	rolw	#8,%d0
	swap	%d1
	swap	%d0
	rolw	#8,%d0
	rolw	#8,%d1
#endif
	rts
END(bswap64)
