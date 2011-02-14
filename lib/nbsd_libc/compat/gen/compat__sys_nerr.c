/*	$NetBSD: compat__sys_nerr.c,v 1.1 2005/09/13 01:44:09 christos Exp $	*/

/*
 * Written by J.T. Conklin, December 12, 1994
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: compat__sys_nerr.c,v 1.1 2005/09/13 01:44:09 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#if defined(__indr_reference) && !defined(__lint__)
__indr_reference(_sys_nerr, sys_nerr)
__indr_reference(_sys_nerr, __sys_nerr) /* Backwards compat with v.12 */
#endif
/* LINTED empty translation unit */
