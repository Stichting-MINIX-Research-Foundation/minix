/*	$NetBSD: dot_init.h,v 1.7 2008/05/10 15:31:04 martin Exp $	*/

/*-
 * Copyright (c) 2001, 2006 Ross Harvey
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

#define MD_SECTION_PROLOGUE(sect, entry_pt)		    \
		__asm (					    \
		".globl  " #entry_pt "			\n" \
		".globl ." #entry_pt "			\n" \
		".pushsection \".opd\",\"aw\"		\n" \
		".align 3				\n" \
		#entry_pt": .quad ." #entry_pt ",.TOC.@tocbase,0\n"	\
		".size " #entry_pt ",24			\n" \
		".type " #entry_pt ",@function		\n" \
		".pushsection "#sect",\"ax\",@progbits	\n" \
		"." #entry_pt":				\n" \
		"	stdu	%r1,-48(%r1)		\n" \
		"	mflr	%r0			\n" \
		"	std	%r0,16(%r1)		\n" \
		"	/* fall thru */			\n" \
		".popsection				\n" \
		".popsection")

#define MD_SECTION_EPILOGUE(sect)			    \
		__asm (					    \
		".pushsection "#sect",\"ax\",@progbits	\n" \
		"	ld	%r0,16(%r1)		\n" \
		"	mtlr	%r0			\n" \
		"	la	%r1,48(%r1)		\n" \
		"	blr				\n" \
		".popsection")

#define MD_INIT_SECTION_PROLOGUE MD_SECTION_PROLOGUE(.init, _init)
#define MD_FINI_SECTION_PROLOGUE MD_SECTION_PROLOGUE(.fini, _fini)

#define MD_INIT_SECTION_EPILOGUE MD_SECTION_EPILOGUE(.init)
#define MD_FINI_SECTION_EPILOGUE MD_SECTION_EPILOGUE(.fini)
