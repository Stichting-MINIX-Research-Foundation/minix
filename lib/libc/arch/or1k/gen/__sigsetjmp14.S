/*	$NetBSD: __sigsetjmp14.S,v 1.1 2014/09/03 19:34:25 matt Exp $	*/

#include "SYS.h"
#include "assym.h"

#if defined(LIBC_SCCS)
__RCSID("$NetBSD: __sigsetjmp14.S,v 1.1 2014/09/03 19:34:25 matt Exp $")
#endif

ENTRY(__sigsetjmp14)
	l.sfeqi	r4, 0
	l.bf	PLT(_C_LABEL(_setjmp))
	l.nop
	l.j	PLT(_C_LABEL(__setjmp14))
	l.nop
END(__sigsetjmp14)

ENTRY(__siglongjmp14)
	l.lwz	r5, JB_MAGIC(r3)
	l.sfeqi	r5, 0
	l.bf	PLT(_C_LABEL(_longjmp))
	l.nop
	l.j	PLT(_C_LABEL(__longjmp14))
	l.nop
END(__siglongjmp14)
