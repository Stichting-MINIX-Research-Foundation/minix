/*	$NetBSD: acpi_wakeup_low.S,v 1.10 2018/07/14 14:29:40 maxv Exp $	*/

/*
 * Copyright (c) 2007 Joerg Sonnenberger <joerg@netbsd.org>
 * Copyright (c) 2001 Takanori Watanabe <takawata@jp.freebsd.org>
 * Copyright (c) 2001 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "assym.h"
#include <machine/asm.h>
#include <machine/segments.h>
#include <machine/specialreg.h>

	.text

	.p2align 2, 0x90
ENTRY(acpi_md_sleep_exit)
	lgdt	ACPI_SUSPEND_GDT(%r8)

	/* Reload fixed descriptors for new GDT */
	movw	$GSEL(GUDATA_SEL, SEL_UPL),%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	$GSEL(GDATA_SEL, SEL_KPL),%ax
	movw	%ax,%ss

	/*
	 * FS and GS are driven by MSRs, so use NULL for them. If we're
	 * returning to a 32bit LWP, %fs/%gs will be restored in
	 * INTRFASTEXIT.
	 */
	xorw	%ax,%ax
	movw	%ax,%fs
	movw	%ax,%gs

	movl	$MSR_EFER,%ecx
	movl	ACPI_SUSPEND_EFER(%r8),%eax
	movl	$0,%edx
	wrmsr

	movl	$MSR_FSBASE,%ecx
	movl	ACPI_SUSPEND_FS(%r8),%eax
	movl	ACPI_SUSPEND_FS+4(%r8),%edx
	wrmsr

	movl	$MSR_GSBASE,%ecx
	movl	ACPI_SUSPEND_GS(%r8),%eax
	movl	ACPI_SUSPEND_GS+4(%r8),%edx
	wrmsr

	movl	$MSR_KERNELGSBASE,%ecx
	movl	ACPI_SUSPEND_KGS(%r8),%eax
	movl	ACPI_SUSPEND_KGS+4(%r8),%edx
	wrmsr

	movq	ACPI_SUSPEND_CR8(%r8),%rax
	movq	%rax,%cr8
	movq	ACPI_SUSPEND_CR4(%r8),%rax
	movq	%rax,%cr4
	movq	ACPI_SUSPEND_CR3(%r8),%rax
	movq	%rax,%cr3
	movq	ACPI_SUSPEND_CR2(%r8),%rax
	movq	%rax,%cr2
	movq	ACPI_SUSPEND_CR0(%r8),%rax
	movq	%rax,%cr0

	jmp	1f
1:

	movq	CPUVAR(GDT),%rax
	movzwq	ACPI_SUSPEND_TR(%r8),%rdx
	andq	$~0x0200,4(%rax,%rdx, 1)

	ltr	%dx
	lldt	ACPI_SUSPEND_LDT(%r8)
	lidt	ACPI_SUSPEND_IDT(%r8)

	movq	ACPI_SUSPEND_REG+(0*8)(%r8),%rsp
	movq	ACPI_SUSPEND_REG+(1*8)(%r8),%rbx
	movq	ACPI_SUSPEND_REG+(2*8)(%r8),%rbp
	movq	ACPI_SUSPEND_REG+(3*8)(%r8),%r12
	movq	ACPI_SUSPEND_REG+(4*8)(%r8),%r13
	movq	ACPI_SUSPEND_REG+(5*8)(%r8),%r14
	movq	ACPI_SUSPEND_REG+(6*8)(%r8),%r15

	xorq	%rax,%rax

	pushq	ACPI_SUSPEND_REG+(7*8)(%r8)
	popfq
	ret
END(acpi_md_sleep_exit)

	.p2align 2, 0x90
ENTRY(acpi_md_sleep_prepare)
	movq	CPUVAR(SELF),%r8
	movq	%rbx,ACPI_SUSPEND_REG+(1*8)(%r8)
	movq	%rbp,ACPI_SUSPEND_REG+(2*8)(%r8)
	movq	%r12,ACPI_SUSPEND_REG+(3*8)(%r8)
	movq	%r13,ACPI_SUSPEND_REG+(4*8)(%r8)
	movq	%r14,ACPI_SUSPEND_REG+(5*8)(%r8)
	movq	%r15,ACPI_SUSPEND_REG+(6*8)(%r8)

	movq	%cr0,%rax
	movq	%rax,ACPI_SUSPEND_CR0(%r8)
	movq	%cr2,%rax
	movq	%rax,ACPI_SUSPEND_CR2(%r8)
	movq	%cr3,%rax
	movq	%rax,ACPI_SUSPEND_CR3(%r8)
	movq	%cr4,%rax
	movq	%rax,ACPI_SUSPEND_CR4(%r8)
	movq	%cr8,%rax
	movq	%rax,ACPI_SUSPEND_CR8(%r8)

	pushfq
	popq	ACPI_SUSPEND_REG+(7*8)(%r8)

	movq	%rsp,ACPI_SUSPEND_REG+(0*8)(%r8)

	movl	$MSR_FSBASE,%ecx
	rdmsr
	movl	%eax,ACPI_SUSPEND_FS(%r8)
	movl	%edx,ACPI_SUSPEND_FS+4(%r8)

	movl	$MSR_GSBASE,%ecx
	rdmsr
	movl	%eax,ACPI_SUSPEND_GS(%r8)
	movl	%edx,ACPI_SUSPEND_GS+4(%r8)

	movl	$MSR_KERNELGSBASE,%ecx
	rdmsr
	movl	%eax,ACPI_SUSPEND_KGS(%r8)
	movl	%edx,ACPI_SUSPEND_KGS+4(%r8)

	movl	$MSR_EFER,%ecx
	rdmsr
	movl	%eax,ACPI_SUSPEND_EFER(%r8)

	sgdt	ACPI_SUSPEND_GDT(%r8)
	sidt	ACPI_SUSPEND_IDT(%r8)
	sldt	ACPI_SUSPEND_LDT(%r8)
	str	ACPI_SUSPEND_TR(%r8)

	call	acpi_md_sleep_enter
	/* acpi_md_sleep_enter only returns on failure. */
	movl	$-1,%eax
	ret
END(acpi_md_sleep_prepare)
