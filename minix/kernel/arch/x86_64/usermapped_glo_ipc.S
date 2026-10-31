#include <minix/ipcconst.h>
#include <machine/asm.h>

/* Put usermapped stubs in the section mapped into user address space. */
#undef _ENTRY
#define _ENTRY(x) \
	_ALIGN_TEXT; .globl x; .type x,@function; x:

.section .usermapped_glo, "wax", @progbits

/*
 * AMD64 usermapped IPC stubs.
 *
 * Two variants per function:
 *   _softint  -- INT $IPCVEC_UM   (universal fallback)
 *   _syscall  -- SYSCALL fast path (main path for IPC)
 *
 * The SYSENTER variant has been removed; SYSENTER is an i386-ism and
 * AMD64 uses SYSCALL instead.
 *
 * Register convention (matches _ipc.S and the kernel):
 *   C caller:  rdi=endpoint|table, rsi=msg_ptr|count, rdx=status_ptr
 *   Kernel:    rdi=call_nr, rsi=endpoint|count, rdx=msg_ptr|table
 *
 * Mapping for most IPC ops (save rdx first to avoid clobbering msg_ptr):
 *   movq %rsi, %rdx    -- msg_ptr -> r3
 *   movq %rdi, %rsi    -- endpoint -> r2
 *   movq $OP,  %rdi    -- call_nr -> r1
 *
 * SYSCALL clobbers %rcx (return RIP) and %r11 (RFLAGS) in userspace --
 * both are caller-saved in the AMD64 ABI so no explicit save is needed.
 * Userspace must pre-save its RSP in %r10 before SYSCALL so the kernel's
 * per-CPU entry can restore it on return.
 *
 * After the trap:
 *   %rax = return code (do_ipc error value)
 *   %rbx = IPC status  (IPC_STATUS_REG = bx, set by kernel for RECEIVE/SENDREC)
 */

/* ---- SEND ---- */
ENTRY(usermapped_send_softint)
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movq	$SEND, %rdi
	int	$IPCVEC_UM
	ret

ENTRY(usermapped_send_syscall)
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movq	$SEND, %rdi
	movq	%rsp, %r10
	syscall
	ret

/* ---- RECEIVE ---- */
/*
 * For RECEIVE the third argument is a status_ptr we must preserve across
 * the trap.  Save it on the user stack (user RSP is intact across both INT
 * and SYSCALL; see mpx.S for the SYSCALL RSP restore guarantee).
 * After the trap, %rbx holds the IPC status; pop status_ptr into %rcx
 * (caller-saved, safe to clobber) and store.
 */
ENTRY(usermapped_receive_softint)
	push	%rbx
	push	%rdx		/* save status_ptr */
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movq	$RECEIVE, %rdi
	int	$IPCVEC_UM
	pop	%rcx		/* rcx = status_ptr */
	movl	%ebx, (%rcx)
	pop	%rbx
	ret

ENTRY(usermapped_receive_syscall)
	push	%rbx
	push	%rdx		/* save status_ptr */
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movq	$RECEIVE, %rdi
	movq	%rsp, %r10	/* r10 = RSP (with status_ptr and rbx on stack) */
	syscall
	pop	%rcx		/* rcx = status_ptr (stack restored to same depth) */
	movl	%ebx, (%rcx)
	pop	%rbx
	ret

/* ---- SENDREC ---- */
ENTRY(usermapped_sendrec_softint)
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movq	$SENDREC, %rdi
	int	$IPCVEC_UM
	ret

ENTRY(usermapped_sendrec_syscall)
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movq	$SENDREC, %rdi
	movq	%rsp, %r10
	syscall
	ret

/* ---- SENDNB ---- */
ENTRY(usermapped_sendnb_softint)
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movq	$SENDNB, %rdi
	int	$IPCVEC_UM
	ret

ENTRY(usermapped_sendnb_syscall)
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movq	$SENDNB, %rdi
	movq	%rsp, %r10
	syscall
	ret

/* ---- NOTIFY ---- */
ENTRY(usermapped_notify_softint)
	movq	%rdi, %rsi
	movq	$NOTIFY, %rdi
	int	$IPCVEC_UM
	ret

ENTRY(usermapped_notify_syscall)
	movq	%rdi, %rsi
	movq	$NOTIFY, %rdi
	movq	%rsp, %r10
	syscall
	ret

/* ---- SENDA ---- */
/*
 * Kernel expects: rdi=SENDA, rsi=count, rdx=table.
 * C caller passes: rdi=table, rsi=count.
 * Move table to rdx first (rsi=count stays in place).
 */
ENTRY(usermapped_senda_softint)
	movq	%rdi, %rdx	/* rdx = table */
	movq	$SENDA, %rdi	/* rdi = SENDA; rsi = count unchanged */
	int	$IPCVEC_UM
	ret

ENTRY(usermapped_senda_syscall)
	movq	%rdi, %rdx
	movq	$SENDA, %rdi
	movq	%rsp, %r10
	syscall
	ret

/* ---- Sysenter stubs (AMD64 has no SYSENTER fast path; use INT) ---- */
ENTRY(usermapped_send_sysenter)
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movq	$SEND, %rdi
	int	$IPCVEC_UM
	ret

ENTRY(usermapped_receive_sysenter)
	push	%rbx
	push	%rdx
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movq	$RECEIVE, %rdi
	int	$IPCVEC_UM
	pop	%rcx
	movl	%ebx, (%rcx)
	pop	%rbx
	ret

ENTRY(usermapped_sendrec_sysenter)
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movq	$SENDREC, %rdi
	int	$IPCVEC_UM
	ret

ENTRY(usermapped_sendnb_sysenter)
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movq	$SENDNB, %rdi
	int	$IPCVEC_UM
	ret

ENTRY(usermapped_notify_sysenter)
	movq	%rdi, %rsi
	movq	$NOTIFY, %rdi
	int	$IPCVEC_UM
	ret

ENTRY(usermapped_senda_sysenter)
	movq	%rdi, %rdx
	movq	$SENDA, %rdi
	int	$IPCVEC_UM
	ret

ENTRY(usermapped_do_kernel_call_sysenter)
	int	$KERVEC_UM
	ret

/* ---- Kernel call ---- */
/*
 * Kernel calls always use INT $KERVEC_UM.  There is no SYSCALL fast path
 * for kernel calls -- the per-CPU SYSCALL entry points are dedicated to IPC.
 * The message pointer is in %rdi (first C argument); kernel_call_entry_common
 * reads it from DIREG after SAVE_PROCESS_CTX.
 */
ENTRY(usermapped_do_kernel_call_softint)
	int	$KERVEC_UM
	ret

ENTRY(usermapped_do_kernel_call_syscall)
	int	$KERVEC_UM	/* no SYSCALL fast path for kernel calls */
	ret

.data
LABEL(usermapped_offset)
.space 4
