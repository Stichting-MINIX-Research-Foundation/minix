.text
.extern	__getpprocnr
.globl	_getpprocnr
.balign	2

_getpprocnr:
	jmp	__getpprocnr
