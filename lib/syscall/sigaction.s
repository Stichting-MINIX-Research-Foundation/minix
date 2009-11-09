.sect .text
.extern	__sigaction
.define	_sigaction
.align 2

_sigaction:
	jmp	__sigaction
