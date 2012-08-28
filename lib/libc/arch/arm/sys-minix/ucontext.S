#include <machine/asm.h>
#include <ucontextoffsets.h>


IMPORT(getuctx)
IMPORT(setuctx)
IMPORT(resumecontext)


/* MCF_MAGIC value from <mcontext.h> */
#define MCF_MAGIC	0xc0ffee

/* Values from <sys/ucontext.h> */
#define UCF_IGNFPU	0x002
#define UCF_IGNSIGM	0x004


/* EINVAL from errno.h */
#define EFAULT		14
#define EINVAL 		22

/* int getcontext(ucontext_t *ucp)
 *	Initialise the structure pointed to by ucp to the current user context
 *	of the calling thread. */
ENTRY(getcontext)
	/* In case a process does not use the FPU and is neither interested in
	 * saving its signal mask, then we can skip the context switch to
	 * PM and kernel altogether and only save general-purpose registers. */

	mov r3, lr		/* Save return address:
				 * When setcontext or swapcontext is called,
				 * we jump to this address and continue
				 * running. */

	/* r0 = ucp */

	/* Check null pointer */
	cmp r0, #0			/* ucp == NULL? */
	bne 3f				/* Not null, continue */
	mov r1, #EFAULT
	ldr r2, =_C_LABEL(errno)
	str r1, [r2]			/* errno = EFAULT */
	mov r0, #-1			/* return -1 */
	bx lr

3:	/* Check flags */
	ldr r1, [r0, #UC_FLAGS]		/* r1 = ucp->uc_flags */
	mov r2, #(UCF_IGNFPU | UCF_IGNSIGM)
	cmp r1, r2			/* is UCF_IGNFPU or UCF_IGNSIGM set? */
	beq 1f				/* Both are set, skip getuctx */

0:
	push {r0, r3}
	bl _C_LABEL(getuctx)		/* getuctx(ucp) */
	pop {r0, r3}

1:
	/* Save the context */
	mov lr, r3		/* Restore lr */
	str lr, [r0, #LRREG]	/* Save lr */
	str lr, [r0, #PCREG]	/* Save real RTA in mcp struct */
	str sp, [r0, #SPREG]	/* Save stack pointer */
	str fp, [r0, #FPREG]		/* Save fp */
	str r4, [r0, #REG4]		/* Save r4 */
	str r5, [r0, #REG5]		/* Save r5 */
	str r6, [r0, #REG6]		/* Save r6 */
	str r7, [r0, #REG7]		/* Save r7 */
	str r8, [r0, #REG8]		/* Save r8 */
	str r9, [r0, #REG9]		/* Save r9 */
	str r10, [r0, #REG10]		/* Save r10 */

	ldr r1, =MCF_MAGIC
	str r1, [r0, #MAGIC]	/* Set magic value */

	mov r1, #0
	str r1, [r0, #REG0]		/* Return 0 */
	mov r0, #0			/* Return 0 */

2:
	bx lr			/* Restore return address */


/* int setcontext(const ucontext_t *ucp)
 *	Restore the user context pointed to by ucp. A successful call to
 *	setcontext does not return; program execution resumes at the point
 *	specified by the ucp argument. If ucp was created with getcontext(),
 *	program execution continues as if the corresponding call of getcontext()
 *	had just returned. If ucp was created with makecontext(), program
 *	execution continues with the function passed to makecontext(). */
ENTRY(setcontext)
	/* In case a process does not use the FPU and is neither interested in
	 * restoring its signal mask, then we can skip the context switch to
	 * PM and kernel altogether and restore state here. */

	/* r0 = ucp */

	/* Check null pointer */
	cmp r0, #0			/* ucp == NULL? */
	bne 3f				/* Not null, continue */
	mov r1, #EFAULT
	ldr r2, =_C_LABEL(errno)
	str r1, [r2]			/* errno = EFAULT */
	mov r0, #-1			/* return -1 */
	bx lr

3:	/* Check flags */
	ldr r1, [r0, #MAGIC]		/* r1 = ucp->mc_context.mc_magic */
	ldr r2, =MCF_MAGIC
	cmp r1, r2		/* is the magic value set (is context valid)?*/
	beq 4f				/* is set, proceed */
	mov r1, #EINVAL			/* not set, return error code */
	ldr r2, =_C_LABEL(errno)
	str r1, [r2]			/* errno = EINVAL */
	mov r0, #-1			/* return -1 */
	bx lr


4:	ldr r1, [r0, #UC_FLAGS]		/* r1 = ucp->uc_flags */
	mov r2, #(UCF_IGNFPU | UCF_IGNSIGM)
	cmp r1, r2		/* Are UCF_IGNFPU and UCF_IGNSIGM flags set? */
	beq 1f			/* Both are set, so don't bother restoring FPU
				 * state and signal mask */

	push {r0, r3}
0:	bl _C_LABEL(setuctx)		/* setuctx(ucp) */
	pop {r0, r3}

1:	/* Restore the registers */
	ldr r1,  [r0, #REG1]		/* Restore r1 */
	ldr r2,  [r0, #REG2]		/* Restore r2 */
	ldr r3,  [r0, #REG3]		/* Restore r3 */
	ldr r4,  [r0, #REG4]		/* Restore r4 */
	ldr r5,  [r0, #REG5]		/* Restore r5 */
	ldr r6,  [r0, #REG6]		/* Restore r6 */
	ldr r7,  [r0, #REG7]		/* Restore r7 */
	ldr r8,  [r0, #REG8]		/* Restore r8 */
	ldr r9,  [r0, #REG9]		/* Restore r9 */
	ldr r10, [r0, #REG10]		/* Restore r10 */
	ldr r12, [r0, #REG12]		/* Restore r12 */
	ldr fp,  [r0, #FPREG]		/* Restore fp */
	ldr sp,  [r0, #SPREG]		/* Restore sp */
	ldr lr,  [r0, #LRREG]		/* Restore lr */
	mov r4, r0
	ldr r0,  [r4, #REG0]		/* Restore r0 */
2:
	ldr pc,  [r4, #PCREG]		/* Restore pc */


/* void ctx_start()
 *	A wrapper to call resumecontext. Makecontext puts the ucp in r4.
 *	This function moves the ucp into r0 so that the ucp is the first
 *	parameter for resumecontext. The call to resumecontext will start
 *	the next context in the linked list (or exit the program if there
 *	is no context). */
ENTRY(ctx_start)
	mov r0, r4
	b _C_LABEL(resumecontext)
