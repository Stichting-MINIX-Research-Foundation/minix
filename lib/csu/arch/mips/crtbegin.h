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

__asm(	"\n\t"
	".pushsection .init, \"ax\", @progbits"			"\n\t"
#ifdef __mips_o32
	".set noreorder"					"\n\t"
	".set nomacro"						"\n\t"
	"move	$28,$16" 					"\n\t"
	"lw	$25,%got(__do_global_ctors_aux)($28)"		"\n\t"
	"nop"							"\n\t"
	"addiu   $25,$25,%lo(__do_global_ctors_aux)"		"\n\t"
	".reloc	1f,R_MIPS_JALR,__do_global_ctors_aux"		"\n\t"
	"1:	jalr	$25"					"\n\t"
        "nop"							"\n\t"
	".set macro"						"\n\t"
	".set reorder"						"\n\t"
#else
	"jal	__do_global_ctors_aux"				"\n\t"
#endif
	".popsection");

__asm(	"\n\t"
	".pushsection .fini, \"ax\", @progbits" "\n\t"
#ifdef __mips_o32
	".set noreorder"					"\n\t"
	".set nomacro"						"\n\t"
	"move	$28,$16" 					"\n\t"
	"lw	$25,%got(__do_global_dtors_aux)($28)"		"\n\t"
	"nop"							"\n\t"
	"addiu   $25,$25,%lo(__do_global_dtors_aux)"		"\n\t"
	".reloc	1f,R_MIPS_JALR,__do_global_dtors_aux"		"\n\t"
	"1:	jalr	$25"					"\n\t"
        "nop"							"\n\t"
	".set macro"						"\n\t"
	".set reorder"						"\n\t"
#else
	"jal	__do_global_dtors_aux"				"\n\t"
#endif
	".popsection");
