#include <minix/ipcconst.h>
#include <machine/asm.h>

ENTRY(ipc_minix_kerninfo)
	push	{fp}
	mov	fp, sp
	push	{r0}
	mov	r1, #0
	mov	r2, #0
	mov	r0, #MINIX_KERNINFO /* _ipc_minix_kerninfo() */
	mov	r3, #IPCVEC_INTR  /* r3 determines the SVC type */
	svc	#0           /* trap to kernel */
	pop     {r2}         /* r2 = return struct ptr (was r0) */
	str     r1, [r2]
	pop	{fp}
	bx	lr

