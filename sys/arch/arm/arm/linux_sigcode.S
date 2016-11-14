/*	$NetBSD: linux_sigcode.S,v 1.2 2002/01/17 22:50:38 bjh21 Exp $	*/

/*
 * Copyright (C) 1994-1997 Mark Brinicombe
 * Copyright (C) 1994 Brini
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Brini.
 * 4. The name of Brini may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>

RCSID("$NetBSD: linux_sigcode.S,v 1.2 2002/01/17 22:50:38 bjh21 Exp $")

#include <compat/linux/linux_syscall.h>

/*
 * Signal trampoline; copied to top of user stack.
 */

ENTRY_NP(linux_sigcode)
/*
 * r0-r2 are our signal handler parameters
 * r3 is the handler address
 */

	add	lr, pc, #0			/* Set return address */
	mov	pc, r3				/* Call the handler */

/*
 * Call sigreturn.
 * Linux sigreturn finds the context from our stack pointer.
 */
	swi	LINUX_SYS_sigreturn

/* Well if that failed we better exit quick ! */

	swi	LINUX_SYS_exit
	b	. - 8

	.align	0
        .global _C_LABEL(linux_esigcode)
_C_LABEL(linux_esigcode):
