.sect .text
.extern	__dup2
.define	_dup2
.align 2

_dup2:
	jmp	__dup2
