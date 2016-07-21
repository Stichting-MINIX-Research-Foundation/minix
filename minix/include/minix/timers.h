/* This library provides generic watchdog timer management functionality.
 * The functions operate on a timer queue provided by the caller. Note that
 * the timers must use absolute time to allow sorting. The library provides:
 *
 *    tmrs_settimer:     (re)set a new watchdog timer in the timers queue
 *    tmrs_clrtimer:     remove a timer from both the timers queue
 *    tmrs_exptimers:    check for expired timers and run watchdog functions
 *
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 *    Adapted from tmr_settimer and tmr_clrtimer in src/kernel/clock.c.
 *    Last modified: September 30, 2004.
 */
#ifndef _MINIX_TIMERS_H
#define _MINIX_TIMERS_H

#include <limits.h>

#include <sys/types.h>
#include <minix/const.h>
#include <minix/u64.h>
#include <minix/minlib.h>
#include <minix/endpoint.h>

typedef void (*tmr_func_t)(int arg);

/* A minix_timer_t variable must be declare for each distinct timer to be used.
 * The timers watchdog function and expiration time are automatically set
 * by the library function tmrs_settimer, but its argument is not.  In general,
 * the timer is in use when it has a callback function.
 */
typedef struct minix_timer
{
  struct minix_timer	*tmr_next;	/* next in a timer chain */
  clock_t 		tmr_exp_time;	/* expiration time (absolute) */
  tmr_func_t		tmr_func;	/* function to call when expired */
  int			tmr_arg;	/* integer argument */
} minix_timer_t;

/*
 * Clock times may wrap.  Thus, we must only ever compare relative times, which
 * means they must be no more than half the total maximum time value apart.
 * The clock_t type is unsigned (int or long), thus we take half that.
 */
#define TMRDIFF_MAX		(INT_MAX)

/* This value must be used only instead of a timer difference value. */
#define TMR_NEVER		((clock_t)TMRDIFF_MAX + 1)

/* These definitions can be used to set or get data from a timer variable. */
#define tmr_exp_time(tp)	((tp)->tmr_exp_time)
#define tmr_is_set(tp)		((tp)->tmr_func != NULL)
/*
 * tmr_is_first() returns TRUE iff the first given absolute time is sooner than
 * or equal to the second given time.
 */
#define tmr_is_first(a,b)	((clock_t)(b) - (clock_t)(a) <= TMRDIFF_MAX)
#define tmr_has_expired(tp,now)	tmr_is_first((tp)->tmr_exp_time, (now))

/* Timers should be initialized once before they are being used. Be careful
 * not to reinitialize a timer that is in a list of timers, or the chain
 * will be broken.
 */
#define tmr_inittimer(tp) (void)((tp)->tmr_func = NULL, (tp)->tmr_next = NULL)

/* The following generic timer management functions are available. They
 * can be used to operate on the lists of timers. Adding a timer to a list
 * automatically takes care of removing it.
 */
int tmrs_settimer(minix_timer_t **tmrs, minix_timer_t *tp, clock_t exp_time,
	tmr_func_t watchdog, int arg, clock_t *old_head, clock_t *new_head);
int tmrs_clrtimer(minix_timer_t **tmrs, minix_timer_t *tp, clock_t *old_head,
	clock_t *new_head);
int tmrs_exptimers(minix_timer_t **tmrs, clock_t now, clock_t *new_head);

#define PRINT_STATS(cum_spenttime, cum_instances) {		\
		if(ex64hi(cum_spenttime)) { util_stacktrace(); printf(" ( ??? %lu %lu)\n",	\
			ex64hi(cum_spenttime), ex64lo(cum_spenttime)); } \
		printf("%s:%d,%lu,%lu\n", \
			__FILE__, __LINE__, cum_instances,	\
			 ex64lo(cum_spenttime)); \
	}

#define RESET_STATS(starttime, cum_instances, cum_spenttime, cum_starttime) { \
		cum_instances = 0;				\
		cum_starttime = starttime;			\
		cum_spenttime = make64(0,0);			\
}

#define TIME_BLOCK_VAR(timed_code_block, time_interval) do {	\
	static u64_t _cum_spenttime, _cum_starttime;		\
	static int _cum_instances;				\
	u64_t _next_cum_spent, _starttime, _endtime, _dt, _cum_dt;	\
	u32_t _dt_micros;					\
	read_tsc_64(&_starttime);				\
	do { timed_code_block } while(0);			\
	read_tsc_64(&_endtime);					\
	_dt = _endtime - _starttime;				\
	if(_cum_instances == 0) {				\
		RESET_STATS(_starttime, _cum_instances, _cum_spenttime, _cum_starttime); \
	 }							\
	_next_cum_spent = add64(_cum_spenttime, _dt);		\
	if(ex64hi(_next_cum_spent)) { 				\
		PRINT_STATS(_cum_spenttime, _cum_instances);	\
		RESET_STATS(_starttime, _cum_instances, _cum_spenttime, _cum_starttime); \
	} 							\
	_cum_spenttime = add64(_cum_spenttime, _dt);		\
	_cum_instances++;					\
	_cum_dt = _endtime - _cum_starttime;			\
	if(_cum_dt > make64(0, 120)) {				\
		PRINT_STATS(_cum_spenttime, _cum_instances);	\
		RESET_STATS(_starttime, _cum_instances, _cum_spenttime, _cum_starttime); 	\
	} 							\
} while(0)

#define TIME_BLOCK(timed_code_block) TIME_BLOCK_VAR(timed_code_block, 100)
#define TIME_BLOCK_T(timed_code_block, t) TIME_BLOCK_VAR(timed_code_block, t)

/* Timers abstraction for system processes. This would be in minix/sysutil.h
 * if it weren't for naming conflicts.
 */

void init_timer(minix_timer_t *tp);
void set_timer(minix_timer_t *tp, clock_t ticks, tmr_func_t watchdog, int arg);
void cancel_timer(minix_timer_t *tp);
void expire_timers(clock_t now);

#endif /* _MINIX_TIMERS_H */
