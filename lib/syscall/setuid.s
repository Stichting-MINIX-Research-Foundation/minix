.sect .text
.extern	__setuid
.define	_setuid
.align 2

_setuid:
	jmp	__setuid
