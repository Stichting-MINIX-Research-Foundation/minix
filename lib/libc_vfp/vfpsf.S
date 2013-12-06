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

#include <arm/asm.h>
#include <arm/vfpreg.h>

RCSID("$NetBSD: vfpsf.S,v 1.2 2013/06/23 06:19:55 matt Exp $")

/*
 * This file provides softfloat compatible routines which use VFP instructions
 * to do the actual work.  This should give near hard-float performance while
 * being compatible with soft-float code.
 *
 * This file implements the single precision floating point routines.
 */

#ifdef __ARM_EABI__
#define	__addsf3	__aeabi_fadd
#define	__divsf3	__aeabi_fdiv
#define	__mulsf3	__aeabi_fmul
#define	__subsf3	__aeabi_fsub
#define	__negsf2	__aeabi_fneg
#define	__truncdfsf2	__aeabi_d2f
#define	__fixsfsi	__aeabi_f2iz
#define	__fixunssfsi	__aeabi_f2uiz
#define	__floatsisf	__aeabi_i2f
#define	__floatunsisf	__aeabi_ui2f
#endif

ENTRY(__addsf3)
	vmov		s0, s1, r0, r1
	vadd.f32	s0, s0, s1
	vmov		r0, s0
	RET
END(__addsf3)

ENTRY(__subsf3)
	vmov		s0, s1, r0, r1
	vsub.f32	s0, s0, s1
	vmov		r0, s0
	RET
END(__subsf3)

#ifdef __ARM_EABI__
ENTRY(__aeabi_frsub)
	vmov		s0, s1, r0, r1
	vsub.f32	s0, s1, s0
	vmov		r0, s0
	RET
END(__aeabi_frsub)
#endif

ENTRY(__mulsf3)
	vmov		s0, s1, r0, r1
	vmul.f32	s0, s0, s1
	vmov		r0, s0
	RET
END(__mulsf3)

ENTRY(__divsf3)
	vmov		s0, s1, r0, r1
	vdiv.f32	s0, s0, s1
	vmov		r0, s0
	RET
END(__divsf3)

ENTRY(__negsf2)
	vmov		s0, r0
	vneg.f32	s0, s0
	vmov		r0, s0
	RET
END(__negsf2)

ENTRY(__truncdfsf2)
#ifdef __ARMEL__
	vmov		d0, r0, r1
#else
	vmov		d0, r1, r0
#endif
	vcvt.f32.f64	s0, d0
	vmov		r0, s0
	RET
END(__truncdfsf2)

ENTRY(__fixsfsi)
	vmov		s0, r0
	vcvt.s32.f32	s0, s0
	vmov		r0, s0
	RET
END(__fixsfsi)

ENTRY(__fixunssfsi)
	vmov		s0, r0
	vcvt.u32.f32	s0, s0
	vmov		r0, s0
	RET
END(__fixunssfsi)

ENTRY(__floatsisf)
	vmov		s0, r0
	vcvt.f32.s32	s0, s0
	vmov		r0, s0
	RET
END(__floatsisf)

ENTRY(__floatunsisf)
	vmov		s0, r0
	vcvt.f32.u32	s0, s0
	vmov		r0, s0
	RET
END(__floatunsisf)

/*
 * Effect of a floating point comparision on the condition flags.
 *      N Z C V
 * EQ = 0 1 1 0
 * LT = 1 0 0 0
 * GT = 0 0 1 0
 * UN = 0 0 1 1
 */
#ifdef __ARM_EABI__
ENTRY(__aeabi_cfcmpeq)
	vmov		s0, s1, r0, r1
	vcmp.f32	s0, s1
	vmrs		APSR_nzcv, fpscr
	RET
END(__aeabi_cfcmpeq)

ENTRY(__aeabi_cfcmple)
	vmov		s0, s1, r0, r1
	vcmpe.f32	s0, s1
	vmrs		APSR_nzcv, fpscr
	RET
END(__aeabi_cfcmple)

ENTRY(__aeabi_cfrcmple)
	vmov		s0, s1, r0, r1
	vcmpe.f32	s1, s0
	vmrs		APSR_nzcv, fpscr
	RET
END(__aeabi_cfrcmple)

ENTRY(__aeabi_fcmpeq)
	vmov		s0, s1, r0, r1
	vcmp.f32	s0, s1
	vmrs		APSR_nzcv, fpscr
	moveq		r0, #1		/* (a == b) */
	movne		r0, #0		/* (a != b) or unordered */
	RET
END(__aeabi_fcmpeq)

ENTRY(__aeabi_fcmplt)
	vmov		s0, s1, r0, r1
	vcmp.f32	s0, s1
	vmrs		APSR_nzcv, fpscr
	movlt		r0, #1		/* (a < b) */
	movcs		r0, #0		/* (a >= b) or unordered */
	RET
END(__aeabi_fcmplt)

ENTRY(__aeabi_fcmple)
	vmov		s0, s1, r0, r1
	vcmp.f32	s0, s1
	vmrs		APSR_nzcv, fpscr
	movls		r0, #1		/* (a <= b) */
	movhi		r0, #0		/* (a > b) or unordered */
	RET
END(__aeabi_fcmple)

ENTRY(__aeabi_fcmpge)
	vmov		s0, s1, r0, r1
	vcmp.f32	s0, s1
	vmrs		APSR_nzcv, fpscr
	movge		r0, #1		/* (a >= b) */
	movlt		r0, #0		/* (a < b) or unordered */
	RET
END(__aeabi_fcmpge)

ENTRY(__aeabi_fcmpgt)
	vmov		s0, s1, r0, r1
	vcmp.f32	s0, s1
	vmrs		APSR_nzcv, fpscr
	movgt		r0, #1		/* (a > b) */
	movle		r0, #0		/* (a <= b) or unordered */
	RET
END(__aeabi_fcmpgt)

ENTRY(__aeabi_fcmpun)
	vmov		s0, s1, r0, r1
	vcmp.f32	s0, s1
	vmrs		APSR_nzcv, fpscr
	movvs		r0, #1		/* (isnan(a) || isnan(b)) */
	movvc		r0, #0		/* !isnan(a) && !isnan(b) */
	RET
END(__aeabi_fcmpun)

#else
/* N set if compare <= result */
/* Z set if compare = result */
/* C set if compare (=,>=,UNORD) result */
/* V set if compare UNORD result */

STRONG_ALIAS(__eqsf2, __nesf2)
ENTRY(__nesf2)
	vmov		s0, s1, r0, r1
	vcmp.f32	s0, s1
	vmrs		APSR_nzcv, fpscr
	moveq		r0, #0		/* !(a == b) */
	movne		r0, #1		/* !(a == b) */
	RET
END(__nesf2)

STRONG_ALIAS(__gesf2, __ltsf2)
ENTRY(__ltsf2)
	vmov		s0, s1, r0, r1
	vcmp.f32	s0, s1
	vmrs		APSR_nzcv, fpscr
	mvnmi		r0, #0		/* -(a < b) */
	movpl		r0, #0		/* -(a < b) */
	RET
END(__ltsf2)

STRONG_ALIAS(__gtsf2, __lesf2)
ENTRY(__lesf2)
	vmov		s0, s1, r0, r1
	vcmp.f32	s0, s1
	vmrs		APSR_nzcv, fpscr
	movgt		r0, #1		/* (a > b) */
	movle		r0, #0		/* (a > b) */
	RET
END(__lesf2)

ENTRY(__unordsf2)
	vmov		s0, s1, r0, r1
	vcmp.f32	s0, s1
	vmrs		APSR_nzcv, fpscr
	movvs		r0, #1		/* isnan(a) || isnan(b) */
	movvc		r0, #0		/* isnan(a) || isnan(b) */
	RET
END(__unordsf2)
#endif /* !__ARM_EABI__ */
