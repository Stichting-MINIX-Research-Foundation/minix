/*	$NetBSD: compat_sigprocmask.S,v 1.3 2006/01/06 04:08:47 uwe Exp $	*/

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
 *	from: @(#)sigprocmask.s	5.2 (Berkeley) 12/17/90
 */

#include <machine/asm.h>
#if defined(SYSLIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: compat_sigprocmask.S,v 1.3 2006/01/06 04:08:47 uwe Exp $")
#endif /* SYSLIBC_SCCS and not lint */

#include "SYS.h"

WARN_REFERENCES(sigprocmask, \
    "warning: reference to compatibility sigprocmask(); include <signal.h> for correct reference")

/*
 * int
 * sigprocmask(int how, const sigset_t * restrict set,
 *     sigset_t * restrict oset);
 */
ENTRY(sigprocmask)
	tst	r5, r5			/* set == NULL? */
	bf	1f

	bra	2f			/* r5 (new mask) is already zero */
	 mov	#1, r4			/* how = SIG_BLOCK */

1:	mov.l	@r5, r5			/* fetch new mask */

2:	mov	#SYS_compat_13_sigprocmask13, r0
	trapa	#0x80
	bf	err

	tst	r6, r6			/* oset == NULL? */
	bt	3f
	mov.l	r0, @r6			/* store old mask */
3:	rts
	 mov	#0, r0

err:
	JUMP_CERROR

	SET_ENTRY_SIZE(sigprocmask)
