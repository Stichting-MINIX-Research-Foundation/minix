#include <minix/ipcconst.h>

.globl	__senda

	SYSVEC = 33

	MSGTAB = 8	/* message table */
	TABCOUNT = 12	/* number of entries in message table */

.text

__senda:
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	TABCOUNT(%ebp), %eax	/* eax = count */
	movl	MSGTAB(%ebp), %ebx	/* ebx = table */
	movl	$SENDA, %ecx	/* _senda(table, count) */
	int	$SYSVEC	/* trap to the kernel */
	pop	%ebx
	pop	%ebp
	ret
