.sect .text
.extern	__mmap
.define	_mmap
.extern	__munmap
.define	_munmap
.extern	__munmap_text
.define	_munmap_text
.align 2

_mmap:
	jmp	__mmap

_munmap:
	jmp	__munmap

_munmap_text:
	jmp	__munmap_text
