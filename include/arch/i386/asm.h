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
 *	@(#)asm.h	5.5 (Berkeley) 5/7/91
 */

#ifndef _I386_ASM_H_
#define _I386_ASM_H_

#ifdef __ELF__
# define _C_LABEL(x)    x
#else
# ifdef __STDC__
#  define _C_LABEL(x)   _ ## x
# else
#  define _C_LABEL(x)   _/**/x
# endif
#endif
#define _ASM_LABEL(x)   x

/* allow overriding entrypoint alignment */
#if !defined(_ALIGN_TEXT)
# define _ALIGN_TEXT .balign  16
#endif

#ifdef __ACK__
#define _ENTRY(x) \
	.text; _ALIGN_TEXT; .globl x; x:
#else
#define _ENTRY(x) \
	.text; _ALIGN_TEXT; .globl x; .type x,@function; x:
#endif
#define _LABEL(x) \
	.globl x; x:

#define	ENTRY(y)	_ENTRY(_C_LABEL(y))
#define	NENTRY(y)	_ENTRY(_C_LABEL(y))
#define	ASENTRY(y)	_ENTRY(_ASM_LABEL(y))
#define	LABEL(y)	_LABEL(_C_LABEL(y))
#define	END(y)		.size y, . - y

#define IMPORT(sym)               \
        .extern _C_LABEL(sym)

#endif /* !_I386_ASM_H_ */
