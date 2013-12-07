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

	mfcr		%r0
	stw		%r0, 128(%r3) /* CR */
	mflr		%r0
	stw		%r0, 132(%r3) /* LR */
	stw		%r0, 144(%r3) /* LR */
	mfctr		%r0
	stw		%r0, 136(%r3) /* CTR */
	mfxer		%r0
	stw		%r0, 140(%r3) /*  XER */

	blr

	.hidden _ZNK7_Unwind15Registers_ppc326jumptoEv
ENTRY(_ZNK7_Unwind15Registers_ppc326jumptoEv)
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

	lwz		%r0, 128(%r3) /* CR */
	mtcr		%r0
	lwz		%r0, 132(%r3) /* LR */
	mtlr		%r0
	lwz		%r0, 136(%r3) /* CTR */
	mtctr		%r0
	lwz		%r0, 140(%r3) /*  XER */
	mtxer		%r0
	lwz		%r0, 144(%r3) /* SRR0 ? */
	mtctr		%r0

	lwz		%r0, 0(%r3)   /* do r0 now */
	lwz		%r1,4(%r3)    /* do sp now */
	lwz		%r3,12(%r3)   /* do r3 last */
	bctr
#endif
