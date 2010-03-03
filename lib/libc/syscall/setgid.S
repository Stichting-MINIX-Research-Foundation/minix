.text
.extern	__setgid
.globl	_setgid
.globl	_setegid
.balign	2

_setgid:
	jmp	__setgid

_setegid:
	jmp	__setegid
