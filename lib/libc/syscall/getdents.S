.text
.extern	__getdents
.globl	_getdents
.balign	2

_getdents:
	jmp	__getdents
