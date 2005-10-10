.sect .text; .sect .rom; .sect .data; .sect .bss
.define .sbf4

	.sect .text
.sbf4:
	mov	bx,sp
	flds	8(bx)
	fsubs	4(bx)
	fstps	8(bx)
	wait
	ret
