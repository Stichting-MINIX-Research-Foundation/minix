.text
.extern	__fork
.globl	_fork
.balign	2

_fork:
	jmp	__fork
