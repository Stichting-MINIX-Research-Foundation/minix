.text
.extern	___exit
.globl	__exit
.balign	2

__exit:
	jmp	___exit
