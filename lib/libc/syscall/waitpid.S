.text
.extern	__waitpid
.globl	_waitpid
.balign	2

_waitpid:
	jmp	__waitpid
