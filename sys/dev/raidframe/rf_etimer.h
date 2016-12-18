/*	$NetBSD: rf_etimer.h,v 1.11 2006/06/07 22:33:37 kardel Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
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

#ifndef _RF__RF_TIMER_H_
#define _RF__RF_TIMER_H_

#include "rf_utils.h"

#include <sys/time.h>
#include <sys/kernel.h>

struct RF_Etimer_s {
        struct timeval st;
        struct timeval et;
        struct timeval diff;
};

#define RF_ETIMER_START(_t_)                                    \
                {                                               \
                        memset(&(_t_), 0, sizeof (_t_));        \
			getmicrouptime(&(_t_).st);		\
                }

#define RF_ETIMER_STOP(_t_)                                     \
                {                                               \
			getmicrouptime(&(_t_).et);		\
                }

#define RF_ETIMER_EVAL(_t_)                                     \
        {                                                       \
                RF_TIMEVAL_DIFF(&(_t_).st, &(_t_).et, &(_t_).diff) \
        }

#define RF_ETIMER_VAL_US(_t_)      (RF_TIMEVAL_TO_US((_t_).diff))
#define RF_ETIMER_VAL_MS(_t_)      (RF_TIMEVAL_TO_US((_t_).diff)/1000)

#endif				/* !_RF__RF_TIMER_H_ */
