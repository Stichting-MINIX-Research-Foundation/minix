.text
.extern	__isatty
.globl	_isatty
.balign	2

_isatty:
	jmp	__isatty
