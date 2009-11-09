.sect .text
.extern	__waitpid
.define	_waitpid
.align 2

_waitpid:
	jmp	__waitpid
