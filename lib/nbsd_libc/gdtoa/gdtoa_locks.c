/* $NetBSD: gdtoa_locks.c,v 1.1 2006/01/25 15:36:13 kleink Exp $ */

/*
 * Written by Klaus Klein <kleink@NetBSD.org>, November 16, 2005.
 * Public domain.
 */

#include "gdtoaimp.h"

#ifdef _REENTRANT /* !__minix */
mutex_t __gdtoa_locks[2] = { MUTEX_INITIALIZER, MUTEX_INITIALIZER };
#endif /* _REENTRANT */
