//===------------------------- unwind_registers.S -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//
// Abstracts accessing local vs remote address spaces.
//
//===----------------------------------------------------------------------===//
#include <machine/asm.h>

#ifdef __i386__
	.hidden _ZN7_Unwind13Registers_x86C1Ev
ENTRY(_ZN7_Unwind13Registers_x86C1Ev)
	pushl	%eax
	movl	8(%esp), %eax	/* Load this */
	/* Save all registers except EAX, EIP and ESP */
	/* Skip ECX */
	/* Skip EDX */
	movl	%ebx, 12(%eax)
	movl	%ebp, 20(%eax)
	movl	%esi, 24(%eax)
	movl	%edi, 28(%eax)

	leal	8(%esp), %edx	/* Compute ESP from the call site */
	movl	%edx, 16(%eax)	/* ...and store it as ESP */
	movl	4(%esp), %edx	/* Load return address */
	movl	%edx, 32(%eax)	/* ...and store it as EIP */
	popl	%edx		/* Take old EAX from stack */
	movl	%edx, 0(%eax)	/* ...and store it */	// XXX skip
	ret

	.hidden _ZNK7_Unwind13Registers_x866jumptoEv
ENTRY(_ZNK7_Unwind13Registers_x866jumptoEv)
	movl	4(%esp), %eax	/* Load this */
	movl	16(%eax), %edx	/* Load new stack pointer */
	subl	$4, %edx	/* Reserve space on new stack for EIP */
	movl	32(%eax), %ebx	/* Load new EIP */
	movl	%ebx, 0(%edx)	/* ...and save it on the new stack */
	pushl	%edx		/* Save new stack pointer on old stack */
	/* New stack is prepared, now restore all registers except ESP */
	/* EAX is the index register and must be restored last */
	movl	4(%eax), %ecx
	movl	8(%eax), %edx
	movl	12(%eax), %ebx
	movl	20(%eax), %ebp
	/* 16 is ESP */
	movl	24(%eax), %esi
	movl	28(%eax), %edi
	movl	0(%eax), %eax
	/* Now load new stack pointer pushed on the old stack earlier */
	popl	%esp
	/* Return address is already on the new stack. */
	ret
#endif

#ifdef __x86_64
	.hidden _ZN7_Unwind16Registers_x86_64C1Ev
ENTRY(_ZN7_Unwind16Registers_x86_64C1Ev)
	/* RDI == this */
	/* Skip RAX */
	/* Skip RDX */
	/* Skip RCX */
	movq	%rbx, 24(%rdi)
	/* Skip RSI */
	/* Skip RDI */
	movq	%rbp, 48(%rdi)
	leaq	8(%rsp), %rax
	movq	%rax, 56(%rdi)
	/* Skip R8 */
	/* Skip R9 */
	/* Skip R10 */
	/* Skip R11 */
	movq	%r12, 96(%rdi)
	movq	%r13, 104(%rdi)
	movq	%r14, 112(%rdi)
	movq	%r15, 120(%rdi)
	movq	(%rsp), %rax
	movq	%rax, 128(%rdi)
	ret

	.hidden _ZNK7_Unwind16Registers_x86_646jumptoEv
ENTRY(_ZNK7_Unwind16Registers_x86_646jumptoEv)
	/* RDI == this */
	movq	56(%rdi), %rax
	subq	$8, %rax	/* Reserve space on new stack for RIP */
	movq	128(%rdi), %rbx	/* Load new RIP */
	movq	%rbx, 0(%rax)	/* ...and save it on the new stack */
	pushq	%rax		/* Save new stack pointer on old stack */
	/* New stack is prepared, now restore all registers */
	movq	0(%rdi), %rax
	movq	8(%rdi), %rdx
	movq	16(%rdi), %rcx
	movq	24(%rdi), %rbx
	movq	32(%rdi), %rsi
	/* RDI restored later as it is still used as index register */
	movq	48(%rdi), %rbp
	/* RSP is restored later */
	movq	64(%rdi), %r8
	movq	72(%rdi), %r9
	movq	80(%rdi), %r10
	movq	88(%rdi), %r11
	movq	96(%rdi), %r12
	movq	104(%rdi), %r13
	movq	112(%rdi), %r14
	movq	120(%rdi), %r15
	movq	40(%rdi), %rdi
	/* Now load new stack pointer pushed on the old stack earlier */
	popq	%rsp
	/* Return address is already on the new stack. */
	ret
#endif

#ifdef __powerpc__
	.hidden _ZN7_Unwind15Registers_ppc32C1Ev
ENTRY(_ZN7_Unwind15Registers_ppc32C1Ev)
	/* TODO: skip non-callee-safe registers */
	stw		 %r0,  0(%r3)
	stw		 %r1,  4(%r3)
	stw		 %r2,  8(%r3)
	stw		 %r3, 12(%r3)
	stw		 %r4, 16(%r3)
	stw		 %r5, 20(%r3)
	stw		 %r6, 24(%r3)
	stw		 %r7, 28(%r3)
	stw		 %r8, 32(%r3)
	stw		 %r9, 36(%r3)
	stw		%r10, 40(%r3)
	stw		%r11, 44(%r3)
	stw		%r12, 48(%r3)
	stw		%r13, 52(%r3)
	stw		%r14, 56(%r3)
	stw		%r15, 60(%r3)
	stw		%r16, 64(%r3)
	stw		%r17, 68(%r3)
	stw		%r18, 72(%r3)
	stw		%r19, 76(%r3)
	stw		%r20, 80(%r3)
	stw		%r21, 84(%r3)
	stw		%r22, 88(%r3)
	stw		%r23, 92(%r3)
	stw		%r24, 96(%r3)
	stw		%r25,100(%r3)
	stw		%r26,104(%r3)
	stw		%r27,108(%r3)
	stw		%r28,112(%r3)
	stw		%r29,116(%r3)
	stw		%r30,120(%r3)
	stw		%r31,124(%r3)
	mflr		%r0
	stw		%r0, 136(%r3) /* SRR0 */
	mfcr		%r0
	stw		%r0, 132(%r3) /* CR */

	stfd		 %f0, 144(%r3)
	stfd		 %f1, 152(%r3)
	stfd		 %f2, 160(%r3)
	stfd		 %f3, 168(%r3)
	stfd		 %f4, 176(%r3)
	stfd		 %f5, 184(%r3)
	stfd		 %f6, 192(%r3)
	stfd		 %f7, 200(%r3)
	stfd		 %f8, 208(%r3)
	stfd		 %f9, 216(%r3)
	stfd		%f10, 224(%r3)
	stfd		%f11, 232(%r3)
	stfd		%f12, 240(%r3)
	stfd		%f13, 248(%r3)
	stfd		%f14, 256(%r3)
	stfd		%f15, 264(%r3)
	stfd		%f16, 272(%r3)
	stfd		%f17, 280(%r3)
	stfd		%f18, 288(%r3)
	stfd		%f19, 296(%r3)
	stfd		%f20, 304(%r3)
	stfd		%f21, 312(%r3)
	stfd		%f22, 320(%r3)
	stfd		%f23, 328(%r3)
	stfd		%f24, 336(%r3)
	stfd		%f25, 344(%r3)
	stfd		%f26, 352(%r3)
	stfd		%f27, 360(%r3)
	stfd		%f28, 368(%r3)
	stfd		%f29, 376(%r3)
	stfd		%f30, 384(%r3)
	stfd		%f31, 392(%r3)

	/* LR is undefined */
	blr

	.hidden _ZNK7_Unwind15Registers_ppc326jumptoEv
ENTRY(_ZNK7_Unwind15Registers_ppc326jumptoEv)
	lfd		 %f0, 144(%r3)
	lfd		 %f1, 152(%r3)
	lfd		 %f2, 160(%r3)
	lfd		 %f3, 168(%r3)
	lfd		 %f4, 176(%r3)
	lfd		 %f5, 184(%r3)
	lfd		 %f6, 192(%r3)
	lfd		 %f7, 200(%r3)
	lfd		 %f8, 208(%r3)
	lfd		 %f9, 216(%r3)
	lfd		%f10, 224(%r3)
	lfd		%f11, 232(%r3)
	lfd		%f12, 240(%r3)
	lfd		%f13, 248(%r3)
	lfd		%f14, 256(%r3)
	lfd		%f15, 264(%r3)
	lfd		%f16, 272(%r3)
	lfd		%f17, 280(%r3)
	lfd		%f18, 288(%r3)
	lfd		%f19, 296(%r3)
	lfd		%f20, 304(%r3)
	lfd		%f21, 312(%r3)
	lfd		%f22, 320(%r3)
	lfd		%f23, 328(%r3)
	lfd		%f24, 336(%r3)
	lfd		%f25, 344(%r3)
	lfd		%f26, 352(%r3)
	lfd		%f27, 360(%r3)
	lfd		%f28, 368(%r3)
	lfd		%f29, 376(%r3)
	lfd		%f30, 384(%r3)
	lfd		%f31, 392(%r3)

	lwz		 %r2, 8(%r3)
	/* skip r3 for now */
	lwz		 %r4, 16(%r3)
	lwz		 %r5, 20(%r3)
	lwz		 %r6, 24(%r3)
	lwz		 %r7, 28(%r3)
	lwz		 %r8, 32(%r3)
	lwz		 %r9, 36(%r3)
	lwz		%r10, 40(%r3)
	lwz		%r11, 44(%r3)
	lwz		%r12, 48(%r3)
	lwz		%r13, 52(%r3)
	lwz		%r14, 56(%r3)
	lwz		%r15, 60(%r3)
	lwz		%r16, 64(%r3)
	lwz		%r17, 68(%r3)
	lwz		%r18, 72(%r3)
	lwz		%r19, 76(%r3)
	lwz		%r20, 80(%r3)
	lwz		%r21, 84(%r3)
	lwz		%r22, 88(%r3)
	lwz		%r23, 92(%r3)
	lwz		%r24, 96(%r3)
	lwz		%r25,100(%r3)
	lwz		%r26,104(%r3)
	lwz		%r27,108(%r3)
	lwz		%r28,112(%r3)
	lwz		%r29,116(%r3)
	lwz		%r30,120(%r3)
	lwz		%r31,124(%r3)

	lwz		%r0, 128(%r3) /* LR */
	mtlr		%r0
	lwz		%r0, 132(%r3) /* CR */
	mtcr		%r0
	lwz		%r0, 136(%r3) /* SRR0 */
	mtctr		%r0

	lwz		%r0,  0(%r3)   /* do r0 now */
	lwz		%r1,  4(%r3)   /* do sp now */
	lwz		%r3, 12(%r3)   /* do r3 last */
	bctr
#endif

#ifdef __aarch64__
	.hidden _ZN7_Unwind17Registers_aarch64C1Ev
ENTRY(_ZN7_Unwind17Registers_aarch64C1Ev)
	stp	x0, x1, [x0]
	add	x0, x0, #16
	stp	x2, x3, [x0], #16
	stp	x4, x5, [x0], #16
	stp	x6, x7, [x0], #16
	stp	x8, x9, [x0], #16
	stp	x10, x11, [x0], #16
	stp	x12, x13, [x0], #16
	stp	x14, x15, [x0], #16
	stp	x16, x17, [x0], #16
	stp	x18, x19, [x0], #16
	stp	x20, x22, [x0], #16
	stp	x22, x24, [x0], #16
	stp	x24, x26, [x0], #16
	stp	x26, x27, [x0], #16
	stp	x28, x29, [x0], #16
	mov	x1, sp
	stp	x30, x1,  [x0], #16

	add	x0, x0, #8
	str	xzr, [x0], #8

	stp	q0, q1, [x0], #64
	stp	q2, q3, [x0], #64
	stp	q4, q5, [x0], #64
	stp	q6, q7, [x0], #64
	stp	q8, q9, [x0], #64
	stp	q10, q11, [x0], #64
	stp	q12, q13, [x0], #64
	stp	q14, q15, [x0], #64
	stp	q16, q17, [x0], #64
	stp	q18, q19, [x0], #64
	stp	q20, q21, [x0], #64
	stp	q22, q23, [x0], #64
	stp	q24, q25, [x0], #64
	stp	q26, q27, [x0], #64
	stp	q28, q29, [x0], #64
	stp	q30, q31, [x0], #64

	ret
END(_ZN7_Unwind17Registers_aarch64C1Ev)

	.hidden _ZNK7_Unwind17Registers_aarch646jumptoEv
ENTRY(_ZNK7_Unwind17Registers_aarch646jumptoEv)
	ldp	x2, x3, [x0, #16]
	ldp	x4, x6, [x0, #32]
	ldp	x6, x7, [x0, #48]
	ldp	x8, x9, [x0, #64]
	ldp	x10, x11, [x0, #80]
	ldp	x12, x13, [x0, #96]
	ldp	x14, x16, [x0, #112]
	ldp	x16, x17, [x0, #128]
	ldp	x18, x19, [x0, #144]
	ldp	x20, x21, [x0, #160]
	ldp	x22, x23, [x0, #176]
	ldp	x24, x26, [x0, #192]
	ldp	x26, x27, [x0, #208]
	ldp	x28, x29, [x0, #224]
	ldp	x30, x1, [x0, #240]
	mov	sp, x1

	ldp	x0, x1, [x0, #0]
	ret
END(_ZNK7_Unwind17Registers_aarch646jumptoEv)
#endif /* __aarch64__ */

#if defined(__arm__)
	.fpu vfpv3
	.hidden _ZN7_Unwind15Registers_arm32C1Ev
ARM_ENTRY(_ZN7_Unwind15Registers_arm32C1Ev)
	stmia	r0, {r0-r14}
	str	lr, [r0, #60]	/* PC */
	mrs	r1, cpsr
	str	r1, [r0, #64]	/* CPSR */
	mov	r1, #0
	str	r1, [r0, #68]
	RET
END(_ZN7_Unwind15Registers_arm32C1Ev)

	.hidden _ZN7_Unwind15Registers_arm328lazyVFP1Ev
ARM_ENTRY(_ZN7_Unwind15Registers_arm328lazyVFP1Ev)
	add	r0, #72
	vstmia	r0, {d0-d15}
	RET
END(_ZN7_Unwind15Registers_arm328lazyVFP1Ev)

	.hidden _ZN7_Unwind15Registers_arm328lazyVFP3Ev
ARM_ENTRY(_ZN7_Unwind15Registers_arm328lazyVFP3Ev)
	add	r0, #200
	vstmia	r0, {d16-d31}
	RET
END(_ZN7_Unwind15Registers_arm328lazyVFP3Ev)

	.hidden _ZNK7_Unwind15Registers_arm326jumptoEv
ARM_ENTRY(_ZNK7_Unwind15Registers_arm326jumptoEv)
	ldrb	r1, [r0, #68]
	tst	r1, #1
	beq	.Lnovfp1
	add	r2, r0, #72
	vldmia	r2, {d0-d15}
.Lnovfp1:
	tst	r1, #2
	beq	.Lnovfp3
	add	r2, r0, #200
	vldmia	r2, {d16-d31}
.Lnovfp3:
	ldr	r1, [r0, #64]
	msr	cpsr_sxc, r1
	ldmia	r0, {r0-r15}
END(_ZNK7_Unwind15Registers_arm326jumptoEv)
#endif

#if defined(__vax__)
	.hidden _ZN7_Unwind13Registers_vaxC1Ev
ENTRY(_ZN7_Unwind13Registers_vaxC1Ev, R0)
	subl2	$4, %sp
	movl	4(%ap), %r0
	movl	 %r1,  4(%r0)
	movl	 %r2,  8(%r0)
	movl	 %r3, 12(%r0)
	movl	 %r4, 16(%r0)
	movl	 %r5, 20(%r0)
	movl	 %r6, 24(%r0)
	movl	 %r7, 28(%r0)
	movl	 %r8, 32(%r0)
	movl	 %r9, 36(%r0)
	movl	%r10, 40(%r0)
	movl	%r11, 44(%r0)
	movl	8(%fp), 48(%r0)
	movl	12(%fp), 52(%r0)
	addl3	$36, %sp, 56(%r0)
	/* Return PC */
	movl	16(%fp), 60(%r0)
	/* Load saved value of r0 as r1 */
	movl	20(%fp), 0(%r0)
	/* Saved PSW */
	movl	4(%fp), 64(%r0)
	ret
END(_ZN7_Unwind13Registers_vaxC1Ev)

	.hidden _ZNK7_Unwind13Registers_vax6jumptoEv
ENTRY(_ZNK7_Unwind13Registers_vax6jumptoEv, 0)
	subl2	$4, %sp
	movl	 4(%ap),  %r0
	movl	 4(%r0),  %r1
	movl	 8(%r0),  %r2
	movl	12(%r0),  %r3
	movl	16(%r0),  %r4
	movl	20(%r0),  %r5
	movl	24(%r0),  %r6
	movl	28(%r0),  %r7
	movl	32(%r0),  %r8
	movl	36(%r0),  %r9
	movl	40(%r0), %r10
	movl	44(%r0), %r11
	movl	48(%r0), %r12
	movl	52(%r0), %r13
	movl	56(%r0), %r14
	movl	60(%r0), -(%sp)
	movl	0(%r0), %r0
	/* XXX restore PSW */
	rsb
END(_ZNK7_Unwind13Registers_vax6jumptoEv)
#endif

#if defined(__m68k__)
ENTRY(_ZN7_Unwind14Registers_M68KC1Ev)
	move.l	4(%sp), %a0
	movem.l	%d0-%d7/%a0-%a7, (%a0)
	fmovem	%fp0-%fp7, 72(%a0)
	move.l	0(%sp), %a1
	move.l	%a1, 64(%a0)
	addq.l	#4, 60(%a0)
	rts
END(_ZN7_Unwind14Registers_M68KC1Ev)

ENTRY(_ZNK7_Unwind14Registers_M68K6jumptoEv)
	move.l	4(%sp), %a0
	subq.l	#4, 60(%a0)
	move.l	64(%a0), %a1
	move.l	60(%a0), %a2
	move.l	%a1, (%a2)
	fmovem	72(%a0), %fp0-%fp7
	movem.l	(%a0), %d0-%d7/%a0-%a7
	rts
END(_ZNK7_Unwind14Registers_M68K6jumptoEv)
#endif

#if defined(__sh3__)
	.hidden _ZN7_Unwind13Registers_SH3C1Ev
ENTRY(_ZN7_Unwind13Registers_SH3C1Ev)
	add	#64, r4
	mov.l	r8, @-r15
	sts.l	pr, @-r15
	mov.l	@r15+, r8
	mov.l	r8, @r4
	mov.l	@r15+, r8

	mov.l   r15, @-r4
	mov.l	r14, @-r4
	mov.l	r13, @-r4
	mov.l	r12, @-r4
	mov.l	r11, @-r4
	mov.l	r10, @-r4
	mov.l	r9, @-r4
	mov.l	r8, @-r4
	mov.l	r7, @-r4
	mov.l	r6, @-r4
	mov.l	r5, @-r4
	add	#-4, r4
	mov.l	r3, @-r4
	mov.l	r2, @-r4
	mov.l	r1, @-r4
	mov.l	r0, @-r4
	rts
	  mov.l	r4, @(16,r4)
SET_ENTRY_SIZE(_ZN7_Unwind13Registers_SH3C1Ev)

	.hidden _ZNK7_Unwind13Registers_SH36jumptoEv
ENTRY(_ZNK7_Unwind13Registers_SH36jumptoEv)
	mov	r4, r0
	add	#4, r0
	mov.l	@r0+, r1
	mov.l	@r0+, r2
	mov.l	@r0+, r3
	mov.l	@r0+, r4
	mov.l	@r0+, r5
	mov.l	@r0+, r6
	mov.l	@r0+, r7
	mov.l	@r0+, r8
	mov.l	@r0+, r9
	mov.l	@r0+, r10
	mov.l	@r0+, r11
	mov.l	@r0+, r12
	mov.l	@r0+, r13
	mov.l	@(12, r0), r14
	lds	r14, pr
	mov.l	@r0+, r14
	mov.l	@r0+, r15
	mov.l	@r0, r0
	jmp	@r0
	  nop
SET_ENTRY_SIZE(_ZNK7_Unwind13Registers_SH36jumptoEv)
#endif

#if defined(__sparc64__)
#include <machine/trap.h>
	.register %g2, #ignore
	.register %g3, #ignore
	.register %g6, #ignore
	.register %g7, #ignore
	.hidden _ZN7_Unwind17Registers_SPARC64C1Ev
ENTRY(_ZN7_Unwind17Registers_SPARC64C1Ev)
	t	ST_FLUSHWIN
	stx	%g0, [%o0 + 0]
	stx	%g1, [%o0 + 8]
	stx	%g2, [%o0 + 16]
	stx	%g3, [%o0 + 24]
	stx	%g4, [%o0 + 32]
	stx	%g5, [%o0 + 40]
	stx	%g6, [%o0 + 48]
	stx	%g7, [%o0 + 56]
	stx	%o0, [%o0 + 64]
	stx	%o1, [%o0 + 72]
	stx	%o2, [%o0 + 80]
	stx	%o3, [%o0 + 88]
	stx	%o4, [%o0 + 96]
	stx	%o5, [%o0 + 104]
	stx	%o6, [%o0 + 112]
	stx	%o7, [%o0 + 120]
	stx	%l0, [%o0 + 128]
	stx	%l1, [%o0 + 136]
	stx	%l2, [%o0 + 144]
	stx	%l3, [%o0 + 152]
	stx	%l4, [%o0 + 160]
	stx	%l5, [%o0 + 168]
	stx	%l6, [%o0 + 176]
	stx	%l7, [%o0 + 184]
	stx	%i0, [%o0 + 192]
	stx	%i1, [%o0 + 200]
	stx	%i2, [%o0 + 208]
	stx	%i3, [%o0 + 216]
	stx	%i4, [%o0 + 224]
	stx	%i5, [%o0 + 232]
	stx	%i6, [%o0 + 240]
	stx	%i7, [%o0 + 248]
	add	%o7, 8, %g1
	retl
	 stx	%g1, [%o0 + 256]
END(_ZN7_Unwind17Registers_SPARC64C1Ev)

	.hidden _ZNK7_Unwind17Registers_SPARC646jumptoEv
ENTRY(_ZNK7_Unwind17Registers_SPARC646jumptoEv)
	t	ST_FLUSHWIN
	ldx	[%o0 + 0], %g0
	ldx	[%o0 + 8], %g1
	ldx	[%o0 + 16], %g2
	ldx	[%o0 + 24], %g3
	ldx	[%o0 + 32], %g4
	ldx	[%o0 + 40], %g5
	ldx	[%o0 + 48], %g6
	ldx	[%o0 + 56], %g7
	ldx	[%o0 + 72], %o1
	ldx	[%o0 + 80], %o2
	ldx	[%o0 + 88], %o3
	ldx	[%o0 + 96], %o4
	ldx	[%o0 + 104], %o5
	ldx	[%o0 + 112], %g1
	sub	%g1, 2047, %o6
	ldx	[%o0 + 120], %o7
	ldx	[%o0 + 128], %l0
	ldx	[%o0 + 136], %l1
	ldx	[%o0 + 144], %l2
	ldx	[%o0 + 152], %l3
	ldx	[%o0 + 160], %l4
	ldx	[%o0 + 168], %l5
	ldx	[%o0 + 176], %l6
	ldx	[%o0 + 184], %l7
	ldx	[%o0 + 192], %i0
	ldx	[%o0 + 200], %i1
	ldx	[%o0 + 208], %i2
	ldx	[%o0 + 216], %i3
	ldx	[%o0 + 224], %i4
	ldx	[%o0 + 232], %i5
	ldx	[%o0 + 240], %i6
	ldx	[%o0 + 248], %i7
	ldx	[%o0 + 256], %g1
	jmpl	%g1, %g0
	  ldx	[%o0 + 64], %o0
END(_ZNK7_Unwind17Registers_SPARC646jumptoEv)
#elif defined(__sparc__)
#include <machine/trap.h>

	.hidden _ZN7_Unwind15Registers_SPARCC1Ev
ENTRY(_ZN7_Unwind15Registers_SPARCC1Ev)
	t	ST_FLUSHWIN
	st	%g0, [%o0 + 0]
	st	%g1, [%o0 + 4]
	st	%g2, [%o0 + 8]
	st	%g3, [%o0 + 12]
	st	%g4, [%o0 + 16]
	st	%g5, [%o0 + 20]
	st	%g6, [%o0 + 24]
	st	%g7, [%o0 + 28]
	st	%o0, [%o0 + 32]
	st	%o1, [%o0 + 36]
	st	%o2, [%o0 + 40]
	st	%o3, [%o0 + 44]
	st	%o4, [%o0 + 48]
	st	%o5, [%o0 + 52]
	st	%o6, [%o0 + 56]
	st	%o7, [%o0 + 60]
	st	%l0, [%o0 + 64]
	st	%l1, [%o0 + 68]
	st	%l2, [%o0 + 72]
	st	%l3, [%o0 + 76]
	st	%l4, [%o0 + 80]
	st	%l5, [%o0 + 84]
	st	%l6, [%o0 + 88]
	st	%l7, [%o0 + 92]
	st	%i0, [%o0 + 96]
	st	%i1, [%o0 + 100]
	st	%i2, [%o0 + 104]
	st	%i3, [%o0 + 108]
	st	%i4, [%o0 + 112]
	st	%i5, [%o0 + 116]
	st	%i6, [%o0 + 120]
	st	%i7, [%o0 + 124 ]
	add	%o7, 8, %g1
	retl
	 st	%g1, [%o0 + 128]
END(_ZN7_Unwind15Registers_SPARCC1Ev)

	.hidden _ZNK7_Unwind15Registers_SPARC6jumptoEv
ENTRY(_ZNK7_Unwind15Registers_SPARC6jumptoEv)
	t	ST_FLUSHWIN
	ld	[%o0 + 0], %g0
	ld	[%o0 + 4], %g1
	ld	[%o0 + 8], %g2
	ld	[%o0 + 12], %g3
	ld	[%o0 + 16], %g4
	ld	[%o0 + 20], %g5
	ld	[%o0 + 24], %g6
	ld	[%o0 + 28], %g7
	ld	[%o0 + 36], %o1
	ld	[%o0 + 40], %o2
	ld	[%o0 + 44], %o3
	ld	[%o0 + 48], %o4
	ld	[%o0 + 52], %o5
	ld	[%o0 + 56], %o6
	ld	[%o0 + 60], %o7
	ld	[%o0 + 64], %l0
	ld	[%o0 + 68], %l1
	ld	[%o0 + 72], %l2
	ld	[%o0 + 76], %l3
	ld	[%o0 + 80], %l4
	ld	[%o0 + 84], %l5
	ld	[%o0 + 88], %l6
	ld	[%o0 + 92], %l7
	ld	[%o0 + 96], %i0
	ld	[%o0 + 100], %i1
	ld	[%o0 + 104], %i2
	ld	[%o0 + 108], %i3
	ld	[%o0 + 112], %i4
	ld	[%o0 + 116], %i5
	ld	[%o0 + 120], %i6
	ld	[%o0 + 124], %i7
	ld	[%o0 + 128], %g1
	jmpl	%g1, %g0
	  ld	[%o0 + 32], %o0
END(_ZNK7_Unwind15Registers_SPARC6jumptoEv)
#endif

#if defined(__alpha__)
	.set nomacro
	.set noat
	.hidden _ZN7_Unwind15Registers_AlphaC1Ev
LEAF_NOPROFILE(_ZN7_Unwind15Registers_AlphaC1Ev, 1)
	stq $0, 0($16)
	stq $1, 8($16)
	stq $2, 16($16)
	stq $3, 24($16)
	stq $4, 32($16)
	stq $5, 40($16)
	stq $6, 48($16)
	stq $7, 56($16)
	stq $8, 64($16)
	stq $9, 72($16)
	stq $10, 80($16)
	stq $11, 88($16)
	stq $12, 96($16)
	stq $13, 104($16)
	stq $14, 112($16)
	stq $15, 120($16)
	stq $16, 128($16)
	stq $17, 136($16)
	stq $18, 144($16)
	stq $19, 152($16)
	stq $20, 160($16)
	stq $21, 168($16)
	stq $22, 176($16)
	stq $23, 184($16)
	stq $24, 192($16)
	stq $25, 200($16)
	stq $26, 208($16)
	stq $27, 216($16)
	stq $28, 224($16)
	stq $29, 232($16)
	stq $30, 240($16)
	stq $26, 248($16)

	stt $f0, 256($16)
	stt $f1, 264($16)
	stt $f2, 272($16)
	stt $f3, 280($16)
	stt $f4, 288($16)
	stt $f5, 296($16)
	stt $f6, 304($16)
	stt $f7, 312($16)
	stt $f8, 320($16)
	stt $f9, 328($16)
	stt $f10, 336($16)
	stt $f11, 344($16)
	stt $f12, 352($16)
	stt $f13, 360($16)
	stt $f14, 368($16)
	stt $f15, 376($16)
	stt $f16, 384($16)
	stt $f17, 392($16)
	stt $f18, 400($16)
	stt $f19, 408($16)
	stt $f20, 416($16)
	stt $f21, 424($16)
	stt $f22, 432($16)
	stt $f23, 440($16)
	stt $f24, 448($16)
	stt $f25, 456($16)
	stt $f26, 464($16)
	stt $f27, 472($16)
	stt $f28, 480($16)
	stt $f29, 488($16)
	stt $f30, 496($16)
	ret $31, ($26), 1
END(_ZN7_Unwind15Registers_AlphaC1Ev)

	.set nomacro
	.set noat
	.hidden _ZNK7_Unwind15Registers_Alpha6jumptoEv
LEAF_NOPROFILE(_ZNK7_Unwind15Registers_Alpha6jumptoEv, 1)
	ldq $0, 0($16)
	ldq $1, 8($16)
	ldq $2, 16($16)
	ldq $3, 24($16)
	ldq $4, 32($16)
	ldq $5, 40($16)
	ldq $6, 48($16)
	ldq $7, 56($16)
	ldq $8, 64($16)
	ldq $9, 72($16)
	ldq $10, 80($16)
	ldq $11, 88($16)
	ldq $12, 96($16)
	ldq $13, 104($16)
	ldq $14, 112($16)
	ldq $15, 120($16)
	ldq $17, 136($16)
	ldq $18, 144($16)
	ldq $19, 152($16)
	ldq $20, 160($16)
	ldq $21, 168($16)
	ldq $22, 176($16)
	ldq $23, 184($16)
	ldq $24, 192($16)
	ldq $25, 200($16)
	ldq $27, 216($16)
	ldq $28, 224($16)
	ldq $29, 232($16)
	ldq $30, 240($16)
	ldq $26, 248($16)

	ldt $f0, 256($16)
	ldt $f1, 264($16)
	ldt $f2, 272($16)
	ldt $f3, 280($16)
	ldt $f4, 288($16)
	ldt $f5, 296($16)
	ldt $f6, 304($16)
	ldt $f7, 312($16)
	ldt $f8, 320($16)
	ldt $f9, 328($16)
	ldt $f10, 336($16)
	ldt $f11, 344($16)
	ldt $f12, 352($16)
	ldt $f13, 360($16)
	ldt $f14, 368($16)
	ldt $f15, 376($16)
	ldt $f16, 384($16)
	ldt $f17, 392($16)
	ldt $f18, 400($16)
	ldt $f19, 408($16)
	ldt $f20, 416($16)
	ldt $f21, 424($16)
	ldt $f22, 432($16)
	ldt $f23, 440($16)
	ldt $f24, 448($16)
	ldt $f25, 456($16)
	ldt $f26, 464($16)
	ldt $f27, 472($16)
	ldt $f28, 480($16)
	ldt $f29, 488($16)
	ldt $f30, 496($16)
	ldq $16, 128($16)
	ret $31, ($26), 1
END(_ZNK7_Unwind15Registers_Alpha6jumptoEv)
#endif

#if defined(__mips_n64) || defined(__mips_n32)
	.set noat
LEAF(_ZN7_Unwind16Registers_MIPS64C1Ev)
#if 0
	FP_S	$f0, 256($4)
	FP_S	$f1, 264($4)
	FP_S	$f2, 272($4)
	FP_S	$f3, 280($4)
	FP_S	$f4, 288($4)
	FP_S	$f5, 296($4)
	FP_S	$f6, 304($4)
	FP_S	$f7, 312($4)
	FP_S	$f8, 320($4)
	FP_S	$f9, 328($4)
	FP_S	$f10, 336($4)
	FP_S	$f11, 344($4)
	FP_S	$f12, 352($4)
	FP_S	$f13, 360($4)
	FP_S	$f14, 368($4)
	FP_S	$f15, 376($4)
	FP_S	$f16, 384($4)
	FP_S	$f17, 392($4)
	FP_S	$f18, 400($4)
	FP_S	$f19, 408($4)
	FP_S	$f20, 416($4)
	FP_S	$f21, 424($4)
	FP_S	$f22, 432($4)
	FP_S	$f23, 440($4)
	FP_S	$f24, 448($4)
	FP_S	$f25, 456($4)
	FP_S	$f26, 464($4)
	FP_S	$f27, 472($4)
	FP_S	$f28, 480($4)
	FP_S	$f29, 488($4)
	FP_S	$f30, 496($4)
	FP_S	$f31, 504($4)
#endif
	sd	$31, 0($4)
	sd	$1, 4($4)
	sd	$2, 8($4)
	sd	$3, 12($4)
	sd	$4, 16($4)
	sd	$5, 20($4)
	sd	$6, 24($4)
	sd	$7, 28($4)
	sd	$8, 32($4)
	sd	$9, 36($4)
	sd	$10, 40($4)
	sd	$11, 44($4)
	sd	$12, 48($4)
	sd	$13, 52($4)
	sd	$14, 56($4)
	sd	$15, 60($4)
	sd	$16, 64($4)
	sd	$17, 68($4)
	sd	$18, 72($4)
	sd	$19, 76($4)
	sd	$20, 80($4)
	sd	$21, 84($4)
	sd	$22, 88($4)
	sd	$23, 92($4)
	sd	$24, 96($4)
	sd	$25, 100($4)
	sd	$26, 104($4)
	sd	$27, 108($4)
	sd	$28, 112($4)
	sd	$29, 116($4)
	sd	$30, 120($4)
	sd	$31, 124($4)
	jr	$31
	  nop
END(_ZN7_Unwind16Registers_MIPS64C1Ev)

LEAF(_ZNK7_Unwind16Registers_MIPS646jumptoEv)
#if 0
	FP_L	$f0, 256($4)
	FP_L	$f1, 264($4)
	FP_L	$f2, 272($4)
	FP_L	$f3, 280($4)
	FP_L	$f4, 288($4)
	FP_L	$f5, 296($4)
	FP_L	$f6, 304($4)
	FP_L	$f7, 312($4)
	FP_L	$f8, 320($4)
	FP_L	$f9, 328($4)
	FP_L	$f10, 336($4)
	FP_L	$f11, 344($4)
	FP_L	$f12, 352($4)
	FP_L	$f13, 360($4)
	FP_L	$f14, 368($4)
	FP_L	$f15, 376($4)
	FP_L	$f16, 384($4)
	FP_L	$f17, 392($4)
	FP_L	$f18, 400($4)
	FP_L	$f19, 408($4)
	FP_L	$f20, 416($4)
	FP_L	$f21, 424($4)
	FP_L	$f22, 432($4)
	FP_L	$f23, 440($4)
	FP_L	$f24, 448($4)
	FP_L	$f25, 456($4)
	FP_L	$f26, 464($4)
	FP_L	$f27, 472($4)
	FP_L	$f28, 480($4)
	FP_L	$f29, 488($4)
	FP_L	$f30, 496($4)
	FP_L	$f31, 504($4)
#endif

	ld	$31, 0($4)
	ld	$1, 8($4)
	ld	$2, 16($4)
	ld	$3, 24($4)
	ld	$5, 40($4)
	ld	$6, 48($4)
	ld	$7, 56($4)
	ld	$8, 64($4)
	ld	$9, 72($4)
	ld	$10, 80($4)
	ld	$11, 88($4)
	ld	$12, 96($4)
	ld	$13, 104($4)
	ld	$14, 112($4)
	ld	$15, 120($4)
	ld	$16, 128($4)
	ld	$17, 136($4)
	ld	$18, 144($4)
	ld	$19, 152($4)
	ld	$20, 160($4)
	ld	$21, 168($4)
	ld	$22, 176($4)
	ld	$23, 184($4)
	ld	$24, 192($4)
	ld	$25, 200($4)
	ld	$26, 208($4)
	ld	$27, 216($4)
	ld	$28, 224($4)
	ld	$29, 232($4)
	ld	$30, 240($4)
	 ld	$4, 32($4)
	jr	$31
	  nop
END(_ZNK7_Unwind16Registers_MIPS646jumptoEv)
#elif defined(__mips__)
	.set noat
LEAF(_ZN7_Unwind14Registers_MIPSC1Ev)
#if 0
#if __mips > 1
	FP_S	$f0, 128($4)
	FP_S	$f1, 136($4)
	FP_S	$f2, 144($4)
	FP_S	$f3, 152($4)
	FP_S	$f4, 160($4)
	FP_S	$f5, 168($4)
	FP_S	$f6, 176($4)
	FP_S	$f7, 184($4)
	FP_S	$f8, 192($4)
	FP_S	$f9, 200($4)
	FP_S	$f10, 208($4)
	FP_S	$f11, 216($4)
	FP_S	$f12, 224($4)
	FP_S	$f13, 232($4)
	FP_S	$f14, 240($4)
	FP_S	$f15, 248($4)
	FP_S	$f16, 256($4)
	FP_S	$f17, 264($4)
	FP_S	$f18, 272($4)
	FP_S	$f19, 280($4)
	FP_S	$f20, 288($4)
	FP_S	$f21, 296($4)
	FP_S	$f22, 304($4)
	FP_S	$f23, 312($4)
	FP_S	$f24, 320($4)
	FP_S	$f25, 328($4)
	FP_S	$f26, 336($4)
	FP_S	$f27, 344($4)
	FP_S	$f28, 352($4)
	FP_S	$f29, 360($4)
	FP_S	$f30, 368($4)
	FP_S	$f31, 376($4)
#endif
#endif
	sw	$31, 0($4)
	sw	$1, 4($4)
	sw	$2, 8($4)
	sw	$3, 12($4)
	sw	$4, 16($4)
	sw	$5, 20($4)
	sw	$6, 24($4)
	sw	$7, 28($4)
	sw	$8, 32($4)
	sw	$9, 36($4)
	sw	$10, 40($4)
	sw	$11, 44($4)
	sw	$12, 48($4)
	sw	$13, 52($4)
	sw	$14, 56($4)
	sw	$15, 60($4)
	sw	$16, 64($4)
	sw	$17, 68($4)
	sw	$18, 72($4)
	sw	$19, 76($4)
	sw	$20, 80($4)
	sw	$21, 84($4)
	sw	$22, 88($4)
	sw	$23, 92($4)
	sw	$24, 96($4)
	sw	$25, 100($4)
	sw	$26, 104($4)
	sw	$27, 108($4)
	sw	$28, 112($4)
	sw	$29, 116($4)
	sw	$30, 120($4)
	sw	$31, 124($4)
	jr	$31
	  nop
END(_ZN7_Unwind14Registers_MIPSC1Ev)

LEAF(_ZNK7_Unwind14Registers_MIPS6jumptoEv)
#if 0
#if __mips > 1
	FP_L	$f0, 128($4)
	FP_L	$f1, 136($4)
	FP_L	$f2, 144($4)
	FP_L	$f3, 152($4)
	FP_L	$f4, 160($4)
	FP_L	$f5, 168($4)
	FP_L	$f6, 176($4)
	FP_L	$f7, 184($4)
	FP_L	$f8, 192($4)
	FP_L	$f9, 200($4)
	FP_L	$f10, 208($4)
	FP_L	$f11, 216($4)
	FP_L	$f12, 224($4)
	FP_L	$f13, 232($4)
	FP_L	$f14, 240($4)
	FP_L	$f15, 248($4)
	FP_L	$f16, 256($4)
	FP_L	$f17, 264($4)
	FP_L	$f18, 272($4)
	FP_L	$f19, 280($4)
	FP_L	$f20, 288($4)
	FP_L	$f21, 296($4)
	FP_L	$f22, 304($4)
	FP_L	$f23, 312($4)
	FP_L	$f24, 320($4)
	FP_L	$f25, 328($4)
	FP_L	$f26, 336($4)
	FP_L	$f27, 344($4)
	FP_L	$f28, 352($4)
	FP_L	$f29, 360($4)
	FP_L	$f30, 368($4)
	FP_L	$f31, 376($4)
#endif
#endif

	lw	$31, 0($4)
	lw	$1, 4($4)
	lw	$2, 8($4)
	lw	$3, 12($4)
	lw	$5, 20($4)
	lw	$6, 24($4)
	lw	$7, 28($4)
	lw	$8, 32($4)
	lw	$9, 36($4)
	lw	$10, 40($4)
	lw	$11, 44($4)
	lw	$12, 48($4)
	lw	$13, 52($4)
	lw	$14, 56($4)
	lw	$15, 60($4)
	lw	$16, 64($4)
	lw	$17, 68($4)
	lw	$18, 72($4)
	lw	$19, 76($4)
	lw	$20, 80($4)
	lw	$21, 84($4)
	lw	$22, 88($4)
	lw	$23, 92($4)
	lw	$24, 96($4)
	lw	$25, 100($4)
	lw	$26, 104($4)
	lw	$27, 108($4)
	lw	$28, 112($4)
	lw	$29, 116($4)
	lw	$30, 120($4)
	 lw	$4, 16($4)
	jr	$31
	  nop
END(_ZNK7_Unwind14Registers_MIPS6jumptoEv)
#endif

#if defined(__hppa__)
LEAF_ENTRY_NOPROFILE(_ZN7_Unwind14Registers_HPPAC1Ev)
	stw %r2, 0(%r26)
	stw %r1, 4(%r26)
	stw %r2, 8(%r26)
	stw %r3, 12(%r26)
	stw %r4, 16(%r26)
	stw %r5, 20(%r26)
	stw %r6, 24(%r26)
	stw %r7, 28(%r26)
	stw %r8, 32(%r26)
	stw %r9, 36(%r26)
	stw %r10, 40(%r26)
	stw %r11, 44(%r26)
	stw %r12, 48(%r26)
	stw %r13, 52(%r26)
	stw %r14, 56(%r26)
	stw %r15, 60(%r26)
	stw %r16, 64(%r26)
	stw %r17, 68(%r26)
	stw %r18, 72(%r26)
	stw %r19, 76(%r26)
	stw %r20, 80(%r26)
	stw %r21, 84(%r26)
	stw %r22, 88(%r26)
	stw %r23, 92(%r26)
	stw %r24, 96(%r26)
	stw %r25, 100(%r26)
	stw %r26, 104(%r26)
	stw %r27, 108(%r26)
	stw %r28, 112(%r26)
	stw %r29, 116(%r26)
	stw %r30, 120(%r26)
	stw %r31, 124(%r26)
	ldi 128, %r19
	addl %r19, %r26, %r19
	fstds,ma %fr4, 8(%r19)
	fstds,ma %fr5, 8(%r19)
	fstds,ma %fr6, 8(%r19)
	fstds,ma %fr7, 8(%r19)
	fstds,ma %fr8, 8(%r19)
	fstds,ma %fr9, 8(%r19)
	fstds,ma %fr10, 8(%r19)
	fstds,ma %fr11, 8(%r19)
	fstds,ma %fr12, 8(%r19)
	fstds,ma %fr13, 8(%r19)
	fstds,ma %fr14, 8(%r19)
	fstds,ma %fr15, 8(%r19)
	fstds,ma %fr16, 8(%r19)
	fstds,ma %fr17, 8(%r19)
	fstds,ma %fr18, 8(%r19)
	fstds,ma %fr19, 8(%r19)
	fstds,ma %fr20, 8(%r19)
	fstds,ma %fr21, 8(%r19)
	fstds,ma %fr22, 8(%r19)
	fstds,ma %fr23, 8(%r19)
	fstds,ma %fr24, 8(%r19)
	fstds,ma %fr25, 8(%r19)
	fstds,ma %fr19, 8(%r19)
	fstds,ma %fr27, 8(%r19)
	fstds,ma %fr28, 8(%r19)
	fstds,ma %fr29, 8(%r19)
	fstds,ma %fr30, 8(%r19)
	fstds,ma %fr31, 8(%r19)
	ldw 76(%r26), %r19
	bv,n %r0(%r2)
EXIT(_ZN7_Unwind14Registers_HPPAC1Ev)

LEAF_ENTRY_NOPROFILE(_ZNK7_Unwind14Registers_HPPA6jumptoEv)
	ldi 128, %r19
	addl %r19, %r26, %r19
	fldds,ma 8(%r19), %fr4
	fldds,ma 8(%r19), %fr5
	fldds,ma 8(%r19), %fr6
	fldds,ma 8(%r19), %fr7
	fldds,ma 8(%r19), %fr8
	fldds,ma 8(%r19), %fr9
	fldds,ma 8(%r19), %fr10
	fldds,ma 8(%r19), %fr11
	fldds,ma 8(%r19), %fr12
	fldds,ma 8(%r19), %fr13
	fldds,ma 8(%r19), %fr14
	fldds,ma 8(%r19), %fr15
	fldds,ma 8(%r19), %fr16
	fldds,ma 8(%r19), %fr17
	fldds,ma 8(%r19), %fr18
	fldds,ma 8(%r19), %fr19
	fldds,ma 8(%r19), %fr20
	fldds,ma 8(%r19), %fr21
	fldds,ma 8(%r19), %fr22
	fldds,ma 8(%r19), %fr23
	fldds,ma 8(%r19), %fr24
	fldds,ma 8(%r19), %fr25
	fldds,ma 8(%r19), %fr26
	fldds,ma 8(%r19), %fr27
	fldds,ma 8(%r19), %fr28
	fldds,ma 8(%r19), %fr29
	fldds,ma 8(%r19), %fr30
	fldds,ma 8(%r19), %fr31
	ldw 0(%r26), %r2
	ldw 4(%r26), %r1
	ldw 12(%r26), %r3
	ldw 16(%r26), %r4
	ldw 20(%r26), %r5
	ldw 24(%r26), %r6
	ldw 28(%r26), %r7
	ldw 32(%r26), %r8
	ldw 36(%r26), %r9
	ldw 40(%r26), %r10
	ldw 44(%r26), %r11
	ldw 48(%r26), %r12
	ldw 52(%r26), %r13
	ldw 56(%r26), %r14
	ldw 60(%r26), %r15
	ldw 64(%r26), %r16
	ldw 68(%r26), %r17
	ldw 72(%r26), %r18
	ldw 76(%r26), %r19
	ldw 80(%r26), %r20
	ldw 84(%r26), %r21
	ldw 88(%r26), %r22
	ldw 92(%r26), %r23
	ldw 96(%r26), %r24
	ldw 100(%r26), %r25
	ldw 108(%r26), %r27
	ldw 112(%r26), %r28
	ldw 116(%r26), %r29
	ldw 120(%r26), %r30
	ldw 124(%r26), %r31
	ldw 104(%r26), %r26
	bv,n %r0(%r2)
EXIT(_ZNK7_Unwind14Registers_HPPA6jumptoEv)
#endif

#ifdef __or1k__
ENTRY_NP(_ZN7_Unwind14Registers_or1kC1Ev)
	l.sw	(0*3)(r3), r0
	l.sw	(1*3)(r3), r1
	l.sw	(2*3)(r3), r2
	l.sw	(3*3)(r3), r3
	l.sw	(4*3)(r3), r4
	l.sw	(5*3)(r3), r5
	l.sw	(6*3)(r3), r6
	l.sw	(7*3)(r3), r7
	l.sw	(8*3)(r3), r8
	l.sw	(9*3)(r3), r9
	l.sw	(10*3)(r3), r10
	l.sw	(11*3)(r3), r11
	l.sw	(12*3)(r3), r12
	l.sw	(13*3)(r3), r13
	l.sw	(14*3)(r3), r14
	l.sw	(15*3)(r3), r15
	l.sw	(16*3)(r3), r16
	l.sw	(17*3)(r3), r17
	l.sw	(18*3)(r3), r18
	l.sw	(19*3)(r3), r19
	l.sw	(20*3)(r3), r20
	l.sw	(21*3)(r3), r21
	l.sw	(22*3)(r3), r22
	l.sw	(23*3)(r3), r23
	l.sw	(24*3)(r3), r24
	l.sw	(25*3)(r3), r25
	l.sw	(26*3)(r3), r26
	l.sw	(27*3)(r3), r27
	l.sw	(28*3)(r3), r28
	l.sw	(29*3)(r3), r29
	l.sw	(30*3)(r3), r30
	l.sw	(31*3)(r3), r31
	l.mfspr	r4, r0, 20
	l.sw	(32*4)(r3), r4
	l.jr	lr
	l.nop
END(_ZN7_Unwind14Registers_or1kC1Ev)

ENTRY_NP(_ZNK7_Unwind14Registers_or1k6jumptoEv)
	l.lwz	r6, (32*4)(r3)
	l.mtspr	r0, r6, 20
	l.lwz	r0, (0*4)(r3)
	l.lwz	r1, (1*4)(r3)
	l.lwz	r2, (2*4)(r3)
	l.lwz	r4, (4*4)(r3)
	l.lwz	r5, (5*4)(r3)
	l.lwz	r6, (6*4)(r3)
	l.lwz	r7, (7*4)(r3)
	l.lwz	r8, (8*4)(r3)
	l.lwz	r9, (9*4)(r3)
	l.lwz	r10, (10*4)(r3)
	l.lwz	r11, (11*4)(r3)
	l.lwz	r12, (12*4)(r3)
	l.lwz	r13, (13*4)(r3)
	l.lwz	r14, (14*4)(r3)
	l.lwz	r15, (15*4)(r3)
	l.lwz	r16, (16*4)(r3)
	l.lwz	r17, (17*4)(r3)
	l.lwz	r18, (18*4)(r3)
	l.lwz	r19, (19*4)(r3)
	l.lwz	r20, (20*4)(r3)
	l.lwz	r21, (21*4)(r3)
	l.lwz	r22, (22*4)(r3)
	l.lwz	r23, (23*4)(r3)
	l.lwz	r24, (24*4)(r3)
	l.lwz	r25, (25*4)(r3)
	l.lwz	r26, (26*4)(r3)
	l.lwz	r27, (27*4)(r3)
	l.lwz	r28, (28*4)(r3)
	l.lwz	r29, (29*4)(r3)
	l.lwz	r30, (30*4)(r3)
	l.lwz	r31, (31*4)(r3)

	l.lwz	r3, (3*4)(r3)		/* return r3 */
	l.jr	lr
END(_ZNK7_Unwind14Registers_or1k6jumptoEv)

#endif
