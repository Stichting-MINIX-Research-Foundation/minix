.text
.extern	__geteuid
.globl	_geteuid
.balign	2

_geteuid:
	jmp	__geteuid
