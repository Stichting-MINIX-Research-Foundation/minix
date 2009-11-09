!	strncmp()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! int strncmp(const char *s1, const char *s2, size_t cx)
!	Compare two strings.
!
.sect .text
.define __strncmp
__strncmp:
	push	bp
	mov	bp, sp
	push	si
	push	di
	xor	ax, ax		! Prepare return value
	test	cx, cx		! Max length is zero?
	je	equal
	mov	si, 4(bp)	! si = string s1
	mov	di, 6(bp)	! di = string s2
	cld
compare:
	cmpsb			! Compare two bytes
	jne	unequal
	cmpb	-1(si), #0	! End of string?
	je	equal
	dec	cx		! Length limit reached?
	jne	compare
	jmp	equal
unequal:
	ja	after
	sub	ax, #2		! if (s1 < s2) ax -= 2;
after:	inc	ax		! ax++, now it's -1 or 1
equal:	pop	di
	pop	si
	pop	bp
	ret
