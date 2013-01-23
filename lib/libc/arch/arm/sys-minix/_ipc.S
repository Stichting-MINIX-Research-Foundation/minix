#include <minix/ipcconst.h>
#include <machine/asm.h>

/**========================================================================* */
/*                           IPC assembly routines			  * */
/**========================================================================* */
ENTRY(_send_orig)
	push	{fp}
	mov	fp, sp
	mov	r2, r1       /* r2 = msg ptr */
	mov	r1, r0       /* r1 = src_dest */
	mov	r0, #SEND    /* _send(dest, ptr) */
	mov	r3, #IPCVEC  /* r3 determines the SVC type */
	svc	#0           /* trap to kernel */
	pop	{fp}
	bx	lr

ENTRY(_receive_orig)
	push	{fp}
	mov	fp, sp
	push	{r2}         /* save status ptr */
	mov	r2, r1       /* r2 = msg ptr */
	mov	r1, r0       /* r1 = src_dest */
	mov	r0, #RECEIVE /* _receive(src, ptr) */
	mov	r3, #IPCVEC  /* r3 determines the SVC type */
	svc	#0           /* trap to kernel */
	pop	{r2}         /* restore status ptr */
	str	r1, [r2]
	pop	{fp}
	bx	lr

ENTRY(_sendrec_orig)
	push	{fp}
	mov	fp, sp
	mov	r2, r1       /* r2 = msg ptr */
	mov	r1, r0       /* r1 = src_dest */
	mov	r0, #SENDREC /* _sendrec(srcdest, ptr) */
	mov	r3, #IPCVEC  /* r3 determines the SVC type */
	svc	#0           /* trap to kernel */
	pop	{fp}
	bx	lr

ENTRY(_minix_kernel_info_struct)
	push	{fp}
	mov	fp, sp
	push	{r0}
	mov	r1, #0
	mov	r2, #0
	mov	r0, #MINIX_KERNINFO /* kerninfo() */
	mov	r3, #IPCVEC  /* r3 determines the SVC type */
	svc	#0           /* trap to kernel */
	pop     {r2}         /* r2 = return struct ptr (was r0) */
	str     r1, [r2]
	pop	{fp}
	bx	lr

ENTRY(_notify_orig)
	push	{fp}
	mov	fp, sp
	mov	r1, r0       /* r1 = src_dest */
	mov	r0, #NOTIFY  /* _notify(srcdst) */
	mov	r3, #IPCVEC  /* r3 determines the SVC type */
	svc	#0           /* trap to kernel */
	pop	{fp}
	bx	lr

ENTRY(_sendnb_orig)
	push	{fp}
	mov	fp, sp
	mov	r2, r1       /* r2 = msg ptr */
	mov	r1, r0       /* r1 = src_dest */
	mov	r0, #SENDNB  /* _sendnb(dest, ptr) */
	mov	r3, #IPCVEC  /* r3 determines the SVC type */
	svc	#0           /* trap to kernel */
	pop	{fp}
	bx	lr


ENTRY(_do_kernel_call_orig)
	/* r0 already holds msg ptr */
	mov	r3, #KERVEC /* r3 determines the SVC type */
	svc	#0          /* trap to kernel */
	bx	lr
