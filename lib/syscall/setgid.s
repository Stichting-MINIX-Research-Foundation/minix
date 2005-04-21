.sect .text
.extern	__setgid
.define	_setgid
.align 2

_setgid:
	jmp	__setgid
