/*	$NetBSD: compat__sys_siglist.c,v 1.1 2005/09/13 01:44:09 christos Exp $	*/

/*
 * Written by J.T. Conklin, December 12, 1994
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: compat__sys_siglist.c,v 1.1 2005/09/13 01:44:09 christos Exp $");
#endif /* LIBC_SCCS and not lint */

__warn_references(sys_siglist,
    "warning: reference to compatibility sys_siglist[]; include <signal.h> or <unistd.h> for correct reference")
__warn_references(__sys_siglist,
    "warning: reference to deprecated __sys_siglist[]; include <signal.h> or <unistd.h> and use sys_siglist")

#if defined(__indr_reference) && !defined(__lint__)
__indr_reference(_sys_siglist, sys_siglist)
__indr_reference(_sys_siglist, __sys_siglist) /* Backwards compat with v.12 */
#else

#undef _sys_siglist
#undef rcsid
#define	_sys_siglist	sys_siglist
#define	rcsid		_rcsid
#include "compat_siglist.c"

#undef _sys_siglist
#undef rcsid
#define	_sys_siglist	__sys_siglist
#define	rcsid		__rcsid
#include "compat_siglist.c"

#endif
