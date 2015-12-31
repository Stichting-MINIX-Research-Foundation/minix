#include "namespace.h"
#include <sys/cdefs.h>
#define ISC_FORMAT_PRINTF(a,b) __attribute__((__format__(__printf__,a,b)))
#define ISC_SOCKLEN_T	socklen_t
#ifdef __NetBSD__
#define DE_CONST(c,v)	v = __UNCONST(c)
#else
#define DE_CONST(c,v)	v = ((c) ? \
	strchr((const void *)(c), *(const char *)(const void *)(c)) : NULL)
#endif
#ifndef lint
#define UNUSED(a)	(void)&a
#else
#define UNUSED(a)	a = a
#endif
