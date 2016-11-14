/*	$NetBSD: kern_time.c,v 1.182 2015/10/06 15:03:34 christos Exp $	*/

/*-
 * Copyright (c) 2000, 2004, 2005, 2007, 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christopher G. Demetriou, and by Andrew Doran.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)kern_time.c	8.4 (Berkeley) 5/26/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_time.c,v 1.182 2015/10/06 15:03:34 christos Exp $");

#include <sys/param.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>
#include <sys/syslog.h>
#include <sys/timetc.h>
#include <sys/timex.h>
#include <sys/kauth.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/cpu.h>

static void	timer_intr(void *);
static void	itimerfire(struct ptimer *);
static void	itimerfree(struct ptimers *, int);

kmutex_t	timer_lock;

static void	*timer_sih;
static TAILQ_HEAD(, ptimer) timer_queue;

struct pool ptimer_pool, ptimers_pool;

#define	CLOCK_VIRTUAL_P(clockid)	\
	((clockid) == CLOCK_VIRTUAL || (clockid) == CLOCK_PROF)

CTASSERT(ITIMER_REAL == CLOCK_REALTIME);
CTASSERT(ITIMER_VIRTUAL == CLOCK_VIRTUAL);
CTASSERT(ITIMER_PROF == CLOCK_PROF);
CTASSERT(ITIMER_MONOTONIC == CLOCK_MONOTONIC);

/*
 * Initialize timekeeping.
 */
void
time_init(void)
{

	pool_init(&ptimer_pool, sizeof(struct ptimer), 0, 0, 0, "ptimerpl",
	    &pool_allocator_nointr, IPL_NONE);
	pool_init(&ptimers_pool, sizeof(struct ptimers), 0, 0, 0, "ptimerspl",
	    &pool_allocator_nointr, IPL_NONE);
}

void
time_init2(void)
{

	TAILQ_INIT(&timer_queue);
	mutex_init(&timer_lock, MUTEX_DEFAULT, IPL_SCHED);
	timer_sih = softint_establish(SOFTINT_CLOCK | SOFTINT_MPSAFE,
	    timer_intr, NULL);
}

/* Time of day and interval timer support.
 *
 * These routines provide the kernel entry points to get and set
 * the time-of-day and per-process interval timers.  Subroutines
 * here provide support for adding and subtracting timeval structures
 * and decrementing interval timers, optionally reloading the interval
 * timers when they expire.
 */

/* This function is used by clock_settime and settimeofday */
static int
settime1(struct proc *p, const struct timespec *ts, bool check_kauth)
{
	struct timespec delta, now;
	int s;

	/* WHAT DO WE DO ABOUT PENDING REAL-TIME TIMEOUTS??? */
	s = splclock();
	nanotime(&now);
	timespecsub(ts, &now, &delta);

	if (check_kauth && kauth_authorize_system(kauth_cred_get(),
	    KAUTH_SYSTEM_TIME, KAUTH_REQ_SYSTEM_TIME_SYSTEM, __UNCONST(ts),
	    &delta, KAUTH_ARG(check_kauth ? false : true)) != 0) {
		splx(s);
		return (EPERM);
	}

#ifdef notyet
	if ((delta.tv_sec < 86400) && securelevel > 0) { /* XXX elad - notyet */
		splx(s);
		return (EPERM);
	}
#endif

	tc_setclock(ts);

	timespecadd(&boottime, &delta, &boottime);

	resettodr();
	splx(s);

	return (0);
}

int
settime(struct proc *p, struct timespec *ts)
{
	return (settime1(p, ts, true));
}

/* ARGSUSED */
int
sys___clock_gettime50(struct lwp *l,
    const struct sys___clock_gettime50_args *uap, register_t *retval)
{
	/* {
		syscallarg(clockid_t) clock_id;
		syscallarg(struct timespec *) tp;
	} */
	int error;
	struct timespec ats;

	error = clock_gettime1(SCARG(uap, clock_id), &ats);
	if (error != 0)
		return error;

	return copyout(&ats, SCARG(uap, tp), sizeof(ats));
}

/* ARGSUSED */
int
sys___clock_settime50(struct lwp *l,
    const struct sys___clock_settime50_args *uap, register_t *retval)
{
	/* {
		syscallarg(clockid_t) clock_id;
		syscallarg(const struct timespec *) tp;
	} */
	int error;
	struct timespec ats;

	if ((error = copyin(SCARG(uap, tp), &ats, sizeof(ats))) != 0)
		return error;

	return clock_settime1(l->l_proc, SCARG(uap, clock_id), &ats, true);
}


int
clock_settime1(struct proc *p, clockid_t clock_id, const struct timespec *tp,
    bool check_kauth)
{
	int error;

	switch (clock_id) {
	case CLOCK_REALTIME:
		if ((error = settime1(p, tp, check_kauth)) != 0)
			return (error);
		break;
	case CLOCK_MONOTONIC:
		return (EINVAL);	/* read-only clock */
	default:
		return (EINVAL);
	}

	return 0;
}

int
sys___clock_getres50(struct lwp *l, const struct sys___clock_getres50_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(clockid_t) clock_id;
		syscallarg(struct timespec *) tp;
	} */
	struct timespec ts;
	int error;

	if ((error = clock_getres1(SCARG(uap, clock_id), &ts)) != 0)
		return error;

	if (SCARG(uap, tp))
		error = copyout(&ts, SCARG(uap, tp), sizeof(ts));

	return error;
}

int
clock_getres1(clockid_t clock_id, struct timespec *ts)
{

	switch (clock_id) {
	case CLOCK_REALTIME:
	case CLOCK_MONOTONIC:
		ts->tv_sec = 0;
		if (tc_getfrequency() > 1000000000)
			ts->tv_nsec = 1;
		else
			ts->tv_nsec = 1000000000 / tc_getfrequency();
		break;
	default:
		return EINVAL;
	}

	return 0;
}

/* ARGSUSED */
int
sys___nanosleep50(struct lwp *l, const struct sys___nanosleep50_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(struct timespec *) rqtp;
		syscallarg(struct timespec *) rmtp;
	} */
	struct timespec rmt, rqt;
	int error, error1;

	error = copyin(SCARG(uap, rqtp), &rqt, sizeof(struct timespec));
	if (error)
		return (error);

	error = nanosleep1(l, CLOCK_MONOTONIC, 0, &rqt,
	    SCARG(uap, rmtp) ? &rmt : NULL);
	if (SCARG(uap, rmtp) == NULL || (error != 0 && error != EINTR))
		return error;

	error1 = copyout(&rmt, SCARG(uap, rmtp), sizeof(rmt));
	return error1 ? error1 : error;
}

/* ARGSUSED */
int
sys_clock_nanosleep(struct lwp *l, const struct sys_clock_nanosleep_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(clockid_t) clock_id;
		syscallarg(int) flags;
		syscallarg(struct timespec *) rqtp;
		syscallarg(struct timespec *) rmtp;
	} */
	struct timespec rmt, rqt;
	int error, error1;

	error = copyin(SCARG(uap, rqtp), &rqt, sizeof(struct timespec));
	if (error)
		goto out;

	error = nanosleep1(l, SCARG(uap, clock_id), SCARG(uap, flags), &rqt,
	    SCARG(uap, rmtp) ? &rmt : NULL);
	if (SCARG(uap, rmtp) == NULL || (error != 0 && error != EINTR))
		goto out;

	if ((error1 = copyout(&rmt, SCARG(uap, rmtp), sizeof(rmt))) != 0)
		error = error1;
out:
	*retval = error;
	return 0;
}

int
nanosleep1(struct lwp *l, clockid_t clock_id, int flags, struct timespec *rqt,
    struct timespec *rmt)
{
	struct timespec rmtstart;
	int error, timo;

	if ((error = ts2timo(clock_id, flags, rqt, &timo, &rmtstart)) != 0)
		return error == ETIMEDOUT ? 0 : error;

	/*
	 * Avoid inadvertently sleeping forever
	 */
	if (timo == 0)
		timo = 1;
again:
	error = kpause("nanoslp", true, timo, NULL);
	if (rmt != NULL || error == 0) {
		struct timespec rmtend;
		struct timespec t0;
		struct timespec *t;

		(void)clock_gettime1(clock_id, &rmtend);
		t = (rmt != NULL) ? rmt : &t0;
		if (flags & TIMER_ABSTIME) {
			timespecsub(rqt, &rmtend, t);
		} else {
			timespecsub(&rmtend, &rmtstart, t);
			timespecsub(rqt, t, t);
		}
		if (t->tv_sec < 0)
			timespecclear(t);
		if (error == 0) {
			timo = tstohz(t);
			if (timo > 0)
				goto again;
		}
	}

	if (error == ERESTART)
		error = EINTR;
	if (error == EWOULDBLOCK)
		error = 0;

	return error;
}

/* ARGSUSED */
int
sys___gettimeofday50(struct lwp *l, const struct sys___gettimeofday50_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(struct timeval *) tp;
		syscallarg(void *) tzp;		really "struct timezone *";
	} */
	struct timeval atv;
	int error = 0;
	struct timezone tzfake;

	if (SCARG(uap, tp)) {
		microtime(&atv);
		error = copyout(&atv, SCARG(uap, tp), sizeof(atv));
		if (error)
			return (error);
	}
	if (SCARG(uap, tzp)) {
		/*
		 * NetBSD has no kernel notion of time zone, so we just
		 * fake up a timezone struct and return it if demanded.
		 */
		tzfake.tz_minuteswest = 0;
		tzfake.tz_dsttime = 0;
		error = copyout(&tzfake, SCARG(uap, tzp), sizeof(tzfake));
	}
	return (error);
}

/* ARGSUSED */
int
sys___settimeofday50(struct lwp *l, const struct sys___settimeofday50_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(const struct timeval *) tv;
		syscallarg(const void *) tzp; really "const struct timezone *";
	} */

	return settimeofday1(SCARG(uap, tv), true, SCARG(uap, tzp), l, true);
}

int
settimeofday1(const struct timeval *utv, bool userspace,
    const void *utzp, struct lwp *l, bool check_kauth)
{
	struct timeval atv;
	struct timespec ts;
	int error;

	/* Verify all parameters before changing time. */

	/*
	 * NetBSD has no kernel notion of time zone, and only an
	 * obsolete program would try to set it, so we log a warning.
	 */
	if (utzp)
		log(LOG_WARNING, "pid %d attempted to set the "
		    "(obsolete) kernel time zone\n", l->l_proc->p_pid);

	if (utv == NULL) 
		return 0;

	if (userspace) {
		if ((error = copyin(utv, &atv, sizeof(atv))) != 0)
			return error;
		utv = &atv;
	}

	TIMEVAL_TO_TIMESPEC(utv, &ts);
	return settime1(l->l_proc, &ts, check_kauth);
}

int	time_adjusted;			/* set if an adjustment is made */

/* ARGSUSED */
int
sys___adjtime50(struct lwp *l, const struct sys___adjtime50_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(const struct timeval *) delta;
		syscallarg(struct timeval *) olddelta;
	} */
	int error;
	struct timeval atv, oldatv;

	if ((error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_TIME,
	    KAUTH_REQ_SYSTEM_TIME_ADJTIME, NULL, NULL, NULL)) != 0)
		return error;

	if (SCARG(uap, delta)) {
		error = copyin(SCARG(uap, delta), &atv,
		    sizeof(*SCARG(uap, delta)));
		if (error)
			return (error);
	}
	adjtime1(SCARG(uap, delta) ? &atv : NULL,
	    SCARG(uap, olddelta) ? &oldatv : NULL, l->l_proc);
	if (SCARG(uap, olddelta))
		error = copyout(&oldatv, SCARG(uap, olddelta),
		    sizeof(*SCARG(uap, olddelta)));
	return error;
}

void
adjtime1(const struct timeval *delta, struct timeval *olddelta, struct proc *p)
{
	extern int64_t time_adjtime;  /* in kern_ntptime.c */

	if (olddelta) {
		mutex_spin_enter(&timecounter_lock);
		olddelta->tv_sec = time_adjtime / 1000000;
		olddelta->tv_usec = time_adjtime % 1000000;
		if (olddelta->tv_usec < 0) {
			olddelta->tv_usec += 1000000;
			olddelta->tv_sec--;
		}
		mutex_spin_exit(&timecounter_lock);
	}
	
	if (delta) {
		mutex_spin_enter(&timecounter_lock);
		time_adjtime = delta->tv_sec * 1000000 + delta->tv_usec;

		if (time_adjtime) {
			/* We need to save the system time during shutdown */
			time_adjusted |= 1;
		}
		mutex_spin_exit(&timecounter_lock);
	}
}

/*
 * Interval timer support. Both the BSD getitimer() family and the POSIX
 * timer_*() family of routines are supported.
 *
 * All timers are kept in an array pointed to by p_timers, which is
 * allocated on demand - many processes don't use timers at all. The
 * first three elements in this array are reserved for the BSD timers:
 * element 0 is ITIMER_REAL, element 1 is ITIMER_VIRTUAL, element
 * 2 is ITIMER_PROF, and element 3 is ITIMER_MONOTONIC. The rest may be
 * allocated by the timer_create() syscall.
 *
 * Realtime timers are kept in the ptimer structure as an absolute
 * time; virtual time timers are kept as a linked list of deltas.
 * Virtual time timers are processed in the hardclock() routine of
 * kern_clock.c.  The real time timer is processed by a callout
 * routine, called from the softclock() routine.  Since a callout may
 * be delayed in real time due to interrupt processing in the system,
 * it is possible for the real time timeout routine (realtimeexpire,
 * given below), to be delayed in real time past when it is supposed
 * to occur.  It does not suffice, therefore, to reload the real timer
 * .it_value from the real time timers .it_interval.  Rather, we
 * compute the next time in absolute time the timer should go off.  */

/* Allocate a POSIX realtime timer. */
int
sys_timer_create(struct lwp *l, const struct sys_timer_create_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(clockid_t) clock_id;
		syscallarg(struct sigevent *) evp;
		syscallarg(timer_t *) timerid;
	} */

	return timer_create1(SCARG(uap, timerid), SCARG(uap, clock_id),
	    SCARG(uap, evp), copyin, l);
}

int
timer_create1(timer_t *tid, clockid_t id, struct sigevent *evp,
    copyin_t fetch_event, struct lwp *l)
{
	int error;
	timer_t timerid;
	struct ptimers *pts;
	struct ptimer *pt;
	struct proc *p;

	p = l->l_proc;

	if ((u_int)id > CLOCK_MONOTONIC)
		return (EINVAL);

	if ((pts = p->p_timers) == NULL)
		pts = timers_alloc(p);

	pt = pool_get(&ptimer_pool, PR_WAITOK);
	if (evp != NULL) {
		if (((error =
		    (*fetch_event)(evp, &pt->pt_ev, sizeof(pt->pt_ev))) != 0) ||
		    ((pt->pt_ev.sigev_notify < SIGEV_NONE) ||
			(pt->pt_ev.sigev_notify > SIGEV_SA)) ||
			(pt->pt_ev.sigev_notify == SIGEV_SIGNAL &&
			 (pt->pt_ev.sigev_signo <= 0 ||
			  pt->pt_ev.sigev_signo >= NSIG))) {
			pool_put(&ptimer_pool, pt);
			return (error ? error : EINVAL);
		}
	}

	/* Find a free timer slot, skipping those reserved for setitimer(). */
	mutex_spin_enter(&timer_lock);
	for (timerid = 3; timerid < TIMER_MAX; timerid++)
		if (pts->pts_timers[timerid] == NULL)
			break;
	if (timerid == TIMER_MAX) {
		mutex_spin_exit(&timer_lock);
		pool_put(&ptimer_pool, pt);
		return EAGAIN;
	}
	if (evp == NULL) {
		pt->pt_ev.sigev_notify = SIGEV_SIGNAL;
		switch (id) {
		case CLOCK_REALTIME:
		case CLOCK_MONOTONIC:
			pt->pt_ev.sigev_signo = SIGALRM;
			break;
		case CLOCK_VIRTUAL:
			pt->pt_ev.sigev_signo = SIGVTALRM;
			break;
		case CLOCK_PROF:
			pt->pt_ev.sigev_signo = SIGPROF;
			break;
		}
		pt->pt_ev.sigev_value.sival_int = timerid;
	}
	pt->pt_info.ksi_signo = pt->pt_ev.sigev_signo;
	pt->pt_info.ksi_errno = 0;
	pt->pt_info.ksi_code = 0;
	pt->pt_info.ksi_pid = p->p_pid;
	pt->pt_info.ksi_uid = kauth_cred_getuid(l->l_cred);
	pt->pt_info.ksi_value = pt->pt_ev.sigev_value;
	pt->pt_type = id;
	pt->pt_proc = p;
	pt->pt_overruns = 0;
	pt->pt_poverruns = 0;
	pt->pt_entry = timerid;
	pt->pt_queued = false;
	timespecclear(&pt->pt_time.it_value);
	if (!CLOCK_VIRTUAL_P(id))
		callout_init(&pt->pt_ch, CALLOUT_MPSAFE);
	else
		pt->pt_active = 0;

	pts->pts_timers[timerid] = pt;
	mutex_spin_exit(&timer_lock);

	return copyout(&timerid, tid, sizeof(timerid));
}

/* Delete a POSIX realtime timer */
int
sys_timer_delete(struct lwp *l, const struct sys_timer_delete_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(timer_t) timerid;
	} */
	struct proc *p = l->l_proc;
	timer_t timerid;
	struct ptimers *pts;
	struct ptimer *pt, *ptn;

	timerid = SCARG(uap, timerid);
	pts = p->p_timers;
	
	if (pts == NULL || timerid < 2 || timerid >= TIMER_MAX)
		return (EINVAL);

	mutex_spin_enter(&timer_lock);
	if ((pt = pts->pts_timers[timerid]) == NULL) {
		mutex_spin_exit(&timer_lock);
		return (EINVAL);
	}
	if (CLOCK_VIRTUAL_P(pt->pt_type)) {
		if (pt->pt_active) {
			ptn = LIST_NEXT(pt, pt_list);
			LIST_REMOVE(pt, pt_list);
			for ( ; ptn; ptn = LIST_NEXT(ptn, pt_list))
				timespecadd(&pt->pt_time.it_value,
				    &ptn->pt_time.it_value,
				    &ptn->pt_time.it_value);
			pt->pt_active = 0;
		}
	}
	itimerfree(pts, timerid);

	return (0);
}

/*
 * Set up the given timer. The value in pt->pt_time.it_value is taken
 * to be an absolute time for CLOCK_REALTIME/CLOCK_MONOTONIC timers and
 * a relative time for CLOCK_VIRTUAL/CLOCK_PROF timers.
 */
void
timer_settime(struct ptimer *pt)
{
	struct ptimer *ptn, *pptn;
	struct ptlist *ptl;

	KASSERT(mutex_owned(&timer_lock));

	if (!CLOCK_VIRTUAL_P(pt->pt_type)) {
		callout_halt(&pt->pt_ch, &timer_lock);
		if (timespecisset(&pt->pt_time.it_value)) {
			/*
			 * Don't need to check tshzto() return value, here.
			 * callout_reset() does it for us.
			 */
			callout_reset(&pt->pt_ch,
			    pt->pt_type == CLOCK_MONOTONIC ?
			    tshztoup(&pt->pt_time.it_value) :
			    tshzto(&pt->pt_time.it_value),
			    realtimerexpire, pt);
		}
	} else {
		if (pt->pt_active) {
			ptn = LIST_NEXT(pt, pt_list);
			LIST_REMOVE(pt, pt_list);
			for ( ; ptn; ptn = LIST_NEXT(ptn, pt_list))
				timespecadd(&pt->pt_time.it_value,
				    &ptn->pt_time.it_value,
				    &ptn->pt_time.it_value);
		}
		if (timespecisset(&pt->pt_time.it_value)) {
			if (pt->pt_type == CLOCK_VIRTUAL)
				ptl = &pt->pt_proc->p_timers->pts_virtual;
			else
				ptl = &pt->pt_proc->p_timers->pts_prof;

			for (ptn = LIST_FIRST(ptl), pptn = NULL;
			     ptn && timespeccmp(&pt->pt_time.it_value,
				 &ptn->pt_time.it_value, >);
			     pptn = ptn, ptn = LIST_NEXT(ptn, pt_list))
				timespecsub(&pt->pt_time.it_value,
				    &ptn->pt_time.it_value,
				    &pt->pt_time.it_value);

			if (pptn)
				LIST_INSERT_AFTER(pptn, pt, pt_list);
			else
				LIST_INSERT_HEAD(ptl, pt, pt_list);

			for ( ; ptn ; ptn = LIST_NEXT(ptn, pt_list))
				timespecsub(&ptn->pt_time.it_value,
				    &pt->pt_time.it_value,
				    &ptn->pt_time.it_value);

			pt->pt_active = 1;
		} else
			pt->pt_active = 0;
	}
}

void
timer_gettime(struct ptimer *pt, struct itimerspec *aits)
{
	struct timespec now;
	struct ptimer *ptn;

	KASSERT(mutex_owned(&timer_lock));

	*aits = pt->pt_time;
	if (!CLOCK_VIRTUAL_P(pt->pt_type)) {
		/*
		 * Convert from absolute to relative time in .it_value
		 * part of real time timer.  If time for real time
		 * timer has passed return 0, else return difference
		 * between current time and time for the timer to go
		 * off.
		 */
		if (timespecisset(&aits->it_value)) {
			if (pt->pt_type == CLOCK_REALTIME) {
				getnanotime(&now);
			} else { /* CLOCK_MONOTONIC */
				getnanouptime(&now);
			}
			if (timespeccmp(&aits->it_value, &now, <))
				timespecclear(&aits->it_value);
			else
				timespecsub(&aits->it_value, &now,
				    &aits->it_value);
		}
	} else if (pt->pt_active) {
		if (pt->pt_type == CLOCK_VIRTUAL)
			ptn = LIST_FIRST(&pt->pt_proc->p_timers->pts_virtual);
		else
			ptn = LIST_FIRST(&pt->pt_proc->p_timers->pts_prof);
		for ( ; ptn && ptn != pt; ptn = LIST_NEXT(ptn, pt_list))
			timespecadd(&aits->it_value,
			    &ptn->pt_time.it_value, &aits->it_value);
		KASSERT(ptn != NULL); /* pt should be findable on the list */
	} else
		timespecclear(&aits->it_value);
}



/* Set and arm a POSIX realtime timer */
int
sys___timer_settime50(struct lwp *l,
    const struct sys___timer_settime50_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(timer_t) timerid;
		syscallarg(int) flags;
		syscallarg(const struct itimerspec *) value;
		syscallarg(struct itimerspec *) ovalue;
	} */
	int error;
	struct itimerspec value, ovalue, *ovp = NULL;

	if ((error = copyin(SCARG(uap, value), &value,
	    sizeof(struct itimerspec))) != 0)
		return (error);

	if (SCARG(uap, ovalue))
		ovp = &ovalue;

	if ((error = dotimer_settime(SCARG(uap, timerid), &value, ovp,
	    SCARG(uap, flags), l->l_proc)) != 0)
		return error;

	if (ovp)
		return copyout(&ovalue, SCARG(uap, ovalue),
		    sizeof(struct itimerspec));
	return 0;
}

int
dotimer_settime(int timerid, struct itimerspec *value,
    struct itimerspec *ovalue, int flags, struct proc *p)
{
	struct timespec now;
	struct itimerspec val, oval;
	struct ptimers *pts;
	struct ptimer *pt;
	int error;

	pts = p->p_timers;

	if (pts == NULL || timerid < 2 || timerid >= TIMER_MAX)
		return EINVAL;
	val = *value;
	if ((error = itimespecfix(&val.it_value)) != 0 ||
	    (error = itimespecfix(&val.it_interval)) != 0)
		return error;

	mutex_spin_enter(&timer_lock);
	if ((pt = pts->pts_timers[timerid]) == NULL) {
		mutex_spin_exit(&timer_lock);
		return EINVAL;
	}

	oval = pt->pt_time;
	pt->pt_time = val;

	/*
	 * If we've been passed a relative time for a realtime timer,
	 * convert it to absolute; if an absolute time for a virtual
	 * timer, convert it to relative and make sure we don't set it
	 * to zero, which would cancel the timer, or let it go
	 * negative, which would confuse the comparison tests.
	 */
	if (timespecisset(&pt->pt_time.it_value)) {
		if (!CLOCK_VIRTUAL_P(pt->pt_type)) {
			if ((flags & TIMER_ABSTIME) == 0) {
				if (pt->pt_type == CLOCK_REALTIME) {
					getnanotime(&now);
				} else { /* CLOCK_MONOTONIC */
					getnanouptime(&now);
				}
				timespecadd(&pt->pt_time.it_value, &now,
				    &pt->pt_time.it_value);
			}
		} else {
			if ((flags & TIMER_ABSTIME) != 0) {
				getnanotime(&now);
				timespecsub(&pt->pt_time.it_value, &now,
				    &pt->pt_time.it_value);
				if (!timespecisset(&pt->pt_time.it_value) ||
				    pt->pt_time.it_value.tv_sec < 0) {
					pt->pt_time.it_value.tv_sec = 0;
					pt->pt_time.it_value.tv_nsec = 1;
				}
			}
		}
	}

	timer_settime(pt);
	mutex_spin_exit(&timer_lock);

	if (ovalue)
		*ovalue = oval;

	return (0);
}

/* Return the time remaining until a POSIX timer fires. */
int
sys___timer_gettime50(struct lwp *l,
    const struct sys___timer_gettime50_args *uap, register_t *retval)
{
	/* {
		syscallarg(timer_t) timerid;
		syscallarg(struct itimerspec *) value;
	} */
	struct itimerspec its;
	int error;

	if ((error = dotimer_gettime(SCARG(uap, timerid), l->l_proc,
	    &its)) != 0)
		return error;

	return copyout(&its, SCARG(uap, value), sizeof(its));
}

int
dotimer_gettime(int timerid, struct proc *p, struct itimerspec *its)
{
	struct ptimer *pt;
	struct ptimers *pts;

	pts = p->p_timers;
	if (pts == NULL || timerid < 2 || timerid >= TIMER_MAX)
		return (EINVAL);
	mutex_spin_enter(&timer_lock);
	if ((pt = pts->pts_timers[timerid]) == NULL) {
		mutex_spin_exit(&timer_lock);
		return (EINVAL);
	}
	timer_gettime(pt, its);
	mutex_spin_exit(&timer_lock);

	return 0;
}

/*
 * Return the count of the number of times a periodic timer expired
 * while a notification was already pending. The counter is reset when
 * a timer expires and a notification can be posted.
 */
int
sys_timer_getoverrun(struct lwp *l, const struct sys_timer_getoverrun_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(timer_t) timerid;
	} */
	struct proc *p = l->l_proc;
	struct ptimers *pts;
	int timerid;
	struct ptimer *pt;

	timerid = SCARG(uap, timerid);

	pts = p->p_timers;
	if (pts == NULL || timerid < 2 || timerid >= TIMER_MAX)
		return (EINVAL);
	mutex_spin_enter(&timer_lock);
	if ((pt = pts->pts_timers[timerid]) == NULL) {
		mutex_spin_exit(&timer_lock);
		return (EINVAL);
	}
	*retval = pt->pt_poverruns;
	mutex_spin_exit(&timer_lock);

	return (0);
}

/*
 * Real interval timer expired:
 * send process whose timer expired an alarm signal.
 * If time is not set up to reload, then just return.
 * Else compute next time timer should go off which is > current time.
 * This is where delay in processing this timeout causes multiple
 * SIGALRM calls to be compressed into one.
 */
void
realtimerexpire(void *arg)
{
	uint64_t last_val, next_val, interval, now_ns;
	struct timespec now, next;
	struct ptimer *pt;
	int backwards;

	pt = arg;

	mutex_spin_enter(&timer_lock);
	itimerfire(pt);

	if (!timespecisset(&pt->pt_time.it_interval)) {
		timespecclear(&pt->pt_time.it_value);
		mutex_spin_exit(&timer_lock);
		return;
	}

	if (pt->pt_type == CLOCK_MONOTONIC) {
		getnanouptime(&now);
	} else {
		getnanotime(&now);
	}
	backwards = (timespeccmp(&pt->pt_time.it_value, &now, >));
	timespecadd(&pt->pt_time.it_value, &pt->pt_time.it_interval, &next);
	/* Handle the easy case of non-overflown timers first. */
	if (!backwards && timespeccmp(&next, &now, >)) {
		pt->pt_time.it_value = next;
	} else {
		now_ns = timespec2ns(&now);
		last_val = timespec2ns(&pt->pt_time.it_value);
		interval = timespec2ns(&pt->pt_time.it_interval);

		next_val = now_ns +
		    (now_ns - last_val + interval - 1) % interval;

		if (backwards)
			next_val += interval;
		else
			pt->pt_overruns += (now_ns - last_val) / interval;

		pt->pt_time.it_value.tv_sec = next_val / 1000000000;
		pt->pt_time.it_value.tv_nsec = next_val % 1000000000;
	}

	/*
	 * Don't need to check tshzto() return value, here.
	 * callout_reset() does it for us.
	 */
	callout_reset(&pt->pt_ch, pt->pt_type == CLOCK_MONOTONIC ?
	    tshztoup(&pt->pt_time.it_value) : tshzto(&pt->pt_time.it_value),
	    realtimerexpire, pt);
	mutex_spin_exit(&timer_lock);
}

/* BSD routine to get the value of an interval timer. */
/* ARGSUSED */
int
sys___getitimer50(struct lwp *l, const struct sys___getitimer50_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) which;
		syscallarg(struct itimerval *) itv;
	} */
	struct proc *p = l->l_proc;
	struct itimerval aitv;
	int error;

	error = dogetitimer(p, SCARG(uap, which), &aitv);
	if (error)
		return error;
	return (copyout(&aitv, SCARG(uap, itv), sizeof(struct itimerval)));
}

int
dogetitimer(struct proc *p, int which, struct itimerval *itvp)
{
	struct ptimers *pts;
	struct ptimer *pt;
	struct itimerspec its;

	if ((u_int)which > ITIMER_MONOTONIC)
		return (EINVAL);

	mutex_spin_enter(&timer_lock);
	pts = p->p_timers;
	if (pts == NULL || (pt = pts->pts_timers[which]) == NULL) {
		timerclear(&itvp->it_value);
		timerclear(&itvp->it_interval);
	} else {
		timer_gettime(pt, &its);
		TIMESPEC_TO_TIMEVAL(&itvp->it_value, &its.it_value);
		TIMESPEC_TO_TIMEVAL(&itvp->it_interval, &its.it_interval);
	}
	mutex_spin_exit(&timer_lock);	

	return 0;
}

/* BSD routine to set/arm an interval timer. */
/* ARGSUSED */
int
sys___setitimer50(struct lwp *l, const struct sys___setitimer50_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) which;
		syscallarg(const struct itimerval *) itv;
		syscallarg(struct itimerval *) oitv;
	} */
	struct proc *p = l->l_proc;
	int which = SCARG(uap, which);
	struct sys___getitimer50_args getargs;
	const struct itimerval *itvp;
	struct itimerval aitv;
	int error;

	if ((u_int)which > ITIMER_MONOTONIC)
		return (EINVAL);
	itvp = SCARG(uap, itv);
	if (itvp &&
	    (error = copyin(itvp, &aitv, sizeof(struct itimerval))) != 0)
		return (error);
	if (SCARG(uap, oitv) != NULL) {
		SCARG(&getargs, which) = which;
		SCARG(&getargs, itv) = SCARG(uap, oitv);
		if ((error = sys___getitimer50(l, &getargs, retval)) != 0)
			return (error);
	}
	if (itvp == 0)
		return (0);

	return dosetitimer(p, which, &aitv);
}

int
dosetitimer(struct proc *p, int which, struct itimerval *itvp)
{
	struct timespec now;
	struct ptimers *pts;
	struct ptimer *pt, *spare;

	KASSERT((u_int)which <= CLOCK_MONOTONIC);
	if (itimerfix(&itvp->it_value) || itimerfix(&itvp->it_interval))
		return (EINVAL);

	/*
	 * Don't bother allocating data structures if the process just
	 * wants to clear the timer.
	 */
	spare = NULL;
	pts = p->p_timers;
 retry:
	if (!timerisset(&itvp->it_value) && (pts == NULL ||
	    pts->pts_timers[which] == NULL))
		return (0);
	if (pts == NULL)
		pts = timers_alloc(p);
	mutex_spin_enter(&timer_lock);
	pt = pts->pts_timers[which];
	if (pt == NULL) {
		if (spare == NULL) {
			mutex_spin_exit(&timer_lock);
			spare = pool_get(&ptimer_pool, PR_WAITOK);
			goto retry;
		}
		pt = spare;
		spare = NULL;
		pt->pt_ev.sigev_notify = SIGEV_SIGNAL;
		pt->pt_ev.sigev_value.sival_int = which;
		pt->pt_overruns = 0;
		pt->pt_proc = p;
		pt->pt_type = which;
		pt->pt_entry = which;
		pt->pt_queued = false;
		if (pt->pt_type == CLOCK_REALTIME)
			callout_init(&pt->pt_ch, CALLOUT_MPSAFE);
		else
			pt->pt_active = 0;

		switch (which) {
		case ITIMER_REAL:
		case ITIMER_MONOTONIC:
			pt->pt_ev.sigev_signo = SIGALRM;
			break;
		case ITIMER_VIRTUAL:
			pt->pt_ev.sigev_signo = SIGVTALRM;
			break;
		case ITIMER_PROF:
			pt->pt_ev.sigev_signo = SIGPROF;
			break;
		}
		pts->pts_timers[which] = pt;
	}

	TIMEVAL_TO_TIMESPEC(&itvp->it_value, &pt->pt_time.it_value);
	TIMEVAL_TO_TIMESPEC(&itvp->it_interval, &pt->pt_time.it_interval);

	if (timespecisset(&pt->pt_time.it_value)) {
		/* Convert to absolute time */
		/* XXX need to wrap in splclock for timecounters case? */
		switch (which) {
		case ITIMER_REAL:
			getnanotime(&now);
			timespecadd(&pt->pt_time.it_value, &now,
			    &pt->pt_time.it_value);
			break;
		case ITIMER_MONOTONIC:
			getnanouptime(&now);
			timespecadd(&pt->pt_time.it_value, &now,
			    &pt->pt_time.it_value);
			break;
		default:
			break;
		}
	}
	timer_settime(pt);
	mutex_spin_exit(&timer_lock);
	if (spare != NULL)
		pool_put(&ptimer_pool, spare);

	return (0);
}

/* Utility routines to manage the array of pointers to timers. */
struct ptimers *
timers_alloc(struct proc *p)
{
	struct ptimers *pts;
	int i;

	pts = pool_get(&ptimers_pool, PR_WAITOK);
	LIST_INIT(&pts->pts_virtual);
	LIST_INIT(&pts->pts_prof);
	for (i = 0; i < TIMER_MAX; i++)
		pts->pts_timers[i] = NULL;
	pts->pts_fired = 0;
	mutex_spin_enter(&timer_lock);
	if (p->p_timers == NULL) {
		p->p_timers = pts;
		mutex_spin_exit(&timer_lock);
		return pts;
	}
	mutex_spin_exit(&timer_lock);
	pool_put(&ptimers_pool, pts);
	return p->p_timers;
}

/*
 * Clean up the per-process timers. If "which" is set to TIMERS_ALL,
 * then clean up all timers and free all the data structures. If
 * "which" is set to TIMERS_POSIX, only clean up the timers allocated
 * by timer_create(), not the BSD setitimer() timers, and only free the
 * structure if none of those remain.
 */
void
timers_free(struct proc *p, int which)
{
	struct ptimers *pts;
	struct ptimer *ptn;
	struct timespec ts;
	int i;

	if (p->p_timers == NULL)
		return;

	pts = p->p_timers;
	mutex_spin_enter(&timer_lock);
	if (which == TIMERS_ALL) {
		p->p_timers = NULL;
		i = 0;
	} else {
		timespecclear(&ts);
		for (ptn = LIST_FIRST(&pts->pts_virtual);
		     ptn && ptn != pts->pts_timers[ITIMER_VIRTUAL];
		     ptn = LIST_NEXT(ptn, pt_list)) {
			KASSERT(ptn->pt_type == CLOCK_VIRTUAL);
			timespecadd(&ts, &ptn->pt_time.it_value, &ts);
		}
		LIST_FIRST(&pts->pts_virtual) = NULL;
		if (ptn) {
			KASSERT(ptn->pt_type == CLOCK_VIRTUAL);
			timespecadd(&ts, &ptn->pt_time.it_value,
			    &ptn->pt_time.it_value);
			LIST_INSERT_HEAD(&pts->pts_virtual, ptn, pt_list);
		}
		timespecclear(&ts);
		for (ptn = LIST_FIRST(&pts->pts_prof);
		     ptn && ptn != pts->pts_timers[ITIMER_PROF];
		     ptn = LIST_NEXT(ptn, pt_list)) {
			KASSERT(ptn->pt_type == CLOCK_PROF);
			timespecadd(&ts, &ptn->pt_time.it_value, &ts);
		}
		LIST_FIRST(&pts->pts_prof) = NULL;
		if (ptn) {
			KASSERT(ptn->pt_type == CLOCK_PROF);
			timespecadd(&ts, &ptn->pt_time.it_value,
			    &ptn->pt_time.it_value);
			LIST_INSERT_HEAD(&pts->pts_prof, ptn, pt_list);
		}
		i = 3;
	}
	for ( ; i < TIMER_MAX; i++) {
		if (pts->pts_timers[i] != NULL) {
			itimerfree(pts, i);
			mutex_spin_enter(&timer_lock);
		}
	}
	if (pts->pts_timers[0] == NULL && pts->pts_timers[1] == NULL &&
	    pts->pts_timers[2] == NULL) {
		p->p_timers = NULL;
		mutex_spin_exit(&timer_lock);
		pool_put(&ptimers_pool, pts);
	} else
		mutex_spin_exit(&timer_lock);
}

static void
itimerfree(struct ptimers *pts, int index)
{
	struct ptimer *pt;

	KASSERT(mutex_owned(&timer_lock));

	pt = pts->pts_timers[index];
	pts->pts_timers[index] = NULL;
	if (!CLOCK_VIRTUAL_P(pt->pt_type))
		callout_halt(&pt->pt_ch, &timer_lock);
	if (pt->pt_queued)
		TAILQ_REMOVE(&timer_queue, pt, pt_chain);
	mutex_spin_exit(&timer_lock);
	if (!CLOCK_VIRTUAL_P(pt->pt_type))
		callout_destroy(&pt->pt_ch);
	pool_put(&ptimer_pool, pt);
}

/*
 * Decrement an interval timer by a specified number
 * of nanoseconds, which must be less than a second,
 * i.e. < 1000000000.  If the timer expires, then reload
 * it.  In this case, carry over (nsec - old value) to
 * reduce the value reloaded into the timer so that
 * the timer does not drift.  This routine assumes
 * that it is called in a context where the timers
 * on which it is operating cannot change in value.
 */
static int
itimerdecr(struct ptimer *pt, int nsec)
{
	struct itimerspec *itp;

	KASSERT(mutex_owned(&timer_lock));
	KASSERT(CLOCK_VIRTUAL_P(pt->pt_type));

	itp = &pt->pt_time;
	if (itp->it_value.tv_nsec < nsec) {
		if (itp->it_value.tv_sec == 0) {
			/* expired, and already in next interval */
			nsec -= itp->it_value.tv_nsec;
			goto expire;
		}
		itp->it_value.tv_nsec += 1000000000;
		itp->it_value.tv_sec--;
	}
	itp->it_value.tv_nsec -= nsec;
	nsec = 0;
	if (timespecisset(&itp->it_value))
		return (1);
	/* expired, exactly at end of interval */
expire:
	if (timespecisset(&itp->it_interval)) {
		itp->it_value = itp->it_interval;
		itp->it_value.tv_nsec -= nsec;
		if (itp->it_value.tv_nsec < 0) {
			itp->it_value.tv_nsec += 1000000000;
			itp->it_value.tv_sec--;
		}
		timer_settime(pt);
	} else
		itp->it_value.tv_nsec = 0;		/* sec is already 0 */
	return (0);
}

static void
itimerfire(struct ptimer *pt)
{

	KASSERT(mutex_owned(&timer_lock));

	/*
	 * XXX Can overrun, but we don't do signal queueing yet, anyway.
	 * XXX Relying on the clock interrupt is stupid.
	 */
	if (pt->pt_ev.sigev_notify != SIGEV_SIGNAL || pt->pt_queued) {
		return;
	}
	TAILQ_INSERT_TAIL(&timer_queue, pt, pt_chain);
	pt->pt_queued = true;
	softint_schedule(timer_sih);
}

void
timer_tick(lwp_t *l, bool user)
{
	struct ptimers *pts;
	struct ptimer *pt;
	proc_t *p;

	p = l->l_proc;
	if (p->p_timers == NULL)
		return;

	mutex_spin_enter(&timer_lock);
	if ((pts = l->l_proc->p_timers) != NULL) {
		/*
		 * Run current process's virtual and profile time, as needed.
		 */
		if (user && (pt = LIST_FIRST(&pts->pts_virtual)) != NULL)
			if (itimerdecr(pt, tick * 1000) == 0)
				itimerfire(pt);
		if ((pt = LIST_FIRST(&pts->pts_prof)) != NULL)
			if (itimerdecr(pt, tick * 1000) == 0)
				itimerfire(pt);
	}
	mutex_spin_exit(&timer_lock);
}

static void
timer_intr(void *cookie)
{
	ksiginfo_t ksi;
	struct ptimer *pt;
	proc_t *p;
	
	mutex_enter(proc_lock);
	mutex_spin_enter(&timer_lock);
	while ((pt = TAILQ_FIRST(&timer_queue)) != NULL) {
		TAILQ_REMOVE(&timer_queue, pt, pt_chain);
		KASSERT(pt->pt_queued);
		pt->pt_queued = false;

		if (pt->pt_proc->p_timers == NULL) {
			/* Process is dying. */
			continue;
		}
		p = pt->pt_proc;
		if (pt->pt_ev.sigev_notify != SIGEV_SIGNAL) {
			continue;
		}
		if (sigismember(&p->p_sigpend.sp_set, pt->pt_ev.sigev_signo)) {
			pt->pt_overruns++;
			continue;
		}

		KSI_INIT(&ksi);
		ksi.ksi_signo = pt->pt_ev.sigev_signo;
		ksi.ksi_code = SI_TIMER;
		ksi.ksi_value = pt->pt_ev.sigev_value;
		pt->pt_poverruns = pt->pt_overruns;
		pt->pt_overruns = 0;
		mutex_spin_exit(&timer_lock);
		kpsignal(p, &ksi, NULL);
		mutex_spin_enter(&timer_lock);
	}
	mutex_spin_exit(&timer_lock);
	mutex_exit(proc_lock);
}

/*
 * Check if the time will wrap if set to ts.
 *
 * ts - timespec describing the new time
 * delta - the delta between the current time and ts
 */
bool
time_wraps(struct timespec *ts, struct timespec *delta)
{

	/*
	 * Don't allow the time to be set forward so far it
	 * will wrap and become negative, thus allowing an
	 * attacker to bypass the next check below.  The
	 * cutoff is 1 year before rollover occurs, so even
	 * if the attacker uses adjtime(2) to move the time
	 * past the cutoff, it will take a very long time
	 * to get to the wrap point.
	 */
	if ((ts->tv_sec > LLONG_MAX - 365*24*60*60) ||
	    (delta->tv_sec < 0 || delta->tv_nsec < 0))
		return true;

	return false;
}
