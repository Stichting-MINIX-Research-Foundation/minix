#include <minix/ipcconst.h>
#include <machine/asm.h>

ENTRY(_do_kernel_call_intr)
	/* pass the message pointer to kernel in the %eax register */
	movl	4(%esp), %eax
	int	$KERVEC_INTR
	ret
