.sect .text; .sect .rom; .sect .data; .sect .bss
.define .mlf4

	.sect .text
.mlf4:
	mov	bx,sp
	flds	4(bx)
	fmuls	8(bx)
	fstps	8(bx)
	wait
	ret
