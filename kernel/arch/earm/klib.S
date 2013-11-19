/* sections */


#include <minix/config.h>
#include <minix/const.h>
#include <machine/asm.h>
#include <machine/interrupt.h>
#include <machine/vm.h>
#include "archconst.h"
#include "kernel/const.h"
#include "sconst.h"
#include <machine/multiboot.h>


/*===========================================================================*/
/*				copy_msg_from_user			     */
/*===========================================================================*/
/*
 * int copy_msg_from_user(message * user_mbuf, message * dst);
 *
 * Copies a message of 64 bytes from user process space to a kernel buffer. This
 * function assumes that the process address space is installed (ttbr loaded).
 *
 * This function from the callers point of view either succeeds or returns an
 * error which gives the caller a chance to respond accordingly. In fact it
 * either succeeds or if it generates a pagefault, general protection or other
 * exception, the trap handler has to redirect the execution to
 * __user_copy_msg_pointer_failure where the error is reported to the caller
 * without resolving the pagefault. It is not kernel's problem to deal with
 * wrong pointers from userspace and the caller should return an error to
 * userspace as if wrong values or request were passed to the kernel
 */
ENTRY(copy_msg_from_user)
	push	{r4-r10, lr}
	/* load the source pointer */
	mov	r9, r0
	/* load the destination pointer */
	mov	r10, r1
	/* do the copy, first 32 bytes */
	ldm	r9,  {r0-r7}
	stm	r10, {r0-r7}

	/* next 32 bytes */
	add	r9,  r9, #32
	add	r10, r10, #32
	ldm	r9,  {r0-r7}
	stm	r10, {r0-r7}

LABEL(__copy_msg_from_user_end)
	pop	{r4-r10, lr}
	mov	r0, #0
	bx	lr

/*===========================================================================*/
/*				copy_msg_to_user			     */
/*===========================================================================*/
/*
 * void copy_msg_to_user(message * src, message * user_mbuf);
 *
 * Copies a message of 64 bytes to user process space from a kernel buffer.
 *
 * All the other copy_msg_from_user() comments apply here as well!
 */
ENTRY(copy_msg_to_user)
	push	{r4-r10, lr}
	/* load the source pointer */
	mov	r9, r0
	/* load the destination pointer */
	mov	r10, r1
	/* do the copy, first 32 bytes */
	ldm	r9,  {r0-r7}
	stm	r10, {r0-r7}

	/* next 32 bytes */
	add	r9,  r9,  #32
	add	r10, r10, #32
	ldm	r9,  {r0-r7}
	stm	r10, {r0-r7}

LABEL(__copy_msg_to_user_end)
	pop	{r4-r10, lr}
	mov	r0, #0
	bx	lr

/*
 * if a function from a selected set of copies from or to userspace fails, it is
 * because of a wrong pointer supplied by the userspace. We have to clean up and
 * and return -1 to indicated that something wrong has happend. The place it was
 * called from has to handle this situation. The exception handler redirect us
 * here to continue, clean up and report the error
 */
ENTRY(__user_copy_msg_pointer_failure)
	pop	{r4-r10, lr}
	mov	r0, #-1
	bx	lr

ENTRY(intr_enable)
ENTRY(interrupts_enable)
	dsb
	cpsie if
	bx lr

ENTRY(intr_disable)
ENTRY(interrupts_disable)
	dsb
	cpsid if
	bx lr
