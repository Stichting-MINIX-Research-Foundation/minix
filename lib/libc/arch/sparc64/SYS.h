/*	$NetBSD: SYS.h,v 1.14 2011/03/28 11:19:13 martin Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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
 *	@(#)SYS.h	8.1 (Berkeley) 6/4/93
 *
 *	from: Header: SYS.h,v 1.2 92/07/03 18:57:00 torek Exp
 */

#include <machine/asm.h>
#include <sys/syscall.h>
#include <machine/trap.h>

#ifdef __STDC__
#define _CAT(x,y) x##y
#else
#define _CAT(x,y) x/**/y
#endif

/*
 * ERROR branches to cerror.  This is done with a macro so that I can
 * change it to be position independent later, if need be.
 */
#ifdef PIC
#ifdef BIGPIC
#define	JUMP(name) \
	PIC_PROLOGUE(%g1,%g5); \
	sethi %hi(_C_LABEL(name)),%g5; \
	or %g5,%lo(_C_LABEL(name)),%g5; \
	ldx [%g1+%g5],%g5; \
	jmp %g5; \
	nop
#else
#define	JUMP(name) \
	PIC_PROLOGUE(%g1,%g5); \
	ldx [%g1+_C_LABEL(name)],%g5; jmp %g5; nop
#endif
#else
#define	JUMP(name)	set _C_LABEL(name),%g1; jmp %g1; nop
#endif
#define	ERROR()		JUMP(__cerror)
/*
 * SYSCALL is used when further action must be taken before returning.
 * Note that it adds a `nop' over what we could do, if we only knew what
 * came at label 1....
 */
#define	_SYSCALL(x,y) \
	ENTRY(x); mov _CAT(SYS_,y),%g1; t ST_SYSCALL; bcc 1f; nop; ERROR(); 1:

#define	SYSCALL(x) \
	_SYSCALL(x,x)

/*
 * RSYSCALL is used when the system call should just return.  Here
 * we use the SYSCALL_G5RFLAG to put the `success' return address in %g5
 * and avoid a branch.
 */
#define	RSYSCALL(x) \
	ENTRY(x); mov (_CAT(SYS_,x))|SYSCALL_G5RFLAG,%g1; add %o7,8,%g5; \
	t ST_SYSCALL; ERROR()

/*
 * PSEUDO(x,y) is like RSYSCALL(y) except that the name is x.
 */
#define	PSEUDO(x,y) \
	ENTRY(x); mov (_CAT(SYS_,y))|SYSCALL_G5RFLAG,%g1; add %o7,8,%g5; \
	t ST_SYSCALL; ERROR()

/*
 * WSYSCALL(weak,strong) is like RSYSCALL(weak), except that weak is
 * a weak internal alias for the strong symbol.
 */
#define	WSYSCALL(weak,strong) \
	WEAK_ALIAS(weak,strong); \
	PSEUDO(strong,weak)

/*
 * SYSCALL_NOERROR is like SYSCALL, except it's used for syscalls 
 * that never fail.
 *
 * XXX - This should be optimized.
 */
#define SYSCALL_NOERROR(x) \
	ENTRY(x); mov _CAT(SYS_,x),%g1; t ST_SYSCALL

/*
 * RSYSCALL_NOERROR is like RSYSCALL, except it's used for syscalls 
 * that never fail.
 *
 * XXX - This should be optimized.
 */
#define RSYSCALL_NOERROR(x) \
	ENTRY(x); mov (_CAT(SYS_,x))|SYSCALL_G5RFLAG,%g1; add %o7,8,%g5; \
	t ST_SYSCALL

/*
 * PSEUDO_NOERROR(x,y) is like RSYSCALL_NOERROR(y) except that the name is x.
 */
#define PSEUDO_NOERROR(x,y) \
	ENTRY(x); mov (_CAT(SYS_,y))|SYSCALL_G5RFLAG,%g1; add %o7,8,%g5; \
	t ST_SYSCALL

	.globl	_C_LABEL(__cerror)
