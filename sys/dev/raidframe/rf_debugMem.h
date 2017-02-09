/*	$NetBSD: rf_debugMem.h,v 1.13 2011/05/01 06:49:43 mrg Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Daniel Stodolsky, Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * rf_debugMem.h -- memory leak debugging module
 *
 * IMPORTANT:  if you put the lock/unlock mutex stuff back in here, you
 *             need to take it out of the routines in debugMem.c
 *
 */

#ifndef _RF__RF_DEBUGMEM_H_
#define _RF__RF_DEBUGMEM_H_

#include "rf_alloclist.h"

#include <sys/types.h>
#include <sys/malloc.h>

#ifndef RF_DEBUG_MEM
#define RF_DEBUG_MEM 0
#endif

#if RF_DEBUG_MEM
#define RF_Malloc(_p_, _size_, _cast_)                                      \
  {                                                                         \
     _p_ = _cast_ malloc((u_long)_size_, M_RAIDFRAME, M_WAITOK);            \
     memset((char *)_p_, 0, _size_);                                        \
     if (rf_memDebug) rf_record_malloc(_p_, _size_, __LINE__, __FILE__);    \
  }
#else
#define RF_Malloc(_p_, _size_, _cast_)                                      \
  {                                                                         \
     _p_ = _cast_ malloc((u_long)_size_, M_RAIDFRAME, M_WAITOK);            \
     memset((char *)_p_, 0, _size_);                                        \
  }
#endif

#define RF_MallocAndAdd(__p_, __size_, __cast_, __alist_)                   \
  {                                                                         \
     RF_Malloc(__p_, __size_, __cast_);                                     \
     if (__alist_) rf_AddToAllocList(__alist_, __p_, __size_);              \
  }

#if RF_DEBUG_MEM
#define RF_Free(_p_, _sz_)                                                  \
  {                                                                         \
     free((void *)(_p_), M_RAIDFRAME);                                      \
     if (rf_memDebug) rf_unrecord_malloc(_p_, (u_int32_t) (_sz_));          \
  }
#else
#define RF_Free(_p_, _sz_)                                                  \
  {                                                                         \
     free((void *)(_p_), M_RAIDFRAME);                                      \
  }
#endif

void    rf_record_malloc(void *p, int size, int line, const char *filen);
void    rf_unrecord_malloc(void *p, int sz);
void    rf_print_unfreed(void);
int     rf_ConfigureDebugMem(RF_ShutdownList_t ** listp);

#endif				/* !_RF__RF_DEBUGMEM_H_ */
