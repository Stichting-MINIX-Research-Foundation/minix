.text
.extern	__sigreturn
.globl	_sigreturn
.balign	2

_sigreturn:
	jmp	__sigreturn
