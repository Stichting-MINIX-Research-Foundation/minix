.sect .text
.extern	__tcflush
.define	_tcflush
.align 2

_tcflush:
	jmp	__tcflush
