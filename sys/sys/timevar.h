/*	$NetBSD: timevar.h,v 1.34 2015/08/07 08:11:33 ozaki-r Exp $	*/

/*
 *  Copyright (c) 2005, 2008 The NetBSD Foundation.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 *  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)time.h	8.5 (Berkeley) 5/4/95
 */

#ifndef _SYS_TIMEVAR_H_
#define _SYS_TIMEVAR_H_

#include <sys/callout.h>
#include <sys/queue.h>
#include <sys/signal.h>
#include <sys/systm.h>

/*
 * Structure used to manage timers in a process.
 */
struct 	ptimer {
	union {
		callout_t	pt_ch;
		struct {
			LIST_ENTRY(ptimer)	pt_list;
			int	pt_active;
		} pt_nonreal;
	} pt_data;
	struct	sigevent pt_ev;
	struct	itimerspec pt_time;
	struct	ksiginfo pt_info;
	int	pt_overruns;	/* Overruns currently accumulating */
	int	pt_poverruns;	/* Overruns associated w/ a delivery */
	int	pt_type;
	int	pt_entry;
	int	pt_queued;
	struct proc *pt_proc;
	TAILQ_ENTRY(ptimer) pt_chain;
};

#define pt_ch	pt_data.pt_ch
#define pt_list	pt_data.pt_nonreal.pt_list
#define pt_active	pt_data.pt_nonreal.pt_active

#define	TIMER_MAX	32	/* See ptimers->pts_fired if you enlarge this */
#define	TIMERS_ALL	0
#define	TIMERS_POSIX	1

LIST_HEAD(ptlist, ptimer);

struct	ptimers {
	struct ptlist pts_virtual;
	struct ptlist pts_prof;
	struct ptimer *pts_timers[TIMER_MAX];
	int pts_fired;
};

/*
 * Functions for looking at our clock: [get]{bin,nano,micro}[up]time()
 *
 * Functions without the "get" prefix returns the best timestamp
 * we can produce in the given format.
 *
 * "bin"   == struct bintime  == seconds + 64 bit fraction of seconds.
 * "nano"  == struct timespec == seconds + nanoseconds.
 * "micro" == struct timeval  == seconds + microseconds.
 *              
 * Functions containing "up" returns time relative to boot and
 * should be used for calculating time intervals.
 *
 * Functions without "up" returns GMT time.
 *
 * Functions with the "get" prefix returns a less precise result
 * much faster than the functions without "get" prefix and should
 * be used where a precision of 1/HZ (eg 10 msec on a 100HZ machine)
 * is acceptable or where performance is priority.
 * (NB: "precision", _not_ "resolution" !) 
 * 
 */

void	binuptime(struct bintime *);
void	nanouptime(struct timespec *);
void	microuptime(struct timeval *);

void	bintime(struct bintime *);
void	nanotime(struct timespec *);
void	microtime(struct timeval *);

void	getbinuptime(struct bintime *);
void	getnanouptime(struct timespec *);
void	getmicrouptime(struct timeval *);

void	getbintime(struct bintime *);
void	getnanotime(struct timespec *);
void	getmicrotime(struct timeval *);

/* Other functions */
int	ts2timo(clockid_t, int, struct timespec *, int *, struct timespec *);
void	adjtime1(const struct timeval *, struct timeval *, struct proc *);
int	clock_getres1(clockid_t, struct timespec *);
int	clock_gettime1(clockid_t, struct timespec *);
int	clock_settime1(struct proc *, clockid_t, const struct timespec *, bool);
int	dogetitimer(struct proc *, int, struct itimerval *);
int	dosetitimer(struct proc *, int, struct itimerval *);
int	dotimer_gettime(int, struct proc *, struct itimerspec *);
int	dotimer_settime(int, struct itimerspec *, struct itimerspec *, int,
	    struct proc *);
int	tshzto(const struct timespec *);
int	tshztoup(const struct timespec *);
int	tvhzto(const struct timeval *);
void	inittimecounter(void);
int	itimerfix(struct timeval *);
int	itimespecfix(struct timespec *);
int	ppsratecheck(struct timeval *, int *, int);
int	ratecheck(struct timeval *, const struct timeval *);
void	realtimerexpire(void *);
int	settime(struct proc *p, struct timespec *);
int	nanosleep1(struct lwp *, clockid_t, int, struct timespec *,
	    struct timespec *);
int	settimeofday1(const struct timeval *, bool,
	    const void *, struct lwp *, bool);
int	timer_create1(timer_t *, clockid_t, struct sigevent *, copyin_t,
	    struct lwp *);
void	timer_gettime(struct ptimer *, struct itimerspec *);
void	timer_settime(struct ptimer *);
struct	ptimers *timers_alloc(struct proc *);
void	timers_free(struct proc *, int);
void	timer_tick(struct lwp *, bool);
int	tstohz(const struct timespec *);
int	tvtohz(const struct timeval *);
int	inittimeleft(struct timespec *, struct timespec *);
int	gettimeleft(struct timespec *, struct timespec *);
void	timerupcall(struct lwp *);
void	time_init(void);
void	time_init2(void);
bool	time_wraps(struct timespec *, struct timespec *);

extern volatile time_t time_second;	/* current second in the epoch */
extern volatile time_t time_uptime;	/* system uptime in seconds */

static inline time_t time_mono_to_wall(time_t t)
{

	return t - time_uptime + time_second;
}

static inline time_t time_wall_to_mono(time_t t)
{

	return t - time_second + time_uptime;
}

#endif /* !_SYS_TIMEVAR_H_ */
