.sect .text; .sect .rom; .sect .data; .sect .bss
.define .cii

.sect .text
.cii:
	pop     ebx              ! return address
				! pop     ecx, dest. size
				! pop     edx, src. size
				! eax is source
	cmp	edx,1
	jne	2f
	movsxb	eax,al
	mov	edx,4
	jmp	1f
2:
	cmp	edx,2
	jne	1f
	cwde			! convert from 2 to 4 bytes
	mov	edx,4
1:
	cmp     edx,ecx
	jne     9f
	cmp	edx,4
	jne	9f
	jmp     ebx
9:
.extern EILLINS
.extern .fat
	mov     eax,EILLINS
	push    eax
	jmp     .fat
