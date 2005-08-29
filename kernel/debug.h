#ifndef DEBUG_H
#define DEBUG_H

/* This header file defines all debugging constants and macros, and declares
 * some variables. Certain debugging features redefine standard constants
 * and macros. Therefore, this header file should be included after the
 * other kernel headers.
 */

#include "config.h"

/* It's interesting to measure the time spent withing locked regions, because
 * this is the time that the system is deaf to interrupts.
 */
#if DEBUG_TIME_LOCKS

#define TIMING_POINTS		20	/* timing resolution */
#define TIMING_CATEGORIES	20
#define TIMING_NAME		10

/* Definition of the data structure to store lock() timing data. */ 
struct lock_timingdata {
	char names[TIMING_NAME];
	unsigned long lock_timings[TIMING_POINTS];
	unsigned long lock_timings_range[2];
	unsigned long binsize, resets, misses, measurements;
};

/* The data is declared here, but allocated in debug.c. */
extern struct lock_timingdata timingdata[TIMING_CATEGORIES];

/* Prototypes for the timing functionality. */
_PROTOTYPE( void timer_start, (int cat, char *name) );
_PROTOTYPE( void timer_end, (int cat) );

#define locktimestart(c, v) timer_start(c, v)
#define locktimeend(c) timer_end(c)
#else
#define locktimestart(c, v)
#define locktimeend(c)
#endif /* DEBUG_TIME_LOCKS */

/* The locking checks counts relocking situation, which are dangerous because
 * the inner lock may unlock the outer one.
 */
#if DEBUG_LOCK_CHECK
#define lockcheck if (!(read_cpu_flags() & X86_FLAG_I)) kinfo.relocking++;
#else
#define lockcheck
#endif /* DEBUG_LOCK_CHECK */

/* This check makes sure that the scheduling queues are in a consistent state.
 * The check is run when the queues are updated with ready() and unready().
 */ 
#if DEBUG_SCHED_CHECK 					
_PROTOTYPE( void check_runqueues, (char *when) );
#endif /* DEBUG_SCHED_CHECK */

/* The timing and checking of kernel locking requires a redefine of the lock()
 * and unlock() macros. That's done here. This redefine requires that this 
 * header is included after the other kernel headers.
 */
#if (DEBUG_TIME_LOCKS || DEBUG_LOCK_CHECK)
#  undef lock
#  define lock(c, v)	do { lockcheck; \
	intr_disable(); \
	locktimestart(c, v); \
	} while(0)
#  undef unlock
#  define unlock(c)	do { locktimeend(c); \
	intr_enable();\
	 } while(0)
#endif

#endif /* DEBUG_H */
