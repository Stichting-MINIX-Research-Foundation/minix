#include <minix/ipcconst.h>
#include <machine/asm.h>

	SRC_DST = 8	/* source/ destination process  */
	MESSAGE = 12	/* message pointer  */
	STATUS = 16	/* status pointer  */

/**========================================================================* */
/*                           IPC assembly routines			  * */
/**========================================================================* */
/* all message passing routines save ebx, but destroy eax and ecx. */
ENTRY(_send_orig)
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	SRC_DST(%ebp), %eax	/* eax = dest-src */
	movl	MESSAGE(%ebp), %ebx	/* ebx = message pointer */
	movl	$SEND, %ecx	/* _send(dest, ptr) */
	int	$IPCVEC_ORIG	/* trap to the kernel */
	pop	%ebx
	pop	%ebp
	ret

ENTRY(_receive_orig)
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	SRC_DST(%ebp), %eax	/* eax = dest-src */
	movl	MESSAGE(%ebp), %ebx	/* ebx = message pointer */
	movl	$RECEIVE, %ecx	/* _receive(src, ptr) */
	int	$IPCVEC_ORIG	/* trap to the kernel */
	movl	STATUS(%ebp), %ecx	/* ecx = status pointer */
	movl	%ebx, (%ecx)
	pop	%ebx
	pop	%ebp
	ret

ENTRY(_sendrec_orig)
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	SRC_DST(%ebp), %eax	/* eax = dest-src */
	movl	MESSAGE(%ebp), %ebx	/* ebx = message pointer */
	movl	$SENDREC, %ecx	/* _sendrec(srcdest, ptr) */
	int	$IPCVEC_ORIG	/* trap to the kernel */
	pop	%ebx
	pop	%ebp
	ret

ENTRY(_minix_kernel_info_struct)
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	$0, %eax
	movl	$0, %ebx
	movl	$MINIX_KERNINFO, %ecx
	int	$IPCVEC_ORIG	/* trap to the kernel */
	movl	8(%ebp), %ecx	/* ecx = return struct ptr */
	movl	%ebx, (%ecx)
	pop	%ebx
	pop	%ebp
	ret

ENTRY(_notify_orig)
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	SRC_DST(%ebp), %eax	/* eax = destination  */
	movl	$NOTIFY, %ecx	/* _notify(srcdst) */
	int	$IPCVEC_ORIG	/* trap to the kernel */
	pop	%ebx
	pop	%ebp
	ret

ENTRY(_sendnb_orig)
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	SRC_DST(%ebp), %eax	/* eax = dest-src */
	movl	MESSAGE(%ebp), %ebx	/* ebx = message pointer */
	movl	$SENDNB, %ecx	/* _sendnb(dest, ptr) */
	int	$IPCVEC_ORIG	/* trap to the kernel */
	pop	%ebx
	pop	%ebp
	ret

ENTRY(_do_kernel_call_orig)
	/* pass the message pointer to kernel in the %eax register */
	movl	4(%esp), %eax
	int	$KERVEC_ORIG
	ret
