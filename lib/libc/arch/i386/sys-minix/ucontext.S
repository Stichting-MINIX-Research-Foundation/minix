#include <machine/asm.h>
#include <ucontextoffsets.h>

#ifdef __ACK__
.text
begtext:
#ifdef __ACK__
.rom
#else
.data
#endif
begrom:
.data
begdata:
.bss
begbss:
#endif


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

	mov (%esp), %ecx	/* Save return address:
				 * When setcontext or swapcontext is called,
				 * we jump to this address and continue
				 * running. */

	mov 4(%esp), %edx		/* edx = ucp */
	/* Check null pointer */
	cmp $0, %edx			/* edx == NULL? */
	jne 3f				/* Not null, continue */
	movl $EFAULT, (_C_LABEL(errno))
	xor %eax, %eax
	dec %eax			/* return -1 */
	ret
	
3:	/* Check flags */
	push %ecx			/* save ecx */
	push %ebx			/* save ebx */
	lea UC_FLAGS(%edx), %ebx	/* ebx = &(ucp->uc_flags) */
	mov (%ebx), %ecx		/* ecx = ucp->uc_flags */
	mov $UCF_IGNFPU, %eax
	or $UCF_IGNSIGM, %eax
	cmp %eax, %ecx			/* is UCF_IGNFPU or UCF_IGNSIGM set? */
	pop %ebx			/* restore ebx */
	pop %ecx			/* restore ecx */
	jz 1f				/* Both are set, skip getuctx */

0:
	push %ecx			/* Save ecx */
	push %edx		 
	call _C_LABEL(getuctx)		/* getuctx(ucp) */
	pop %edx			/* clean up stack and restore edx */
	pop %ecx			/* Restore ecx */

1: 
	/* Save the context */
	mov 4(%esp), %edx		/* edx = ucp */
	pop %eax			/* retaddr */
	mov %eax, PC(%edx)		/* Save real RTA in mcp struct */
	mov %esp, SP(%edx)	/* Save stack pointer (now pointing to ucp) */
	/* Save GP registers */
	mov %ebp, BP(%edx)		/* Save EBP */
	mov %esi, SI(%edx)		/* Save ESI */
	mov %edi, DI(%edx)		/* Save EDI */
	mov %ebx, BX(%edx)		/* Save EBX */
	mov %ecx, CX(%edx)		/* Save ECX */
	movl $MCF_MAGIC, MAGIC(%edx)	/* Set magic value */
	push %eax			/* Restore retaddr */

	xor %eax, %eax			/* Return 0 */

2:	
	add $4, %esp			/* Remove stale (setcontext) RTA */
	jmp *%ecx			/* Restore return address */
	

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
	movl $EFAULT, (_C_LABEL(errno))
	xor %eax, %eax
	dec %eax			/* return -1 */
	ret
	
3:	/* Check flags */
	push %ebx			/* save ebx */
	lea MAGIC(%edx), %ebx		/* ebx = &(ucp->mc_context.mc_magic) */
	mov (%ebx), %ecx		/* ecx = ucp->mc_context.mc_magic */
	pop %ebx			/* restore ebx */
	cmp $MCF_MAGIC, %ecx	/* is the magic value set (is context valid)?*/
	jz 4f				/* is set, proceed */
	movl $EINVAL, (_C_LABEL(errno)) /* not set, return error code */
	xor %eax, %eax
	dec %eax			/* return -1 */
	ret	


4:	push %ebx			/* save ebx */
	lea UC_FLAGS(%edx), %ebx 	/* ebx = &(ucp->uc_flags) */
	mov (%ebx), %ecx		/* ecx = ucp->uc_flags */
	pop %ebx			/* restore ebx */
	mov $UCF_IGNFPU, %eax
	or $UCF_IGNSIGM, %eax
	cmp %eax, %ecx		/* Are UCF_IGNFPU and UCF_IGNSIGM flags set? */
	jz 1f			/* Both are set, so don't bother restoring FPU
				 * state and signal mask */

0:	push %ecx			/* Save ecx */
	push %edx		 
	call _C_LABEL(setuctx)		/* setuctx(ucp) */
	pop %edx			/* Clean up stack and restore edx */
	pop %ecx			/* Restore ecx */

1:	/* Restore the registers */
	mov 4(%esp), %edx		/* edx = ucp */
	mov CX(%edx), %ecx		/* Restore ECX */
	mov BX(%edx), %ebx		/* Restore EBX */
	mov DI(%edx), %edi		/* Restore EDI */
	mov SI(%edx), %esi		/* Restore ESI */
	mov BP(%edx), %ebp		/* Restore EBP */
	mov SP(%edx), %esp		/* Restore stack pointer */

2:
	jmp *PC(%edx)  	/* Push RTA onto stack so we can return to it */


/* void ctx_start((void *func)(int arg1, ..., argn), arg1, ..., argn,
 *		  ucontext_t *ucp)
 *	A wrapper to start function `func'. ESI register will contain a pointer
 *	to ucp on the stack. By setting ESP to ESI, we effectively 'remove' all
 *	arguments to `func' from the stack. Finally, a call to resumecontext
 *	will start the next context in the linked list (or exit the program if
 *	there is no context). */
ENTRY(ctx_start)
	/* 0(esp) -> func
	 * 4(esp) -> arg1
	 * ...
	 * 4*n(esp) -> argn
	 * 4*(n+1)(esp) -> ucp */

	pop %eax			/* eax = func */
	call *%eax			/* func(arg1, ..., argn) */
	mov %esi, %esp			/* Clean up stack */
	/* ucp is now at the top of the stack again */
	call _C_LABEL(resumecontext)	/* resumecontext(ucp) */
	ret			/* never reached */


