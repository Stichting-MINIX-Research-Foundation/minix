.sect .text
.extern	__execl
.define	_execl
.align 2

_execl:
	jmp	__execl
