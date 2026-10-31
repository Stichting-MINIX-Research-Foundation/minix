#include <machine/asm.h>
#include <ucontextoffsets.h>

	.globl	_C_LABEL(__errno)

/*
 * int getcontext(ucontext_t *ucp)
 *
 * ucp is in %rdi (SysV AMD64 ABI, arg 1).
 * When _UC_IGNFPU|_UC_IGNSIGM are both set we skip the PM/kernel round-trip
 * and only save callee-saved GP registers, RIP, and RSP in user space.
 */
ENTRY(getcontext)
ENTRY(_getcontext)
	/* Check null pointer */
	testq	%rdi, %rdi
	jnz	3f
	call	PIC_PLT(_C_LABEL(__errno))
	movl	$EFAULT, (%rax)
	xorl	%eax, %eax
	decl	%eax			/* return -1 */
	ret

3:	/* Check flags for fast path */
	movl	UC_FLAGS(%rdi), %eax
	andl	$[_UC_IGNFPU|_UC_IGNSIGM], %eax
	cmpl	$[_UC_IGNFPU|_UC_IGNSIGM], %eax
	jz	5f			/* both set → skip getuctx */

	/* Slow path: save signal mask and FPU state via PM/kernel */
	pushq	%rdi			/* preserve ucp across call */
	call	PIC_PLT(_C_LABEL(getuctx))	/* getuctx(ucp) */
	popq	%rdi			/* restore ucp */

5:	/* Save callee-saved GP registers, RIP, and RSP */
	movq	0(%rsp), %rax		/* return address */
	movq	%rax, PC(%rdi)		/* save RIP */
	leaq	8(%rsp), %rax		/* caller's RSP (after this call returns) */
	movq	%rax, SP(%rdi)		/* save RSP */
	movq	%rbp, BP(%rdi)		/* save RBP */
	movq	%rbx, BX(%rdi)		/* save RBX */
	movq	%r12, R12(%rdi)		/* save R12 */
	movq	%r13, R13(%rdi)		/* save R13 */
	movq	%r14, R14(%rdi)		/* save R14 */
	movq	%r15, R15(%rdi)		/* save R15 */
	movl	$MCF_MAGIC, MAGIC(%rdi)	/* mark context valid */
	xorl	%eax, %eax		/* return 0 */
	ret				/* pop return address, restore RSP, return 0 */


/*
 * int setcontext(const ucontext_t *ucp)
 *
 * ucp is in %rdi. Restores the context saved by getcontext, or the context
 * set up by makecontext.  Does not return on success.
 */
ENTRY(setcontext)
	/* Check null pointer */
	testq	%rdi, %rdi
	jnz	3f
	movl	$EFAULT, %edx
0:	pushq	%rdx			/* preserve errno value */
	call	PIC_PLT(_C_LABEL(__errno))
	popq	%rdx
	movl	%edx, (%rax)
	xorl	%eax, %eax
	decl	%eax			/* return -1 */
	ret

3:	/* Validate magic */
	cmpl	$MCF_MAGIC, MAGIC(%rdi)
	jz	4f
	movl	$EINVAL, %edx
	jmp	0b

4:	/* Check flags */
	movl	UC_FLAGS(%rdi), %eax
	andl	$[_UC_IGNFPU|_UC_IGNSIGM], %eax
	cmpl	$[_UC_IGNFPU|_UC_IGNSIGM], %eax
	jz	5f			/* both set → skip setuctx */

	/* Slow path: restore signal mask and FPU state via PM/kernel */
	pushq	%rdi			/* preserve ucp across call */
	call	PIC_PLT(_C_LABEL(setuctx))	/* setuctx(ucp) */
	popq	%rdi			/* restore ucp */

5:	/* Restore GP registers, RSP, then jump to saved RIP.
	 * Load PC into %r11 (caller-saved scratch) before clobbering %rdi. */
	movq	R15(%rdi), %r15		/* restore R15 */
	movq	R14(%rdi), %r14		/* restore R14 */
	movq	R13(%rdi), %r13		/* restore R13 */
	movq	R12(%rdi), %r12		/* restore R12 */
	movq	BX(%rdi), %rbx		/* restore RBX */
	movq	BP(%rdi), %rbp		/* restore RBP */
	movq	SP(%rdi), %rsp		/* restore RSP */
	movq	PC(%rdi), %r11		/* stash saved RIP in scratch */
	movq	SI(%rdi), %rsi		/* restore RSI (arg reg, used by makecontext) */
	movq	DX(%rdi), %rdx		/* restore RDX */
	movq	CX(%rdi), %rcx		/* restore RCX */
	movq	R8(%rdi), %r8		/* restore R8 */
	movq	R9(%rdi), %r9		/* restore R9 */
	movq	DI(%rdi), %rdi		/* restore RDI last (was ucp pointer) */
	xorl	%eax, %eax		/* return 0 */
	jmp	*%r11			/* jump to saved RIP */


/*
 * ctx_start - thread function entry trampoline for makecontext contexts.
 *
 * Entered via setcontext when makecontext has set up:
 *   RIP = ctx_start          (this function)
 *   R12 = func               (the thread function to call)
 *   R13 = ucp                (ucontext_t pointer for resumecontext)
 *   RDI, RSI, RDX, RCX, R8, R9 = first 6 arguments (if any)
 *   Stack = additional arguments (if argc > 6), 16-byte aligned RSP
 *
 * R12 and R13 are callee-saved, so they survive the call to func.
 */
ENTRY(ctx_start)
	call	*%r12			/* func(args): args already in RDI/RSI/... */
	movq	%r13, %rdi		/* pass ucp to resumecontext */
	call	PIC_PLT(_C_LABEL(resumecontext))
	/* never reached */
