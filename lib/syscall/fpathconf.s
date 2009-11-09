.sect .text
.extern	__fpathconf
.define	_fpathconf
.align 2

_fpathconf:
	jmp	__fpathconf
