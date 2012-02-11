/*	$NetBSD: compat_times.c,v 1.2 2009/01/11 02:46:25 christos Exp $	*/

/*
 * Ben Harris, 2002.
 * This file is in the Public Domain.
 */

#define __LIBC12_SOURCE__
#include "namespace.h"
#include <sys/cdefs.h>
#include <time.h>
#include <compat/include/time.h>
#include <sys/times.h>
#include <compat/sys/times.h>
#include <sys/resource.h>
#include <compat/sys/resource.h>

#ifdef __weak_alias
__weak_alias(times,_times)
#endif

__warn_references(times,
    "warning: reference to compatibility times(); include <sys/times.h> for correct reference")

#define __times_rusage struct rusage50
#define __times_timeval struct timeval50

#include "gen/times.c"
