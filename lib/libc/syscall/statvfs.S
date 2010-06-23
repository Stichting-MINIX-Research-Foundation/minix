.text
.extern	__statvfs
.globl	_statvfs
.balign	2

_statvfs:
	jmp	__statvfs
