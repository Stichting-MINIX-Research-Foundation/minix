/*	$NetBSD: mptramp.S,v 1.28 2019/11/14 16:23:52 maxv Exp $	*/

/*
 * Copyright (c) 2000, 2016 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by RedBack Networks Inc. (Author: Bill Sommerfeld), and Maxime Villard.
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
 * Copyright (c) 1999 Stefan Grefen
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR AND CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * MP startup ...
 * the stuff from cpu_spinup_trampoline to mp_startup is copied into the
 * first 640 KB.
 *
 * We startup the processors now when the kthreads become ready.
 * The steps are:
 *     1) Get the processors running kernel-code from a special
 *        page-table and stack page, do chip identification.
 *     2) halt the processors waiting for them to be enabled
 *        by a idle-thread
 */

#include "assym.h"
#include "opt_kcsan.h"
#include "opt_kmsan.h"
#include <machine/asm.h>
#include <machine/specialreg.h>
#include <machine/segments.h>
#include <machine/mpbiosvar.h>
#include <machine/i82489reg.h>
#include <machine/gdt.h>

#define _TRMP_LABEL(a)	a = . - _C_LABEL(cpu_spinup_trampoline) + MP_TRAMPOLINE

/*
 * A smp_data structure is packed at the end of the trampoline page. The stack
 * is right below this structure.
 */
#define SMP_DATA	(MP_TRAMPOLINE + PAGE_SIZE - 3 * 4)
#define SMP_DATA_STACK	(SMP_DATA + 0 * 4)
#define SMP_DATA_LARGE	(SMP_DATA + 0 * 4)
#define SMP_DATA_NOX	(SMP_DATA + 1 * 4)
#define SMP_DATA_PDIR	(SMP_DATA + 2 * 4)

	.global _C_LABEL(cpu_spinup_trampoline)
	.global _C_LABEL(cpu_spinup_trampoline_end)
	.global _C_LABEL(cpu_hatch)

	.text
	.align 4,0x0
	.code16
/* XXX ENTRY() */
LABEL(cpu_spinup_trampoline)
	cli
	xorw	%ax,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss

	/* load flat descriptor table */
#ifdef __clang__
	lgdt (mptramp_gdt32_desc)
#else
	data32 addr32 lgdt (mptramp_gdt32_desc)
#endif

	/* enable protected mode */
	movl	%cr0,%eax
	orl	$CR0_PE,%eax
	movl	%eax,%cr0
	ljmpl	$0x8,$mp_startup

_TRMP_LABEL(mp_startup)
	.code32

	movl	$0x10,%eax	/* data segment */
	movw	%ax,%ds
	movw	%ax,%ss
	movw	%ax,%es
	movw	%ax,%fs
	movw	%ax,%gs

	/* bootstrap stack end */
	movl	$SMP_DATA_STACK,%esp

	/* First, reset the PSL. */
	pushl	$PSL_MBO
	popfl

	/* Enable PAE, SSE, and PSE if available */
	movl	%cr4,%eax
	orl	$(CR4_PAE|CR4_OSFXSR|CR4_OSXMMEXCPT),%eax
	movl	$SMP_DATA_LARGE,%ecx
	movl	(%ecx),%ecx
	orl	%ecx,%ecx
	jz	.Lno_PSE
	orl	$CR4_PSE,%eax
.Lno_PSE:
	movl	%eax,%cr4

	/*
	 * Set Long Mode Enable in EFER. Also enable the syscall extensions,
	 * and NOX if available.
	 */
	movl	$MSR_EFER,%ecx
	rdmsr
	xorl	%eax,%eax
	orl	$(EFER_LME|EFER_SCE),%eax
	movl	$SMP_DATA_NOX,%ebx
	movl	(%ebx),%ebx
	cmpl	$0,%ebx
	je 	.Lno_NOX
	orl	$(EFER_NXE),%eax
.Lno_NOX:
	wrmsr

	/* Load %cr3. */
	movl	$SMP_DATA_PDIR,%ecx
	movl	(%ecx),%ecx		/* guaranteed < 4G */
	movl	%ecx,%cr3		/* load PTD addr into MMU */

	/* Enable paging and the rest of it. */
	movl	%cr0,%eax
	orl	$(CR0_PE|CR0_PG|CR0_NE|CR0_TS|CR0_MP|CR0_WP|CR0_AM),%eax
	movl	%eax,%cr0

	jmp	.Lmptramp_compat
.Lmptramp_compat:

	movl	$GSEL(GDATA_SEL, SEL_KPL),%eax	/* switch to new segment */
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%ss

	movl	$mptramp_gdt64_desc,%eax
	lgdt	(%eax)
	movl	$mptramp_jmp64,%eax
	ljmp	*(%eax)

_TRMP_LABEL(mptramp_jmp64)
	.long	mptramp_longmode
	.word	GSEL(GCODE_SEL, SEL_KPL)

#define GDT_LIMIT	0x17	/* 23 = 3 * 8 - 1 */
_TRMP_LABEL(mptramp_gdt32)
	.quad 0x0000000000000000
	.quad 0x00cf9f000000ffff	/* CS */
	.quad 0x00cf93000000ffff	/* DS */
_TRMP_LABEL(mptramp_gdt32_desc)
	.word GDT_LIMIT
	.long mptramp_gdt32

_TRMP_LABEL(mptramp_gdt64)
	.quad 0x0000000000000000
	.quad 0x00af9a000000ffff	/* CS */
	.quad 0x00cf92000000ffff	/* DS */
_TRMP_LABEL(mptramp_gdt64_desc)
	.word GDT_LIMIT
	.long mptramp_gdt64
#undef	GDT_LIMIT

_TRMP_LABEL(mptramp_longmode)
	.code64
	movabsq	$_C_LABEL(cpu_spinup_trampoline_end),%rax
	jmp	*%rax


_C_LABEL(cpu_spinup_trampoline_end):	/* end of code copied to MP_TRAMPOLINE */

	/* Wait until BP has done init sequence. */
1:
	movq	_C_LABEL(cpu_starting),%rdi
	pause
	testq	%rdi,%rdi
	jz	1b

	movq	CPU_INFO_IDLELWP(%rdi),%rsi
	movq	L_PCB(%rsi),%rsi

	movq	PCB_RSP(%rsi),%rsp
	movq	PCB_RBP(%rsi),%rbp

	movq	CPU_INFO_GDT(%rdi),%rax
	movw	$(MAXGDTSIZ-1),-10(%rsp)
	movq	%rax,-8(%rsp)
	lgdt	-10(%rsp)

	/* Switch address space. */
	movq	PCB_CR3(%rsi),%rax
	movq	%rax,%cr3

	movl	PCB_CR0(%rsi),%eax
	movq	%rax,%cr0

#if defined(KCSAN) || defined(KMSAN)
	/*
	 * The C instrumentation uses GS.base, so initialize it right now. It
	 * gets re-initialized later, that's fine.
	 */
	movl	$MSR_GSBASE,%ecx
	movq	%rdi,%rax
	movq	%rdi,%rdx
	shrq	$32,%rdx
	wrmsr
#endif

	call	_C_LABEL(cpu_hatch)
END(cpu_spinup_trampoline)

