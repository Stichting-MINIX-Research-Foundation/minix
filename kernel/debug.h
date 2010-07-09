#ifndef DEBUG_H
#define DEBUG_H

/* This header file defines all debugging constants and macros, and declares
 * some variables. Certain debugging features redefine standard constants
 * and macros. Therefore, this header file should be included after the
 * other kernel headers.
 */

#include <ansi.h>
#include <minix/debug.h>
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
#define DEBUG_ENABLE_IPC_WARNINGS	1
#define DEBUG_STACKTRACE		1
#define DEBUG_TIME_LOCKS		1

/* Sanity checks. */
#define DEBUG_SANITYCHECKS		0

/* Verbose messages. */
#define DEBUG_TRACE			0

/* DEBUG_RACE makes every process preemptible, schedules
 * every process on the same priority queue, and randomizes
 * the next process to run, in order to help catch race
 * conditions that could otherwise be masked.
 */
#define DEBUG_RACE			0

/* DEBUG_DUMPIPC dumps all IPC to serial; due to the amount of logging it is 
 * strongly recommended to set "ctty 0" in the boot monitor and run inside a 
 * virtual machine if you enable this; on the hardware it would take forever 
 * just to boot
 */
#define DEBUG_DUMPIPC			0

#if DEBUG_TRACE

#define VF_SCHEDULING		(1L << 1)
#define VF_PICKPROC		(1L << 2)

#define TRACE(code, statement) if(verboseflags & code) { printf("%s:%d: ", __FILE__, __LINE__); statement }

#else
#define TRACE(code, statement)
#endif

#ifdef CONFIG_BOOT_VERBOSE
#define BOOT_VERBOSE(x)	x
#else
#define BOOT_VERBOSE(x)
#endif

#ifdef _SYSTEM
#define DEBUG_PRINT(params, level) do { \
	if (verboseboot >= (level)) printf params; } while (0)
#define DEBUGBASIC(params) DEBUG_PRINT(params, VERBOSEBOOT_BASIC)
#define DEBUGEXTRA(params) DEBUG_PRINT(params, VERBOSEBOOT_EXTRA)
#define DEBUGMAX(params)   DEBUG_PRINT(params, VERBOSEBOOT_MAX)
#endif

#endif /* DEBUG_H */
