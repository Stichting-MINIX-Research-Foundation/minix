/*	$NetBSD: SYS.h,v 1.15 2013/08/19 22:13:34 matt Exp $	*/

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
 */

#define _TEXT_SECTION	.section .text.hot, "ax"

#include <machine/asm.h>
#include <sys/syscall.h>
#include <arm/swi.h>

#ifndef __STDC__
#error __STDC__ not defined
#endif

#if !defined(__thumb__)
#define SYSTRAP(x)	svc #SWI_OS_NETBSD | SYS_ ## x
#else
.macro	emitsvc	x
	mov	ip, r0
.ifeq	\x / 256
	movs	r0, #\x
.else
#if defined(_ARM_ARCH_7)
	movw	r0, #\x
#else
.ifeq (\x & 3)
	movs	r0, #(\x / 4)
	lsls	r0, r0, #3
.else
.ifeq (\x & 1)
	movs	r0, #(\x / 2)
	lsls	r0, r0, #1
.else
	movs	r0, #(\x / 256)
	lsls	r0, r0, #8
	adds	r0, r0, #(\x & 255)
.endif
.endif
#endif /* !_ARM_ARCH_7 */
.endif
	svc	#255
.endm
#define SYSTRAP(x)	emitsvc SYS_ ## x
#endif /* __thumb__ */

#define	CERROR		_C_LABEL(__cerror)
#define	CURBRK		_C_LABEL(__curbrk)

#define _SYSCALL_NOERROR(x,y)						\
	ENTRY(x);							\
	SYSTRAP(y)

#if  !defined(__thumb__) || defined(_ARM_ARCH_T2)
#define	_INVOKE_CERROR()	bcs CERROR
#else
#define	_INVOKE_CERROR()	\
	bcc 86f; push {r3,lr}; bl CERROR; pop {r3,pc}; 86:
#endif
#define _SYSCALL(x, y)							\
	_SYSCALL_NOERROR(x,y);						\
	_INVOKE_CERROR()

#define SYSCALL_NOERROR(x)						\
	_SYSCALL_NOERROR(x,x)

#define SYSCALL(x)							\
	_SYSCALL(x,x)


#define PSEUDO_NOERROR(x,y)						\
	_SYSCALL_NOERROR(x,y);						\
	RET;								\
	END(x)

#define PSEUDO(x,y)							\
	_SYSCALL(x,y);							\
	RET;								\
	END(x)


#define RSYSCALL_NOERROR(x)						\
	PSEUDO_NOERROR(x,x)

#define RSYSCALL(x)							\
	PSEUDO(x,x)

#ifdef WEAK_ALIAS
#define	WSYSCALL(weak,strong)						\
	WEAK_ALIAS(weak,strong);					\
	PSEUDO(strong,weak)
#else
#define	WSYSCALL(weak,strong)						\
	PSEUDO(weak,weak)
#endif

	.hidden	CERROR
	.globl	CERROR
