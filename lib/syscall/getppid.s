.sect .text
.extern	__getppid
.define	_getppid
.align 2

_getppid:
	jmp	__getppid
