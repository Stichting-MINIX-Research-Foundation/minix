/*	$NetBSD: compat___fts50.c,v 1.2 2009/10/19 17:52:05 christos Exp $	*/

#include "namespace.h"
#include <sys/cdefs.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>

#define __LIBC12_SOURCE__
__warn_references(__fts_children50,
    "warning: reference to compatibility __fts_children50();"
    " include <fts.h> for correct reference")
__warn_references(__fts_close50,
    "warning: reference to compatibility __fts_close50();"
    " include <fts.h> for correct reference")
__warn_references(__fts_open50,
    "warning: reference to compatibility __fts_open50();"
    " include <fts.h> for correct reference")
__warn_references(__fts_read50,
    "warning: reference to compatibility __fts_read50();"
    " include <fts.h> for correct reference")
__warn_references(__fts_set50,
    "warning: reference to compatibility __fts_set50();"
    " include <fts.h> for correct reference")

#define	__fts_level_t	short
#undef	fts_children
#define	fts_children __fts_children50
#undef	fts_close
#define	fts_close __fts_close50
#undef	fts_open
#define	fts_open  __fts_open50
#undef	fts_read
#define	fts_read __fts_read50
#undef	fts_set
#define	fts_set __fts_set50

#include <fts.h>
#include <compat/include/fts.h>

#define	__FTS_COMPAT_LEVEL

#include "gen/fts.c"
