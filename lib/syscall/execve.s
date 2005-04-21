.sect .text
.extern	__execve
.define	_execve
.align 2

_execve:
	jmp	__execve
