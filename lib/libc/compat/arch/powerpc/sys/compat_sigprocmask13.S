/*	$NetBSD: compat_sigprocmask13.S,v 1.2 2011/01/15 07:31:13 matt Exp $	*/

#include "SYS.h"
#include "assym.h"

#if defined(LIBC_SCCS)
__RCSID("$NetBSD: compat_sigprocmask13.S,v 1.2 2011/01/15 07:31:13 matt Exp $")
#endif

WARN_REFERENCES(sigprocmask, \
    "warning: reference to compatibility sigprocmask(); include <signal.h> for correct reference")

/*
 * sigprocmask(int how, sigset_t *set, sigset_t *oset)
 */
ENTRY(sigprocmask)
	or.	%r4,%r4,%r4	# set == NULL?
	li	%r6,SIG_BLOCK
	beq	1f
	lwz	%r4,0(%r4)	# if not, replace it in r4 with *set
	mr	%r6,%r3
1:
	mr	%r3,%r6		# ... using sigprocmask(SIG_BLOCK)
	_DOSYSCALL(compat_13_sigprocmask13)
	bso	3f
	or.	%r5,%r5,%r5	# check to see of oset requested
	beq	2f		# if oset != NULL,
	stw	%r3,0(%r5)	#    *oset = oldmask
2:
	li	%r3,0
	blr			# in any case, return 0
3:
	b	_C_LABEL(__cerror)
END(sigprocmask)
