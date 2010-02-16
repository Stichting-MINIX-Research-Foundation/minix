! fpu_compare() - compare doubles	Author: Erik van der Kouwe
! fpu_sw_get() - get FPU status      	17 Dec 2009
! fpu_xam() - examine double
.sect .text
.define _fpu_compare
.define _fpu_sw_get
.define _fpu_xam

! u16_t fpu_compare(double x, double y)
_fpu_compare:
	! move the values onto the floating point stack
	fldd	12(esp)
	fldd	4(esp)

	! compare values and return status word
	fcompp
	jmp	_fpu_sw_get

! u16_t fpu_sw_get(void)
_fpu_sw_get:
	! clear unused high-order word and get status word
	xor	eax,	eax
	.data1	0xdf, 0xe0	! fnstsw	ax
	ret
	
! u16_t fpu_xam(double value)
_fpu_xam:
	! move the value onto the floating point stack
	fldd	4(esp)

	! examine value and get status word
	fxam
	call	_fpu_sw_get

	! pop the value
	fstp	st
	ret

