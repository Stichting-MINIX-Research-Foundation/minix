#include <minix/ipcconst.h>

.globl	__notify, __send, __senda, __sendnb, __receive, __sendrec, __do_kernel_call

	IPCVEC = 33	/* ipc trap to kernel  */
	KERVEC = 32	/* syscall trap to kernel */

	SRC_DST = 8	/* source/ destination process  */
	MESSAGE = 12	/* message pointer  */
	STATUS = 16	/* status pointer  */

/**========================================================================* */
/*                           IPC assembly routines			  * */
/**========================================================================* */
/* all message passing routines save ebx, but destroy eax and ecx. */
.text
__send:
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	SRC_DST(%ebp), %eax	/* eax = dest-src */
	movl	MESSAGE(%ebp), %ebx	/* ebx = message pointer */
	movl	$SEND, %ecx	/* _send(dest, ptr) */
	int	$IPCVEC	/* trap to the kernel */
	pop	%ebx
	pop	%ebp
	ret

__receive:
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	SRC_DST(%ebp), %eax	/* eax = dest-src */
	movl	MESSAGE(%ebp), %ebx	/* ebx = message pointer */
	movl	$RECEIVE, %ecx	/* _receive(src, ptr) */
	int	$IPCVEC	/* trap to the kernel */
	movl	STATUS(%ebp), %ecx	/* ecx = status pointer */
	movl	%ebx, (%ecx)
	pop	%ebx
	pop	%ebp
	ret

__sendrec:
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	SRC_DST(%ebp), %eax	/* eax = dest-src */
	movl	MESSAGE(%ebp), %ebx	/* ebx = message pointer */
	movl	$SENDREC, %ecx	/* _sendrec(srcdest, ptr) */
	int	$IPCVEC	/* trap to the kernel */
	pop	%ebx
	pop	%ebp
	ret

__notify:
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	SRC_DST(%ebp), %eax	/* eax = destination  */
	movl	$NOTIFY, %ecx	/* _notify(srcdst) */
	int	$IPCVEC	/* trap to the kernel */
	pop	%ebx
	pop	%ebp
	ret

__sendnb:
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	SRC_DST(%ebp), %eax	/* eax = dest-src */
	movl	MESSAGE(%ebp), %ebx	/* ebx = message pointer */
	movl	$SENDNB, %ecx	/* _sendnb(dest, ptr) */
	int	$IPCVEC	/* trap to the kernel */
	pop	%ebx
	pop	%ebp
	ret

__do_kernel_call:
	/* pass the message pointer to kernel in the %eax register */
	movl	4(%esp), %eax
	int	$KERVEC
	ret
