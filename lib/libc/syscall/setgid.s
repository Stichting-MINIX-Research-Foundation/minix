.sect .text
.extern	__setgid
.define	_setgid
.define	_setegid
.align 2

_setgid:
	jmp	__setgid

_setegid:
	jmp	__setegid
