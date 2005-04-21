!	htonX(), ntohX() - Host to network byte order conversion
!							Author: Kees J. Bot
!								7 Jan 1997
!
! This is a little endian 8086, so we swap bytes to/from the big endian network
! order.  The normal <net/hton.h> macros are not used, they give lousy code.

.text
.define _htons, _ntohs
_htons:
_ntohs:
	mov	bx, sp
	movb	ah, 2(bx)	! Load bytes into ax in reverse order
	movb	al, 3(bx)
	ret

.define _htonl, _ntohl
_htonl:
_ntohl:
	mov	bx, sp
	movb	dh, 2(bx)	! Load bytes into dx:ax in reverse order
	movb	dl, 3(bx)
	movb	ah, 4(bx)
	movb	al, 5(bx)
	ret
