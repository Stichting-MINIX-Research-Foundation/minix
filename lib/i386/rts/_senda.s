.sect .text; .sect .rom; .sect .data; .sect .bss
.define __senda

SENDA = 16
SYSVEC = 33

MSGTAB = 8			! message table
TABCOUNT = 12			! number of entries in message table

.sect .text

__senda:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, TABCOUNT(ebp)	! eax = count
	mov	ebx, MSGTAB(ebp)	! ebx = table
	mov	ecx, SENDA		! _senda(table, count)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret
