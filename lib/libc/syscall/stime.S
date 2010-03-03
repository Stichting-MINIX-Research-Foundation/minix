.text
.extern	__stime
.globl	_stime
.balign	2

_stime:
	jmp	__stime
