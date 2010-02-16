.sect .text
.extern	__lseek64
.define	_lseek64
.align 2

_lseek64:
	jmp	__lseek64
