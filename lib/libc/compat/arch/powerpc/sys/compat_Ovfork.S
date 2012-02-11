/*	$NetBSD: compat_Ovfork.S,v 1.2 2011/01/15 07:31:13 matt Exp $	*/

/*
 * pid = vfork();
 *
 * r4 == 0 in parent process, 1 in child process.
 * r3 == pid of child in parent, pid of parent in child.
 */

#include "SYS.h"

#if defined(LIBC_SCCS)
__RCSID("$NetBSD: compat_Ovfork.S,v 1.2 2011/01/15 07:31:13 matt Exp $")
#endif

WARN_REFERENCES(vfork, \
    "warning: reference to compatibility vfork(); include <unistd.h> for correct reference")

SYSCALL(vfork)
	addi	%r4,%r4,-1	# from 1 to 0 in child, 0 to -1 in parent
	and	%r3,%r3,%r4	# return 0 in child, pid in parent
	blr
END(vfork)
