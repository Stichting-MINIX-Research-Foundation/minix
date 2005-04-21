!	memcmp()					Author: Kees J. Bot
!								2 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! int memcmp(const void *s1, const void *s2, size_t n)
!	Compare two chunks of memory.
!
.sect .text
.define _memcmp
	.align	16
_memcmp:
	cld
	push	ebp
	mov	ebp, esp
	push	esi
	push	edi
	mov	esi, 8(ebp)	! String s1
	mov	edi, 12(ebp)	! String s2
	mov	ecx, 16(ebp)	! Length
	cmp	ecx, 16
	jb	cbyte		! Don't bother being smart with short arrays
	mov	eax, esi
	or	eax, edi
	testb	al, 1
	jnz	cbyte		! Bit 0 set, use byte compare
	testb	al, 2
	jnz	cword		! Bit 1 set, use word compare
clword:	shrd	eax, ecx, 2	! Save low two bits of ecx in eax
	shr	ecx, 2
	repe
	cmps			! Compare longwords
	sub	esi, 4
	sub	edi, 4
	inc	ecx		! Recompare the last longword
	shld	ecx, eax, 2	! And any excess bytes
	jmp	last
cword:	shrd	eax, ecx, 1	! Save low bit of ecx in eax
	shr	ecx, 1
	repe
    o16	cmps			! Compare words
	sub	esi, 2
	sub	edi, 2
	inc	ecx		! Recompare the last word
	shld	ecx, eax, 1	! And one more byte?
cbyte:	test	ecx, ecx	! Set 'Z' flag if ecx = 0
last:	repe
	cmpsb			! Look for the first differing byte
	seta	al		! al = (s1 > s2)
	setb	ah		! ah = (s1 < s2)
	subb	al, ah
	movsxb	eax, al		! eax = (s1 > s2) - (s1 < s2), i.e. -1, 0, 1
	mov	edx, esi	! For bcmp() to play with
	pop	edi
	pop	esi
	pop	ebp
	ret
