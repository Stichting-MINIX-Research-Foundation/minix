.text
.extern	__fstatvfs
.globl	_fstatvfs
.balign	2

_fstatvfs:
	jmp	__fstatvfs
