/*	$NetBSD: compat_scandir.c,v 1.1 2005/09/13 01:44:09 christos Exp $	*/

#include <sys/stat.h>
#define __LIBC12_SOURCE__
#include "namespace.h"
#include <dirent.h>
#include <compat/include/dirent.h>

#ifdef __weak_alias
__weak_alias(scandir,_scandir)
#endif

#ifdef __warn_references
__warn_references(scandir,
    "warning: reference to compatibility scandir(); include <dirent.h> for correct reference")
#endif

#define dirent dirent12

#include "gen/scandir.c"
