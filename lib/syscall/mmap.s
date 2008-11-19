.sect .text
.extern	__mmap
.define	_mmap
.align 2

_mmap:
	jmp	__mmap
