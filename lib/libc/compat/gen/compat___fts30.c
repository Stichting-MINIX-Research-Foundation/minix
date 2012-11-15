/*	$NetBSD: compat___fts30.c,v 1.6 2012/03/14 00:25:19 christos Exp $	*/

#include "namespace.h"
#include <sys/cdefs.h>
#include <dirent.h>

#define	__LIBC12_SOURCE__

__warn_references(__fts_children30,
    "warning: reference to compatibility __fts_children30();"
    " include <fts.h> for correct reference")
__warn_references(__fts_close30,
    "warning: reference to compatibility __fts_close30();"
    " include <fts.h> for correct reference")
__warn_references(__fts_open30,
    "warning: reference to compatibility __fts_open30();"
    " include <fts.h> for correct reference")
__warn_references(__fts_read30,
    "warning: reference to compatibility __fts_read30();"
    " include <fts.h> for correct reference")
__warn_references(__fts_set30,
    "warning: reference to compatibility __fts_set30();"
    " include <fts.h> for correct reference")

#include <sys/stat.h>
#include <compat/sys/time.h>
#include <compat/sys/stat.h>

#define	__fts_stat_t	struct stat30
#define	__fts_length_t	u_short
#define	__fts_number_t	long
#define	__fts_dev_t	uint32_t
#define	__fts_level_t	short

#define	stat		__stat30
#define	lstat		__lstat30
#define	fstat		__fstat30

#ifndef ftsent_namelen_truncate
#define ftsent_namelen_truncate(a)	\
    ((a) > USHRT_MAX ? USHRT_MAX : (unsigned short)(a))
#endif
#ifndef ftsent_pathlen_truncate
#define ftsent_pathlen_truncate(a)	\
    ((a) > USHRT_MAX ? USHRT_MAX : (unsigned short)(a))
#endif

#undef	fts_children
#define	fts_children __fts_children30
#undef	fts_close
#define	fts_close __fts_close30
#undef	fts_open
#define	fts_open  __fts_open30
#undef	fts_read
#define	fts_read __fts_read30
#undef	fts_set
#define	fts_set __fts_set30

#include <fts.h>
#include <compat/include/fts.h>

#define	__FTS_COMPAT_LENGTH
#define	__FTS_COMPAT_LEVEL

#include "gen/fts.c"
