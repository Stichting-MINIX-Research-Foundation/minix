.sect .text
.extern	__mktemp
.define	_mktemp
.align 2

_mktemp:
	jmp	__mktemp
