.sect .text; .sect .rom; .sect .data; .sect .bss
.define .cfu

	.sect .text
.cfu:
	mov	bx,sp
	fstcw	4(bx)
	wait
	mov	dx,4(bx)
	or	4(bx),0xc00	! truncating mode
	wait
	fldcw	4(bx)
	cmp	8(bx),4
	jne	2f
				! loc 4 loc ? cfu
	flds	12(bx)
	fabs			! ???
	fiaddl	(bigmin)
	fistpl	12(bx)
	wait
	mov	ax,12(bx)
	sub	ax,(bigmin)
	mov	12(bx),ax
1:
	mov	4(bx),dx
	wait
	fldcw	4(bx)
	ret
2:
				! loc 8 loc ? cfu
	fldd	12(bx)
	fabs			! ???
	fiaddl	(bigmin)
	fistpl	16(bx)
	mov	ax,16(bx)
	sub	ax,(bigmin)
	mov	16(bx),ax
	jmp	1b
