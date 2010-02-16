.sect .text
.extern	__sigreturn
.define	_sigreturn
.align 2

_sigreturn:
	jmp	__sigreturn
