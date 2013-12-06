/*	$NetBSD: compat_glob.c,v 1.4 2013/10/04 21:07:37 christos Exp $	*/

/*
 * Written by Jason R. Thorpe <thorpej@NetBSD.org>, October 21, 1997.
 * Public domain.
 */
#include "namespace.h"
#include <pwd.h>
#define __LIBC12_SOURCE__

#include <sys/stat.h>
#include <compat/sys/time.h>
#include <compat/sys/stat.h>
#include <dirent.h>
#include <compat/include/dirent.h>
#define __gl_stat_t struct stat12

#include <glob.h>
#include <compat/include/glob.h>

#ifdef __weak_alias
__weak_alias(glob,_glob)
__weak_alias(globfree,_globfree)
#endif /* __weak_alias */

__warn_references(glob,
    "warning: reference to compatibility glob(); include <glob.h> for correct reference")
__warn_references(globfree,
    "warning: reference to compatibility globfree(); include <glob.h> for correct reference")

#define stat __compat_stat
#define lstat __compat_lstat
#define fstat __compat_fstat

#include "gen/glob.c"
