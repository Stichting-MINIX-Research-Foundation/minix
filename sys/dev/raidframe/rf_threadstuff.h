/*	$NetBSD: rf_threadstuff.h,v 1.34 2014/02/28 10:16:51 skrll Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, Daniel Stodolsky, Jim Zelenka
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
 * threadstuff.h -- definitions for threads, locks, and synchronization
 *
 * The purpose of this file is provide some illusion of portability.
 * If the functions below can be implemented with the same semantics on
 * some new system, then at least the synchronization and thread control
 * part of the code should not require modification to port to a new machine.
 * the only other place where the pthread package is explicitly used is
 * threadid.h
 *
 * this file should be included above stdio.h to get some necessary defines.
 *
 */

#ifndef _RF__RF_THREADSTUFF_H_
#define _RF__RF_THREADSTUFF_H_

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/mutex.h>

#include <dev/raidframe/raidframevar.h>


typedef struct lwp *RF_Thread_t;
typedef void *RF_ThreadArg_t;

#define rf_declare_mutex2(_m_)           kmutex_t _m_
#define rf_declare_cond2(_c_)            kcondvar_t _c_

#define rf_lock_mutex2(_m_)              mutex_enter(&(_m_))
#define rf_unlock_mutex2(_m_)            mutex_exit(&(_m_))

#define rf_init_mutex2(_m_, _p_)         mutex_init(&(_m_), MUTEX_DEFAULT, (_p_))
#define rf_destroy_mutex2(_m_)           mutex_destroy(&(_m_))

#define rf_owned_mutex2(_m_)             mutex_owned(&(_m_))

#define rf_init_cond2(_c_, _w_)          cv_init(&(_c_), (_w_))
#define rf_destroy_cond2(_c_)            cv_destroy(&(_c_))
      
#define rf_wait_cond2(_c_,_m_)           cv_wait(&(_c_), &(_m_))
#define rf_timedwait_cond2(_c_,_m_,_t_)  cv_timedwait(&(_c_), &(_m_), (_t_))
#define rf_signal_cond2(_c_)             cv_signal(&(_c_))
#define rf_broadcast_cond2(_c_)          cv_broadcast(&(_c_))

#define rf_sleep(_w_,_t_,_m_)            kpause((_w_), false, (_t_), &(_m_))

/*
 * In NetBSD, kernel threads are simply processes which share several
 * substructures and never run in userspace.
 */

#define	RF_CREATE_THREAD(_handle_, _func_, _arg_, _name_) \
	kthread_create(PRI_SOFTBIO, 0, NULL, (void (*)(void *))(_func_), \
	    (void *)(_arg_), &(_handle_), _name_)

#define	RF_CREATE_ENGINE_THREAD(_handle_, _func_, _arg_, _fmt_, _fmt_arg_) \
	kthread_create(PRI_SOFTBIO, 0, NULL, (void (*)(void *))(_func_), \
	    (void *)(_arg_), &(_handle_), _fmt_, _fmt_arg_)

#endif				/* !_RF__RF_THREADSTUFF_H_ */
