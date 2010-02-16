.sect .text
.extern	__sigismember
.define	_sigismember
.align 2

_sigismember:
	jmp	__sigismember
