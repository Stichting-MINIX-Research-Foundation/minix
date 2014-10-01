/*	$NetBSD: tinytest_local.h,v 1.1.1.1 2013/04/11 16:43:32 christos Exp $	*/

#ifdef WIN32
#include <winsock2.h>
#endif

#include "event2/util.h"
#include "util-internal.h"

#ifdef snprintf
#undef snprintf
#endif
#define snprintf evutil_snprintf
