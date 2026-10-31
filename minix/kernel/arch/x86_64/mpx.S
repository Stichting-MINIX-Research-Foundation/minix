/* This file is part of the lowest layer of the MINIX kernel.  (The other part
 * is "proc.c".)  The lowest layer does process switching and message handling.
 * Furthermore it contains the assembler startup code for Minix and the 64-bit
 * interrupt handlers.  It cooperates with the code in "start.c" to set up a
 * good environment for main().
 *
 * Kernel is entered either because of kernel-calls, ipc-calls, interrupts or
 * exceptions. TSS is set so that the kernel stack is loaded. The user context is
 * saved to the proc table and the handler of the event is called. Once the
 * handler is done, switch_to_user() function is called to pick a new process,
 * finish what needs to be done for the next process to run, sets its context
 * and switch to userspace.
 *
 * AMD64 IPC register convention (Approach B):
 *   %rdi = IPC call number  (r1 for do_ipc)
 *   %rsi = endpoint         (r2 for do_ipc)
 *   %rdx = message pointer  (r3 for do_ipc)
 *
 * ipc_entry_common reloads all three IPC arguments from p_reg (the saved proc
 * struct) so the reload is correct for both paths:
 *   - softint: SAVE_PROCESS_CTX clobbers %rsi via RESTORE_KERNEL_SEGS; p_reg.si
 *              still has the correct endpoint value saved before the clobber.
 *   - SYSCALL: per-CPU entry handler saves all GP regs to p_reg explicitly.
 *
 * SYSCALL fast path:
 *   CPU stores RIP -> %rcx, RFLAGS -> %r11; does NOT switch %rsp.
 *   Userspace stores its own RSP in %r10 before SYSCALL.
 *   Per-CPU entry points load the kernel stack from k_percpu_stacks[cpu].
 *   proc_ptr is stored at the top cell of each per-CPU kernel stack by
 *   arch_finish_switch_to_user() before returning to userspace.
 *
 * Register save policy:
 *   All GP registers (rax-rdi, r8-r15, rbp) are saved on every kernel entry.
 *   Saving r12-r15 unconditionally simplifies restore_user_context: both
 *   same-process returns and context switches use the same restore path without
 *   any bookkeeping about which registers were actually saved.
 */

#include "kernel/kernel.h"

#include <machine/vm.h>
#include "kernel/kernel.h"
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ipcconst.h>
#include <minix/com.h>
#include <machine/asm.h>
#include <machine/interrupt.h>
#include "archconst.h"
#include "kernel/const.h"
#include "kernel/proc.h"
#include "sconst.h"
#include "arch_proto.h" /* K_STACK_SIZE */

#ifdef CONFIG_SMP
#include "kernel/smp.h"
#endif

IMPORT(usermapped_offset)
IMPORT(copr_not_available_handler)
IMPORT(params_size)
IMPORT(params_offset)
IMPORT(switch_to_user)
IMPORT(multiboot_init)

.text

/*===========================================================================*/
/*			hardware interrupt handlers (IRQs)		     */
/*===========================================================================*/

/* Pass irq as first argument per AMD64 ABI (%edi), not on the stack as on
 * i386 — irq_handle(int) reads %rdi, so a pushed arg leaves it garbage. */
#define PIC_IRQ_HANDLER(irq)						\
	movl	$irq, %edi						;\
	call	_C_LABEL(irq_handle)					;

#define hwint_master(irq) \
	TEST_INT_IN_KERNEL(8, 0f)					;\
									\
	SAVE_PROCESS_CTX(0, KTS_INT_HARD)				;\
	movq	%rbp, %rdi	/* context_stop(proc_ptr) */		;\
	push	%rbp							;\
	movq	$0, %rbp						;\
	call	_C_LABEL(context_stop)					;\
	pop	%rbp							;\
	PIC_IRQ_HANDLER(irq)						;\
	movb	$END_OF_INT, %al					;\
	outb	$INT_CTL						;\
	jmp	_C_LABEL(switch_to_user)				;\
									\
0:									\
	pushaq								;\
	call	_C_LABEL(context_stop_idle)				;\
	PIC_IRQ_HANDLER(irq)						;\
	movb	$END_OF_INT, %al					;\
	outb	$INT_CTL						;\
	CLEAR_IF(KERNEL_IRQ_RFLAGS_OFF(%rsp))				;\
	popaq								;\
	iretq								;

ENTRY(hwint00)	hwint_master(0)
ENTRY(hwint01)	hwint_master(1)
ENTRY(hwint02)	hwint_master(2)
ENTRY(hwint03)	hwint_master(3)
ENTRY(hwint04)	hwint_master(4)
ENTRY(hwint05)	hwint_master(5)
ENTRY(hwint06)	hwint_master(6)
ENTRY(hwint07)	hwint_master(7)

#define hwint_slave(irq) \
	TEST_INT_IN_KERNEL(8, 0f)					;\
									\
	SAVE_PROCESS_CTX(0, KTS_INT_HARD)				;\
	movq	%rbp, %rdi	/* context_stop(proc_ptr) */		;\
	push	%rbp							;\
	movq	$0, %rbp						;\
	call	_C_LABEL(context_stop)					;\
	pop	%rbp							;\
	PIC_IRQ_HANDLER(irq)						;\
	movb	$END_OF_INT, %al					;\
	outb	$INT_CTL						;\
	outb	$INT2_CTL						;\
	jmp	_C_LABEL(switch_to_user)				;\
									\
0:									\
	pushaq								;\
	call	_C_LABEL(context_stop_idle)				;\
	PIC_IRQ_HANDLER(irq)						;\
	movb	$END_OF_INT, %al					;\
	outb	$INT_CTL						;\
	outb	$INT2_CTL						;\
	CLEAR_IF(KERNEL_IRQ_RFLAGS_OFF(%rsp))				;\
	popaq								;\
	iretq								;

ENTRY(hwint08)	hwint_slave(8)
ENTRY(hwint09)	hwint_slave(9)
ENTRY(hwint10)	hwint_slave(10)
ENTRY(hwint11)	hwint_slave(11)
ENTRY(hwint12)	hwint_slave(12)
ENTRY(hwint13)	hwint_slave(13)
ENTRY(hwint14)	hwint_slave(14)
ENTRY(hwint15)	hwint_slave(15)

/*===========================================================================*/
/*			SYSCALL fast path -- per-CPU entry points	     */
/*===========================================================================*/
/*
 * One entry point per logical CPU.  The LSTAR MSR for CPU N is set to
 * ipc_entry_syscall_cpuN at boot so each CPU knows its own stack without
 * needing swapgs or a per-CPU data segment.
 *
 * k_percpu_stacks[cpu] == tss[cpu].rsp0 == address of the top cell of the
 * per-CPU kernel stack.  arch_finish_switch_to_user() stores the current
 * proc_ptr at that address before returning to userspace, so:
 *   proc_ptr = *(k_percpu_stacks[cpu])
 *
 * Register state on AMD64 SYSCALL entry:
 *   %rcx  = saved user RIP   (CPU-saved)
 *   %r11  = saved user RFLAGS (CPU-saved)
 *   %rsp  = user RSP   (NOT switched by CPU)
 *   %rdi  = call_nr    (IPC arg 1, set by userspace)
 *   %rsi  = endpoint   (IPC arg 2, set by userspace)
 *   %rdx  = msg_ptr    (IPC arg 3, set by userspace)
 *   %r10  = user RSP   (pre-saved by userspace: mov %rsp, %r10 before syscall)
 *   %rbp  = user %rbp  (must be saved; not clobbered by SYSCALL)
 *
 * Saving user %rbp requires care because we immediately overwrite %rbp with
 * proc_ptr.  Strategy: push user %rbp to the kernel stack one slot below the
 * proc_ptr cell, load proc_ptr, save all GP regs, then pop the saved user
 * %rbp into %rax (scratch) and write to BPREG.
 */
#define ipc_entry_syscall_percpu(cpu)					\
ENTRY(ipc_entry_syscall_cpu ## cpu)					;\
	/* Switch to this CPU's kernel stack. */			\
	movq	k_percpu_stacks + 8*cpu, %rsp				;\
	/* Reserve one slot for user %rbp. */				\
	sub	$8, %rsp						;\
	movq	%rbp, (%rsp)	/* stash user %rbp */			;\
	/* Load proc_ptr from the cell above. */			\
	movq	8(%rsp), %rbp					;\
	movl	$KTS_SYSCALL, P_KERN_TRAP_STYLE(%rbp)			;\
	/* Save user RSP / return RIP / RFLAGS. */			\
	movq	%r10, SPREG(%rbp)	/* user RSP (from %r10) */	;\
	movq	%rcx, PCREG(%rbp)	/* return RIP (CPU-saved) */	;\
	movq	%r11, PSWREG(%rbp)	/* RFLAGS (CPU-saved) */	;\
	/* Save all GP registers. */					\
	movq	%rax, AXREG(%rbp)					;\
	movq	%rbx, BXREG(%rbp)					;\
	movq	%rcx, CXREG(%rbp)	/* return RIP */		;\
	movq	%rdx, DXREG(%rbp)					;\
	movq	%rsi, SIREG(%rbp)					;\
	movq	%rdi, DIREG(%rbp)					;\
	movq	%r8,  R8REG(%rbp)					;\
	movq	%r9,  R9REG(%rbp)					;\
	movq	%r10, R10REG(%rbp)	/* user RSP */			;\
	movq	%r11, R11REG(%rbp)	/* RFLAGS */			;\
	movq	%r12, R12REG(%rbp)					;\
	movq	%r13, R13REG(%rbp)					;\
	movq	%r14, R14REG(%rbp)					;\
	movq	%r15, R15REG(%rbp)					;\
	/* Pop stashed user %rbp and write to BPREG. */			\
	popq	%rax						;\
	movq	%rax, BPREG(%rbp)				;\
	/* SYSCALL leaves the user data segment selectors loaded; the INT	\
	 * entry path (SAVE_PROCESS_CTX) restores kernel segs here, so the	\
	 * per-CPU SYSCALL stub must do the same before running C code.	*/	\
	RESTORE_KERNEL_SEGS						;\
	jmp	ipc_entry_common

ipc_entry_syscall_percpu(0)
ipc_entry_syscall_percpu(1)
ipc_entry_syscall_percpu(2)
ipc_entry_syscall_percpu(3)
ipc_entry_syscall_percpu(4)
ipc_entry_syscall_percpu(5)
ipc_entry_syscall_percpu(6)
ipc_entry_syscall_percpu(7)

/*===========================================================================*/
/*			software-interrupt IPC entry (INT IPCVEC)	     */
/*===========================================================================*/

/* ipc_entry_sysenter -- SYSENTER is i386-only; redirect to the softint path. */
ENTRY(ipc_entry_sysenter)
	SAVE_PROCESS_CTX(0, KTS_INT_ORIG)
	jmp	ipc_entry_common

ENTRY(ipc_entry_softint_orig)
	SAVE_PROCESS_CTX(0, KTS_INT_ORIG)
	jmp	ipc_entry_common

ENTRY(ipc_entry_softint_um)
	SAVE_PROCESS_CTX(0, KTS_INT_UM)
	jmp	ipc_entry_common

/*
 * Common IPC dispatch.
 *
 * On entry %rbp = proc_ptr, and p_reg holds the complete saved user context.
 *
 * We always reload IPC args from p_reg rather than relying on live registers
 * because SAVE_PROCESS_CTX clobbers %rsi (RESTORE_KERNEL_SEGS uses it as a
 * scratch register).  Reloading from p_reg works for both paths.
 *
 * context_stop() is a C function that clobbers all caller-saved registers
 * including %rdi/%rsi/%rdx, so IPC args are reloaded after it returns.
 */
ENTRY(ipc_entry_common)
	push	%rbp			/* save proc_ptr at [rsp] */

	/* context_stop(proc_ptr) -- first arg in %rdi (AMD64 ABI) */
	movq	%rbp, %rdi
	movq	$0, %rbp		/* clear frame pointer for stack trace */
	call	_C_LABEL(context_stop)

	/* Reload proc_ptr (context_stop clobbered %rbp) */
	movq	(%rsp), %rbp

	/* Reload IPC args from p_reg -- valid for both softint and SYSCALL */
	movq	DIREG(%rbp), %rdi	/* call_nr  -> arg 1 */
	movq	SIREG(%rbp), %rsi	/* endpoint -> arg 2 */
	movq	DXREG(%rbp),  %rdx	/* msg_ptr  -> arg 3 */

	call	_C_LABEL(do_ipc)	/* do_ipc(rdi, rsi, rdx) */

	pop	%rsi			/* recover proc_ptr */
	movq	%rax, AXREG(%rsi)	/* save return value */

	jmp	_C_LABEL(switch_to_user)

/*===========================================================================*/
/*			kernel call entry (INT KERVEC)			     */
/*===========================================================================*/

ENTRY(kernel_call_entry_orig)
	SAVE_PROCESS_CTX(0, KTS_INT_ORIG)
	jmp	kernel_call_entry_common

ENTRY(kernel_call_entry_um)
	SAVE_PROCESS_CTX(0, KTS_INT_UM)
	jmp	kernel_call_entry_common

/*
 * kernel_call(message *m_user, struct proc *caller):
 *   m_user  = kernel call message ptr (userspace passed it in %rdi per AMD64
 *             ABI, so SAVE_PROCESS_CTX saved it to DIREG)
 *   caller  = proc_ptr (in %rbp after SAVE_PROCESS_CTX)
 */
ENTRY(kernel_call_entry_common)
	/* context_stop(proc_ptr) */
	movq	%rbp, %rdi
	push	%rbp
	movq	$0, %rbp
	call	_C_LABEL(context_stop)
	pop	%rbp

	/* kernel_call(m_user=%rdi, caller=%rsi) -- AMD64 ABI */
	movq	DIREG(%rbp), %rdi	/* m_user: msg ptr from saved %rdi (first C arg) */
	movq	%rbp, %rsi		/* caller: proc_ptr */
	call	_C_LABEL(kernel_call)

	jmp	_C_LABEL(switch_to_user)

/*===========================================================================*/
/*			context restore					     */
/*===========================================================================*/

/*
 * restore_user_context_syscall(struct proc *p)
 *
 * Return via SYSRETQ.  CPU restores RIP from %rcx and RFLAGS from %r11.
 * %rsp is NOT restored by SYSRETQ; the kernel sets it explicitly.
 *
 * AMD64 SysV ABI: proc_ptr in %rdi (first argument register).
 */
ENTRY(restore_user_context_syscall)
	movq	%rdi, %rbp		/* rbp = proc_ptr */

	/*
	 * Set user segment registers before RESTORE_GP_REGS so the %si
	 * clobber from movw is immediately overwritten when RESTORE_GP_REGS
	 * restores %rsi from SIREG.
	 */
	movw	$USER_DS_SELECTOR, %si
	movw	%si, %ds
	movw	%si, %es
	movw	%si, %fs
	movw	%si, %gs

	/* Restore all GP registers from p_reg. */
	RESTORE_GP_REGS(%rbp)

	/*
	 * Override %rcx/%r11 for SYSRETQ.  RESTORE_GP_REGS already wrote the
	 * saved CXREG/R11REG values; override here in case the kernel modified
	 * PCREG/PSWREG (e.g. signal delivery redirected the return address).
	 */
	movq	PCREG(%rbp),  %rcx	/* return RIP */
	movq	PSWREG(%rbp), %r11	/* return RFLAGS */

	/* Switch to user stack, restore %rbp last. */
	movq	SPREG(%rbp), %rsp
	movq	BPREG(%rbp), %rbp

	sysretq

/*
 * restore_user_context_sysenter -- not used on AMD64 (sysenter is i386-only).
 * Redirect to the INT path rather than crashing.
 */
ENTRY(restore_user_context_sysenter)
	jmp	_C_LABEL(restore_user_context_int)

/*
 * restore_user_context_int(struct proc *p)
 *
 * Return via IRETQ.  Reconstructs the long-mode interrupt frame on the
 * kernel stack.
 *
 * AMD64 SysV ABI: proc_ptr in %rdi (first argument register).
 */
ENTRY(restore_user_context_int)
	movq	%rdi, %rbp		/* rbp = proc_ptr */

	/*
	 * Build the IRETQ frame.  %rax is used as a scratch register here;
	 * RESTORE_GP_REGS will restore the correct %rax from AXREG.
	 */
	pushq	$USER_DS_SELECTOR	/* SS = DS in flat AMD64 user mode */
	movq	SPREG(%rbp), %rax;	pushq	%rax
	movq	PSWREG(%rbp), %rax;	pushq	%rax
	pushq	$USER_CS_SELECTOR
	movq	PCREG(%rbp), %rax;	pushq	%rax

	/*
	 * Set user segment registers before RESTORE_GP_REGS so the %si
	 * clobber is overwritten when %rsi is restored from SIREG.
	 */
	movw	$USER_DS_SELECTOR, %si
	movw	%si, %ds
	movw	%si, %es
	movw	%si, %fs
	movw	%si, %gs

	/* Restore all GP registers (%rax, %rsi, etc. are corrected here). */
	RESTORE_GP_REGS(%rbp)

	/* Restore %rbp last (it was proc_ptr until now). */
	movq	BPREG(%rbp), %rbp

	iretq

/*===========================================================================*/
/*			exception entry					     */
/*===========================================================================*/
/*
 * On AMD64 the CPU-pushed exception frame is 5 x 8 = 40 bytes:
 *   [rsp+0]  RIP
 *   [rsp+8]  CS
 *   [rsp+16] RFLAGS
 *   [rsp+24] RSP
 *   [rsp+32] SS
 *
 * Exception stubs push $vector + $error_code = 16 bytes before reaching
 * exception_entry, so the CPU frame starts 16 bytes above %rsp.
 * SAVE_PROCESS_CTX(16, ...) accounts for those 16 extra bytes.
 * TEST_INT_IN_KERNEL checks CS: with the two software-pushed words, CS is at
 * 8 (error) + 8 (vector) + 8 (RIP) + 8 (CS) = CS at [rsp + 24].
 */
.balign 16
exception_entry:
	TEST_INT_IN_KERNEL(24, exception_entry_nested)

exception_entry_from_user:
	SAVE_PROCESS_CTX(16, KTS_INT_HARD)

	movq	%rbp, %rdi
	push	%rbp
	movq	$0, %rbp
	call	_C_LABEL(context_stop)
	pop	%rbp

	movq	%rsp, %rsi		/* frame ptr -> 2nd arg (%rsi) */
	xorl	%edi, %edi		/* is_nested=0  -> 1st arg (%rdi) */
	call	_C_LABEL(exception_handler)

	jmp	_C_LABEL(switch_to_user)

exception_entry_nested:
	pushaq
	movq	%rsp, %rsi
	addq	$(15 * 8), %rsi		/* skip pushaq frame; point at exception frame */
	movl	$1, %edi		/* is_nested=1  -> 1st arg (%rdi) */
	call	_C_LABEL(exception_handler)
	popaq
	add	$16, %rsp		/* discard error code + vector */
	iretq

/*===========================================================================*/
/*			exception handlers (IDT stubs)			     */
/*===========================================================================*/

#define EXCEPTION_ERR_CODE(vector)	\
	push	$vector			;\
	jmp	exception_entry

#define EXCEPTION_NO_ERR_CODE(vector)	\
	pushq	$0			;\
	EXCEPTION_ERR_CODE(vector)

LABEL(divide_error)		EXCEPTION_NO_ERR_CODE(DIVIDE_VECTOR)
LABEL(single_step_exception)	EXCEPTION_NO_ERR_CODE(DEBUG_VECTOR)

LABEL(nmi)
#ifndef USE_WATCHDOG
	EXCEPTION_NO_ERR_CODE(NMI_VECTOR)
#else
	pushw	%ds; pushw	%es; pushw	%fs; pushw	%gs
	pushaq
	mov	%ss, %si;  mov	%si, %ds;  mov	%si, %es
	mov	%rsp, %rdi
	call	_C_LABEL(nmi_watchdog_handler)
	popaq
	popw	%gs; popw	%fs; popw	%es; popw	%ds
	iretq
#endif

LABEL(breakpoint_exception)	EXCEPTION_NO_ERR_CODE(BREAKPOINT_VECTOR)
LABEL(overflow)			EXCEPTION_NO_ERR_CODE(OVERFLOW_VECTOR)
LABEL(bounds_check)		EXCEPTION_NO_ERR_CODE(BOUNDS_VECTOR)
LABEL(inval_opcode)		EXCEPTION_NO_ERR_CODE(INVAL_OP_VECTOR)

LABEL(copr_not_available)
	TEST_INT_IN_KERNEL(8, copr_not_available_in_kernel)
	cld
	SAVE_PROCESS_CTX(0, KTS_INT_HARD)
	movq	%rbp, %rdi
	push	%rbp;  movq	$0, %rbp
	call	_C_LABEL(context_stop)
	pop	%rbp			/* balance the push; restore proc_ptr */
	call	_C_LABEL(copr_not_available_handler)
	jmp	_C_LABEL(switch_to_user)

copr_not_available_in_kernel:
	pushq	$0;  pushq	$COPROC_NOT_VECTOR
	jmp	exception_entry_nested

LABEL(double_fault)		EXCEPTION_ERR_CODE(DOUBLE_FAULT_VECTOR)
LABEL(copr_seg_overrun)		EXCEPTION_NO_ERR_CODE(COPROC_SEG_VECTOR)
LABEL(inval_tss)		EXCEPTION_ERR_CODE(INVAL_TSS_VECTOR)
LABEL(segment_not_present)	EXCEPTION_ERR_CODE(SEG_NOT_VECTOR)
LABEL(stack_exception)		EXCEPTION_ERR_CODE(STACK_FAULT_VECTOR)
LABEL(general_protection)	EXCEPTION_ERR_CODE(PROTECTION_VECTOR)
LABEL(page_fault)		EXCEPTION_ERR_CODE(PAGE_FAULT_VECTOR)
LABEL(copr_error)		EXCEPTION_NO_ERR_CODE(COPROC_ERR_VECTOR)
LABEL(alignment_check)		EXCEPTION_NO_ERR_CODE(ALIGNMENT_CHECK_VECTOR)
LABEL(machine_check)		EXCEPTION_NO_ERR_CODE(MACHINE_CHECK_VECTOR)
LABEL(simd_exception)		EXCEPTION_NO_ERR_CODE(SIMD_EXCEPTION_VECTOR)

/*===========================================================================*/
/*			miscellaneous kernel routines			     */
/*===========================================================================*/

ENTRY(reload_cr3)
	push	%rbp
	mov	%rsp, %rbp
	mov	%cr3, %rax
	mov	%rax, %cr3
	pop	%rbp
	ret

#ifdef CONFIG_SMP
ENTRY(startup_ap_32)
	movw	$KERN_DS_SELECTOR, %ax
	mov	%ax, %ds
	mov	%ax, %ss
	mov	$_C_LABEL(k_boot_stktop) - 8, %rsp
	call	_C_LABEL(prot_load_selectors)
	jmp	_C_LABEL(smp_ap_boot)
	hlt
#endif

/*===========================================================================*/
/*				data					     */
/*===========================================================================*/

.data
.short	0x526F	/* magic number -- must be the first data entry */

.bss
k_initial_stack:
.space	K_STACK_SIZE
LABEL(__k_unpaged_k_initial_stktop)

k_boot_stack:
.space	K_STACK_SIZE
LABEL(k_boot_stktop)

.balign K_STACK_SIZE
LABEL(k_stacks_start)
.space	2 * (K_STACK_SIZE * CONFIG_MAX_CPUS)
LABEL(k_stacks_end)
