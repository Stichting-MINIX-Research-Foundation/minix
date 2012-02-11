/*	$NetBSD: compat___glob13.c,v 1.2 2009/01/11 02:46:25 christos Exp $	*/

/*
 * Written by Jason R. Thorpe <thorpej@NetBSD.org>, October 21, 1997.
 * Public domain.
 */
#include "namespace.h"
#include <pwd.h>
#include <sys/stat.h>
#include <dirent.h>
#define __gl_size_t int

#define __LIBC12_SOURCE__
__warn_references(__glob13,
    "warning: reference to compatibility __glob13(); include <glob.h> for correct reference")
__warn_references(__globfree13,
    "warning: reference to compatibility __globfree13(); include <glob.h> for correct reference")

#undef glob
#undef globfree
#define glob __glob13
#define globfree __globfree13

#include <glob.h>
#include <compat/include/glob.h>

#include "gen/glob.c"
