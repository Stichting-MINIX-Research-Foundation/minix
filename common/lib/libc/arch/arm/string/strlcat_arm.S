/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>

RCSID("$NetBSD: strlcat_arm.S,v 1.6 2014/05/06 16:02:11 joerg Exp $")

/* LINTSTUB: size_t strlcat(char *, const char *, size_t) */
ENTRY(strlcat)
#if defined(__ARM_EABI__) && defined(__UNWIND_TABLES__)
# if !defined(__ARM_DWARF_EH__)
	.fnstart
# endif
	.cfi_startproc
#endif
	push	{r4-r6, lr}
#if defined(__ARM_EABI__) && defined(__UNWIND_TABLES__)
# if !defined(__ARM_DWARF_EH__)
	.save	{r4-r6, lr}
# endif
	.cfi_def_cfa_offset 16
	.cfi_offset 14, -4
	.cfi_offset 6, -8
	.cfi_offset 5, -12
	.cfi_offset 4, -16
#endif
	mov	r4, r0			/* save dst */
	mov	r5, r1			/* save src */
	mov	r6, r2			/* save siz */
        
	mov	r1, r6			/* get siz */
	bl	PLT_SYM(strnlen)	/* find length of dst (up to siz) */

	subs	r2, r6, r0		/* get remaining space in dst */
	mov	r1, r5			/* restore src */
	mov	r5, r0			/* save remaining space */
	adds	r0, r0, r4		/* set start to end of dst */
	bl	PLT_SYM(strlcpy)	/* attempt to copy src */
	adds	r0, r0, r5		/* add remaining to strlcpy return */
	pop	{r4-r6, pc}		/* restore registers and return */
#if defined(__ARM_EABI__) && defined(__UNWIND_TABLES__)
	.cfi_endproc
# if !defined(__ARM_DWARF_EH__)
	.fnend
# endif
#endif
END(strlcat)
