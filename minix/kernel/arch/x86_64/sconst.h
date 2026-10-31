#ifndef __SCONST_H__
#define __SCONST_H__

#include "kernel/const.h"
#include "kernel/procoffsets.h"

/*
 * pushaq / popaq: push/pop all 15 GP registers (all except %rsp).
 * After pushaq the stack has 15 x 8 = 120 bytes of saved registers above the
 * original stack pointer.
 */
#define pushaq	\
	push	%rax	;\
	push	%rcx	;\
	push	%rdx	;\
	push	%rbx	;\
	push	%rbp	;\
	push	%rsi	;\
	push	%rdi	;\
	push	%r8	;\
	push	%r9	;\
	push	%r10	;\
	push	%r11	;\
	push	%r12	;\
	push	%r13	;\
	push	%r14	;\
	push	%r15

#define popaq	\
	pop	%r15	;\
	pop	%r14	;\
	pop	%r13	;\
	pop	%r12	;\
	pop	%r11	;\
	pop	%r10	;\
	pop	%r9	;\
	pop	%r8	;\
	pop	%rdi	;\
	pop	%rsi	;\
	pop	%rbp	;\
	pop	%rbx	;\
	pop	%rdx	;\
	pop	%rcx	;\
	pop	%rax

/*
 * Offset of RFLAGS from %rsp after pushaq in an in-kernel interrupt handler.
 * pushaq saves 15 regs x 8 = 120 bytes; RFLAGS is 2 x 8 = 16 bytes into the
 * CPU-pushed interrupt frame, so it sits at 120 + 16 = 136 bytes from %rsp.
 */
#define KERNEL_IRQ_RFLAGS_OFF	136

/*
 * Offset from RSP to the proc_ptr slot at the top of the kernel stack,
 * measured after a privilege-changing INT/exception.  The CPU pushes five
 * 8-byte words (SS, RSP, RFLAGS, CS, RIP) before transferring control, so
 * proc_ptr lives 40 bytes above the new RSP.
 */
#define CURR_PROC_PTR		40

/*
 * Test whether the interrupt came from kernel context.  If so, jump to label.
 * displ is the byte offset from RSP to the saved CS at the point of the check.
 * The kernel CS selector has the lower 3 bits (RPL/TI) cleared.
 */
#define TEST_INT_IN_KERNEL(displ, label)	\
	cmpq	$KERN_CS_SELECTOR, displ(%rsp)	;\
	je	label				;

/*
 * Save the CPU-pushed interrupt frame (RIP/CS/RFLAGS/RSP/SS) from the kernel
 * stack into the proc structure pointed to by pptr.  tmp is a scratch register.
 * displ is the extra bytes pushed before SAVE_PROCESS_CTX was invoked (e.g.
 * exception vector + error code = 16 bytes → displ=16).
 */
#define SAVE_TRAP_CTX(displ, pptr, tmp)			\
	movq	(0 + displ)(%rsp), tmp			;\
	movq	tmp, PCREG(pptr)			;\
	movq	(8 + displ)(%rsp), tmp			;\
	movq	tmp, CSREG(pptr)			;\
	movq	(16 + displ)(%rsp), tmp			;\
	movq	tmp, PSWREG(pptr)			;\
	movq	(24 + displ)(%rsp), tmp			;\
	movq	tmp, SPREG(pptr)

/*
 * Restore kernel data segment selectors.  CS is already correct; FS/GS are
 * not used by the kernel in flat 64-bit mode.
 */
#define RESTORE_KERNEL_SEGS				\
	mov	$KERN_DS_SELECTOR, %si			;\
	mov	%si, %ds				;\
	mov	%si, %es				;\
	movw	$0, %si				;\
	mov	%si, %gs				;\
	mov	%si, %fs				;

/*
 * Save/restore all GP registers except rbp (handled separately as the
 * proc_ptr scratch register in SAVE_PROCESS_CTX) and rsp (in the CPU frame).
 * Saving r12-r15 on every entry simplifies restore_user_context: both
 * same-process returns and context switches use the same restore path.
 */
#define SAVE_GP_REGS(pptr)				\
	mov	%rax, AXREG(pptr)			;\
	mov	%rcx, CXREG(pptr)			;\
	mov	%rdx, DXREG(pptr)			;\
	mov	%rbx, BXREG(pptr)			;\
	mov	%rsi, SIREG(pptr)			;\
	mov	%rdi, DIREG(pptr)			;\
	mov	%r8,  R8REG(pptr)			;\
	mov	%r9,  R9REG(pptr)			;\
	mov	%r10, R10REG(pptr)			;\
	mov	%r11, R11REG(pptr)			;\
	mov	%r12, R12REG(pptr)			;\
	mov	%r13, R13REG(pptr)			;\
	mov	%r14, R14REG(pptr)			;\
	mov	%r15, R15REG(pptr)			;

#define RESTORE_GP_REGS(pptr)				\
	mov	AXREG(pptr),  %rax			;\
	mov	CXREG(pptr),  %rcx			;\
	mov	DXREG(pptr),  %rdx			;\
	mov	BXREG(pptr),  %rbx			;\
	mov	SIREG(pptr),  %rsi			;\
	mov	DIREG(pptr),  %rdi			;\
	mov	R8REG(pptr),  %r8			;\
	mov	R9REG(pptr),  %r9			;\
	mov	R10REG(pptr), %r10			;\
	mov	R11REG(pptr), %r11			;\
	mov	R12REG(pptr), %r12			;\
	mov	R13REG(pptr), %r13			;\
	mov	R14REG(pptr), %r14			;\
	mov	R15REG(pptr), %r15			;

/*
 * Save the complete user context on a kernel entry caused by an interrupt or
 * IPC trap.  On entry, %rbp is pushed first to obtain a scratch register,
 * then the proc_ptr is loaded from the kernel stack top.
 *
 * displ = extra bytes already pushed before this macro (e.g. 16 for exception
 * entry with vector + error code; 0 for plain IPC/IRQ entry).
 *
 * After SAVE_PROCESS_CTX the proc_ptr is in %rbp and all GP registers plus
 * the interrupt frame have been copied into p_reg.
 */
#define SAVE_PROCESS_CTX(displ, trapcode)			\
									\
	cld /* set direction flag to a known state */			;\
									\
	push	%rbp						;\
									\
	movq	(CURR_PROC_PTR + 8 + displ)(%rsp), %rbp		;\
									\
	SAVE_GP_REGS(%rbp)					;\
	movl	$trapcode, P_KERN_TRAP_STYLE(%rbp)			;\
	pop	%rsi	/* recover the pushed %rbp and save it */	;\
	mov	%rsi, BPREG(%rbp)				;\
									\
	RESTORE_KERNEL_SEGS					;\
	SAVE_TRAP_CTX(displ, %rbp, %rsi)			;

/*
 * Clear the IF flag in a saved RFLAGS word stored in memory.  iret/popf will
 * load the new value later.
 */
#define CLEAR_IF(where)					\
	mov	where, %rax					;\
	andq	$~(1 << 9), %rax				;\
	mov	%rax, where					;

#endif /* __SCONST_H__ */
