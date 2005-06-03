.sect .text
.extern	__freemem
.define	_freemem
.align 2

_freemem:
	jmp	__freemem
