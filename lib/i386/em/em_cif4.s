.sect .text; .sect .rom; .sect .data; .sect .bss
.define .cif4

	.sect .text
.cif4:
	mov	bx,sp
	fildl	8(bx)
	fstps	8(bx)
	wait
	ret
