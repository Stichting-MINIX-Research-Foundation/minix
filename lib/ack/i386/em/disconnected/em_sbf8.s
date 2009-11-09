.sect .text; .sect .rom; .sect .data; .sect .bss
.define .sbf8

	.sect .text
.sbf8:
	mov	bx,sp
	fldd	12(bx)
	fsubd	4(bx)
	fstpd	12(bx)
	wait
	ret
