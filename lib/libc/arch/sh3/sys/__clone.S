/*	$NetBSD: __clone.S,v 1.7 2013/09/12 15:36:16 joerg Exp $	*/

/*-
 * Copyright (c) 2001 Tsubai Masanari.  All rights reserved.
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
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/errno.h>
#include "SYS.h"

#ifdef WEAK_ALIAS
WEAK_ALIAS(clone,__clone)
#endif

/*
 * int __clone(int (*fn)(void *), void *stack, int flags, void *arg);
 */
ENTRY(__clone)
	/*
	 * Sanity checks: func and stack may not be NULL.
	 */
	tst	r4, r4
	bt	inval
	tst	r5, r5
	bt	inval

	mov	r4, r2		/* save entry point for later */

	mov	r6, r4		/* r4 = flags, r5 = stack (already) */
	mov.l	.L_SYS___clone, r0
	trapa	#0x80
	bf	err

	tst	r0, r0
	bf	3f		/* we're the parent, just return */

	jsr	@r2		/* clone does _exit((*fn)(arg)); */
	 mov	r7, r4

	mov.l	.L__exit, r1
#ifdef __PIC__
	mov	r0, r4
	mov.l	.L_got, r12	/* PIC_PROLOGUE_NOSAVE(.L_got) */
	mova	.L_got, r0	/*  expanded to fill the delay slot */
1:	JUMP	r1
	 add	r0, r12
#else
	JUMP	r1
	 mov	r0, r4
#endif
	/* NOTREACHED */

inval:
	mov	#EINVAL, r0
err:
	JUMP_CERROR
	/* NOTREACHED */

3:	rts
	 nop

	.align	2
.L_SYS___clone:	.long	SYS___clone

.L_got:		PIC_GOT_DATUM
.L__exit:	CALL_DATUM(_C_LABEL(_exit), 1b)
	SET_ENTRY_SIZE(__clone)
