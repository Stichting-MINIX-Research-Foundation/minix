.sect .text; .sect .rom; .sect .data; .sect .bss
.define .mlf8

	.sect .text
.mlf8:
	mov	bx,sp
	fldd	4(bx)
	fmuld	12(bx)
	fstpd	12(bx)
	wait
	ret
