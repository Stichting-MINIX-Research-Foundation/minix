.sect .text; .sect .rom; .sect .data; .sect .bss
.define .ngf4

	.sect .text
.ngf4:
	mov	bx,sp
	flds	4(bx)
	fchs
	fstps	4(bx)
	wait
	ret
