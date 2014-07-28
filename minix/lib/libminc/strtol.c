/* Adapted from common/lib/libc/stdlib/strtoul.c */
#include <sys/cdefs.h>

#include <sys/param.h>
#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>

#define	_FUNCNAME	strtol
#define	__INT		long
#define	__INT_MIN	LONG_MIN
#define	__INT_MAX	LONG_MAX

long    strtol(const char * __restrict, char ** __restrict, int);

#include "_strtol.h"
