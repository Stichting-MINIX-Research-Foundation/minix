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
#define DEBUG_VMASSERT			1
#define DEBUG_SCHED_CHECK		1
#define DEBUG_TIME_LOCKS		1

/* It's interesting to measure the time spent withing locked regions, because
 * this is the time that the system is deaf to interrupts.
 */

#define TIMING_POINTS		20	/* timing resolution */
#define TIMING_CATEGORIES	20
#define TIMING_NAME		10

#endif /* DEBUG_H */
