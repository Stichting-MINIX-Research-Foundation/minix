.text
.extern	__getppid
.globl	_getppid
.balign	2

_getppid:
	jmp	__getppid
