/*	$NetBSD: _setjmp.S,v 1.22 2010/09/03 17:22:51 matt Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 */

#include <machine/cdefs.h>
#include <mips/regnum.h>
#include <mips/asm.h>
#include <machine/setjmp.h>
#include <machine/signal.h>		/* XXX */

#include "assym.h"

#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	RCSID("from: @(#)_setjmp.s	8.1 (Berkeley) 6/4/93")
#else
	RCSID("$NetBSD: _setjmp.S,v 1.22 2010/09/03 17:22:51 matt Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * C library -- _setjmp, _longjmp
 *
 *	_longjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	_setjmp(a)
 * by restoring registers from the stack,
 * The previous signal state is NOT restored.
 */

	.set	noreorder

LEAF(_setjmp)
	REG_PROLOGUE
	REG_LI	v0, 0xACEDBADE			# sigcontext magic number
	REG_S	ra, _OFFSETOF_SC_PC(a0)		# sc_pc = return address
	REG_S	v0, _OFFSETOF_SC_REGS(a0)	#   saved in sc_regs[0]
	REG_S	s0, _OFFSETOF_SC_REGS_S0(a0)
	REG_S	s1, _OFFSETOF_SC_REGS_S1(a0)
	REG_S	s2, _OFFSETOF_SC_REGS_S2(a0)
	REG_S	s3, _OFFSETOF_SC_REGS_S3(a0)
	REG_S	s4, _OFFSETOF_SC_REGS_S4(a0)
	REG_S	s5, _OFFSETOF_SC_REGS_S5(a0)
	REG_S	s6, _OFFSETOF_SC_REGS_S6(a0)
	REG_S	s7, _OFFSETOF_SC_REGS_S7(a0)
	REG_S	sp, _OFFSETOF_SC_REGS_SP(a0)
	REG_S	s8, _OFFSETOF_SC_REGS_S8(a0)
#if defined(__mips_n32) || defined(__mips_n64)
	REG_S	gp, _OFFSETOF_SC_REGS_GP(a0)	# newabi gp is callee-saved
#endif
	/*
	 * In N32, FP registers F20, F22, F24, F26, F28, F30 are callee-saved.
	 * In N64, FP registers F23 .. F31 are callee-saved.
	 * In O32, FP registers F20 .. F23 are callee-saved.
	 */
#ifndef SOFTFLOAT_FOR_GCC
	cfc1	v0, $31				# too bad cant check if FP used
	INT_S	v0, _OFFSETOF_SC_FPREGS_FCSR(a0)
#if defined(__mips_n64) || defined(__mips_n32)
	FP_S	$f30, _OFFSETOF_SC_FPREGS_F30(a0)
	FP_S	$f28, _OFFSETOF_SC_FPREGS_F28(a0)
	FP_S	$f26, _OFFSETOF_SC_FPREGS_F26(a0)
	FP_S	$f24, _OFFSETOF_SC_FPREGS_F24(a0)
#endif
#if defined(__mips_n32) || defined(__mips_o32) || defined(__mips_o64)
	FP_S	$f22, _OFFSETOF_SC_FPREGS_F22(a0)
	FP_S	$f20, _OFFSETOF_SC_FPREGS_F20(a0)
#endif
#if defined(__mips_o32) || defined(__mips_o64)
	FP_S	$f21, _OFFSETOF_SC_FPREGS_F21(a0)
	FP_S	$f23, _OFFSETOF_SC_FPREGS_F23(a0)
#endif
#if defined(__mips_n64)
	FP_S	$f25, _OFFSETOF_SC_FPREGS_F25(a0)
	FP_S	$f27, _OFFSETOF_SC_FPREGS_F27(a0)
	FP_S	$f29, _OFFSETOF_SC_FPREGS_F29(a0)
	FP_S	$f31, _OFFSETOF_SC_FPREGS_F31(a0)
#endif
#endif	/* SOFTFLOAT_FOR_GCC */
	REG_EPILOGUE

	j	ra
	move	v0, zero
END(_setjmp)

LEAF(_longjmp)
	PIC_PROLOGUE(_longjmp)
	PTR_SUBU	sp, sp, CALLFRAME_SIZ
	SAVE_GP(CALLFRAME_GP)

	REG_PROLOGUE
	REG_L		v0, _OFFSETOF_SC_REGS(a0)	# get magic number
	REG_L		ra, _OFFSETOF_SC_PC(a0)
	REG_LI		t0, 0xACEDBADE
	bne		v0, t0, botch		# jump if error
	PTR_ADDU	sp, sp, CALLFRAME_SIZ	# does not matter, sanity
	REG_S		a1, _OFFSETOF_SC_REGS_V0(a0)	# save return value
	REG_L		s0, _OFFSETOF_SC_REGS_S0(a0)
	REG_L		s1, _OFFSETOF_SC_REGS_S1(a0)
	REG_L		s2, _OFFSETOF_SC_REGS_S2(a0)
	REG_L		s3, _OFFSETOF_SC_REGS_S3(a0)
	REG_L		s4, _OFFSETOF_SC_REGS_S4(a0)
	REG_L		s5, _OFFSETOF_SC_REGS_S5(a0)
	REG_L		s6, _OFFSETOF_SC_REGS_S6(a0)
	REG_L		s7, _OFFSETOF_SC_REGS_S7(a0)
#if defined(__mips_n32) || defined(__mips_n64)
	REG_L		gp, _OFFSETOF_SC_REGS_GP(a0)
#endif
	# get fpu status
	REG_L		sp, _OFFSETOF_SC_REGS_SP(a0)
	REG_L		s8, _OFFSETOF_SC_REGS_S8(a0)
#ifndef SOFTFLOAT_FOR_GCC
	INT_L		v0, _OFFSETOF_SC_FPREGS_FCSR(a0)
	ctc1		v0, $31
	/*
	 * In N32, FP registers F20, F22, F24, F26, F28, F30 are callee-saved.
	 * In N64, FP registers F23 .. F31 are callee-saved.
	 * In O32, FP registers F20 .. F23 are callee-saved.
	 */
#if defined(__mips_n64) || defined(__mips_n32)
	FP_L	$f30, _OFFSETOF_SC_FPREGS_F30(a0)
	FP_L	$f28, _OFFSETOF_SC_FPREGS_F28(a0)
	FP_L	$f26, _OFFSETOF_SC_FPREGS_F26(a0)
	FP_L	$f24, _OFFSETOF_SC_FPREGS_F24(a0)
#endif
#if defined(__mips_n32) || defined(__mips_o32) || defined(__mips_o64)
	FP_L	$f22, _OFFSETOF_SC_FPREGS_F22(a0)
	FP_L	$f20, _OFFSETOF_SC_FPREGS_F20(a0)
#endif
#if defined(__mips_o32) || defined(__mips_o64)
	FP_L	$f21, _OFFSETOF_SC_FPREGS_F21(a0)
	FP_L	$f23, _OFFSETOF_SC_FPREGS_F23(a0)
#endif
#if defined(__mips_n64)
	FP_L	$f25, _OFFSETOF_SC_FPREGS_F25(a0)
	FP_L	$f27, _OFFSETOF_SC_FPREGS_F27(a0)
	FP_L	$f29, _OFFSETOF_SC_FPREGS_F29(a0)
	FP_L	$f31, _OFFSETOF_SC_FPREGS_F31(a0)
#endif
#endif	/* SOFTFLOAT_FOR_GCC */

	REG_EPILOGUE
	move	v0, a1			# get return value in 1st arg
	j	ra
	nop

botch:
	/*
	 * We know we aren't returning so we don't care about restoring
	 * our caller's GP.
	 */
	jal	_C_LABEL(longjmperror)
	
	PIC_TAILCALL(abort)
END(_longjmp)
