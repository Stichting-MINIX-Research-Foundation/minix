.sect .text
.extern	__execle
.define	_execle
.align 2

_execle:
	jmp	__execle
