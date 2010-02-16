!	strncmp()					Author: Kees J. Bot
!								1 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! int strncmp(const char *s1, const char *s2, size_t ecx)
!	Compare two strings.
!
.sect .text
.define __strncmp
	.align	16
__strncmp:
	push	ebp
	mov	ebp, esp
	push	esi
	push	edi
	test	ecx, ecx	! Max length is zero?
	je	done
	mov	esi, 8(ebp)	! esi = string s1
	mov	edi, 12(ebp)	! edi = string s2
	cld
compare:
	cmpsb			! Compare two bytes
	jne	done
	cmpb	-1(esi), 0	! End of string?
	je	done
	dec	ecx		! Length limit reached?
	jne	compare
done:	seta	al		! al = (s1 > s2)
	setb	ah		! ah = (s1 < s2)
	subb	al, ah
	movsxb	eax, al		! eax = (s1 > s2) - (s1 < s2), i.e. -1, 0, 1
	pop	edi
	pop	esi
	pop	ebp
	ret
