.sect .text
.extern	__lseek
.define	_lseek
.align 2

_lseek:
	jmp	__lseek
