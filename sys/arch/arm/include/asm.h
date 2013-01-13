/*	$NetBSD: asm.h,v 1.16 2012/09/01 14:46:25 matt Exp $	*/

/*
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
 *	from: @(#)asm.h	5.5 (Berkeley) 5/7/91
 */

#ifndef _ARM32_ASM_H_
#define _ARM32_ASM_H_

#include <arm/cdefs.h>

#define	__BIT(n)	(1 << (n))
#define __BITS(hi,lo)	((~((~0)<<((hi)+1)))&((~0)<<(lo)))

#define _C_LABEL(x)	x
#define	_ASM_LABEL(x)	x

#ifdef __STDC__
# define __CONCAT(x,y)	x ## y
# define __STRING(x)	#x
#else
# define __CONCAT(x,y)	x/**/y
# define __STRING(x)	"x"
#endif

#ifndef _ALIGN_TEXT
# define _ALIGN_TEXT .align 0
#endif

/*
 * gas/arm uses @ as a single comment character and thus cannot be used here
 * Instead it recognised the # instead of an @ symbols in .type directives
 * We define a couple of macros so that assembly code will not be dependent
 * on one or the other.
 */
#define _ASM_TYPE_FUNCTION	%function
#define _ASM_TYPE_OBJECT	%object
#ifdef __thumb__
#define _ENTRY(x) \
	.text; _ALIGN_TEXT; .globl x; .type x,_ASM_TYPE_FUNCTION; .thumb_func; x:
#else
#define _ENTRY(x) \
	.text; _ALIGN_TEXT; .globl x; .type x,_ASM_TYPE_FUNCTION; x:
#endif
#define	_END(x)		.size x,.-x

#ifdef GPROF
# define _PROF_PROLOGUE	\
	mov ip, lr; bl __mcount
#else
# define _PROF_PROLOGUE
#endif

#define	ENTRY(y)	_ENTRY(_C_LABEL(y)); _PROF_PROLOGUE
#define	ENTRY_NP(y)	_ENTRY(_C_LABEL(y))
#define	END(y)		_END(_C_LABEL(y))
#define	ASENTRY(y)	_ENTRY(_ASM_LABEL(y)); _PROF_PROLOGUE
#define	ASENTRY_NP(y)	_ENTRY(_ASM_LABEL(y))
#define	ASEND(y)	_END(_ASM_LABEL(y))

#if defined(__minix)
#define _LABEL(x) \
	.globl x; x:
#define	LABEL(y)	_LABEL(_C_LABEL(y))

#endif /* defined(__minix) */
#define	ASMSTR		.asciz

#if defined(PIC)
#ifdef __thumb__
#define	PLT_SYM(x)	x
#define	GOT_SYM(x)	PIC_SYM(x, GOTOFF)
#define	GOT_GET(x,got,sym)	\
	ldr	x, sym;		\
	add	x, got;		\
	ldr	x, [x]
#else
#define	PLT_SYM(x)	PIC_SYM(x, PLT)
#define	GOT_SYM(x)	PIC_SYM(x, GOT)
#define	GOT_GET(x,got,sym)	\
	ldr	x, sym;		\
	ldr	x, [x, got]
#endif /* __thumb__ */

#define	GOT_INIT(got,gotsym,pclabel) \
	ldr	got, gotsym;	\
	add	got, got, pc;	\
	pclabel:
#define	GOT_INITSYM(gotsym,pclabel) \
	gotsym: .word _C_LABEL(_GLOBAL_OFFSET_TABLE_) + (. - (pclabel+4))

#ifdef __STDC__
#define	PIC_SYM(x,y)	x ## ( ## y ## )
#else
#define	PIC_SYM(x,y)	x/**/(/**/y/**/)
#endif

#else
#define	PLT_SYM(x)	x
#define	GOT_SYM(x)	x
#define	GOT_GET(x,got,sym)	\
	ldr	x, sym;
#define	GOT_INIT(got,gotsym,pclabel)
#define	GOT_INITSYM(gotsym,pclabel)
#define	PIC_SYM(x,y)	x
#endif	/* PIC */

#define RCSID(x)	.pushsection ".ident"; .asciz x; .popsection

#define	WEAK_ALIAS(alias,sym)						\
	.weak alias;							\
	alias = sym

/*
 * STRONG_ALIAS: create a strong alias.
 */
#define STRONG_ALIAS(alias,sym)						\
	.globl alias;							\
	alias = sym

#ifdef __STDC__
#define	WARN_REFERENCES(sym,msg)					\
	.pushsection .gnu.warning. ## sym;				\
	.ascii msg;							\
	.popsection
#else
#define	WARN_REFERENCES(sym,msg)					\
	.pushsection .gnu.warning./**/sym;				\
	.ascii msg;							\
	.popsection
#endif /* __STDC__ */

#ifdef __thumb__
# define XPUSH		push
# define XPOP		pop
# define XPOPRET	pop	{pc}
#else
# define XPUSH		stmfd	sp!,
# define XPOP		ldmfd	sp!,
# ifdef _ARM_ARCH_5
#  define XPOPRET	ldmfd	sp!, {pc}
# else
#  define XPOPRET	ldmfd	sp!, {lr}; mov pc, lr
# endif
#endif
  
#if defined (_ARM_ARCH_4T)
# define RET		bx		lr
# define RETc(c)	__CONCAT(bx,c)	lr
#else
# define RET		mov		pc, lr
# define RETc(c)	__CONCAT(mov,c)	pc, lr
#endif

#ifdef __minix
#define IMPORT(sym)               \
        .extern _C_LABEL(sym)
#endif

#endif /* !_ARM_ASM_H_ */
