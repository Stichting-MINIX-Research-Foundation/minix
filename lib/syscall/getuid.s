.sect .text
.extern	__getuid
.define	_getuid
.align 2

_getuid:
	jmp	__getuid
