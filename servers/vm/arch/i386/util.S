#include <machine/asm.h>

/**===========================================================================* */
/**                              i386_invlpg                                  * */
/**===========================================================================* */
/* PUBLIC void i386_invlpg(u32_t addr) */
/* Tell the processor to invalidate a tlb entry at virtual address addr. */
ENTRY(i386_invlpg)
	push	%ebp
	mov	%esp, %ebp
	push	%eax

	mov	8(%ebp), %eax
#ifdef __ACK__
	invlpg	%eax
#else
	invlpg	(%eax)
#endif

	pop	%eax
	pop	%ebp
	ret
