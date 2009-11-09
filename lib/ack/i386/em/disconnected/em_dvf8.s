.sect .text; .sect .rom; .sect .data; .sect .bss
.define .dvf8

	.sect .text
.dvf8:
	mov	bx,sp
	fldd	12(bx)
	fdivd	4(bx)
	fstpd	12(bx)
	wait
	ret
