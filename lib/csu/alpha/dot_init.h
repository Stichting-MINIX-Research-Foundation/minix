/* $NetBSD: dot_init.h,v 1.8 2008/05/10 15:31:03 martin Exp $ */

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
#include <machine/asm.h>

#define	ASM_SETUP "sp = $30; gp = $29; pv = $27; ra = $26 \n"

#define	MD_SECTION_PROLOGUE(sect, entry_pt)		\
		__asm (					\
		ASM_SETUP				\
		".section "#sect",\"ax\",@progbits	\n"\
		".global "#entry_pt"			\n"\
		#entry_pt":				\n"\
		"	ldgp	gp, 0(pv)		\n"\
		"	lda	sp, -32(sp)		\n"\
		"	stq	ra, 0(sp)		\n"\
		"	stq	gp, 8(sp)		\n"\
		"	.align	5			\n"\
		"	/* fall thru */			\n"\
		".previous")

#define	MD_SECTION_EPILOGUE(sect)			\
		__asm (					\
		ASM_SETUP				\
		".section "#sect",\"ax\",@progbits	\n"\
		"	ldq	gp, 8(sp)		\n"\
		"	ldq	ra, 0(sp)		\n"\
		"	lda	sp, 32(sp)		\n"\
		"	RET				\n"\
		".previous")

#define	MD_INIT_SECTION_PROLOGUE MD_SECTION_PROLOGUE(.init, _init)
#define	MD_FINI_SECTION_PROLOGUE MD_SECTION_PROLOGUE(.fini, _fini)

#define	MD_INIT_SECTION_EPILOGUE MD_SECTION_EPILOGUE(.init)
#define	MD_FINI_SECTION_EPILOGUE MD_SECTION_EPILOGUE(.fini)

/* We assume we need to reload our GP. */
#define MD_CALL_STATIC_FUNCTION(section, func) \
__asm(".section " #section "\n"		\
"    br $29, 1f		\n"		\
"1:  ldgp $29, 0($29)	\n"		\
"    unop		\n"		\
"    jsr $26, " #func "\n"		\
"    .align 3; .previous");
