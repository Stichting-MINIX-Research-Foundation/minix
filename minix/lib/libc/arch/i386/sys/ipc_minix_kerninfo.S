#include <minix/ipcconst.h>
#include <machine/asm.h>

ENTRY(ipc_minix_kerninfo)
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	$0, %eax
	movl	$0, %ebx
	movl	$MINIX_KERNINFO, %ecx
	int	$IPCVEC_INTR	/* trap to the kernel */
	movl	8(%ebp), %ecx	/* ecx = return struct ptr */
	movl	%ebx, (%ecx)
	pop	%ebx
	pop	%ebp
	ret

