/*	$NetBSD: mcontext.h,v 1.12 2014/02/15 22:20:42 dsl Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein.
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

#ifndef _I386_MCONTEXT_H_
#define _I386_MCONTEXT_H_


/*
 * mcontext extensions to handle signal delivery.
 */
#define _UC_SETSTACK	0x00010000
#define _UC_CLRSTACK	0x00020000
#define _UC_VM		0x00040000
#define	_UC_TLSBASE	0x00080000

/*
 * Layout of mcontext_t according to the System V Application Binary Interface,
 * Intel386(tm) Architecture Processor Supplement, Fourth Edition.
 */  

/*
 * General register state
 */
#define _NGREG		19
typedef	int		__greg_t;
typedef	__greg_t	__gregset_t[_NGREG];

#define _REG_GS		0
#define _REG_FS		1
#define _REG_ES		2
#define _REG_DS		3
#define _REG_EDI	4
#define _REG_ESI	5
#define _REG_EBP	6
#define _REG_ESP	7
#define _REG_EBX	8
#define _REG_EDX	9
#define _REG_ECX	10
#define _REG_EAX	11
#define _REG_TRAPNO	12
#define _REG_ERR	13
#define _REG_EIP	14
#define _REG_CS		15
#define _REG_EFL	16
#define _REG_UESP	17
#define _REG_SS		18

/*
 * Floating point register state
 */
typedef struct {
	union {
		struct {
			int	__fp_state[27];	/* Environment and registers */
		} __fpchip_state;	/* x87 regs in fsave format */
		struct {
			char	__fp_xmm[512];
		} __fp_xmm_state;	/* x87 and xmm regs in fxsave format */
		int	__fp_fpregs[128];
	} __fp_reg_set;
	int 	__fp_pad[33];			/* Historic padding */
} __fpregset_t;
__CTASSERT(sizeof (__fpregset_t) == 512 + 33 * 4);

typedef struct {
	__gregset_t	__gregs;
	__fpregset_t	__fpregs;
	__greg_t	_mc_tlsbase;
#ifdef __minix
	int	mc_magic;
	int	mc_flags;
#endif
} mcontext_t;

#define _UC_FXSAVE	0x20	/* FP state is in FXSAVE format in XMM space */

#define _UC_MACHINE_PAD	4	/* Padding appended to ucontext_t */

#define _UC_UCONTEXT_ALIGN	(~0xf)

#ifdef _KERNEL_OPT
#include "opt_vm86.h"
#ifdef VM86
/*#include <machine/psl.h>*/
#define PSL_VM 0x00020000
#define _UC_MACHINE_SP(uc) ((uc)->uc_mcontext.__gregs[_REG_UESP] + \
	((uc)->uc_mcontext.__gregs[_REG_EFL] & PSL_VM ? \
	 ((uc)->uc_mcontext.__gregs[_REG_SS] << 4) : 0))
#endif /* VM86 */
#endif /* _KERNEL_OPT */

#ifndef _UC_MACHINE_SP
#define _UC_MACHINE_SP(uc)	((uc)->uc_mcontext.__gregs[_REG_UESP])
#endif
#define _UC_MACHINE_PC(uc)	((uc)->uc_mcontext.__gregs[_REG_EIP])
#define _UC_MACHINE_INTRV(uc)	((uc)->uc_mcontext.__gregs[_REG_EAX])

#define	_UC_MACHINE_SET_PC(uc, pc)	_UC_MACHINE_PC(uc) = (pc)

#if defined(__minix)
#define	_UC_MACHINE_STACK(uc)		((uc)->uc_mcontext.__gregs[_REG_ESP])
#define	_UC_MACHINE_SET_STACK(uc, esp)	_UC_MACHINE_STACK(uc) = (esp)

#define	_UC_MACHINE_EBP(uc)		((uc)->uc_mcontext.__gregs[_REG_EBP])
#define	_UC_MACHINE_SET_EBP(uc, ebp)	_UC_MACHINE_EBP(uc) = (ebp)

#define	_UC_MACHINE_ESI(uc)		((uc)->uc_mcontext.__gregs[_REG_ESI])
#define	_UC_MACHINE_SET_ESI(uc, esi)	_UC_MACHINE_ESI(uc) = (esi)

int setmcontext(const mcontext_t *mcp);
int getmcontext(mcontext_t *mcp);

#define MCF_MAGIC 0xc0ffee
#define _MC_FPU_SAVED	0x001

#endif /* defined(__minix) */

#define	__UCONTEXT_SIZE	776

static __inline void *
__lwp_getprivate_fast(void)
{
	void *__tmp;

	__asm volatile("movl %%gs:0, %0" : "=r" (__tmp));

	return __tmp;
}

#endif	/* !_I386_MCONTEXT_H_ */
