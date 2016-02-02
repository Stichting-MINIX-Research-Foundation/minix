/* $NetBSD: sha224hl.c,v 1.2 2014/12/11 21:54:13 riastradh Exp $ */

/*
 * Derived from code written by Jason R. Thorpe <thorpej@NetBSD.org>,
 * May 20, 2009.
 * Public domain.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: sha224hl.c,v 1.2 2014/12/11 21:54:13 riastradh Exp $");

#define	HASH_ALGORITHM	SHA224
#define	HASH_FNPREFIX	SHA224_
#define HASH_INCLUDE	<sys/sha2.h>

#include "../hashhl.c"
