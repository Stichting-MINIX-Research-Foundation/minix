.text
.extern	__pause
.globl	_pause
.balign	2

_pause:
	jmp	__pause
