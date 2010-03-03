.text
.extern	__write
.globl	_write
.balign	2

_write:
	jmp	__write
