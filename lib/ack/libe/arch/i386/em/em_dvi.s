.sect .text; .sect .rom; .sect .data; .sect .bss
.define .dvi

        ! #bytes in eax
	.sect .text
.dvi:
        pop     ebx              ! return address
        cmp     eax,4
        jne     1f
        pop     eax
        cwd
        pop     ecx
        idiv    ecx
        push    eax
        jmp     ebx
1:
.extern EODDZ
.extern .trp
        mov     eax,EODDZ
        push    ebx
        jmp     .trp
