#include <minix/ipcconst.h>
#include <machine/asm.h>

/* Redefine _ENTRY to put the following function in the section
 * ".usermapped_glo". */
#undef _ENTRY
#define _ENTRY(x) \
	_ALIGN_TEXT; .globl x; .type x,@function; x:

.section .usermapped_glo, "wax", @progbits

/**========================================================================* */
/*                           IPC assembly routines			  * */
/**========================================================================* */
/* all message passing routines save ebx, but destroy eax and ecx. */

#define IPCFUNC(name,SETARGS,VEC,POSTTRAP)			 \
ENTRY(usermapped_ ## name ## _softint)					;\
	push	%ebp							;\
	movl	%esp, %ebp						;\
	push	%ebx							;\
	SETARGS								;\
	int	$VEC	/* trap to the kernel */			;\
	mov	%ebx, %ecx	/* save %ebx */				;\
	POSTTRAP							;\
	pop	%ebx							;\
	pop	%ebp							;\
	ret								;\
ENTRY(usermapped_ ## name ## _sysenter)					;\
	push	%ebp							;\
	movl	%esp, %ebp						;\
	push	%ebp							;\
	push	%edx							;\
	push	%ebx							;\
	push	%esi							;\
	push	%edi							;\
	movl	%esp, %esi	/* kernel uses %esi for restored %esp */;\
	movl	$0f, %edx	/* kernel uses %edx for restored %eip */;\
	movl	$VEC, %edi	/* %edi to distinguish ipc/kerncall */	;\
	/* !!! There is a dependency of proc_stacktrace()		;\
	 * on this stack layout; it needs to find %ebp on it.		;\
	 */								;\
	SETARGS			/* call-specific register setup */	;\
	sysenter		/* disappear into kernel */		;\
0:									;\
	push	%edi		/* kernel has desired PSW in %edi */	;\
	popf			/* set PSW kernel wants us to have */	;\
	mov	%ebx, %ecx	/* return w. state mangled; save %ebx */;\
	pop	%edi							;\
	pop	%esi							;\
	pop	%ebx							;\
	pop	%edx							;\
	pop	%ebp							;\
	POSTTRAP							;\
	pop	%ebp							;\
	ret								;\
ENTRY(usermapped_ ## name ## _syscall)					;\
	push	%ebp							;\
	movl	%esp, %ebp						;\
	push	%ebp							;\
	push	%edx							;\
	push	%ebx							;\
	push	%esi							;\
	push	%edi							;\
	movl	$VEC, %edi	/* %edi to distinguish ipc/kerncall */	;\
	/* !!! There is a dependency of proc_stacktrace()		;\
	 * on this stack layout; it needs to find %ebp on it.		;\
	 */								;\
	SETARGS			/* call-specific register setup */	;\
	movl	%ecx, %edx	/* %ecx is clobbered by SYSCALL */	;\
	syscall			/* disappear into kernel */		;\
	push	%edi		/* kernel has desired PSW in %edi */	;\
	popf			/* set PSW kernel wants us to have */	;\
	mov	%ebx, %ecx	/* return w. state mangled; save %ebx */;\
	pop	%edi							;\
	pop	%esi							;\
	pop	%ebx							;\
	pop	%edx							;\
	pop	%ebp							;\
	POSTTRAP							;\
	pop	%ebp							;\
	ret

#define IPCARGS(opcode)							\
	movl	8(%ebp), %eax	/* eax = dest-src */			;\
	movl	12(%ebp), %ebx	/* ebx = message pointer */		;\
	movl	$opcode, %ecx						;\

#define SENDA_ARGS		\
	movl	12(%ebp), %eax	/* eax = count */			;\
	movl	8(%ebp), %ebx	/* ebx = table */			;\
	movl	$SENDA, %ecx						;\

#define GETSTATUS							\
	push	%eax							;\
	movl    16(%ebp), %eax      /* ecx = saved %ebx */		;\
	movl	%ecx,  (%eax)						;\
	pop	%eax

#define KERNARGS mov 8(%ebp), %eax

IPCFUNC(send,IPCARGS(SEND),IPCVEC_UM,)
IPCFUNC(receive,IPCARGS(RECEIVE),IPCVEC_UM,GETSTATUS)
IPCFUNC(sendrec,IPCARGS(SENDREC),IPCVEC_UM,)
IPCFUNC(sendnb,IPCARGS(SENDNB),IPCVEC_UM,)
IPCFUNC(notify,IPCARGS(NOTIFY),IPCVEC_UM,)
IPCFUNC(senda,SENDA_ARGS,IPCVEC_UM,)
IPCFUNC(do_kernel_call,KERNARGS,KERVEC_UM,)

.data
LABEL(usermapped_offset)
.space 4
