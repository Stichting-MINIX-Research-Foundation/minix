.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .set

	! #bytes in ecx
	! bit # in eax
.set:
	pop     ebx              ! return address
	xor     edx,edx
!ifdef create set
	sub	esp,ecx
	push	ebx
	push	edi
	mov     ebx,esp
	xor	edi,edi
	sar	ecx,2
1:
	mov     8(ebx)(edi),edx
	add	edi,4
	loop	1b
!endif
	mov     ebx,8
	div     ebx
	cmp     eax,edi
	jae     2f
	mov	edi,edx
	movb	dl,bits(edi)
	mov     edi,esp
	add     edi,eax
	orb     8(edi),dl
	pop	edi
	ret
2:
.extern ESET
.extern .trp
	pop	edi
	mov     eax,ESET
	jmp     .trp

	.sect .rom
bits:
	.data1   1,2,4,8,16,32,64,128
