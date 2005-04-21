.sect .text
.extern	__sigpending
.define	_sigpending
.align 2

_sigpending:
	jmp	__sigpending
