/*	$NetBSD: compat__sys_errlist.c,v 1.1 2005/09/13 01:44:09 christos Exp $	*/

/*
 * Written by J.T. Conklin, December 12, 1994
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: compat__sys_errlist.c,v 1.1 2005/09/13 01:44:09 christos Exp $");
#endif /* LIBC_SCCS and not lint */

__warn_references(sys_errlist,
    "warning: reference to compatibility sys_errlist[]; include <errno.h> for correct reference")
__warn_references(__sys_errlist,
    "warning: reference to deprecated __sys_errlist[]; include <errno.h> and use sys_errlist")

__warn_references(sys_nerr,
    "warning: reference to compatibility sys_nerr; include <errno.h> for correct reference")
__warn_references(__sys_nerr,
    "warning: reference to deprecated __sys_nerr; include <errno.h> and use sys_nerr")
 

#if defined(__indr_reference)
__indr_reference(_sys_errlist, sys_errlist)
__indr_reference(_sys_errlist, __sys_errlist) /* Backwards compat with v.12 */
#else

#undef _sys_errlist
#undef _sys_nerr
#undef rcsid
#define	_sys_errlist	sys_errlist
#define	_sys_nerr	sys_nerr
#define	rcsid		_rcsid
#include "__errlist.c"

#undef _sys_errlist
#undef _sys_nerr
#undef rcsid
#define	_sys_errlist	__sys_errlist
#define	_sys_nerr	__sys_nerr
#define	rcsid		__rcsid
#include "__errlist.c"

#endif
