#ifndef DEBUG_H
#define DEBUG_H

/* This header file defines all debugging constants and macros, and declares
 * some variables. Certain debugging features redefine standard constants
 * and macros. Therefore, this header file should be included after the
 * other kernel headers.
 */

#include <ansi.h>
#include "config.h"

/* Enable prints such as
 *  . send/receive failed due to deadlock or dead source or dead destination
 *  . trap not allowed
 *  . bogus message pointer
 *  . kernel call number not allowed by this process
 *
 * Of course the call still fails, but nothing is printed if these warnings
 * are disabled.
 */
#define DEBUG_ENABLE_IPC_WARNINGS	0
#define DEBUG_STACKTRACE		1

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

#endif /* DEBUG_H */
