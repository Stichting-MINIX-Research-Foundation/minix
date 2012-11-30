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

#ifndef _TIMERS_H
#define _TIMERS_H

#include <limits.h>

#include <sys/types.h>
#include <minix/u64.h>
#include <minix/minlib.h>
#include <minix/endpoint.h>

struct timer;
typedef void (*tmr_func_t)(struct timer *tp);
typedef union { int ta_int; long ta_long; void *ta_ptr; } tmr_arg_t;

/* A timer_t variable must be declare for each distinct timer to be used.
 * The timers watchdog function and expiration time are automatically set
 * by the library function tmrs_settimer, but its argument is not.
 */
typedef struct timer
{
  struct timer	*tmr_next;	/* next in a timer chain */
  clock_t 	tmr_exp_time;	/* expiration time */
  tmr_func_t	tmr_func;	/* function to call when expired */
  tmr_arg_t	tmr_arg;	/* random argument */
} timer_t;

/* Used when the timer is not active. */
#define TMR_NEVER    ((clock_t) -1 < 0) ? ((clock_t) LONG_MAX) : ((clock_t) -1)
#undef TMR_NEVER
#define TMR_NEVER	((clock_t) LONG_MAX)

/* These definitions can be used to set or get data from a timer variable. */ 
#define tmr_arg(tp) (&(tp)->tmr_arg)
#define tmr_exp_time(tp) (&(tp)->tmr_exp_time)

/* Timers should be initialized once before they are being used. Be careful
 * not to reinitialize a timer that is in a list of timers, or the chain
 * will be broken.
 */
#define tmr_inittimer(tp) (void)((tp)->tmr_exp_time = TMR_NEVER, \
	(tp)->tmr_next = NULL)

/* The following generic timer management functions are available. They
 * can be used to operate on the lists of timers. Adding a timer to a list 
 * automatically takes care of removing it.
 */
clock_t tmrs_clrtimer(timer_t **tmrs, timer_t *tp, clock_t *new_head);
void tmrs_exptimers(timer_t **tmrs, clock_t now, clock_t *new_head);
clock_t tmrs_settimer(timer_t **tmrs, timer_t *tp, clock_t exp_time,
	tmr_func_t watchdog, clock_t *new_head);

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
	_dt = sub64(_endtime, _starttime);			\
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
	_cum_dt = sub64(_endtime, _cum_starttime);		\
	if(cmp64(_cum_dt, make64(0, 120)) > 0) {		\
		PRINT_STATS(_cum_spenttime, _cum_instances);	\
		RESET_STATS(_starttime, _cum_instances, _cum_spenttime, _cum_starttime); 	\
	} 							\
} while(0)

#define TIME_BLOCK(timed_code_block) TIME_BLOCK_VAR(timed_code_block, 100)
#define TIME_BLOCK_T(timed_code_block, t) TIME_BLOCK_VAR(timed_code_block, t)

#endif /* _TIMERS_H */

