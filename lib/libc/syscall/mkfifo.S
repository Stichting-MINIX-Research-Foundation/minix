.text
.extern	__mkfifo
.globl	_mkfifo
.balign	2

_mkfifo:
	jmp	__mkfifo
