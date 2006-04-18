.sect .text
.extern	__fchmod
.define	_fchmod
.align 2

_fchmod:
	jmp	__fchmod
