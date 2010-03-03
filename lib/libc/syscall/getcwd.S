.text
.extern	__getcwd
.globl	_getcwd
.balign	2

_getcwd:
	jmp	__getcwd
