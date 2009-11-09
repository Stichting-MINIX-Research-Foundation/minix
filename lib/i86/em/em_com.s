.define	.com

	! #bytes in cx
	.text
.com:
	mov	bx,sp
	inc	bx
	inc	bx
	sar	cx,#1
1:
	not	(bx)
	inc	bx
	inc	bx
	loop	1b
	ret
