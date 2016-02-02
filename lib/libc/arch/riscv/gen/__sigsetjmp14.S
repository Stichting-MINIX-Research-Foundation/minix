/*	$NetBSD: __sigsetjmp14.S,v 1.2 2015/03/27 23:23:14 matt Exp $	*/

#include "SYS.h"
#include "assym.h"

#if defined(LIBC_SCCS)
__RCSID("$NetBSD: __sigsetjmp14.S,v 1.2 2015/03/27 23:23:14 matt Exp $")
#endif

ENTRY(__sigsetjmp14)
	bnez	a1, 1f
	tail	PLT(_C_LABEL(_setjmp))
1:	tail	PLT(_C_LABEL(__setjmp14))
END(__sigsetjmp14)

ENTRY(__siglongjmp14)
	REG_L	a2, JB_MAGIC(a0)
	bnez	a2, 1f
	tail	PLT(_C_LABEL(_longjmp))
1:	tail	PLT(_C_LABEL(__longjmp14))
END(__siglongjmp14)
