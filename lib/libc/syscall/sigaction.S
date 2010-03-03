.text
.extern	__sigaction
.globl	_sigaction
.balign	2

_sigaction:
	jmp	__sigaction
