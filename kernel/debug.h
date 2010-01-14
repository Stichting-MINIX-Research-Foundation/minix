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
	int mustunlock = 0; 					\
	if(!intr_disabled()) { lock; mustunlock = 1; }		\
	vmassert(varname == ENTERED || varname == NOTENTERED);	\
	vmassert(magictest == MAGICTEST);			\
	vmassert(varname != ENTERED);				\
	varname = ENTERED;

#define NOREC_RETURN(varname, v) do {				\
	vmassert(intr_disabled());				\
	vmassert(magictest == MAGICTEST);			\
	vmassert(varname == ENTERED || varname == NOTENTERED);	\
	varname = NOTENTERED;					\
	if(mustunlock)	{ unlock;	} 			\
	return v;						\
} while(0)

#if DEBUG_VMASSERT
#define vmassert(t) { \
	if(!(t)) { minix_panic("vm: assert " #t " failed in " __FILE__, __LINE__); } }
#else
#define vmassert(t) { }
#endif

#define NOT_REACHABLE	do {						\
	kprintf("NOT_REACHABLE at %s:%d\n", __FILE__, __LINE__);	\
	minix_panic("execution at an unexpected location\n", NO_NUM);	\
	for(;;);							\
} while(0)

#define NOT_IMPLEMENTED do {	\
		kprintf("NOT_IMPLEMENTED at %s:%d\n", __FILE__, __LINE__); \
		minix_panic("NOT_IMPLEMENTED", NO_NUM); \
} while(0)

#ifdef CONFIG_BOOT_VERBOSE
#define BOOT_VERBOSE(x)	x
#else
#define BOOT_VERBOSE(x)
#endif

#endif /* DEBUG_H */
