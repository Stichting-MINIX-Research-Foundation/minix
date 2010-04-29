.text
.extern	__munmap
.globl	_munmap
.extern	__munmap_text
.globl	_munmap_text
.balign	2

_munmap:
	jmp	__munmap

_munmap_text:
	jmp	__munmap_text
