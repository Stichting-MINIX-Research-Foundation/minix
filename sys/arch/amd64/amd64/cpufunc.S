/*	$NetBSD: cpufunc.S,v 1.65.18.3 2026/01/25 16:38:53 martin Exp $	*/

/*
 * Copyright (c) 1998, 2007, 2008, 2020 The NetBSD Foundation, Inc.
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

#include <sys/errno.h>

#include <machine/asm.h>
#include <machine/frameasm.h>
#include <machine/specialreg.h>
#include <machine/segments.h>

#include "opt_xen.h"
#include "opt_svs.h"

#include "assym.h"

/* Small and slow, so align less. */
#undef _ALIGN_TEXT
#define	_ALIGN_TEXT	.align 8

ENTRY(x86_lfence)
	lfence
	ret
END(x86_lfence)

ENTRY(x86_sfence)
	sfence
	ret
END(x86_sfence)

ENTRY(x86_mfence)
	mfence
	ret
END(x86_mfence)

ENTRY(paravirt_membar_sync)
	/*
	 * Store-before-load ordering with respect to matching logic
	 * on the hypervisor side.
	 *
	 * This is the same as membar_sync, but without hotpatching
	 * away the LOCK prefix on uniprocessor boots -- because under
	 * virtualization, we still have to coordinate with a `device'
	 * backed by a hypervisor that is potentially on another
	 * physical CPU even if we observe only one virtual CPU as the
	 * guest.
	 *
	 * See common/lib/libc/arch/x86_64/atomic/atomic.S for
	 * rationale and keep this in sync with the implementation
	 * of membar_sync there.
	 */
	lock
	addq	$0,-8(%rsp)
	ret
END(paravirt_membar_sync)

#ifdef XEN
STRONG_ALIAS(xen_mb,paravirt_membar_sync)
#endif	/* XEN */

#ifndef XENPV
ENTRY(invlpg)
#ifdef SVS
	movb	_C_LABEL(svs_pcid),%al
	testb	%al,%al
	jz	1f
	pushq	%rdi
	pushq	$PMAP_PCID_USER
	movq	$INVPCID_ADDRESS,%rax
	invpcid	(%rsp),%rax
	addq	$16,%rsp
1:	/* FALLTHROUGH */
#endif
	invlpg	(%rdi)
	ret
END(invlpg)

ENTRY(lgdt)
	/* Reload the descriptor table. */
	movq	%rdi,%rax
	lgdt	(%rax)
	/* Flush the prefetch queue. */
	jmp	1f
	nop
1:	jmp	_C_LABEL(lgdt_finish)
END(lgdt)

ENTRY(lidt)
	lidt	(%rdi)
	ret
END(lidt)

ENTRY(lldt)
	cmpl	%edi, CPUVAR(CURLDT)
	jne	1f
	ret
1:
	movl	%edi, CPUVAR(CURLDT)
	lldt	%di
	ret
END(lldt)

ENTRY(ltr)
	ltr	%di
	ret
END(ltr)

ENTRY(tlbflushg)
	movq	%cr4, %rax
	testq	$CR4_PGE, %rax
	jz	tlbflush
	movq	%rax, %rdx
	andq	$~CR4_PGE, %rdx
	movq	%rdx, %cr4
	movq	%rax, %cr4
	ret
END(tlbflushg)

ENTRY(tlbflush)
#ifdef SVS
	movb	_C_LABEL(svs_pcid),%al
	testb	%al,%al
	jz	1f
	xorq	%rax,%rax
	pushq	%rax
	pushq	%rax
	movq	$INVPCID_ALL_NONGLOBAL,%rax
	invpcid	(%rsp),%rax
	addq	$16,%rsp
	ret
#endif
1:	movq	%cr3, %rax
	movq	%rax, %cr3
	ret
END(tlbflush)

ENTRY(wbinvd)
	wbinvd
	ret
END(wbinvd)

ENTRY(setusergs)
	CLI2(ax, rsi)
	swapgs
	movw	%di, %gs
	swapgs
	STI(ax)
	ret
END(setusergs)

ENTRY(x86_read_flags)
	pushfq
	popq	%rax
	KMSAN_INIT_RET(8)
	ret
END(x86_read_flags)

STRONG_ALIAS(x86_read_psl,x86_read_flags)

ENTRY(x86_write_flags)
	pushq	%rdi
	popfq
	ret
END(x86_write_flags)

STRONG_ALIAS(x86_write_psl,x86_write_flags)

ENTRY(smap_enable)
	SMAP_ENABLE
	ret
END(smap_enable)

ENTRY(smap_disable)
	SMAP_DISABLE
	ret
END(smap_disable)

/*
 * %rdi = name
 * %rsi = sel
 */
ENTRY(x86_hotpatch)
	/* save RFLAGS, and disable intrs */
	pushfq
	cli

	/* save CR0, and disable WP */
	movq	%cr0,%rcx
	pushq	%rcx
	andq	$~CR0_WP,%rcx
	movq	%rcx,%cr0

	callq	_C_LABEL(x86_hotpatch_apply)

	/* write back and invalidate cache */
	wbinvd

	/* restore CR0 */
	popq	%rcx
	movq	%rcx,%cr0

	/* flush instruction pipeline */
	pushq	%rax
	callq	x86_flush
	popq	%rax

	/* clean up */
	movq	%rax,%rdi
	callq	_C_LABEL(x86_hotpatch_cleanup)

	/* restore RFLAGS */
	popfq
	ret
END(x86_hotpatch)
#endif /* !XENPV */

/*
 * cpu_counter and cpu_counter32 could be exact same, but KMSAN needs to have
 * the correct size of the return value.
 */
#define SERIALIZE_lfence	lfence
#define SERIALIZE_mfence	mfence

#define ADD_counter32	addl	CPUVAR(CC_SKEW), %eax
#define ADD_counter	shlq	$32, %rdx		;\
			orq	%rdx, %rax		;\
			addq	CPUVAR(CC_SKEW), %rax

#define RSIZE_counter32	4
#define RSIZE_counter	8

#define CPU_COUNTER_FENCE(counter, fence)	\
ENTRY(cpu_ ## counter ## _ ## fence)		;\
	movq	CPUVAR(CURLWP), %rcx		;\
1:						;\
	movq	L_NCSW(%rcx), %rdi		;\
	SERIALIZE_ ## fence			;\
	rdtsc					;\
	ADD_ ## counter				;\
	cmpq	%rdi, L_NCSW(%rcx)		;\
	jne	2f				;\
	KMSAN_INIT_RET(RSIZE_ ## counter)	;\
	ret					;\
2:						;\
	jmp	1b				;\
END(cpu_ ## counter ## _ ## fence)

CPU_COUNTER_FENCE(counter, lfence)
CPU_COUNTER_FENCE(counter, mfence)
CPU_COUNTER_FENCE(counter32, lfence)
CPU_COUNTER_FENCE(counter32, mfence)

#define CPU_COUNTER_CPUID(counter)		\
ENTRY(cpu_ ## counter ## _cpuid)		;\
	movq	%rbx, %r9			;\
	movq	CPUVAR(CURLWP), %r8		;\
1:						;\
	movq	L_NCSW(%r8), %rdi		;\
	xor	%eax, %eax			;\
	cpuid					;\
	rdtsc					;\
	ADD_ ## counter				;\
	cmpq	%rdi, L_NCSW(%r8)		;\
	jne	2f				;\
	movq	%r9, %rbx			;\
	KMSAN_INIT_RET(RSIZE_ ## counter)	;\
	ret					;\
2:						;\
	jmp	1b				;\
END(cpu_ ## counter ## _cpuid)

CPU_COUNTER_CPUID(counter)
CPU_COUNTER_CPUID(counter32)

ENTRY(rdmsr_safe)
	movq	CPUVAR(CURLWP), %r8
	movq	L_PCB(%r8), %r8
	movq	$_C_LABEL(msr_onfault), PCB_ONFAULT(%r8)

	movl	%edi, %ecx
	rdmsr
	salq	$32, %rdx
	movl	%eax, %eax	/* zero-extend %eax -> %rax */
	orq	%rdx, %rax
	movq	%rax, (%rsi)

	xorq	%rax, %rax
	movq	%rax, PCB_ONFAULT(%r8)
#ifdef KMSAN
	movq	%rsi,%rdi
	movq	$8,%rsi
	xorq	%rdx,%rdx
	callq	_C_LABEL(kmsan_mark)
#endif
	KMSAN_INIT_RET(4)
	ret
END(rdmsr_safe)

ENTRY(msr_onfault)
	movq	CPUVAR(CURLWP), %r8
	movq	L_PCB(%r8), %r8
	movq	$0, PCB_ONFAULT(%r8)
	movl	$EFAULT, %eax
	ret
END(msr_onfault)

ENTRY(breakpoint)
	pushq	%rbp
	movq	%rsp, %rbp
	int	$0x03		/* paranoid, not 'int3' */
	leave
	ret
END(breakpoint)

ENTRY(x86_curcpu)
	movq	%gs:(CPU_INFO_SELF), %rax
	KMSAN_INIT_RET(8)
	ret
END(x86_curcpu)

ENTRY(x86_curlwp)
	movq	%gs:(CPU_INFO_CURLWP), %rax
	KMSAN_INIT_RET(8)
	ret
END(x86_curlwp)

ENTRY(__byte_swap_u32_variable)
	movl	%edi, %eax
	bswapl	%eax
	KMSAN_INIT_RET(4)
	ret
END(__byte_swap_u32_variable)

ENTRY(__byte_swap_u16_variable)
	movl	%edi, %eax
	xchgb	%al, %ah
	KMSAN_INIT_RET(2)
	ret
END(__byte_swap_u16_variable)

/*
 * Reload segments after a GDT change.
 */
ENTRY(lgdt_finish)
	movl	$GSEL(GDATA_SEL, SEL_KPL),%eax
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%ss
	jmp	_C_LABEL(x86_flush)
END(lgdt_finish)

/*
 * Flush instruction pipelines by doing an intersegment (far) return.
 */
ENTRY(x86_flush)
	popq	%rax
	pushq	$GSEL(GCODE_SEL, SEL_KPL)
	pushq	%rax
	lretq
END(x86_flush)

/* Waits - set up stack frame. */
ENTRY(x86_hlt)
	pushq	%rbp
	movq	%rsp, %rbp
	hlt
	leave
	ret
END(x86_hlt)

/* Waits - set up stack frame. */
ENTRY(x86_stihlt)
	pushq	%rbp
	movq	%rsp, %rbp
	sti
	hlt
	leave
	ret
END(x86_stihlt)

ENTRY(x86_monitor)
	movq	%rdi, %rax
	movq	%rsi, %rcx
	monitor	%rax, %rcx, %rdx
	ret
END(x86_monitor)

/* Waits - set up stack frame. */
ENTRY(x86_mwait)
	pushq	%rbp
	movq	%rsp, %rbp
	movq	%rdi, %rax
	movq	%rsi, %rcx
	mwait	%rax, %rcx
	leave
	ret
END(x86_mwait)

ENTRY(stts)
	movq	%cr0, %rax
	orq	$CR0_TS, %rax
	movq	%rax, %cr0
	ret
END(stts)

ENTRY(fldummy)
	ffree	%st(7)
	fldz
	ret
END(fldummy)

ENTRY(inb)
	movq	%rdi, %rdx
	xorq	%rax, %rax
	inb	%dx, %al
	KMSAN_INIT_RET(1)
	ret
END(inb)

ENTRY(inw)
	movq	%rdi, %rdx
	xorq	%rax, %rax
	inw	%dx, %ax
	KMSAN_INIT_RET(2)
	ret
END(inw)

ENTRY(inl)
	movq	%rdi, %rdx
	xorq	%rax, %rax
	inl	%dx, %eax
	KMSAN_INIT_RET(4)
	ret
END(inl)

ENTRY(outb)
	movq	%rdi, %rdx
	movq	%rsi, %rax
	outb	%al, %dx
	ret
END(outb)

ENTRY(outw)
	movq	%rdi, %rdx
	movq	%rsi, %rax
	outw	%ax, %dx
	ret
END(outw)

ENTRY(outl)
	movq	%rdi, %rdx
	movq	%rsi, %rax
	outl	%eax, %dx
	ret
END(outl)

/*
 * Used by SVS only, to make an atomic but fast copy. Doesn't have
 * sanitizer instrumentation, but sanitizers disable SVS, so no problem.
 */
ENTRY(svs_quad_copy)
	movq	%rdx,%rcx
	rep
	movsq
	ret
END(svs_quad_copy)
