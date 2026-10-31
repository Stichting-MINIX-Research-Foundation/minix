/*	$NetBSD: amd64_trap.S,v 1.54.4.2 2026/01/25 16:38:53 martin Exp $	*/

/*
 * Copyright (c) 1998, 2007, 2008, 2017 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, by Andrew Doran and by Maxime Villard.
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
 * Copyright (c) 2001 Wasabi Systems, Inc.
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

#include <machine/asm.h>

#include "opt_xen.h"
#include "opt_dtrace.h"

#define ALIGN_TEXT	.align 16,0x90

#include <machine/frameasm.h>
#include <machine/segments.h>
#include <machine/trap.h>
#include <machine/specialreg.h>

#include "assym.h"

/*
 * Trap and fault vector routines
 *
 * On exit from the kernel to user mode, we always need to check for ASTs.  In
 * addition, we need to do this atomically; otherwise an interrupt may occur
 * which causes an AST, but it won't get processed until the next kernel entry
 * (possibly the next clock tick).  Thus, we disable interrupt before checking,
 * and only enable them again on the final `iret' or before calling the AST
 * handler.
 */

#ifdef	XENPV
#define	PRE_TRAP	CLI2(cx,r11); movq (%rsp),%rcx ; movq 8(%rsp),%r11 ; addq $0x10,%rsp
#else
#define	PRE_TRAP
#endif

#define TRAPENTRY			\
	INTRENTRY			; \
	jmp	.Lalltraps_noentry

#define	TRAP_NJ(a)	PRE_TRAP ; pushq $(a)
#define	ZTRAP_NJ(a)	PRE_TRAP ; pushq $0 ; pushq $(a)
#define	TRAP(a)		TRAP_NJ(a) ; TRAPENTRY
#define	ZTRAP(a)	ZTRAP_NJ(a) ; TRAPENTRY

	.text

/*
 * ASM macro, used to leave the IST3 stack and to put ourselves on a non-IST
 * stack. Only RDX, RCX and RAX are allowed to be used.
 *
 *                            +------------------------------+
 * The iret frame we copy is: | rip | cs | rflags | rsp | ss |
 *                            +------------------------------+
 */
.macro	IST3_LEAVE	is_user
	.if	\is_user
		movq	CPUVAR(CURLWP),%rax
		movq	L_PCB(%rax),%rax
		movq	PCB_RSP0(%rax),%rax
	.else
		movq	TF_RSP(%rsp),%rax
		andq	$(~0xF),%rax
	.endif

	subq	$(5*8),%rax
	movq	%rax,CPUVAR(SCRATCH)

	/* Copy the iret frame. */
	movq	TF_SS(%rsp),%rcx
	movq	%rcx,(4*8)(%rax)
	movq	TF_RSP(%rsp),%rcx
	movq	%rcx,(3*8)(%rax)
	movq	TF_RFLAGS(%rsp),%rcx
	movq	%rcx,(2*8)(%rax)
	movq	TF_CS(%rsp),%rcx
	movq	%rcx,(1*8)(%rax)
	movq	TF_RIP(%rsp),%rcx
	movq	%rcx,(0*8)(%rax)

	/* Restore. */
	movq	TF_RDX(%rsp),%rdx
	movq	TF_RCX(%rsp),%rcx
	movq	TF_RAX(%rsp),%rax

	/* Zero out the stack we used, RDX+RCX+RAX+IRET. */
	movq	$0,TF_RDX(%rsp)
	movq	$0,TF_RCX(%rsp)
	movq	$0,TF_RAX(%rsp)
	movq	$0,TF_RIP(%rsp)
	movq	$0,TF_CS(%rsp)
	movq	$0,TF_RFLAGS(%rsp)
	movq	$0,TF_RSP(%rsp)
	movq	$0,TF_SS(%rsp)

	movq	CPUVAR(SCRATCH),%rsp
.endm

	TEXT_USER_BEGIN

IDTVEC(trap00)		/* #DE - Divide-by-zero error */
	ZTRAP(T_DIVIDE)
IDTVEC_END(trap00)

/*
 * Handle the SS shadow, CVE-2018-8897.
 *
 * We are running on the IST3 stack. If we are under an SS shadow, ignore
 * the exception and return immediately. Otherwise, copy the iret frame
 * onto the non-IST stack, and ZTRAP on it as usual.
 *
 * IST3 is used temporarily, and is mapped in userland by SVS. It contains
 * a few secrets, the values of the CPU context. These secrets are zeroed
 * out when we leave.
 *
 * When we ignore an SS shadow, we can't zero out the iret frame. It is
 * not a problem, because in this particular case, the frame is known not
 * to contain secrets.
 */
IDTVEC(trap01)		/* #DB - Debug */
#ifndef XENPV
	subq	$(TF_REGSIZE+16),%rsp

	/* We clobber only RDX, RCX and RAX. */
	movq	%rdx,TF_RDX(%rsp)
	movq	%rcx,TF_RCX(%rsp)
	movq	%rax,TF_RAX(%rsp)

	testb	$SEL_UPL,TF_CS(%rsp)
	jnz	.Luser_dbentry

	movl	$MSR_GSBASE,%ecx
	rdmsr
	cmpl	$VM_SPACE_SEP_HIGH32,%edx
	jae	.Lkern_dbentry

	/* SS shadow, ignore the exception. */
	xorq	%rax,%rax
	movq	%rax,%dr6

	/* Restore and zero out. */
	movq	TF_RDX(%rsp),%rdx
	movq	TF_RCX(%rsp),%rcx
	movq	TF_RAX(%rsp),%rax
	movq	$0,TF_RDX(%rsp)
	movq	$0,TF_RCX(%rsp)
	movq	$0,TF_RAX(%rsp)

	addq	$(TF_REGSIZE+16),%rsp
	iretq

.Lkern_dbentry:
	IST3_LEAVE	0
	ZTRAP(T_TRCTRAP)

.Luser_dbentry:
	swapgs
	SVS_ENTER_ALTSTACK
	IST3_LEAVE	1
	ZTRAP_NJ(T_TRCTRAP)
	subq	$TF_REGSIZE,%rsp
	INTR_SAVE_GPRS
	cld
	SMAP_ENABLE
	IBRS_ENTER
	KMSAN_ENTER
	movw	%gs,TF_GS(%rsp)
	movw	%fs,TF_FS(%rsp)
	movw	%es,TF_ES(%rsp)
	movw	%ds,TF_DS(%rsp)

	jmp	.Lalltraps_noentry
#else /* !XENPV */
	ZTRAP(T_TRCTRAP)
#endif /* !XENPV */
IDTVEC_END(trap01)

/*
 * Non Maskable Interrupts are a special case: they can be triggered even
 * with interrupts disabled, and once triggered they block further NMIs
 * until an 'iret' instruction is executed.
 *
 * Therefore we don't enable interrupts, because the CPU could switch to
 * another LWP, call 'iret' and unintentionally leave the NMI mode.
 *
 * We need to be careful about %gs too, because it is possible that we were
 * running in kernel mode with a userland %gs.
 */
IDTVEC(trap02)		/* NMI - Non-maskable interrupt */
#if defined(XENPV)
	ZTRAP(T_NMI)
#else /* XENPV */
	ZTRAP_NJ(T_NMI)
	subq	$TF_REGSIZE,%rsp
	INTR_SAVE_GPRS

	testb	$SEL_UPL,TF_CS(%rsp)
	jz	1f
	IBRS_ENTER
1:

	cld
	SMAP_ENABLE
	movw	%gs,TF_GS(%rsp)
	movw	%fs,TF_FS(%rsp)
	movw	%es,TF_ES(%rsp)
	movw	%ds,TF_DS(%rsp)

	SVS_ENTER_NMI
	KMSAN_ENTER

	movl	$MSR_GSBASE,%ecx
	rdmsr
	cmpl	$VM_SPACE_SEP_HIGH32,%edx
	jae	.Lnoswapgs

	swapgs
	movq	%rsp,%rdi
	incq	CPUVAR(NTRAP)
	call	_C_LABEL(nmitrap)
	swapgs
	jmp	.Lnmileave

.Lnoswapgs:
	movq	%rsp,%rdi
	incq	CPUVAR(NTRAP)
	call	_C_LABEL(nmitrap)

.Lnmileave:
	testb	$SEL_UPL,TF_CS(%rsp)
	jz	1f
	MDS_LEAVE
	IBRS_LEAVE
1:

	KMSAN_LEAVE
	SVS_LEAVE_NMI
	INTR_RESTORE_GPRS
	addq	$TF_REGSIZE+16,%rsp
	iretq
#endif /* XENPV */
IDTVEC_END(trap02)

IDTVEC(trap03)		/* #BP - Breakpoint */
#ifndef KDTRACE_HOOKS
	ZTRAP(T_BPTFLT)
#else
	ZTRAP_NJ(T_BPTFLT)
	INTRENTRY
	STI(si)
	/*
	 * DTrace Function Boundary Trace (fbt) probes are triggered
	 * by int3 (0xcc).
	 */
	/* Check if there is no DTrace hook registered. */
	cmpq	$0,dtrace_invop_jump_addr
	je	calltrap

	/*
	 * Set our jump address for the jump back in the event that
	 * the exception wasn't caused by DTrace at all.
	 */
	/* XXX: This doesn't look right for SMP - unless it is a
	 * constant - so why set it everytime. (dsl) */
	movq	$calltrap, dtrace_invop_calltrap_addr(%rip)

	/* Jump to the code hooked in by DTrace. */
	movq	dtrace_invop_jump_addr, %rax
	jmpq	*dtrace_invop_jump_addr
#endif
IDTVEC_END(trap03)

IDTVEC(trap04)		/* #OF - Overflow */
	ZTRAP(T_OFLOW)
IDTVEC_END(trap04)

IDTVEC(trap05)		/* #BR - BOUND range exceeded */
	ZTRAP(T_BOUND)
IDTVEC_END(trap05)

IDTVEC(trap06)		/* #UD - Invalid opcode */
	ZTRAP(T_PRIVINFLT)
IDTVEC_END(trap06)

IDTVEC(trap07)		/* #NM - Device not available (x87) */
	ZTRAP_NJ(T_DNA)
	INTRENTRY
#ifdef DIAGNOSTIC
	movzbl	CPUVAR(ILEVEL),%ebx
#endif
	movq	%rsp,%rdi
	call	_C_LABEL(fpudna)
	jmp	.Lalltraps_checkusr
IDTVEC_END(trap07)

/*
 * Double faults execute on a particular stack, and we must not jump out
 * of it. So don't enable interrupts.
 */
IDTVEC(trap08)		/* #DF - Double fault */
#if defined(XENPV)
	TRAP(T_DOUBLEFLT)
#else /* XENPV */
	TRAP_NJ(T_DOUBLEFLT)
	subq	$TF_REGSIZE,%rsp
	INTR_SAVE_GPRS

	testb	$SEL_UPL,TF_CS(%rsp)
	jz	1f
	IBRS_ENTER
	swapgs
1:

	SVS_ENTER_ALTSTACK

	cld
	SMAP_ENABLE
	movw	%gs,TF_GS(%rsp)
	movw	%fs,TF_FS(%rsp)
	movw	%es,TF_ES(%rsp)
	movw	%ds,TF_DS(%rsp)

	movq	%rsp,%rdi
	incq	CPUVAR(NTRAP)
	call	_C_LABEL(doubletrap)

	testb	$SEL_UPL,TF_CS(%rsp)
	jz	1f
	MDS_LEAVE
	SVS_LEAVE_ALTSTACK
	IBRS_LEAVE
	swapgs
1:

	INTR_RESTORE_GPRS
	addq	$TF_REGSIZE+16,%rsp
	iretq
#endif /* XENPV */
IDTVEC_END(trap08)

IDTVEC(trap09)		/* Coprocessor segment overrun (legacy x87) */
	ZTRAP(T_FPOPFLT)
IDTVEC_END(trap09)

IDTVEC(trap10)		/* #TS - Invalid TSS */
	TRAP(T_TSSFLT)
IDTVEC_END(trap10)

#ifdef XENPV
/*
 * I don't believe XEN generates in-kernel traps for the
 * equivalent of iret, if it does this code would be needed
 * in order to copy the user segment registers into the fault frame.
 */
#define kernuser_reenter alltraps
#endif /* XENPV */

IDTVEC(trap11)		/* #NP - Segment not present */
	TRAP_NJ(T_SEGNPFLT)
	jmp	kernuser_reenter
IDTVEC_END(trap11)

IDTVEC(trap12)		/* #SS - Stack fault */
	TRAP_NJ(T_STKFLT)
	jmp	kernuser_reenter
IDTVEC_END(trap12)

IDTVEC(trap13)		/* #GP - General protection */
	TRAP_NJ(T_PROTFLT)
	jmp	kernuser_reenter
IDTVEC_END(trap13)

IDTVEC(trap14)		/* #PF - Page fault */
	TRAP(T_PAGEFLT)
IDTVEC_END(trap14)

IDTVEC(trap15)		/* XXX ??? */
	ZTRAP_NJ(T_ASTFLT)
	INTRENTRY
#ifdef DIAGNOSTIC
	movzbl	CPUVAR(ILEVEL),%ebx
#endif
	jmp	.Lalltraps_checkusr
IDTVEC_END(trap15)

IDTVEC(trap16)		/* #MF - x87 floating-point exception */
	ZTRAP_NJ(T_ARITHTRAP)
.Ldo_fputrap:
	INTRENTRY
#ifdef XENPV
	/* traps are called with interrupts enabled, and we may have been
	 * interrupted just before the CLI in the trap macro.
	 * we have to check if a FPU reload is needed.
	 */
	movq    CPUVAR(CURLWP),%r14
	HANDLE_DEFERRED_FPU
#endif /* XENPV */
#ifdef DIAGNOSTIC
	movzbl	CPUVAR(ILEVEL),%ebx
#endif
	movq	%rsp,%rdi
	call	_C_LABEL(fputrap)
	jmp	.Lalltraps_checkusr
IDTVEC_END(trap16)

IDTVEC(trap17)		/* #AC - Alignment check */
	TRAP(T_ALIGNFLT)
IDTVEC_END(trap17)

IDTVEC(trap18)		/* #MC - Machine check */
	ZTRAP(T_MCA)
IDTVEC_END(trap18)

IDTVEC(trap19)		/* #XM - SIMD floating-point exception */
	ZTRAP_NJ(T_XMM)
	jmp	.Ldo_fputrap
IDTVEC_END(trap19)

IDTVEC(trap20)		/* #VE - Virtualization (Intel) */
IDTVEC(trap21)		/* #CP - Control protection */
IDTVEC(trap22)
IDTVEC(trap23)
IDTVEC(trap24)
IDTVEC(trap25)
IDTVEC(trap26)
IDTVEC(trap27)
IDTVEC(trap28)		/* #HV - Hypervisor injection (AMD) */
IDTVEC(trap29)		/* #VC - VMM communication (AMD) */
IDTVEC(trap30)		/* #SX - Security (AMD) */
IDTVEC(trap31)
	/* 20 - 31 reserved for future exp */
	ZTRAP(T_RESERVED)
IDTVEC_END(trap20)
IDTVEC_END(trap21)
IDTVEC_END(trap22)
IDTVEC_END(trap23)
IDTVEC_END(trap24)
IDTVEC_END(trap25)
IDTVEC_END(trap26)
IDTVEC_END(trap27)
IDTVEC_END(trap28)
IDTVEC_END(trap29)
IDTVEC_END(trap30)
IDTVEC_END(trap31)

IDTVEC(intrspurious)
	ZTRAP_NJ(T_ASTFLT)
	INTRENTRY
#ifdef DIAGNOSTIC
	movzbl	CPUVAR(ILEVEL),%ebx
#endif
	jmp	.Lalltraps_checkusr
IDTVEC_END(intrspurious)

#ifndef kernuser_reenter
/*
 * We need to worry about traps in kernel mode while the kernel %gs isn't
 * loaded. When such traps happen, we have CPL=0 and %gs=userland, and we
 * must perform an additional swapgs to get %gs=kernel.
 */

#define TF_SMALL(val, reg)		(val - TF_REGSIZE)(reg)
#define TF_SMALL_REGPUSHED(val, reg)	(val - (TF_REGSIZE - 8))(reg)

/*
 * It is possible that we received a trap in kernel mode, but with the user
 * context loaded. There are five cases where this can happen:
 *
 *  o Execution of IRETQ.
 *  o Reload of ES.
 *  o Reload of DS.
 *  o Reload of FS.
 *  o Reload of GS.
 *
 * When this happens, the kernel is re-entered in kernel mode, but the
 * previous context is in kernel mode too.
 *
 * We have two iret frames in the stack. In the first one, we also pushed
 * 'trapno' and 'err'. The 'rsp' field points to the outer iret frame:
 *
 * +---------------------------------------------------+
 * | trapno | err | rip | cs=ring0 | rflags | rsp | ss |
 * +-------------------------------------------|-------+
 *                                             |
 *           +---------------------------------+
 *           |
 *           |    +------------------------------------+
 *           +--> | rip | cs=ring3 | rflags | rsp | ss |
 *                +------------------------------------+
 *
 * We perform a three-step procedure:
 *
 *  o We update RSP to point to the outer frame. This outer frame is in the
 *    same stack as the current frame, and likely just after the current
 *    frame.
 *
 *  o We push, in this outer frame, the 'err' and 'trapno' fields of the
 *    CURRENT frame.
 *
 *  o We do a normal INTRENTRY. Now that RSP points to the outer frame,
 *    everything behaves as if we had received a trap from the outer frame,
 *    that is to say, from userland directly.
 *
 * Finally, we jump to 'calltrap' and handle the trap smoothly.
 *
 * Two notes regarding SVS:
 *
 *  o With SVS, we will receive the trap while the user page tables are
 *    loaded. That's not a problem, we don't touch anything unmapped here.
 *
 *  o With SVS, when the user page tables are loaded, the stack is really
 *    small, and can contain only one trapframe structure. Therefore, in
 *    intrfastexit, we must save the GPRs and pop their part of the stack
 *    right away. If we weren't doing that, and the reload of ES faulted for
 *    example, then the CPU would try to push an iret frame on the current
 *    stack (nested), and would double-fault because it touches the redzone
 *    below the stack (see the documentation in x86/x86/svs.c). By popping
 *    the GPR part of the stack, we leave enough stack for the CPU to push
 *    an iret frame, and for us to push one 8-byte register (%rdi) too.
 */
	_ALIGN_TEXT
LABEL(kernuser_reenter)
	testb	$SEL_UPL,TF_SMALL(TF_CS, %rsp)
	jz	.Lkernelmode

.Lnormal_entry:
	INTRENTRY
	sti
	jmp	calltrap

.Lkernelmode:
	/* We will clobber %rdi */
	pushq	%rdi

	/* Case 1: fault on iretq? */
	leaq	do_iret(%rip),%rdi
	cmpq	%rdi,TF_SMALL_REGPUSHED(TF_RIP, %rsp)
	jne	5f
	movq	TF_SMALL_REGPUSHED(TF_RSP, %rsp),%rdi	/* get %rsp */
	testb	$SEL_UPL,8(%rdi)	/* check %cs of outer iret frame */
	je	.Lnormal_entry		/* jump if iret was to kernel  */
	jmp	.Lkernelmode_but_user	/* to user - must restore %gs */
5:

	/* Case 2: move to %es? */
	leaq	do_mov_es(%rip),%rdi
	cmpq	%rdi,TF_SMALL_REGPUSHED(TF_RIP, %rsp)
	je	.Lkernelmode_but_user

	/* Case 3: move to %ds? */
	leaq	do_mov_ds(%rip),%rdi
	cmpq	%rdi,TF_SMALL_REGPUSHED(TF_RIP, %rsp)
	je	.Lkernelmode_but_user

	/* Case 4: move to %fs? */
	leaq	do_mov_fs(%rip),%rdi
	cmpq	%rdi,TF_SMALL_REGPUSHED(TF_RIP, %rsp)
	je	.Lkernelmode_but_user

	/* Case 5: move to %gs? */
	leaq	do_mov_gs(%rip),%rdi
	cmpq	%rdi,TF_SMALL_REGPUSHED(TF_RIP, %rsp)
	je	.Lkernelmode_but_user

	/* None of the above cases: normal kernel fault */
	popq	%rdi
	jmp	.Lnormal_entry

.Lkernelmode_but_user:
	/*
	 * Here we have %rdi pushed on the stack, hence 8+.
	 */
	movq	%rsp,%rdi
	movq	TF_SMALL_REGPUSHED(TF_RSP, %rsp),%rsp

	/* Push tf_err and tf_trapno */
	pushq	8+8(%rdi)	/* 8+8(%rdi) = current TF_ERR */
	pushq	8+0(%rdi)	/* 8+0(%rdi) = current TF_TRAPNO */

	/* Restore %rdi */
	movq	(%rdi),%rdi

	jmp	.Lnormal_entry
END(kernuser_reenter)
#endif

	TEXT_USER_END

/*
 * All traps go through here. Call the generic trap handler, and
 * check for ASTs afterwards.
 */
ENTRY(alltraps)
	INTRENTRY
.Lalltraps_noentry:
	STI(si)

calltrap:
#ifdef DIAGNOSTIC
	movzbl	CPUVAR(ILEVEL),%ebx
#endif
	movq	%rsp,%rdi
	incq	CPUVAR(NTRAP)
	call	_C_LABEL(trap)

.Lalltraps_checkusr:
	testb	$SEL_RPL,TF_CS(%rsp)
	jz	6f

.Lalltraps_checkast:
	movq	CPUVAR(CURLWP),%r14
	/* Check for ASTs on exit to user mode. */
	CLI2(si, rdi)
	CHECK_ASTPENDING(%r14)
	je	3f
	CLEAR_ASTPENDING(%r14)
	STI(si)
	movl	$T_ASTFLT,TF_TRAPNO(%rsp)
	movq	%rsp,%rdi
	incq	CPUVAR(NTRAP)
	KMSAN_INIT_ARG(8)
	call	_C_LABEL(trap)
	jmp	.Lalltraps_checkast	/* re-check ASTs */
3:	CHECK_DEFERRED_SWITCH
	jnz	9f
	HANDLE_DEFERRED_FPU

6:
#ifdef DIAGNOSTIC
	cmpb	CPUVAR(ILEVEL),%bl
	jne	.Lspl_error
#endif
	INTRFASTEXIT

9:	STI(si)
	call	_C_LABEL(do_pmap_load)
	jmp	.Lalltraps_checkast	/* re-check ASTs */

#ifdef DIAGNOSTIC
.Lspl_error:
	STI(si)
	movabsq	$4f,%rdi
	movzbl	CPUVAR(ILEVEL),%esi
	call	_C_LABEL(panic)
4:	.asciz	"spl not lowered on trap exit, ilevel=%x"
#endif
END(alltraps)

#ifdef KDTRACE_HOOKS
	.bss
	.globl	dtrace_invop_jump_addr
	.align	8
	.type	dtrace_invop_jump_addr, @object
	.size	dtrace_invop_jump_addr, 8
dtrace_invop_jump_addr:
	.zero	8
	.globl	dtrace_invop_calltrap_addr
	.align	8
	.type	dtrace_invop_calltrap_addr, @object
	.size	dtrace_invop_calltrap_addr, 8
dtrace_invop_calltrap_addr:
	.zero	8
#endif

	.section .rodata

LABEL(x86_exceptions)
	.quad	_C_LABEL(Xtrap00), _C_LABEL(Xtrap01)
	.quad	_C_LABEL(Xtrap02), _C_LABEL(Xtrap03)
	.quad	_C_LABEL(Xtrap04), _C_LABEL(Xtrap05)
	.quad	_C_LABEL(Xtrap06), _C_LABEL(Xtrap07)
	.quad	_C_LABEL(Xtrap08), _C_LABEL(Xtrap09)
	.quad	_C_LABEL(Xtrap10), _C_LABEL(Xtrap11)
	.quad	_C_LABEL(Xtrap12), _C_LABEL(Xtrap13)
	.quad	_C_LABEL(Xtrap14), _C_LABEL(Xtrap15)
	.quad	_C_LABEL(Xtrap16), _C_LABEL(Xtrap17)
	.quad	_C_LABEL(Xtrap18), _C_LABEL(Xtrap19)
	.quad	_C_LABEL(Xtrap20), _C_LABEL(Xtrap21)
	.quad	_C_LABEL(Xtrap22), _C_LABEL(Xtrap23)
	.quad	_C_LABEL(Xtrap24), _C_LABEL(Xtrap25)
	.quad	_C_LABEL(Xtrap26), _C_LABEL(Xtrap27)
	.quad	_C_LABEL(Xtrap28), _C_LABEL(Xtrap29)
	.quad	_C_LABEL(Xtrap30), _C_LABEL(Xtrap31)
END(x86_exceptions)

