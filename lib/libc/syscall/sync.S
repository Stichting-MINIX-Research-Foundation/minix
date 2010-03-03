.text
.extern	__sync
.globl	_sync
.balign	2

_sync:
	jmp	__sync
