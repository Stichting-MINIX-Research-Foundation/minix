.sect .text; .sect .rom; .sect .data; .sect .bss
.define .adf4

	.sect .text
.adf4:
	mov	bx,sp
	flds	4(bx)
	fadds	8(bx)
	fstps	8(bx)
	wait
	ret
