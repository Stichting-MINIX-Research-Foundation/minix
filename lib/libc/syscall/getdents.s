.sect .text
.extern	__getdents
.define	_getdents
.align 2

_getdents:
	jmp	__getdents
