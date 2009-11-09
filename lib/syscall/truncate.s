.sect .text
.extern	__truncate
.extern	__ftruncate
.define	_truncate
.define	_ftruncate
.align 2

_truncate:
	jmp	__truncate

.align 2
_ftruncate:
	jmp	__ftruncate
