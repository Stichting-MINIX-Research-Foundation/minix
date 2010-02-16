.sect .text
.extern	__rmdir
.define	_rmdir
.align 2

_rmdir:
	jmp	__rmdir
