.text
.extern	__uname
.globl	_uname
.balign	2

_uname:
	jmp	__uname
