.sect .text
.extern	__execvp
.define	_execvp
.align 2

_execvp:
	jmp	__execvp
