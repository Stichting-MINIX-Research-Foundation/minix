.text
.extern	__mmap
.globl	_mmap
.extern	__munmap
.globl	_munmap
.extern	__munmap_text
.globl	_munmap_text
.balign	2

_mmap:
	jmp	__mmap

_munmap:
	jmp	__munmap

_munmap_text:
	jmp	__munmap_text
