/*	$NetBSD: compat_readdir.c,v 1.1 2005/09/13 01:44:09 christos Exp $	*/

#define __LIBC12_SOURCE__
#include "namespace.h"
#include <dirent.h>
#include <compat/include/dirent.h>

#ifdef __weak_alias
__weak_alias(readdir,_readdir)
__weak_alias(readdir_r,_readdir_r)
#endif

#ifdef __warn_references
__warn_references(readdir,
    "warning: reference to compatibility readdir(); include <dirent.h> for correct reference")
__warn_references(readdir_r,
    "warning: reference to compatibility readdir_r(); include <dirent.h> for correct reference")
#endif

#define dirent dirent12

#include "gen/readdir.c"
