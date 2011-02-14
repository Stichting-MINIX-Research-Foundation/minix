/*	$NetBSD: pipe.S,v 1.1 2006/07/01 16:37:20 ross Exp $	*/

#include "SYS.h"

#ifdef WEAK_ALIAS
WEAK_ALIAS(pipe, _pipe)
#endif

ENTRY(_pipe)
	mr	%r5,%r3		# save pointer
	li	%r0,SYS_pipe
	sc			# assume, that r5 is kept
	bso	1f
	stw	%r3,0(%r5)	# success, store fds
	stw	%r4,4(%r5)
	li	%r3,0
	blr			# and return 0
1:
	b	PIC_PLT(_C_LABEL(__cerror))
