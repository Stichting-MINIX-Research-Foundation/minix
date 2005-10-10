.sect .text; .sect .rom; .sect .data; .sect .bss
.define .adf8

	.sect .text
.adf8:
	mov	bx,sp
	fldd	4(bx)
	faddd	12(bx)
	fstpd	12(bx)
	wait
	ret
