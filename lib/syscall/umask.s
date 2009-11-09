.sect .text
.extern	__umask
.define	_umask
.align 2

_umask:
	jmp	__umask
