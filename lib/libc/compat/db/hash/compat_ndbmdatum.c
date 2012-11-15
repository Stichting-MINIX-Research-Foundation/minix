/*	$NetBSD: compat_ndbmdatum.c,v 1.2 2012/03/13 21:13:31 christos Exp $	*/

/*
 * Written by Klaus Klein <kleink@NetBSD.org>, April 28, 2004.
 * Public domain.
 */

#define __LIBC12_SOURCE__
#include "namespace.h"
#include <sys/cdefs.h>
#include <ndbm.h>
#include <compat/include/ndbm.h>

__warn_references(dbm_delete,
    "warning: reference to compatibility dbm_delete();"
    " include <ndbm.h> for correct reference")
__warn_references(dbm_fetch,
    "warning: reference to compatibility dbm_fetch();"
    " include <ndbm.h> for correct reference")
__warn_references(dbm_firstkey,
    "warning: reference to compatibility dbm_firstkey();"
    " include <ndbm.h> for correct reference")
__warn_references(dbm_nextkey,
    "warning: reference to compatibility dbm_nextkey();"
    " include <ndbm.h> for correct reference")
__warn_references(dbm_store,
    "warning: reference to compatibility dbm_store();"
    " include <ndbm.h> for correct reference")

#define datum datum12
#define datum_truncate(a)	((a) > INT_MAX ? INT_MAX : (int)(a))

#include "db/hash/ndbmdatum.c"
