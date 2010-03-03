.text
.extern	__pathconf
.globl	_pathconf
.balign	2

_pathconf:
	jmp	__pathconf
