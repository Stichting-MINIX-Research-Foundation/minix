/*	$NetBSD: __vfork14.S,v 1.9 2003/08/07 16:42:14 agc Exp $	*/

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

#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	RCSID("from: @(#)Ovfork.s	5.1 (Berkeley) 5/12/90")
#else
	RCSID("$NetBSD: __vfork14.S,v 1.9 2003/08/07 16:42:14 agc Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * @(#)vfork.s	4.1 (Berkeley) 12/21/80
 * C library -- vfork
 */

/*
 * pid = vfork();
 *
 * d1 == 0 in parent process, d1 == 1 in child process.
 * d0 == pid of child in parent, d0 == pid of parent in child.
 *
 * trickery here, due to keith sklower, uses ret to clear the stack,
 * and then returns with a jump indirect, since only one person can return
 * with a ret off this stack... we do the ret before we vfork!
 */

ENTRY(__vfork14)
	movl	%sp@+,%a1
	movl	#SYS___vfork14,%d0
	trap	#0
	jcs	err
	subql	#1,%d1	/* from 1 to 0 in child, 0 to -1 in parent */
	andl	%d1,%d0
	jmp	%a1@
err:
#ifdef	_REENTRANT
	.globl	_C_LABEL(__errno)
	movl	%a1,%sp@-
	movl	%d0,%sp@-
#if defined(PIC) && !defined(__ELF__)
	movl	#_C_LABEL(_GLOBAL_OFFSET_TABLE_),%a1
	lea	%pc@(0,a1:l),%a1
	movl	%a1@(_C_LABEL(__errno):l),%a1
	jsr	%a1@
#else
	jbsr	PIC_PLT(_C_LABEL(__errno))
#endif	/* PIC */
#ifdef	__SVR4_ABI__
	movl	%sp@+,%a0@
#else
	movl	%d0,%a1
	movl	%sp@+,%a1@
#endif
	movl	%sp@+,%a1
#else
	.globl	_C_LABEL(errno)
#ifdef	PIC
#ifdef	__ELF__
	lea	%pc@(_GLOBAL_OFFSET_TABLE_@GOTPC),%a0
	movl	%a0@(_C_LABEL(errno)@GOT:w),%a0
#else
	movl	#_C_LABEL(_GLOBAL_OFFSET_TABLE_),%a0
	lea	%pc@(0,a0:l),%a0
	movl	%a0@(_C_LABEL(errno):w),%a0
#endif
	movl	%d0,%a0@
#else
	movl	%d0,_C_LABEL(errno)
#endif	/* PIC */
#endif	/* _REENTRANT */
	moveq	#-1,%d0
	jmp	%a1@
