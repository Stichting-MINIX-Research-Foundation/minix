.sect .text
.extern	__closedir
.define	_closedir
.align 2

_closedir:
	jmp	__closedir
