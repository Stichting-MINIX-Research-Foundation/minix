! get_bp.s
!
! return BP in AX
!
! Created:	Sep 7, 1992 by Philip Homburg

.sect .text; .sect .rom; .sect .data; .sect .bss

.sect .text
.define _get_bp
_get_bp:
	mov	ax, bp
	ret

! $PchId: get_bp.ack.s,v 1.3 1996/02/23 08:27:48 philip Exp $
