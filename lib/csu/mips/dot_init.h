/* $NetBSD: dot_init.h,v 1.10 2009/12/14 01:04:02 matt Exp $ */

/*-
 * Copyright (c) 2001 Ross Harvey
 * Copyright (c) 2001 Simon Burge
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

#define	t9	"$25"

/*
 * For O32/O64, allocate 8 "slots" for the stack frame.  Store GP in the 4th
 * (zero-based) slot (since this is where compiler generated code for fallthru
 * processing expects it to be), and the RA in seventh (highest address).
 *
 * For N32/N64, allocate 4 8-byte "slots" for the stack frame.  Store GP in the
 * 2nd (zero-based) slot (since ...) and the RA in third (highest address).
 */

#ifdef __mips_o32
#define	sPTR_ADDU		"addu"
#define	sREG_L			"lw"
#define	sREG_S			"sw"
#define sRAOFF			"28"
#define sFRAMESZ		"32"
#define	MD_GPRESTORE		/* nothing */
#else
#define	sPTR_ADDU		"daddu"
#define	sREG_L			"ld"
#define	sREG_S			"sd"
#if defined(__mips_n32) || defined(__mips_n64)
#define	MD_GPRESTORE		"ld	$gp,8($sp)"		"\n\t"
#define sRAOFF			"24"
#define sFRAMESZ		"32"
#elif defined(__mips_o64)
#define sRAOFF			"56"
#define sFRAMESZ		"64"
#define	MD_GPRESTORE		/* nothing */
#endif
#endif

#ifdef __ABICALLS__
#if defined(__mips_o32) || defined(__mips_o64)
#define	MD_FUNCTION_PROLOGUE(entry_pt)					\
		".set	noreorder"				"\n\t"	\
		".cpload "t9					"\n\t"	\
		".set	reorder"				"\n\t"	\
		sPTR_ADDU"	$sp,$sp,-"sFRAMESZ		"\n\t"	\
		".cprestore 16"					"\n\t"	\
		sREG_S"	$ra,"sRAOFF"($sp)"			"\n\t"

#elif defined(__mips_n32) || defined(__mips_n64)
#define	MD_FUNCTION_PROLOGUE(entry_pt)					\
		".set	noreorder"				"\n\t"	\
		"daddu	$sp,$sp,-32"				"\n\t"	\
		".cpsetup "t9", 8, "#entry_pt			"\n\t"	\
		"sd	$ra,24($sp)"				"\n\t"	\
		".set	reorder"				"\n\t"
#else
#error ABI not supported (__ABICALLS)
#endif
#else
#if defined(__mips_o32) || defined(__mips_o64)
#define	MD_FUNCTION_PROLOGUE(entry_pt)					\
		sPTR_ADDU"	$sp,$sp,-"sFRAMESZ		"\n\t"	\
		sREG_S"	$ra,"sRAOFF"($sp)"			"\n\t"

#elif defined(__mips_n32) || defined(__mips_n64)
/*
 * On N32/N64, GP is callee-saved.
 */
#define	MD_FUNCTION_PROLOGUE(entry_pt)					\
		"daddu	$sp,$sp,-32"				"\n\t"	\
		"sd	$gp,8($sp)"				"\n\t"	\
		"sd	$ra,24($sp)"				"\n\t"
#else
#error ABI not supported (!__ABICALLS)
#endif
#endif /* __ABICALLS */


#define	MD_SECTION_PROLOGUE(sect, entry_pt)				\
		__asm (							\
		".section "#sect",\"ax\",@progbits"		"\n\t"	\
		".align 2"					"\n\t"	\
		".globl "#entry_pt				"\n\t"	\
		#entry_pt":"					"\n\t"	\
		MD_FUNCTION_PROLOGUE(entry_pt)				\
		"	/* fall thru */"			"\n\t"	\
		".previous")

#define	MD_SECTION_EPILOGUE(sect)					\
		__asm (							\
		".section "#sect",\"ax\",@progbits"		"\n\t"	\
		sREG_L"	$ra,"sRAOFF"($sp)"			"\n\t"	\
		MD_GPRESTORE						\
		".set	noreorder"				"\n\t"	\
		"j	$ra"					"\n\t"	\
		sPTR_ADDU"	$sp,$sp,"sFRAMESZ		"\n\t"	\
		".set	reorder"				"\n\t"	\
		".previous")

#define	MD_INIT_SECTION_PROLOGUE MD_SECTION_PROLOGUE(.init, _init)
#define	MD_FINI_SECTION_PROLOGUE MD_SECTION_PROLOGUE(.fini, _fini)

#define	MD_INIT_SECTION_EPILOGUE MD_SECTION_EPILOGUE(.init)
#define	MD_FINI_SECTION_EPILOGUE MD_SECTION_EPILOGUE(.fini)
