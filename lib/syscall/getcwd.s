.sect .text
.extern	__getcwd
.define	_getcwd
.align 2

_getcwd:
	jmp	__getcwd
