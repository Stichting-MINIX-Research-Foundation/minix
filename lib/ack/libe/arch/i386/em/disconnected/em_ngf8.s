.sect .text; .sect .rom; .sect .data; .sect .bss
.define .ngf8

	.sect .text
.ngf8:
	mov	bx,sp
	fldd	4(bx)
	fchs
	fstpd	4(bx)
	wait
	ret
