/*	fpu_cw_get() - get FPU control word	Author: Erik van der Kouwe */
/*	fpu_cw_set() - set FPU control word	9 Dec 2009 */
#include <machine/asm.h>

/* u16_t fpu_cw_get(void) */
ENTRY(fpu_cw_get)
/* clear unused bits just to be sure */
	xor	%eax, %eax
	push	%eax
	fstcw	(%esp)
	pop	%eax
	ret

/* void fpu_cw_set(u16_t fpu_cw) */
ENTRY(fpu_cw_set)
/* load control word from parameter */
	fldcw	4(%esp)
	ret
