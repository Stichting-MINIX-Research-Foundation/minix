.sect .text
.extern	__write
.define	_write
.align 2

_write:
	jmp	__write
