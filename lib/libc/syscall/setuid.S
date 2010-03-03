.text
.extern	__setuid
.globl	_setuid
.globl	_seteuid
.balign	2

_setuid:
	jmp	__setuid

_seteuid:
	jmp	__seteuid
