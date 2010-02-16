.sect .text
.extern	__setuid
.define	_setuid
.define	_seteuid
.align 2

_setuid:
	jmp	__setuid

_seteuid:
	jmp	__seteuid
