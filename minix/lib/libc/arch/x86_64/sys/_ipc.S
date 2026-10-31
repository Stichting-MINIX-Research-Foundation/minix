#include <minix/ipcconst.h>
#include <machine/asm.h>

/*
 * Userspace IPC stubs for AMD64 (SysV ABI).
 *
 * C caller convention (SysV AMD64 ABI):
 *   rdi = first  argument (endpoint or table pointer for senda)
 *   rsi = second argument (message pointer, or count for senda)
 *   rdx = third  argument (status pointer, for _ipc_receive_intr only)
 *
 * Kernel IPC register convention:
 *   rdi = call_nr   (r1 for do_ipc)
 *   rsi = endpoint  (r2; count for SENDA)
 *   rdx = msg_ptr   (r3; table for SENDA)
 *
 * Mapping (must do rdx first to avoid clobbering msg_ptr before saving it):
 *   movq %rsi, %rdx    -- msg_ptr -> r3
 *   movq %rdi, %rsi    -- endpoint -> r2
 *   movq $OP,  %rdi    -- call_nr -> r1
 *
 * After INT $IPCVEC_INTR the kernel restores all GP registers from p_reg:
 *   %rax = do_ipc return code (the IPC error code)
 *   %rbx = IPC status (IPC_STATUS_REG = bx; set by kernel before restore)
 */

ENTRY(_ipc_send_intr)
	/* AMD64: rdi=dest, rsi=msg_ptr */
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movq	$SEND, %rdi
	int	$IPCVEC_INTR
	ret

ENTRY(_ipc_receive_intr)
	/*
	 * AMD64: rdi=src, rsi=msg_ptr, rdx=status_ptr.
	 * We need rdx for msg_ptr, so save status_ptr on the stack first.
	 * After INT, %rbx holds the IPC status; pop status_ptr into %rdi
	 * (free to clobber -- already used for call_nr) and store.
	 */
	push	%rbx		/* preserve callee-saved %rbx */
	push	%rdx		/* save status_ptr (rdx about to be repurposed) */
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movq	$RECEIVE, %rdi
	int	$IPCVEC_INTR
	pop	%rdi		/* rdi = status_ptr */
	movl	%ebx, (%rdi)	/* store IPC status (32-bit) at *status_ptr */
	pop	%rbx
	ret

ENTRY(_ipc_sendrec_intr)
	/* AMD64: rdi=srcdest, rsi=msg_ptr */
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movq	$SENDREC, %rdi
	int	$IPCVEC_INTR
	ret

ENTRY(_ipc_notify_intr)
	/* AMD64: rdi=dest (no message) */
	movq	%rdi, %rsi
	movq	$NOTIFY, %rdi
	int	$IPCVEC_INTR
	ret

ENTRY(_ipc_sendnb_intr)
	/* AMD64: rdi=dest, rsi=msg_ptr */
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movq	$SENDNB, %rdi
	int	$IPCVEC_INTR
	ret

ENTRY(_ipc_senda_intr)
	/*
	 * AMD64: rdi=table, rsi=count.
	 * Kernel: rdi=SENDA, rsi=count (r2), rdx=table (r3).
	 * Save table to rdx first; rsi=count is already in place.
	 */
	movq	%rdi, %rdx	/* rdx = table */
	movq	$SENDA, %rdi	/* rdi = call_nr; rsi = count unchanged */
	int	$IPCVEC_INTR
	ret
