/*	$NetBSD: nl_types.h,v 1.3 2014/11/12 15:08:52 joerg Exp $	*/

#if defined(_NLS_PRIVATE)

/* #if defined(__minix) */
/* <sys/cdefs> defines __format_arg, but on some other platforms it doesn't.
 * <nl_types.h> includes <sys/cdefs> because it needs __format_arg. As it might
 * not be defined, we do it here as a work around. */
#ifndef __format_arg
#define __format_arg(fmtarg)	__attribute__((__format_arg__ (fmtarg)))
#endif
/* #endif defined(__minix) */

#include "../../include/nl_types.h"
#elif defined(__GNUC__)
#include_next <nl_types.h>
#endif
