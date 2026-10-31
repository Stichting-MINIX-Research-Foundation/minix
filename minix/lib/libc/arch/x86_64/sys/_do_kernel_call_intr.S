#include <minix/ipcconst.h>
#include <machine/asm.h>

ENTRY(_do_kernel_call_intr)
	/* AMD64: message pointer in %rdi (first SysV argument); kernel
	 * reads it from DIREG after SAVE_PROCESS_CTX. */
	int	$KERVEC_INTR
	ret
