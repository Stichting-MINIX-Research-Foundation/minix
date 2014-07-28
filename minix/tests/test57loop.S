#include <machine/asm.h>

IMPORT(remaining_invocations)
IMPORT(origstate)
IMPORT(newstate)

#define JUNK 0xCC0FFEE0

#define COPY(dest, offset)	\
	mov	$dest, %ebp		;	\
	mov	4*offset(%esp), %ebx	;	\
	mov	%ebx, 4*offset(%ebp) ;

/* Copy the result of a pusha to dest. */
#define COPYA(dest) \
	COPY(dest, 0); COPY(dest, 1); COPY(dest, 2); COPY(dest, 3); \
	COPY(dest, 4); COPY(dest, 5); COPY(dest, 6); COPY(dest, 7); 

/* void check_context_loop() */
ENTRY(check_context_loop)
	/* Save original context so we can restore it. */
	pusha

	/* Put some junk in the registers.
	 * We want to junk the state, and junk it differently per reg,
	 * so it's likelier corruption is actually detected. We can't
	 * touch %esp but we can verify that it doesn't change from its
	 * current value.
	 */
	mov	$JUNK+1, %eax
	mov	$JUNK+2, %ebx
	mov	$JUNK+3, %ecx
	mov	$JUNK+4, %edx
	mov	$JUNK+5, %ebp
	mov	$JUNK+6, %esi
	mov	$JUNK+7, %edi

	/* Save the junked state so we can compare it. */
	pusha
cont:
	/* Check if we're done. */
	cmpl	$0, (_C_LABEL(remaining_invocations))
	jz	done

	/* We're not done. */

	/* Restart loop. */
	jmp	cont

done:
	/* Save the junked, but should be unmodified state
	 * so we can copy it.
	 */
	pusha
	COPYA(_C_LABEL(newstate));
	popa

	/* copy and restore junked state */
	COPYA(_C_LABEL(origstate));
	popa

	/* restore original state and return */
	popa
	ret
