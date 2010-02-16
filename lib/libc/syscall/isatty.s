.sect .text
.extern	__isatty
.define	_isatty
.align 2

_isatty:
	jmp	__isatty
