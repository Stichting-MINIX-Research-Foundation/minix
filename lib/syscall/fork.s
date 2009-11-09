.sect .text
.extern	__fork
.define	_fork
.align 2

_fork:
	jmp	__fork
