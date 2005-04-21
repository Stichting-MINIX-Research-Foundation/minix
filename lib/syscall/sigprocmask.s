.sect .text
.extern	__sigprocmask
.define	_sigprocmask
.align 2

_sigprocmask:
	jmp	__sigprocmask
