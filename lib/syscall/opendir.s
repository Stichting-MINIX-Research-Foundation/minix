.sect .text
.extern	__opendir
.define	_opendir
.align 2

_opendir:
	jmp	__opendir
