/* x86_64 signal return trampoline for MINIX.
 *
 * On x86_64, the kernel pushes the sigcontext pointer (scp) onto the user
 * stack just below the trampoline return address before entering the signal
 * handler.  When the handler executes ret, execution arrives here with:
 *   (%rsp) = scp pointer   (placed by kernel above sf_sc)
 * Load it into %rdi (SysV AMD64 first-arg register) and tail-call sigreturn.
 */
#include <machine/asm.h>

ENTRY(__sigreturn)
	movq	(%rsp), %rdi		/* scp pointer left by kernel */
	/*
	 * The handler was entered with %rsp%16==8 (kernel-aligned frame) and
	 * its `ret` added 8, so we arrive here with %rsp 16-byte aligned.  A C
	 * function expects %rsp%16==8 at entry; realign before tail-calling
	 * sigreturn, otherwise its stack-allocated message (zeroed with movaps)
	 * and the sigprocmask() it runs fault (#GP -> SIGSEGV) on the misaligned
	 * stack.  Affects every program whose signal handler returns.
	 */
	subq	$8, %rsp
	jmp	PIC_PLT(_C_LABEL(sigreturn))
