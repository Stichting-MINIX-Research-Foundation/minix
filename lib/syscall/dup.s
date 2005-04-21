.sect .text
.extern	__dup
.define	_dup
.align 2

_dup:
	jmp	__dup
