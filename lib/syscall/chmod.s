.sect .text
.extern	__chmod
.define	_chmod
.align 2

_chmod:
	jmp	__chmod
