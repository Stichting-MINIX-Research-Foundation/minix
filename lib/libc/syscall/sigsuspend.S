.text
.extern	__sigsuspend
.globl	_sigsuspend
.balign	2

_sigsuspend:
	jmp	__sigsuspend
