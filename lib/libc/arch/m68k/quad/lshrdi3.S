/*	$NetBSD: lshrdi3.S,v 1.9 2014/03/18 18:20:37 riastradh Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
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

#include <machine/asm.h>

| d0 msw
| d1 lsw
| d2 shift
| d3 offset (32 - shift)

ENTRY(__lshrdi3)
	link	%fp,#-12
	moveml	%d2-%d4,(%sp)
	movel	8(%fp),%d0
	movel	12(%fp),%d1
	movel	16(%fp),%d2
	moveq	#32,%d3
	subl	%d2,%d3
	jgt	L2
	negl	%d3
	movel	%d0,%d1
	lsrl	%d3,%d1
	clrl	%d0
	jra	L3
L2:	lsrl	%d2,%d1
	movel	%d0,%d4
	asll	%d3,%d4
	orl	%d4,%d1
	lsrl	%d2,%d0
L3:	moveml	-12(%fp),%d2-%d4
	unlk	%fp
	rts
END(__lshrdi3)
