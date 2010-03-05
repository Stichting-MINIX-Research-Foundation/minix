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

/* Runtime sanity checking. */
#define DEBUG_VMASSERT			0
#define DEBUG_SCHED_CHECK		0
#define DEBUG_STACK_CHECK		0
#define DEBUG_TRACE			0

#if DEBUG_TRACE

#define VF_SCHEDULING		(1L << 1)
#define VF_PICKPROC		(1L << 2)

#define TRACE(code, statement) if(verboseflags & code) { printf("%s:%d: ", __FILE__, __LINE__); statement }

#else
#define TRACE(code, statement)
#endif

#define ENTERED		0xBA5E1514
#define NOTENTERED	0x1415BEE1

#define NOREC_ENTER(varname) 					\
	static int varname = NOTENTERED;			\
	vmassert(varname == ENTERED || varname == NOTENTERED);	\
	vmassert(magictest == MAGICTEST);			\
	vmassert(varname != ENTERED);				\
	varname = ENTERED;

#define NOREC_RETURN(varname, v) do {				\
	vmassert(magictest == MAGICTEST);			\
	vmassert(varname == ENTERED || varname == NOTENTERED);	\
	varname = NOTENTERED;					\
	return v;						\
} while(0)

#if DEBUG_VMASSERT
#define vmassert(t) { \
	if(!(t)) { panic("kernel:%s:%d: assert %s failed", __FILE__, __LINE__, #t); } }
#else
#define vmassert(t) { }
#endif

#define NOT_REACHABLE	do {						\
	panic("NOT_REACHABLE at %s:%d", __FILE__, __LINE__);	\
	for(;;);							\
} while(0)

#define NOT_IMPLEMENTED do {	\
		panic("NOT_IMPLEMENTED at %s:%d", __FILE__, __LINE__); \
} while(0)

#ifdef CONFIG_BOOT_VERBOSE
#define BOOT_VERBOSE(x)	x
#else
#define BOOT_VERBOSE(x)
#endif

#ifdef _SYSTEM
#define DEBUG_PRINT(params, level) do { \
	if (verboseboot >= (level)) printf params; } while (0)
#define DEBUGBASIC(params) DEBUG_PRINT(params, VERBOSEBOOT_BASIC)
#define DEBUGMAX(params)   DEBUG_PRINT(params, VERBOSEBOOT_MAX)
#endif

#endif /* DEBUG_H */
