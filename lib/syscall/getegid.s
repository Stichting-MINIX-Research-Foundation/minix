.sect .text
.extern	__getegid
.define	_getegid
.align 2

_getegid:
	jmp	__getegid
