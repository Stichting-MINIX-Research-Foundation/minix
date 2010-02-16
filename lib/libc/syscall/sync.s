.sect .text
.extern	__sync
.define	_sync
.align 2

_sync:
	jmp	__sync
