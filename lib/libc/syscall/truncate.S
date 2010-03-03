.text
.extern	__truncate
.extern	__ftruncate
.globl	_truncate
.globl	_ftruncate
.balign	2

_truncate:
	jmp	__truncate

.balign	2
_ftruncate:
	jmp	__ftruncate
