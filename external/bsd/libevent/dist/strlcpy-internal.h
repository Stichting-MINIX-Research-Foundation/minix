/*	$NetBSD: strlcpy-internal.h,v 1.1.1.2 2013/04/11 16:43:25 christos Exp $	*/
#ifndef _STRLCPY_INTERNAL_H_
#define _STRLCPY_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "event2/event-config.h"

#ifndef _EVENT_HAVE_STRLCPY
#include <string.h>
size_t _event_strlcpy(char *dst, const char *src, size_t siz);
#define strlcpy _event_strlcpy
#endif

#ifdef __cplusplus
}
#endif

#endif

