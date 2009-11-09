.sect .text
.extern	__getpid
.define	_getpid
.align 2

_getpid:
	jmp	__getpid
