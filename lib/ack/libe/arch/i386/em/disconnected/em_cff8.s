.sect .text; .sect .rom; .sect .data; .sect .bss
.define .cff8

	.sect .text
.cff8:
	mov	bx,sp
	flds	4(bx)
	fstpd	4(bx)
	wait
	ret
