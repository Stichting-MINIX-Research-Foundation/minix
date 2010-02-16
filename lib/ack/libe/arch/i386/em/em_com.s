.sect .text; .sect .rom; .sect .data; .sect .bss
.define	.com

	! #bytes in ecx
	.sect .text
.com:
	mov	ebx,esp
	add	ebx,4
	sar	ecx,2
1:
	not	(ebx)
	add	ebx,4
	loop	1b
	ret
