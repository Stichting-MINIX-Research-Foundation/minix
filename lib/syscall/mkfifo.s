.sect .text
.extern	__mkfifo
.define	_mkfifo
.align 2

_mkfifo:
	jmp	__mkfifo
