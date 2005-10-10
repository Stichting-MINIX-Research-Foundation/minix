.sect .text; .sect .rom; .sect .data; .sect .bss
.define .cuf8

	.sect .text
.cuf8:
	mov	bx,sp
	fildl	8(bx)
	cmp	8(bx),0
	jge	1f
	fisubl	(bigmin)
	fisubl	(bigmin)
1:
	fstpd	4(bx)
	wait
	ret
