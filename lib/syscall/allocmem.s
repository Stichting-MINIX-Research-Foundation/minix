.sect .text
.extern	__allocmem
.define	_allocmem
.align 2

_allocmem:
	jmp	__allocmem
