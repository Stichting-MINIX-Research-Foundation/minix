.sect .text
.extern	_longjmp
.define	_siglongjmp
.align 2

_siglongjmp:
	jmp	_longjmp
