.text
.extern	__chdir
.globl	_chdir
.extern	__fchdir
.globl	_fchdir
.balign	2

_chdir:
	jmp	__chdir
_fchdir:
	jmp	__fchdir
