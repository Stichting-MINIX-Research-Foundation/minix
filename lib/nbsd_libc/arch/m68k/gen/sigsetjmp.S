/*	$NetBSD: sigsetjmp.S,v 1.8 2006/03/09 16:20:27 christos Exp $	*/

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
	RCSID("from: @(#)_setjmp.s	5.1 (Berkeley) 5/12/90")
#else
	RCSID("$NetBSD: sigsetjmp.S,v 1.8 2006/03/09 16:20:27 christos Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * C library -- sigsetjmp, siglongjmp
 *
 *	siglongjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	sigsetjmp(a,m)
 * by restoring registers from the stack,
 * The previous signal state is restored if 'm' was non-zero.
 */

/* grab _JBLEN */
#include <m68k/setjmp.h>

ENTRY(__sigsetjmp14)
	movl	%sp@(8),%d1	/* grab the mask */
        movl	%sp@(4),%a0	/* grab the area pointer */
	movl	%d1,%a0@(_JBLEN * 4)	/* save at end of area */
	tstl	%d1
	bne	dosig
	jra	PIC_PLT(_C_LABEL(_setjmp))
dosig:
	jra	PIC_PLT(_C_LABEL(__setjmp14))


ENTRY(__siglongjmp14)
	movl	%sp@(4),%a0	/* save area pointer */
	tstl	%a0@(_JBLEN * 4) /* check mask... */
	bne	didsig
	jra	PIC_PLT(_C_LABEL(_longjmp))
didsig:
	jra	PIC_PLT(_C_LABEL(__longjmp14))
