/*	$NetBSD: compat_localtime.c,v 1.3 2011/02/21 22:07:44 christos Exp $	*/

/*
 * Written by Jason R. Thorpe <thorpej@NetBSD.org>, October 21, 1997.
 * Public domain.
 */

#include "namespace.h"
#include <sys/cdefs.h>

#define __LIBC12_SOURCE__
#include <time.h>
#include <sys/time.h>
#include <compat/include/time.h>
#include <compat/sys/time.h>

#ifdef __weak_alias
__weak_alias(ctime_r,_ctime_r)
__weak_alias(ctime_rz,_ctime_rz)
__weak_alias(gmtime_r,_gmtime_r)
__weak_alias(localtime_r,_localtime_r)
__weak_alias(localtime_rz,_localtime_rz)
__weak_alias(mktime_z,_mktime_z)
__weak_alias(offtime,_offtime)
__weak_alias(posix2time,_posix2time)
__weak_alias(posix2time_z,_posix2time_z)
__weak_alias(time2posix,_time2posix)
__weak_alias(timegm,_timegm)
__weak_alias(timelocal,_timelocal)
__weak_alias(timeoff,_timeoff)
__weak_alias(tzset,_tzset)
__weak_alias(tzsetwall,_tzsetwall)
#endif

__warn_references(ctime_r,
    "warning: reference to compatibility ctime_r();"
    " include <time.h> for correct reference")
__warn_references(ctime_rz,
    "warning: reference to compatibility ctime_rz();"
    " include <time.h> for correct reference")
__warn_references(gmtime_r,
    "warning: reference to compatibility gmtime_r();"
    " include <time.h> for correct reference")
__warn_references(localtime_r,
    "warning: reference to compatibility localtime_r();"
    " include <time.h> for correct reference")
__warn_references(localtime_rz,
    "warning: reference to compatibility localtime_rz();"
    " include <time.h> for correct reference")
__warn_references(mktime_z,
    "warning: reference to compatibility mktime_z();"
    " include <time.h> for correct reference")
__warn_references(offtime,
    "warning: reference to compatibility offtime();"
    " include <time.h> for correct reference")
__warn_references(posix2time,
    "warning: reference to compatibility posix2time();"
    " include <time.h> for correct reference")
__warn_references(posix2time_z,
    "warning: reference to compatibility posix2time_z();"
    " include <time.h> for correct reference")
__warn_references(time2posix,
    "warning: reference to compatibility time2posix();"
    " include <time.h> for correct reference")
__warn_references(timegm,
    "warning: reference to compatibility timegm();"
    " include <time.h> for correct reference")
__warn_references(timelocal,
    "warning: reference to compatibility timelocal();"
    " include <time.h> for correct reference")
__warn_references(timeoff,
    "warning: reference to compatibility timeoff();"
    " include <time.h> for correct reference")
__warn_references(tzset,
    "warning: reference to compatibility tzset();"
    " include <time.h> for correct reference")
__warn_references(tzsetwall,
    "warning: reference to compatibility tzsetwall();"
    " include <time.h> for correct reference")

#define timeval timeval50
#define timespec timespec50
#define	time_t	int32_t

#include "time/localtime.c"
