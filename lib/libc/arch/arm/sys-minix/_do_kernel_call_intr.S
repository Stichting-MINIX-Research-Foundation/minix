#include <minix/ipcconst.h>
#include <machine/asm.h>

ENTRY(_do_kernel_call_intr)
	/* r0 already holds msg ptr */
	mov	r3, #KERVEC_INTR /* r3 determines the SVC type */
	svc	#0          /* trap to kernel */
	bx	lr
