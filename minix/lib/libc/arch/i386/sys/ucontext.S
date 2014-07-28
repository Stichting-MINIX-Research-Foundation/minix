#include <machine/asm.h>
#include <ucontextoffsets.h>

IMPORT(getuctx)
IMPORT(setuctx)
IMPORT(resumecontext)

	.globl	_C_LABEL(__errno)

/* int getcontext(ucontext_t *ucp) 
 *	Initialise the structure pointed to by ucp to the current user context
 *	of the calling thread. */
ENTRY(getcontext)
ENTRY(_getcontext)
	/* In case a process does not use the FPU and is neither interested in
	 * saving its signal mask, then we can skip the context switch to
	 * PM and kernel altogether and only save general-purpose registers. */

	mov 4(%esp), %edx		/* edx = ucp */
	/* Check null pointer */
	cmp $0, %edx			/* edx == NULL? */
	jne 3f				/* Not null, continue */
	PIC_PROLOGUE
	call PIC_PLT(_C_LABEL(__errno))
	PIC_EPILOGUE
	movl $EFAULT, (%eax)
	xor %eax, %eax
	dec %eax			/* return -1 */
	ret
	
3:	/* Check flags */
	mov UC_FLAGS(%edx), %eax	/* eax = ucp->uc_flags */
	and $[_UC_IGNFPU|_UC_IGNSIGM], %eax
	cmp $[_UC_IGNFPU|_UC_IGNSIGM], %eax
	jz 5f				/* Ignore both, skip getuctx */
	PIC_PROLOGUE
	push %edx /* push a copy for us */
	push %edx /* push a copy as function argument */
	call PIC_PLT(_C_LABEL(getuctx))	/* getuctx(ucp) */
	pop %edx	/* clean up stack */
	pop %edx	/* clean up stack and  restore edx */
	PIC_EPILOGUE

5:
	/* Save the context */
	pop PC(%edx)			/* Save real RTA in mcp struct */
	mov %esp, SP(%edx)	/* Save stack pointer (now pointing to ucp) */
	/* Save GP registers (except EAX and EDX) */
	mov %ebp, BP(%edx)		/* Save EBP */
	mov %esi, SI(%edx)		/* Save ESI */
	mov %edi, DI(%edx)		/* Save EDI */
	mov %ebx, BX(%edx)		/* Save EBX */
	mov %ecx, CX(%edx)		/* Save ECX */
	movl $MCF_MAGIC, MAGIC(%edx)	/* Set magic value */
	xor %eax, %eax			/* Return 0 */
	jmp *PC(%edx)  			/* Return return address */
	

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

	mov 4(%esp), %edx		/* edx = ucp */

	/* Check null pointer */
	cmp $0, %edx			/* edx == NULL? */
	jnz 3f				/* Not null, continue */
	movl $EFAULT, %edx
0:	push %edx			/* preserve errno */
	PIC_PROLOGUE
	call	PIC_PLT(_C_LABEL(__errno))
	PIC_EPILOGUE
	pop %edx
	movl %edx, (%eax)
	xor %eax, %eax
	dec %eax			/* return -1 */
	ret
	
3:	/* Check flags */
	cmpl $MCF_MAGIC, MAGIC(%edx)	/* is the magic value set (is context valid)?*/
	jz 4f				/* is set, proceed */
	movl $EINVAL, %edx		/* not set, return error code */
	jmp 0b


4:	mov UC_FLAGS(%edx), %eax	/* eax = ucp->uc_flags */
	and $[_UC_IGNFPU|_UC_IGNSIGM], %eax
	cmp $[_UC_IGNFPU|_UC_IGNSIGM], %eax
	jz 5f			/* Ignore both, so don't bother restoring FPU
				 * state and signal mask */

	PIC_PROLOGUE
	push %edx /* push a copy for us */
	push %edx /* push a copy as function argument */
	call PIC_PLT(_C_LABEL(setuctx))	/* setuctx(ucp) */
	pop %edx	/* clean up stack */
	pop %edx	/* clean up stack and  restore edx */
	PIC_EPILOGUE

5:	/* Restore the registers (except EAX and EDX) */
	mov CX(%edx), %ecx		/* Restore ECX */
	mov BX(%edx), %ebx		/* Restore EBX */
	mov DI(%edx), %edi		/* Restore EDI */
	mov SI(%edx), %esi		/* Restore ESI */
	mov BP(%edx), %ebp		/* Restore EBP */
	mov SP(%edx), %esp		/* Restore stack pointer */
	xor %eax, %eax			/* Return 0 */
	jmp *PC(%edx)  			/* Return to RTA */

/* void ctx_start((void *func)(int arg1, ..., argn), arg1, ..., argn,
 *		  ucontext_t *ucp)
 *	A wrapper to start function `func'. ESI register will contain a pointer
 *	to ucp on the stack. By setting ESP to ESI, we effectively 'remove' all
 *	arguments to `func' from the stack. Finally, a call to resumecontext
 *	will start the next context in the linked list (or exit the program if
 *	there is no context).
 *
 * Since PIC needs the EBX register, which is pushed on the stack by
 * PIC_PROLOGUE, we need an extra of salsa here.
 */
ENTRY(ctx_start)
	/* 0(esp) -> func
	 * 4(esp) -> arg1
	 * ...
	 * 4*n(esp) -> argn
	 * 4*(n+1)(esp) -> ucp */

	pop %eax			/* eax = func */
	call *%eax			/* func(arg1, ..., argn) */
	PIC_PROLOGUE		/* may push %ebx, but we do not care */
	mov %esi, %esp		/* Clean up stack, keep %ebx = &GOT */
	/* ucp is now at the top of the stack again */
	call PIC_PLT(_C_LABEL(resumecontext))	/* resumecontext(ucp) */
	ret			/* never reached */


