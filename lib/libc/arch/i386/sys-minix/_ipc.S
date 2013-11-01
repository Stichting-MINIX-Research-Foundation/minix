#include <minix/ipcconst.h>
#include <machine/asm.h>

	SRC_DST = 8	/* source/ destination process  */
	MESSAGE = 12	/* message pointer  */
	STATUS = 16	/* status pointer  */

	/* For _ipc_senda() */
	MSGTAB = 8	/* message table */
	TABCOUNT = 12	/* number of entries in message table */

/**========================================================================* */
/*                           IPC assembly routines			  * */
/**========================================================================* */
/* all message passing routines save ebx, but destroy eax and ecx. */
ENTRY(_ipc_send_intr)
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	SRC_DST(%ebp), %eax	/* eax = dest-src */
	movl	MESSAGE(%ebp), %ebx	/* ebx = message pointer */
	movl	$SEND, %ecx	/* _ipc_send(dest, ptr) */
	int	$IPCVEC_INTR	/* trap to the kernel */
	pop	%ebx
	pop	%ebp
	ret

ENTRY(_ipc_receive_intr)
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	SRC_DST(%ebp), %eax	/* eax = dest-src */
	movl	MESSAGE(%ebp), %ebx	/* ebx = message pointer */
	movl	$RECEIVE, %ecx	/* _ipc_receive(src, ptr) */
	int	$IPCVEC_INTR	/* trap to the kernel */
	movl	STATUS(%ebp), %ecx	/* ecx = status pointer */
	movl	%ebx, (%ecx)
	pop	%ebx
	pop	%ebp
	ret

ENTRY(_ipc_sendrec_intr)
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	SRC_DST(%ebp), %eax	/* eax = dest-src */
	movl	MESSAGE(%ebp), %ebx	/* ebx = message pointer */
	movl	$SENDREC, %ecx	/* _ipc_sendrec(srcdest, ptr) */
	int	$IPCVEC_INTR	/* trap to the kernel */
	pop	%ebx
	pop	%ebp
	ret

ENTRY(_ipc_notify_intr)
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	SRC_DST(%ebp), %eax	/* eax = destination  */
	movl	$NOTIFY, %ecx	/* _ipc_notify(srcdst) */
	int	$IPCVEC_INTR	/* trap to the kernel */
	pop	%ebx
	pop	%ebp
	ret

ENTRY(_ipc_sendnb_intr)
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	SRC_DST(%ebp), %eax	/* eax = dest-src */
	movl	MESSAGE(%ebp), %ebx	/* ebx = message pointer */
	movl	$SENDNB, %ecx	/* _ipc_sendnb(dest, ptr) */
	int	$IPCVEC_INTR	/* trap to the kernel */
	pop	%ebx
	pop	%ebp
	ret

ENTRY(_ipc_senda_intr)
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	TABCOUNT(%ebp), %eax	/* eax = count */
	movl	MSGTAB(%ebp), %ebx	/* ebx = table */
	movl	$SENDA, %ecx	/* _ipc_senda(table, count) */
	int	$IPCVEC_INTR	/* trap to the kernel */
	pop	%ebx
	pop	%ebp
	ret

