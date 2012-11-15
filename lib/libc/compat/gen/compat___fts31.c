/*	$NetBSD: compat___fts31.c,v 1.4 2012/03/14 00:25:19 christos Exp $	*/

#include "namespace.h"
#include <sys/cdefs.h>
#include <dirent.h>

#define	__LIBC12_SOURCE__

__warn_references(__fts_children31,
    "warning: reference to compatibility __fts_children31();"
    " include <fts.h> for correct reference")
__warn_references(__fts_close31,
    "warning: reference to compatibility __fts_close31();"
    " include <fts.h> for correct reference")
__warn_references(__fts_open31,
    "warning: reference to compatibility __fts_open31();"
    " include <fts.h> for correct reference")
__warn_references(__fts_read31,
    "warning: reference to compatibility __fts_read31();"
    " include <fts.h> for correct reference")
__warn_references(__fts_set31,
    "warning: reference to compatibility __fts_set31();"
    " include <fts.h> for correct reference")

#include <sys/stat.h>
#include <compat/sys/time.h>
#include <compat/sys/stat.h>

#define	__fts_stat_t	struct stat30
#define	__fts_number_t	long
#define	__fts_dev_t	uint32_t
#define	__fts_level_t	short

#ifndef ftsent_namelen_truncate
#define ftsent_namelen_truncate(a)	\
    ((a) > USHRT_MAX ? USHRT_MAX : (unsigned short)(a))
#endif
#ifndef ftsent_pathlen_truncate
#define ftsent_pathlen_truncate(a)	\
    ((a) > USHRT_MAX ? USHRT_MAX : (unsigned short)(a))
#endif

#define	stat		__stat30
#define	lstat		__lstat30
#define	fstat		__fstat30

#undef	fts_children
#define	fts_children __fts_children31
#undef	fts_close
#define	fts_close __fts_close31
#undef	fts_open
#define	fts_open  __fts_open31
#undef	fts_read
#define	fts_read __fts_read31
#undef	fts_set
#define	fts_set __fts_set31

#include <fts.h>
#include <compat/include/fts.h>

#define	__FTS_COMPAT_LEVEL

#include "gen/fts.c"
