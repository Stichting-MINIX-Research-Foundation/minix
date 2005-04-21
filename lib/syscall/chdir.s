.sect .text
.extern	__chdir
.define	_chdir
.align 2

_chdir:
	jmp	__chdir
