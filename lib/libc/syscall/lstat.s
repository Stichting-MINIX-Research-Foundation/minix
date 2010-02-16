.sect .text
.extern	__lstat
.define	_lstat
.align 2

_lstat:
	jmp	__lstat
