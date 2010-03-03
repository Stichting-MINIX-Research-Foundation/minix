.text
.extern	__sigpending
.globl	_sigpending
.balign	2

_sigpending:
	jmp	__sigpending
