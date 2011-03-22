/* get_bp.s */
/* */
/* return EBP in EAX */
/* */
/* Created:	Sep 7, 1992 by Philip Homburg */

#include <machine/asm.h>

ENTRY(get_bp)
	movl	%ebp, %eax
	ret

/* $PchId: get_bp.ack.s,v 1.3 1996/02/23 08:30:52 philip Exp $ */
