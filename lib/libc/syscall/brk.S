.text
.extern	__brk
.globl	_brk
.balign	2

_brk:
	jmp	__brk
