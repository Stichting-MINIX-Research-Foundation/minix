.sect .text; .sect .rom; .sect .data; .sect .bss
.define .cuf4

	.sect .text
.cuf4:
	mov	bx,sp
	fildl	8(bx)
	cmp	8(bx),0
	jge	1f
	fisubl	(bigmin)
	fisubl	(bigmin)
1:
	fstps	8(bx)
	wait
	ret
