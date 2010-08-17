/* fpu_compare() - compare doubles	Author: Erik van der Kouwe */
/* fpu_sw_get() - get FPU status      	17 Dec 2009 */
/* fpu_xam() - examine double */
#include <machine/asm.h>

/* u16_t fpu_compare(double x, double y) */
ENTRY(fpu_compare)
/* move the values onto the floating point stack */
	fldl	12(%esp)
	fldl	4(%esp)

/* compare values and return status word */
	fcompp
	jmp	_C_LABEL(fpu_sw_get)

/* u16_t fpu_sw_get(void) */
ENTRY(fpu_sw_get)
/* clear unused high-order word and get status word */
	xor	%eax, %eax
.byte	0xdf, 0xe0	/* fnstsw	ax */
	ret

/* u16_t fpu_xam(double value) */
ENTRY(fpu_xam)
/* move the value onto the floating point stack */
	fldl	4(%esp)

/* examine value and get status word */
	fxam
	call	_C_LABEL(fpu_sw_get)

/* pop the value */
	fstp	%st
	ret
