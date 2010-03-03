.text
.extern	__closedir
.globl	_closedir
.balign	2

_closedir:
	jmp	__closedir
