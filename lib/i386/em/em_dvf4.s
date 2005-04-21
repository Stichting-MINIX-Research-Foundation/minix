.sect .text; .sect .rom; .sect .data; .sect .bss
.define .dvf4

	.sect .text
.dvf4:
	mov	bx,sp
	flds	8(bx)
	fdivs	4(bx)
	fstps	8(bx)
	wait
	ret
