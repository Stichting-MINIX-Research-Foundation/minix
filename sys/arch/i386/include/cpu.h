/*	$NetBSD: cpu.h,v 1.178 2011/12/30 17:57:49 cherry Exp $	*/

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
 *	@(#)cpu.h	5.4 (Berkeley) 5/9/91
 */

#ifndef _I386_CPU_H_
#define _I386_CPU_H_

#include <x86/cpu.h>

#ifdef _KERNEL

#if defined(__GNUC__) && !defined(_MODULE)
static struct cpu_info *x86_curcpu(void);
static lwp_t *x86_curlwp(void);

__inline static struct cpu_info * __unused
x86_curcpu(void)
{
	struct cpu_info *ci;

	__asm volatile("movl %%fs:%1, %0" :
	    "=r" (ci) :
	    "m"
	    (*(struct cpu_info * const *)offsetof(struct cpu_info, ci_self)));
	return ci;
}

__inline static lwp_t * __attribute__ ((const))
x86_curlwp(void)
{
	lwp_t *l;

	__asm volatile("movl %%fs:%1, %0" :
	    "=r" (l) :
	    "m"
	    (*(struct cpu_info * const *)offsetof(struct cpu_info, ci_curlwp)));
	return l;
}

__inline static void __unused
cpu_set_curpri(int pri)
{

	__asm volatile(
	    "movl %1, %%fs:%0" :
	    "=m" (*(struct cpu_info *)offsetof(struct cpu_info, ci_schedstate.spc_curpriority)) :
	    "r" (pri)
	);
}
#endif

#define	CLKF_USERMODE(frame)	USERMODE((frame)->cf_if.if_cs, (frame)->cf_if.if_eflags)
#define	CLKF_PC(frame)		((frame)->cf_if.if_eip)
#define	CLKF_INTR(frame)	(curcpu()->ci_idepth > 0)
#define	LWP_PC(l)		((l)->l_md.md_regs->tf_eip)

#ifdef PAE
void cpu_alloc_l3_page(struct cpu_info *);
#endif /* PAE */

#endif	/* _KERNEL */

#if defined(__minix)
#include <x86/psl.h>
/* User flags are S (Status) and C (Control) flags. */
#define X86_FLAGS_USER (PSL_C | PSL_PF | PSL_AF | PSL_Z | \
	PSL_N | PSL_D | PSL_V)
#endif /* defined(__minix) */

#endif /* !_I386_CPU_H_ */
