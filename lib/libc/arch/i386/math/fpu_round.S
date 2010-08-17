/*	fpu_rndint() - round integer	Author: Erik van der Kouwe */
/*	                            	17 Dec 2009 */
#include <machine/asm.h>

/* void fpu_rndint(double *value) */
ENTRY(fpu_rndint)
/* move the value onto the floating point stack */
	mov	4(%esp), %eax
	fldl	(%eax)

/* round it (beware of precision exception!) */
	frndint

/* store the result */
	fstpl	(%eax)
	ret

/* void fpu_remainder(double *x, double y) */
ENTRY(fpu_remainder)
/* move the values onto the floating point stack */
	fldl	8(%esp)
	mov	4(%esp), %edx
	fldl	(%edx)

/* compute remainder, multiple iterations may be needed */
1:
	fprem1
.byte	0xdf, 0xe0	/* fnstsw	ax */
	sahf
	jp	1b

/* store the result and pop the divisor */
	fstpl	(%edx)
	fstp	%st
	ret
