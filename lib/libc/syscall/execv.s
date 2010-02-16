.sect .text
.extern	__execv
.define	_execv
.align 2

_execv:
	jmp	__execv
