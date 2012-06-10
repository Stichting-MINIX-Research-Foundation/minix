#include <minix/ipcconst.h>
#include <machine/asm.h>

	MSGTAB = 8	/* message table */
	TABCOUNT = 12	/* number of entries in message table */

ENTRY(_senda_orig)
	push	%ebp
	movl	%esp, %ebp
	push	%ebx
	movl	TABCOUNT(%ebp), %eax	/* eax = count */
	movl	MSGTAB(%ebp), %ebx	/* ebx = table */
	movl	$SENDA, %ecx	/* _senda(table, count) */
	int	$IPCVEC	/* trap to the kernel */
	pop	%ebx
	pop	%ebp
	ret
