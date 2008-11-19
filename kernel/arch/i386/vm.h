
.define _load_kernel_cr3
.define _last_cr3

#define LOADKERNELCR3			;\
	inc	(_cr3switch)		;\
	mov	eax,	(_kernel_cr3)	;\
	cmp	(_last_cr3), eax	;\
	jz	9f			;\
	push	_load_kernel_cr3	;\
	call	_level0			;\
	pop	eax			;\
	mov	eax,	(_kernel_cr3)	;\
	mov	(_last_cr3), eax	;\
	inc	(_cr3reload)		;\
9:

#define LOADCR3WITHEAX(type, newcr3)	;\
sseg	inc	(_cr3switch)		;\
sseg	mov	eax,	newcr3		;\
sseg	cmp	(_last_cr3), eax	;\
	jz	8f			;\
	mov	cr3, eax		;\
sseg	inc	(_cr3reload)		;\
sseg	mov	(_last_cr3), eax	;\
8:

