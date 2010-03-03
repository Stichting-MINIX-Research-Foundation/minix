.text
.extern	__sbrk
.globl	_sbrk
.balign	2

_sbrk:
	jmp	__sbrk
