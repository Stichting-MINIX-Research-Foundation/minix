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
 *	from: @(#)SYS.h	5.5 (Berkeley) 5/7/91
 *	$NetBSD: SYS.h,v 1.10 2011/01/23 14:08:53 joerg Exp $
 */

#include <machine/asm.h>
#include <sys/syscall.h>

#ifdef __STDC__
#define	IMMEDIATE	#
#define SYSTRAP(x)					\
		mov.l	903f, r0;			\
		trapa	IMMEDIATE 0x80;			\
		bra	904f;				\
		 nop;					\
		.align	2;				\
	903:	.long	(SYS_ ## x);			\
	904:
#else
#define SYSTRAP(x)					\
		mov.l	903f, r0;			\
		trapa	#0x80;				\
		bra	904f;				\
		 nop;					\
		.align	2;				\
	903:	.long	(SYS_/**/x);			\
	904:
#endif

#define _SYSCALL_NOERROR(x,y)				\
		ENTRY(x);				\
		SYSTRAP(y)

#ifdef PIC

#define JUMP_CERROR					\
		mov	r0, r4;				\
		mov.l	912f, r1;			\
		mova	912f, r0;			\
		mov.l	913f, r2;			\
		add	r1, r0;				\
		mov.l	@(r0, r2), r3;			\
		jmp	@r3;				\
		 nop;					\
		.align	2;				\
	912:	.long	_GLOBAL_OFFSET_TABLE_;		\
	913:	.long	PIC_GOT(cerror)

#else  /* !PIC */

#define JUMP_CERROR					\
		mov.l	912f, r3;			\
		jmp	@r3;				\
		 mov	r0, r4;				\
		.align	2;				\
	912:	.long	cerror

#endif /* !PIC */

#define _SYSCALL(x,y)					\
		.text;					\
	911:	JUMP_CERROR;				\
		_SYSCALL_NOERROR(x,y);			\
		bf	911b

#define SYSCALL_NOERROR(x)				\
		_SYSCALL_NOERROR(x,x)

#define SYSCALL(x)					\
		_SYSCALL(x,x)

#define PSEUDO_NOERROR(x,y)				\
		_SYSCALL_NOERROR(x,y);			\
		rts;					\
		 nop

#define PSEUDO(x,y)					\
		_SYSCALL(x,y);				\
		rts;					\
		 nop

#define RSYSCALL_NOERROR(x)				\
		PSEUDO_NOERROR(x,x)

#define RSYSCALL(x)					\
		PSEUDO(x,x)

#ifdef WEAK_ALIAS
#define	WSYSCALL(weak,strong)				\
		WEAK_ALIAS(weak,strong);		\
		PSEUDO(strong,weak)
#else
#define	WSYSCALL(weak,strong)				\
		PSEUDO(weak,weak)
#endif

	.globl	cerror
