/*	$NetBSD: cpufunc.S,v 1.21 2014/09/16 09:42:47 mrg Exp $	*/

/*-
 * Copyright (c) 1998, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, and by Andrew Doran.
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

/*
 * Functions to provide access to i386-specific instructions.
 *
 * These are shared with NetBSD/xen.
 */

#include <sys/errno.h>

#include <machine/asm.h>
__KERNEL_RCSID(0, "$NetBSD: cpufunc.S,v 1.21 2014/09/16 09:42:47 mrg Exp $");

#include "opt_xen.h"

#include <machine/specialreg.h>
#include <machine/segments.h>

#include "assym.h"

ENTRY(x86_lfence)
	lock
	addl	$0, -4(%esp)
	ret
END(x86_lfence)

ENTRY(x86_sfence)
	lock
	addl	$0, -4(%esp)
	ret
END(x86_sfence)

ENTRY(x86_mfence)
	lock
	addl	$0, -4(%esp)
	ret
END(x86_mfence)

ENTRY(lidt)
	movl	4(%esp), %eax
	lidt	(%eax)
	ret
END(lidt)

ENTRY(rcr3)
	movl	%cr3, %eax
	ret
END(rcr3)

ENTRY(lcr4)
	movl	4(%esp), %eax
	movl	%eax, %cr4
	ret
END(lcr4)

ENTRY(rcr4)
	movl	%cr4, %eax
	ret
END(rcr4)

ENTRY(x86_read_flags)
	pushfl
	popl	%eax
	ret
END(x86_read_flags)

ENTRY(x86_write_flags)
	movl	4(%esp), %eax
	pushl	%eax
	popfl
	ret
END(x86_write_flags)

#ifndef XEN
STRONG_ALIAS(x86_write_psl,x86_write_flags)
STRONG_ALIAS(x86_read_psl,x86_read_flags)
#endif	/* XEN */

ENTRY(rdmsr)
	movl	4(%esp), %ecx
	rdmsr
	ret
END(rdmsr)

ENTRY(wrmsr)
	movl	4(%esp), %ecx
	movl	8(%esp), %eax
	movl	12(%esp), %edx
	wrmsr
	ret
END(wrmsr)

ENTRY(rdmsr_locked)
	movl	4(%esp), %ecx
	pushl	%edi
	movl	$OPTERON_MSR_PASSCODE, %edi
	rdmsr
	popl	%edi
	ret
END(rdmsr_locked)

ENTRY(wrmsr_locked)
	movl	4(%esp), %ecx
	movl	8(%esp), %eax
	movl	12(%esp), %edx
	pushl	%edi
	movl	$OPTERON_MSR_PASSCODE, %edi
	wrmsr
	popl	%edi
	ret
END(wrmsr_locked)

/*
 * Support for reading MSRs in the safe manner (returns EFAULT on fault)
 */
/* int rdmsr_safe(u_int msr, uint64_t *data) */
ENTRY(rdmsr_safe)
	movl	CPUVAR(CURLWP), %ecx
	movl	L_PCB(%ecx), %ecx
	movl	$_C_LABEL(msr_onfault), PCB_ONFAULT(%ecx)

	movl	4(%esp), %ecx /* u_int msr */
	rdmsr
	movl	8(%esp), %ecx /* *data */
	movl	%eax, (%ecx)  /* low-order bits */
	movl	%edx, 4(%ecx) /* high-order bits */
	xorl	%eax, %eax    /* "no error" */

	movl	CPUVAR(CURLWP), %ecx
	movl	L_PCB(%ecx), %ecx
	movl	%eax, PCB_ONFAULT(%ecx)

	ret
END(rdmsr_safe)

/* uint64_t rdxcr(uint32_t) */
ENTRY(rdxcr)
	movl	4(%esp), %ecx	/* extended control reg number */
	xgetbv			/* Read to %edx:%eax */
	ret
END(rdxcr)

/* void wrxcr(uint32_t, uint64_t) */
ENTRY(wrxcr)
	movl	4(%esp), %ecx	/* extended control reg number */
	movl	8(%esp), %eax	/* feature mask bits */
	movl	12(%esp), %edx
	xsetbv
	ret
END(wrxcr)
	

/*
 * MSR operations fault handler
 */
NENTRY(msr_onfault)
	movl	CPUVAR(CURLWP), %ecx
	movl	L_PCB(%ecx), %ecx
	movl	$0, PCB_ONFAULT(%ecx)
	movl	$EFAULT, %eax
	ret
END(msr_onfault)

ENTRY(cpu_counter)
	rdtsc
	addl	CPUVAR(CC_SKEW), %eax
	adcl	CPUVAR(CC_SKEW+4), %edx
	ret
END(cpu_counter)

ENTRY(cpu_counter32)
	rdtsc
	addl	CPUVAR(CC_SKEW), %eax
	ret
END(cpu_counter32)

ENTRY(rdpmc)
	movl	4(%esp), %ecx
	rdpmc
	ret
END(rdpmc)

ENTRY(breakpoint)
	pushl	%ebp
	movl	%esp, %ebp
	int	$0x03		/* paranoid, not 'int3' */
	popl	%ebp
	ret
END(breakpoint)

ENTRY(x86_curcpu)
	movl	%fs:(CPU_INFO_SELF), %eax
	ret
END(x86_curcpu)

ENTRY(x86_curlwp)
	movl	%fs:(CPU_INFO_CURLWP), %eax
	ret
END(x86_curlwp)

ENTRY(cpu_set_curpri)
	movl	4(%esp), %eax
	movl	%eax, %fs:(CPU_INFO_CURPRIORITY)
	ret
END(cpu_set_curpri)

ENTRY(__byte_swap_u32_variable)
	movl	4(%esp), %eax
	bswapl	%eax
	ret
END(__byte_swap_u32_variable)

ENTRY(__byte_swap_u16_variable)
	movl	4(%esp), %eax
	xchgb	%al, %ah
	ret
END(__byte_swap_u16_variable)

/*
 * void x86_flush()
 *
 * Flush instruction pipelines by doing an intersegment (far) return.
 */
ENTRY(x86_flush)
	popl	%eax
	pushl	$GSEL(GCODE_SEL, SEL_KPL)
	pushl	%eax
	lret
END(x86_flush)

/* Waits - set up stack frame. */
ENTRY(x86_hlt)
	pushl	%ebp
	movl	%esp, %ebp
	hlt
	leave
	ret
END(x86_hlt)

/* Waits - set up stack frame. */
ENTRY(x86_stihlt)
	pushl	%ebp
	movl	%esp, %ebp
	sti
	hlt
	leave
	ret
END(x86_stihlt)

ENTRY(x86_monitor)
	movl	4(%esp), %eax
	movl	8(%esp), %ecx
	movl	12(%esp), %edx
	monitor	%eax, %ecx, %edx
	ret
END(x86_monitor)

/* Waits - set up stack frame. */
ENTRY(x86_mwait)  
	pushl	%ebp
	movl	%esp, %ebp
	movl	8(%ebp), %eax
	movl	12(%ebp), %ecx
	mwait	%eax, %ecx
	leave
	ret
END(x86_mwait)  

ENTRY(x86_pause)
	pause
	ret
END(x86_pause)

ENTRY(x86_cpuid2)
	pushl	%ebx
	pushl	%edi
	movl	12(%esp), %eax
	movl	16(%esp), %ecx
	movl	20(%esp), %edi
	cpuid
	movl	%eax, 0(%edi)
	movl	%ebx, 4(%edi)
	movl	%ecx, 8(%edi)
	movl	%edx, 12(%edi)
	popl	%edi
	popl	%ebx
	ret
END(x86_cpuid2)

ENTRY(x86_getss)
	movl	%ss, %eax
	ret
END(x86_getss)

ENTRY(fldcw)
	movl	4(%esp), %eax
	fldcw	(%eax)
	ret
END(fldcw)

ENTRY(fnclex)	
	fnclex
	ret
END(fnclex)	

ENTRY(fninit)
	fninit
	ret
END(fninit)

ENTRY(fnsave)
	movl	4(%esp), %eax
	fnsave	(%eax)
	ret
END(fnsave)

ENTRY(fnstcw)
	movl	4(%esp), %eax
	fnstcw	(%eax)
	ret
END(fnstcw)

ENTRY(fngetsw)
	fnstsw	%ax
	ret
END(fngetsw)

ENTRY(fnstsw)
	movl	4(%esp), %eax
	fnstsw	(%eax)
	ret
END(fnstsw)

ENTRY(fp_divide_by_0)
	fldz
	fld1
	fdiv	%st, %st(1)
	fwait
	ret
END(fp_divide_by_0)

ENTRY(frstor)
	movl	4(%esp), %eax
	frstor	(%eax)
	ret
END(frstor)

ENTRY(fwait)
	fwait
	ret
END(fwait)

ENTRY(clts)
	clts
	ret
END(clts)

ENTRY(stts)
	movl	%cr0, %eax
	testl	$CR0_TS, %eax
	jnz	1f
	orl	$CR0_TS, %eax
	movl	%eax, %cr0
1:
	ret
END(stts)

ENTRY(fxsave)
	movl	4(%esp), %eax
	fxsave	(%eax)
	ret
END(fxsave)

ENTRY(fxrstor)
	movl	4(%esp), %eax
	fxrstor	(%eax)
	ret
END(fxrstor)

ENTRY(xsave)
	movl	4(%esp), %ecx
	movl	8(%esp), %eax	/* feature mask bits */
	movl	12(%esp), %edx
	xsave	(%ecx)
	ret
END(xsave)

ENTRY(xsaveopt)
	movl	4(%esp), %ecx
	movl	8(%esp), %eax	/* feature mask bits */
	movl	12(%esp), %edx
	xsaveopt	(%ecx)
	ret
END(xsaveopt)

ENTRY(xrstor)
	movl	4(%esp), %ecx
	movl	8(%esp), %eax	/* feature mask bits */
	movl	12(%esp), %edx
	xrstor	(%ecx)
	ret
END(xrstor)

ENTRY(x86_stmxcsr)
	movl	4(%esp), %eax
	stmxcsr	(%eax)
	ret
END(x86_stmxcsr)

ENTRY(x86_ldmxcsr)
	movl	4(%esp), %eax
	ldmxcsr	(%eax)
	ret
END(x86_ldmxcsr)

ENTRY(fldummy)
	ffree	%st(7)
	fldz
	ret
END(fldummy)

ENTRY(inb)
	movl	4(%esp), %edx
	xorl	%eax, %eax
	inb	%dx, %al
	ret
END(inb)

ENTRY(insb)
	pushl	%edi
	movl	8(%esp), %edx
	movl	12(%esp), %edi
	movl	16(%esp), %ecx
	rep
	insb
	popl	%edi
	ret
END(insb)

ENTRY(inw)
	movl	4(%esp), %edx
	xorl	%eax, %eax
	inw	%dx, %ax
	ret
END(inw)

ENTRY(insw)
	pushl	%edi
	movl	8(%esp), %edx
	movl	12(%esp), %edi
	movl	16(%esp), %ecx
	rep
	insw
	popl	%edi
	ret
END(insw)

ENTRY(inl)
	movl	4(%esp), %edx
	inl	%dx, %eax
	ret
END(inl)

ENTRY(insl)
	pushl	%edi
	movl	8(%esp), %edx
	movl	12(%esp), %edi
	movl	16(%esp), %ecx
	rep
	insl
	popl	%edi
	ret
END(insl)

ENTRY(outb)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	outb	%al, %dx
	ret
END(outb)

ENTRY(outsb)
	pushl	%esi
	movl	8(%esp), %edx
	movl	12(%esp), %esi
	movl	16(%esp), %ecx
	rep
	outsb
	popl	%esi
	ret
END(outsb)

ENTRY(outw)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	outw	%ax, %dx
	ret
END(outw)

ENTRY(outsw)
	pushl	%esi
	movl	8(%esp), %edx
	movl	12(%esp), %esi
	movl	16(%esp), %ecx
	rep
	outsw
	popl	%esi
	ret
END(outsw)

ENTRY(outl)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	outl	%eax, %dx
	ret
END(outl)

ENTRY(outsl)
	pushl	%esi
	movl	8(%esp), %edx
	movl	12(%esp), %esi
	movl	16(%esp), %ecx
	rep
	outsl
	popl	%esi
	ret
END(outsl)
