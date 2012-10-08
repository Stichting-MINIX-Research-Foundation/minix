#include <minix/ipcconst.h>
#include <machine/asm.h>

ENTRY(_senda_orig)
	push	{fp}
	mov	fp, sp
	mov	r2, r0       /* r2 = table */
	/* r1 already holds count */
	mov	r0, #SENDA   /* _senda(table, count) */
	mov	r3, #IPCVEC  /* r3 determines the SVC type */
	svc	#0           /* trap to kernel */
	pop	{fp}
	bx	lr
