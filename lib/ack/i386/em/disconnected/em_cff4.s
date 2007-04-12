.sect .text; .sect .rom; .sect .data; .sect .bss
.define .cff4

	.sect .text
.cff4:
	mov	bx,sp
	fldd	4(bx)
	fstcw	4(bx)
	wait
	mov	dx,4(bx)
	and	4(bx),0xf3ff	! set to rounding mode
	wait
	fldcw	4(bx)
	fstps	8(bx)
	mov	4(bx),dx
	wait
	fldcw	4(bx)
	wait
	ret
