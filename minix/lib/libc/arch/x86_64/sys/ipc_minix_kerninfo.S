#include <minix/ipcconst.h>
#include <machine/asm.h>

/* void ipc_minix_kerninfo(void **result);
 *
 * x86_64 IPC ABI: rdi=call_nr, rsi=endpoint, rdx=msg_ptr.
 * After INT $IPCVEC_INTR the kernel places the kerninfo pointer in %rbx
 * (IPC_STATUS_REG=bx).  We must save the result-ptr arg before clobbering
 * %rdi with the call number.
 */
ENTRY(ipc_minix_kerninfo)
	pushq	%rbx			/* callee-saved; holds kerninfo after trap */
	pushq	%rdi			/* save result-ptr arg; %rdi is needed for call_nr */

	movl	$MINIX_KERNINFO, %edi	/* call_nr */
	xorl	%esi, %esi		/* endpoint = NONE */
	xorl	%edx, %edx		/* no message */
	int	$IPCVEC_INTR		/* trap to kernel */

	popq	%rcx			/* restore result ptr */
	movq	%rbx, (%rcx)		/* *result = kerninfo address from %rbx */
	popq	%rbx
	ret
