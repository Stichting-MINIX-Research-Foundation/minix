/*	$NetBSD: bcmp.S,v 1.6 2013/09/07 19:06:29 chs Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by J.T. Conklin.
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

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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

#include <machine/asm.h>

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	RCSID("from: @(#)bcmp.s 5.1 (Berkeley) 5/12/90")
#else
	RCSID("$NetBSD: bcmp.S,v 1.6 2013/09/07 19:06:29 chs Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

#ifdef __mcoldfire__
#define	CMPMB(a,b)	movb b,%d2; cmpb a,%d2
#define	CMPMW(a,b)	movw b,%d2; cmpw a,%d2
#define	CMPML(a,b)	movl b,%d2; cmpl a,%d2
#else
#define	CMPMB(a,b)	cmpmb a,b
#define	CMPMW(a,b)	cmpmw a,b
#define	CMPML(a,b)	cmpml a,b
#endif

ENTRY(bcmp)
	movl	4(%sp),%a0		| string 1
	movl	8(%sp),%a1		| string 2
	movl	12(%sp),%d1		| length
#ifdef __mcoldfire__
	movl	%d2,-(%sp)		| save temp
#endif

	/*
	 * It isn't worth the overhead of aligning to {long}word boundries
	 * if the string is too short.
	 */
	cmpl	#8,%d1
	jlt	.Lbcbyte			

#ifdef	__mc68010__
	/*
	 * The 68010 cannot access a word or long on an odd boundary,
	 * period.  If the source and the destination addresses aren't
	 * of the same evenness, we're forced to do a bytewise compare.
	 */
	movl	%a0,%d0
	addl	%a1,%d0
	btst	#0,%d0
	jne	.Lbcbyte
#endif	/* __mc68010__ */
	
	/* word align */
	movl	%a0,%d0
	btst	#0,%d0
	jeq	.Lbcalgndw
	CMPMB((%a0)+,(%a1)+)
	jne	.Lbcnoteq
	subql	#1,%d1
.Lbcalgndw:
	/* long word align */
	btst	#1,%d0
	jeq	.Lbcalgndl
	CMPMW((%a0)+,(%a1)+)
	jne	.Lbcnoteq
	subql	#2,%d1
.Lbcalgndl:
	/* compare by 8 longwords */
	movl	%d1,%d0
	lsrl	#5,%d0			| cnt = len / 32
	jeq	.Lbclong			| if (cnt)
	andl	#31,%d1			|	len %= 32
	subql	#1,%d0			|	set up for dbf
.Lbc32loop:
	CMPML((%a0)+,(%a1)+)		|	compare 1 of 8 longwords
	jne	.Lbcnoteq		|	not equal, return non-zero
	CMPML((%a0)+,(%a1)+)		|	compare 2 of 8 longwords
	jne	.Lbcnoteq
	CMPML((%a0)+,(%a1)+)		|	compare 3 of 8 longwords
	jne	.Lbcnoteq
	CMPML((%a0)+,(%a1)+)		|	compare 4 of 8 longwords
	jne	.Lbcnoteq
	CMPML((%a0)+,(%a1)+)		|	compare 5 of 8 longwords
	jne	.Lbcnoteq
	CMPML((%a0)+,(%a1)+)		|	compare 6 of 8 longwords
	jne	.Lbcnoteq
	CMPML((%a0)+,(%a1)+)		|	compare 7 of 8 longwords
	jne	.Lbcnoteq
	CMPML((%a0)+,(%a1)+)		|	compare 8 of 8 longwords
	jne	.Lbcnoteq
#ifndef __mcoldfire__
	dbf	%d0,.Lbc32loop		|	till done
	clrw	%d0
#endif
	subql	#1,%d0
	jcc	.Lbc32loop

.Lbclong:
	/* compare by longwords */
	movl	%d1,%d0
	lsrl	#2,%d0			| cnt = len / 4
	jeq	.Lbcbyte			| if (cnt)
	subql	#1,%d0			|	set up for dbf
.Lbclloop:
	CMPML((%a0)+,(%a1)+)		|	compare a longword
	jne	.Lbcnoteq		|	not equal, return non-zero
#ifdef __mcoldfire__
	subql	#1,%d0			|	decrement
	jcc	.Lbclloop		|	till done
#else
	dbf	%d0,.Lbclloop		|	till done
#endif
	andl	#3,%d1			|	len %= 4
	jeq	.Lbcdone 

	subql	#1,%d1			| set up for dbf
.Lbcbloop:
	CMPMB((%a0)+,(%a1)+)		| compare a byte
	jne	.Lbcnoteq		| not equal, return non-zero
.Lbcbyte:
#ifdef __mcoldfire__
	subql	#1,%d0			|	decrement
	jcc	.Lbcbloop		|	till done
#else
	dbf	%d1,.Lbcbloop
#endif
.Lbcdone:
#ifdef __mcoldfire__
	movl	(%sp)+,%d2		| restore temp
#endif
	movql	#0,%d0
	rts

.Lbcnoteq:
	movql	#1,%d0
	rts
END(bcmp)
