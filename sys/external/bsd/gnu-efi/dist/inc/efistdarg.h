/*	$NetBSD: efistdarg.h,v 1.3 2018/08/16 18:22:05 jmcneill Exp $	*/

#ifndef _EFISTDARG_H_
#define _EFISTDARG_H_

/*++

Copyright (c) 1998  Intel Corporation

Module Name:

    devpath.h

Abstract:

    Defines for parsing the EFI Device Path structures



Revision History

--*/

#ifndef GNU_EFI_USE_EXTERNAL_STDARG
#if defined(__NetBSD__) || defined(__minix)
#include <sys/stdarg.h>
#else
typedef __builtin_va_list va_list;

# define va_start(v,l)	__builtin_va_start(v,l)
# define va_end(v)	__builtin_va_end(v)
# define va_arg(v,l)	__builtin_va_arg(v,l)
# define va_copy(d,s)	__builtin_va_copy(d,s)
#endif
#else
# include <stdarg.h>
#endif

#endif
