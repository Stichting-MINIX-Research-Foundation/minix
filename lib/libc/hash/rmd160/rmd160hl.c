/* $NetBSD: rmd160hl.c,v 1.6 2008/04/13 02:04:32 dholland Exp $ */

/*
 * Derived from code written by Jason R. Thorpe <thorpej@NetBSD.org>,
 * April 29, 1997.
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: rmd160hl.c,v 1.6 2008/04/13 02:04:32 dholland Exp $");
#endif /* LIBC_SCCS and not lint */

#define	HASH_ALGORITHM	RMD160
#define HASH_INCLUDE	<sys/rmd160.h>

#include "../hashhl.c"
