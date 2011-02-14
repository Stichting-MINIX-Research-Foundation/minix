/* $NetBSD: sha1hl.c,v 1.4 2008/04/13 02:04:32 dholland Exp $ */

/*
 * Derived from code written by Jason R. Thorpe <thorpej@NetBSD.org>,
 * April 29, 1997.
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: sha1hl.c,v 1.4 2008/04/13 02:04:32 dholland Exp $");
#endif /* LIBC_SCCS and not lint */

#define	HASH_ALGORITHM	SHA1
#define HASH_INCLUDE	<sha1.h>

#include "../hashhl.c"
