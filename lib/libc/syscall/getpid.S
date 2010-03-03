.text
.extern	__getpid
.globl	_getpid
.balign	2

_getpid:
	jmp	__getpid
