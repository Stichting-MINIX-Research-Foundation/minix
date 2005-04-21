.sect .text
.extern	__fcntl
.define	_fcntl
.align 2

_fcntl:
	jmp	__fcntl
