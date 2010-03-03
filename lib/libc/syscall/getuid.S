.text
.extern	__getuid
.globl	_getuid
.balign	2

_getuid:
	jmp	__getuid
