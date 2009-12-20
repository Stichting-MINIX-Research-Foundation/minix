.sect .text
.extern	__setgroups
.define	_setgroups
.align 2

_setgroups:
	jmp	__setgroups
