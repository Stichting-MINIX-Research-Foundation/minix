.sect .text
.extern	__uname
.define	_uname
.align 2

_uname:
	jmp	__uname
