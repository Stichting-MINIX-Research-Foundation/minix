/*	$NetBSD: ptrace.S,v 1.9 2013/09/12 15:36:16 joerg Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *
 *	from: @(#)ptrace.s	5.1 (Berkeley) 4/23/90
 */

#include <machine/asm.h>
#if defined(SYSLIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: ptrace.S,v 1.9 2013/09/12 15:36:16 joerg Exp $")
#endif /* SYSLIBC_SCCS and not lint */

#include "SYS.h"

/*
 * int
 * ptrace(int request, pid_t pid, caddr_t addr, int data);
 */
ENTRY(ptrace)
#ifdef _REENTRANT

	mov.l	.L___errno, r1
	PIC_PROLOGUE(.L_GOT)
	sts.l   pr, @-sp
	mov.l	r7, @-sp
	mov.l	r6, @-sp
	mov.l	r5, @-sp
1:	CALL	r1
	 mov.l	r4, @-sp
	mov.l	@sp+, r4
	mov.l	@sp+, r5
	mov.l	@sp+, r6
	mov.l	@sp+, r7
	lds.l   @sp+, pr
	PIC_EPILOGUE

#else  /* ! _REENTRANT */

#ifdef __PIC__
	mova	.L_GOT, r0
	mov.l	.L_GOT, r1
	add	r0, r1
	mov.l	.L_errno, r0
	mov.l	@(r0, r1), r0
#else
	mov.l	.L_errno, r0
#endif

#endif /* ! _REENTRANT */

	mov	#0, r1
	mov.l	r1, @r0			/* errno = 0; */

	mov	#SYS_ptrace, r0
	trapa	#0x80
	bf	err
	rts
	 nop
err:
	JUMP_CERROR

	.align	2
.L_GOT:
	PIC_GOT_DATUM

#ifdef _REENTRANT
	.global	_C_LABEL(__errno)
.L___errno:
	CALL_DATUM(_C_LABEL(__errno), 1b)

#else  /* ! _REENTRANT */
	.global	_C_LABEL(errno)
.L_errno:
#ifdef __PIC__
	.long	PIC_GOT(_C_LABEL(errno))
#else
	.long	_C_LABEL(errno)
#endif

#endif /* ! _REENTRANT */

	SET_ENTRY_SIZE(ptrace)
