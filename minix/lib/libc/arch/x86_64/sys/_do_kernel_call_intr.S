#include <minix/ipcconst.h>
#include <machine/asm.h>

#if !defined(__x86_64__) || KERVEC_INTR != 0x40
#error "x86_64 libc kernel-call stub requires kernel-call vector 0x40"
#endif

ENTRY(_do_kernel_call_intr)
	/* AMD64: message pointer in %rdi (first SysV argument); kernel
	 * reads it from DIREG after SAVE_PROCESS_CTX. */
	int	$KERVEC_INTR
	ret
