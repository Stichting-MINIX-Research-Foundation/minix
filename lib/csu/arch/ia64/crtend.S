/*	$NetBSD: crtend.S,v 1.1 2014/03/14 18:15:02 cherry Exp $	*/
/*-
 * Copyright (c) 2010 Joerg Sonnenberger <joerg@NetBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asm.h>

RCSID("$NetBSD: crtend.S,v 1.1 2014/03/14 18:15:02 cherry Exp $")

	.section	.ctors, "aw", @progbits
	.align 8
	.global		__CTOR_LIST_END__
	.hidden 	__CTOR_LIST_END__
__CTOR_LIST_END__:
	.quad 0

	.section	.dtors, "aw", @progbits
	.align 8
	.global		__DTOR_LIST_END__
	.hidden 	__DTOR_LIST_END__
__DTOR_LIST_END__:
	.quad 0

	.section	.eh_frame, "a", @progbits
	.align 8
	.quad 0

	.section	.jcr, "aw", @progbits
	.align 8
	.quad 0
