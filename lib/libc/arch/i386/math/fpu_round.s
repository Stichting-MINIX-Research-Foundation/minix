!	fpu_rndint() - round integer	Author: Erik van der Kouwe
!	                            	17 Dec 2009
.sect .text
.define _fpu_rndint
.define _fpu_remainder

! void fpu_rndint(double *value)
_fpu_rndint:
	! move the value onto the floating point stack
	mov	eax,	4(esp)
	fldd	(eax)

	! round it (beware of precision exception!)
	frndint

	! store the result
	fstpd	(eax)
	ret

! void fpu_remainder(double *x, double y)
_fpu_remainder:
	! move the values onto the floating point stack
	fldd	8(esp)
	mov	edx,	4(esp)
	fldd	(edx)

	! compute remainder, multiple iterations may be needed
1:	fprem1
	.data1	0xdf, 0xe0	! fnstsw	ax	
	sahf
	jp	1b

	! store the result and pop the divisor
	fstpd	(edx)
	fstp	st
	ret
