/*	$NetBSD: pipe.S,v 1.1 2014/09/03 19:34:26 matt Exp $	*/

#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: pipe.S,v 1.1 2014/09/03 19:34:26 matt Exp $")
#endif /* LIBC_SCCS && !lint */

#ifdef WEAK_ALIAS
WEAK_ALIAS(pipe, _pipe)
#endif

ENTRY(_pipe)
	l.or	r5,r3,r0	# save pointer
	_DOSYSCALL(pipe)	# assume, that r5 is kept
	l.bf	_C_LABEL(__cerror)
	l.nop
	l.sw	0(r5),r11	# success, store fds
	l.sw	4(r5),r12
	l.xor	r11,r11,r11
	l.jr	lr		# and return 0
	l.nop
END(_pipe)
