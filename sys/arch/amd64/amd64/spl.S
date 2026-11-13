/*	$NetBSD: spl.S,v 1.48.4.1 2026/01/25 16:38:53 martin Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1998, 2007, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and Andrew Doran.
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

#include "opt_ddb.h"
#include "opt_kasan.h"
#include "opt_kmsan.h"

#define ALIGN_TEXT	.align 16,0x90

#include <machine/asm.h>
#include <machine/trap.h>
#include <machine/segments.h>
#include <machine/frameasm.h>

#include "assym.h"

	.text

/*
 * int splraise(int s);
 */
ENTRY(splraise)
	movzbl	CPUVAR(ILEVEL),%eax
	cmpl	%edi,%eax
	cmoval	%eax,%edi
	movb	%dil,CPUVAR(ILEVEL)
	KMSAN_INIT_RET(4)
	ret
END(splraise)

/*
 * Xsoftintr()
 *
 * Switch to the LWP assigned to handle interrupts from the given
 * source.  We borrow the VM context from the interrupted LWP.
 *
 * On entry:
 *
 *	%rax		intrsource
 *	%r13		address to return to
 */
IDTVEC(softintr)
	/* set up struct switchframe */
	pushq	$_C_LABEL(softintr_ret)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	movb	$IPL_HIGH,CPUVAR(ILEVEL)
	movq	CPUVAR(CURLWP),%r15
	movq	IS_LWP(%rax),%rdi	/* switch to handler LWP */
	movq	L_PCB(%rdi),%rdx
	movq	L_PCB(%r15),%rcx
	movq	%rdi,CPUVAR(CURLWP)

#ifdef KASAN
	/* clear the new stack */
	pushq	%rax
	pushq	%rdx
	pushq	%rcx
	callq	_C_LABEL(kasan_softint)
	popq	%rcx
	popq	%rdx
	popq	%rax
#endif

#ifdef KMSAN
	pushq	%rax
	pushq	%rdx
	pushq	%rcx
	callq	_C_LABEL(kmsan_softint)
	popq	%rcx
	popq	%rdx
	popq	%rax
#endif

	/* save old context */
	movq	%rsp,PCB_RSP(%rcx)
	movq	%rbp,PCB_RBP(%rcx)

	/* switch to the new stack */
	movq	PCB_RSP0(%rdx),%rsp

	/* dispatch */
	STI(di)
	movq	%r15,%rdi		/* interrupted LWP */
	movl	IS_MAXLEVEL(%rax),%esi	/* ipl to run at */
	call	_C_LABEL(softint_dispatch)/* run handlers */
	CLI(di)

	/* restore old context */
	movq	L_PCB(%r15),%rcx
	movq	PCB_RSP(%rcx),%rsp

	/*
	 * for non-interlocked mutex release to work safely the change
	 * to ci_curlwp must not languish in the store buffer. therefore
	 * we use XCHG and not MOV here.  see kern_mutex.c.
	 */
	xchgq	%r15,CPUVAR(CURLWP)	/* restore curlwp */
	popq	%r15			/* unwind switchframe */
	addq	$(5 * 8),%rsp
	jmp	*%r13			/* back to Xspllower/Xdoreti */
IDTVEC_END(softintr)

/*
 * softintr_ret()
 *
 * Trampoline function that gets returned to by cpu_switchto() when
 * an interrupt handler blocks.  On entry:
 *
 *	%rax		prevlwp from cpu_switchto()
 */
ENTRY(softintr_ret)
	incl	CPUVAR(MTX_COUNT)	/* re-adjust after mi_switch */
	CLI(ax)		/* %rax not used by Xspllower/Xdoreti */
	jmp	*%r13			/* back to Xspllower/Xdoreti */
END(softintr_ret)

/*
 * void softint_trigger(uintptr_t machdep);
 *
 * Software interrupt registration.
 */
ENTRY(softint_trigger)
	shlq	$8,%rdi			/* clear upper 8 bits */
	shrq	$8,%rdi
	orq	%rdi,CPUVAR(IPENDING)	/* atomic on local cpu */
	ret
END(softint_trigger)

/*
 * Xrecurse_preempt()
 *
 * Handles preemption interrupts via Xspllower().
 */
IDTVEC(recurse_preempt)
	movb	$IPL_PREEMPT,CPUVAR(ILEVEL)
	STI(di)
	xorq	%rdi,%rdi
	KMSAN_INIT_ARG(8)
	call	_C_LABEL(kpreempt)
	CLI(di)
	jmp	*%r13			/* back to Xspllower */
IDTVEC_END(recurse_preempt)

/*
 * Xresume_preempt()
 *
 * Handles preemption interrupts via Xdoreti().
 */
IDTVEC(resume_preempt)
	movb	$IPL_PREEMPT,CPUVAR(ILEVEL)
	STI(ax)
	testq	$SEL_RPL,TF_CS(%rsp)
	jnz	1f
	movq	TF_RIP(%rsp),%rdi
	KMSAN_INIT_ARG(8)
	call	_C_LABEL(kpreempt)	/* from kernel */
	CLI(ax)
	jmp	*%r13			/* back to Xdoreti */
1:
	call	_C_LABEL(preempt)	/* from user */
	CLI(ax)
	jmp	*%r13			/* back to Xdoreti */
IDTVEC_END(resume_preempt)

/*
 * void spllower(int s);
 */
ENTRY(spllower)
	movzbl	CPUVAR(ILEVEL),%edx
	cmpl	%edx,%edi			/* new level is lower? */
	jae	1f
	xorq	%rcx,%rcx			/* rcx: ci_ipending mask */
	notq	%rcx
	shrq	$8,%rcx
	movq	%rdi,%r9			/* r9: shifted new level */
	shlq	$56,%r9
0:
	movq	CPUVAR(IPENDING),%rax
	testq	%rax,CPUVAR(IUNMASK)(,%rdi,8)	/* deferred interrupts? */
	/*
	 * On the P4 this jump is cheaper than patching in junk
	 * using cmov.  Is cmpxchg expensive if it fails?
	 */
	jnz	2f
	movq	%rax,%r8
	andq	%rcx,%r8
	orq	%r9,%r8
	cmpxchgq %r8,CPUVAR(ISTATE)		/* swap in new ilevel */
	jnz	0b
1:
	ret
2:
	jmp	_C_LABEL(Xspllower)
END(spllower)

/*
 * void Xspllower(int s);
 *
 * Process pending interrupts.
 *
 * Important registers:
 *   ebx - cpl
 *   r13 - address to resume loop at
 *
 * It is important that the bit scan instruction is bsr, it will get
 * the highest 2 bits (currently the IPI and clock handlers) first,
 * to avoid deadlocks where one CPU sends an IPI, another one is at
 * splhigh() and defers it, lands in here via splx(), and handles
 * a lower-prio one first, which needs to take the kernel lock -->
 * the sending CPU will never see the that CPU accept the IPI
 * (see pmap_tlb_shootnow).
 * XenPV note: it seems that we can be called at spl0, so need to use CLI2()
 */
IDTVEC(spllower)
	pushq	%rbx
	pushq	%r13
	pushq	%r12
	movl	%edi,%ebx
	leaq	1f(%rip),%r13		/* address to resume loop at */
1:
	movl	%ebx,%eax		/* get cpl */
	movq	CPUVAR(IUNMASK)(,%rax,8),%rax
	CLI2(si, r12)
	andq	CPUVAR(IPENDING),%rax	/* any non-masked bits left? */
	jz	2f
	bsrq	%rax,%rax
	btrq	%rax,CPUVAR(IPENDING)
	movq	CPUVAR(ISOURCES)(,%rax,8),%rax
	jmp	*IS_RECURSE(%rax)
2:
	movb	%bl,CPUVAR(ILEVEL)
	STI(si)
	popq	%r12
	popq	%r13
	popq	%rbx
	ret
IDTVEC_END(spllower)

/*
 * void Xdoreti(void);
 *
 * Handle return from interrupt after device handler finishes.
 *
 * Important registers:
 *   ebx - cpl to restore
 *   r13 - address to resume loop at
 *   rsi, rdi and r14 - free for local use
 */
IDTVEC(doreti)
	popq	%rbx			/* get previous priority */
	decl	CPUVAR(IDEPTH)
	leaq	1f(%rip),%r13
1:
	movl    %ebx,%eax
	movq	CPUVAR(IUNMASK)(,%rax,8),%rax
	CLI2(si, r14)
	andq	CPUVAR(IPENDING),%rax
	jz	2f
	bsrq	%rax,%rax		/* slow, but not worth optimizing */
	btrq	%rax,CPUVAR(IPENDING)
	movq	CPUVAR(ISOURCES)(,%rax,8),%rax
	jmp	*IS_RESUME(%rax)
2:	/* Check for ASTs on exit to user mode. */
	movb	%bl,CPUVAR(ILEVEL)
5:
	testb	$SEL_RPL,TF_CS(%rsp)
	jz	6f
.Ldoreti_checkast:
	movq	CPUVAR(CURLWP),%r14
	CHECK_ASTPENDING(%r14)
	je	3f
	CLEAR_ASTPENDING(%r14)
	STI(si)
	movl	$T_ASTFLT,TF_TRAPNO(%rsp)	/* XXX undo later.. */
	/* Pushed T_ASTFLT into tf_trapno on entry. */
	movq	%rsp,%rdi
	KMSAN_INIT_ARG(8)
	call	_C_LABEL(trap)
	CLI2(si, rdi)
	jmp	.Ldoreti_checkast
3:
	CHECK_DEFERRED_SWITCH
	jnz	9f
	HANDLE_DEFERRED_FPU
6:
	INTRFASTEXIT
9:
	STI(si)
	call	_C_LABEL(do_pmap_load)
	CLI2(si, rdi)
	jmp	.Ldoreti_checkast		/* recheck ASTs */
IDTVEC_END(doreti)
