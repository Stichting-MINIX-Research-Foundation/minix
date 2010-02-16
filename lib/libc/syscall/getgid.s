.sect .text
.extern	__getgid
.define	_getgid
.align 2

_getgid:
	jmp	__getgid
