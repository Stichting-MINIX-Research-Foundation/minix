.sect .text
.extern	__seekdir
.define	_seekdir
.align 2

_seekdir:
	jmp	__seekdir
