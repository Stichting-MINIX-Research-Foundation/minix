
.define _last_cr3

#define LOADCR3WITHEAX(type, newcr3, ptproc)	;\
sseg	inc	(_cr3switch)		;\
sseg	mov	eax,	newcr3		;\
sseg	cmp	(_last_cr3), eax	;\
	jz	8f			;\
	mov	cr3, eax		;\
sseg	inc	(_cr3reload)		;\
sseg	mov	(_last_cr3), eax	;\
sseg	mov	eax, (ptproc)		;\
sseg	mov	(_ptproc), eax		;\
8:

