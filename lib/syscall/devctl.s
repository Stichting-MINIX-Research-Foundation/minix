.sect .text
.extern	__devctl
.define	_devctl
.align 2

_devctl:
	jmp	__devctl
