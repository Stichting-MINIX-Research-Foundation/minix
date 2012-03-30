/*	$NetBSD: dot_init.h,v 1.7 2008/05/10 15:31:04 martin Exp $	*/

/*-
 * Copyright (c) 2001 Ross Harvey
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */


/*
 * Don't use C versions of _init/_fini, they have become just a
 * rudimentary indirection to the entry points we would supply with
 * MD_SECTION_PROLOGUE.
 */
#define MD_INIT_SECTION_PROLOGUE MD_SECTION_PROLOGUE(.init, _init)
#define MD_FINI_SECTION_PROLOGUE MD_SECTION_PROLOGUE(.fini, _fini)

#define MD_INIT_SECTION_EPILOGUE MD_SECTION_EPILOGUE(.init)
#define MD_FINI_SECTION_EPILOGUE MD_SECTION_EPILOGUE(.fini)


#define MD_ASM_IN_SECTION(section, content)				\
	__asm(								\
		".section " #section ",\"ax\",@progbits\n"		\
			content "\n"					\
		".previous")

/*
 * Declare the entry point global.
 */
#define MD_SECTION_PROLOGUE(section, entry_pt)				\
	MD_ASM_IN_SECTION(section,					\
		"	.global "_C_LABEL_STRING(#entry_pt)"	\n"	\
		"	.type "_C_LABEL_STRING(#entry_pt)",@function\n" \
		_C_LABEL_STRING(#entry_pt)":			\n"	\
		"	mov.l	r14, @-sp			\n"	\
		"	sts.l	pr, @-sp			\n"	\
		"	mov	sp, r14				\n"	\
		"	.p2align 2")

/*
 * NOTE 1: Supply the semicolon here because crtbegin.c doesn't.
 * NOTE 2: We don't use our crtbegin.c for gcc3 and later,
 *         we use gcc crtstuff.c via src/gnu/lib/crtstuff*
 */
#define MD_CALL_STATIC_FUNCTION(section, func)				\
	MD_ASM_IN_SECTION(section,					\
		"	mov.l	1f, r1				\n"	\
		"	mova	2f, r0				\n"	\
		"0:	braf	r1				\n"	\
		"	 lds	r0, pr				\n"	\
									\
		"	.p2align 2				\n"	\
		"1:	.long "_C_LABEL_STRING(#func)" - (0b+4)	\n"	\
		"2:	");

#define MD_SECTION_EPILOGUE(section)					\
	MD_ASM_IN_SECTION(section,					\
		"	mov	r14, sp				\n"	\
		"	lds.l	@sp+, pr			\n"	\
		"	rts					\n"	\
		"	 mov.l	@sp+, r14")
