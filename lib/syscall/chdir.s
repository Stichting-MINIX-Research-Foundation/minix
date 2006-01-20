.sect .text
.extern	__chdir
.define	_chdir
.extern	__fchdir
.define	_fchdir
.align 2

_chdir:
	jmp	__chdir
_fchdir:
	jmp	__fchdir
