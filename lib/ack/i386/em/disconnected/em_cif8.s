.sect .text; .sect .rom; .sect .data; .sect .bss
.define .cif8

	.sect .text
.cif8:
	mov	bx,sp
	fildl	8(bx)
	fstpd	4(bx)
	wait
	ret
