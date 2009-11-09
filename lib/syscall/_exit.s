.sect .text
.extern	___exit
.define	__exit
.align 2

__exit:
	jmp	___exit
