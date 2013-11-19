#include <minix/ipcconst.h>
#include <machine/asm.h>

/**========================================================================* */
/*                           IPC assembly routines			  * */
/**========================================================================* */
ENTRY(_ipc_send_intr)
	push	{fp}
	mov	fp, sp
	mov	r2, r1       /* r2 = msg ptr */
	mov	r1, r0       /* r1 = src_dest */
	mov	r0, #SEND    /* _ipc_send(dest, ptr) */
	mov	r3, #IPCVEC_INTR  /* r3 determines the SVC type */
	svc	#0           /* trap to kernel */
	pop	{fp}
	bx	lr

ENTRY(_ipc_receive_intr)
	push	{fp}
	mov	fp, sp
	push	{r2}         /* save status ptr */
	mov	r2, r1       /* r2 = msg ptr */
	mov	r1, r0       /* r1 = src_dest */
	mov	r0, #RECEIVE /* _ipc_receive(src, ptr) */
	mov	r3, #IPCVEC_INTR  /* r3 determines the SVC type */
	svc	#0           /* trap to kernel */
	pop	{r2}         /* restore status ptr */
	str	r1, [r2]
	pop	{fp}
	bx	lr

ENTRY(_ipc_sendrec_intr)
	push	{fp}
	mov	fp, sp
	mov	r2, r1       /* r2 = msg ptr */
	mov	r1, r0       /* r1 = src_dest */
	mov	r0, #SENDREC /* _ipc_sendrec(srcdest, ptr) */
	mov	r3, #IPCVEC_INTR  /* r3 determines the SVC type */
	svc	#0           /* trap to kernel */
	pop	{fp}
	bx	lr

ENTRY(_ipc_notify_intr)
	push	{fp}
	mov	fp, sp
	mov	r1, r0       /* r1 = src_dest */
	mov	r0, #NOTIFY  /* _ipc_notify(srcdst) */
	mov	r3, #IPCVEC_INTR  /* r3 determines the SVC type */
	svc	#0           /* trap to kernel */
	pop	{fp}
	bx	lr

ENTRY(_ipc_sendnb_intr)
	push	{fp}
	mov	fp, sp
	mov	r2, r1       /* r2 = msg ptr */
	mov	r1, r0       /* r1 = src_dest */
	mov	r0, #SENDNB  /* _ipc_sendnb(dest, ptr) */
	mov	r3, #IPCVEC_INTR  /* r3 determines the SVC type */
	svc	#0           /* trap to kernel */
	pop	{fp}
	bx	lr

ENTRY(_ipc_senda_intr)
	push	{fp}
	mov	fp, sp
	mov	r2, r0       /* r2 = table */
	/* r1 already holds count */
	mov	r0, #SENDA   /* _ipc_senda(table, count) */
	mov	r3, #IPCVEC_INTR  /* r3 determines the SVC type */
	svc	#0           /* trap to kernel */
	pop	{fp}
	bx	lr

