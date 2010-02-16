.sect .text
.extern	__getngid
.define	_getngid
.align 2

_getngid:
	jmp	__getngid
