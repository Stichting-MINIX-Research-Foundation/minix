/*	$NetBSD: compat_sigreturn.S,v 1.1 2006/03/09 16:20:55 christos Exp $	*/

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
	RCSID("from: @(#)sigreturn.s	5.1 (Berkeley) 5/12/90")
#else
	RCSID("$NetBSD: compat_sigreturn.S,v 1.1 2006/03/09 16:20:55 christos Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * We must preserve the state of the registers as the user has set them up.
 */
#ifdef GPROF
#undef ENTRY
#ifdef __STDC__
#define	ENTRY(x) \
	.globl _C_LABEL(x); .even; _C_LABEL(x):; moveml #0xC0C0,%sp@-; .data; \
	PROF ## x:; .long 0; .text; lea PROF ## x,%a0; jbsr MCOUNT_ENTRY; \
	moveml %sp@+,#0x0303
#else
#define	ENTRY(x) \
	.globl _C_LABEL(x); .even; _C_LABEL(x):; moveml #0xC0C0,%sp@-; .data; \
	PROF/**/x:; .long 0; .text; lea PROF/**/x,%a0; jbsr MCOUNT_ENTRY; \
	moveml %sp@+,#0x0303
#endif /* __STDC__ */
#endif /* GPROF */

WARN_REFERENCES(sigreturn, \
    "warning: reference to compatibility sigreturn()")

/*
 * NOTE: Trap #1 is used for compat_13_sigreturn13 on the m68k!
 */
ENTRY(sigreturn)
	trap	#1		/* signals compat_13_sigreturn13() */
	jra	PIC_PLT(CERROR)
