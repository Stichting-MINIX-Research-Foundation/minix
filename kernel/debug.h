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
#define DEBUG_VMASSERT			1
#define DEBUG_SCHED_CHECK		1

#define NOREC_ENTER(varname) \
	static int varname = 0;	\
	int mustunlock = 0; \
	if(!intr_disabled()) { lock; mustunlock = 1; } \
	if(varname) {	\
		minix_panic(#varname " recursive enter", __LINE__); \
	} \
	varname = 1;	\
	FIXME(#varname " recursion check enabled");

#define NOREC_RETURN(varname, v) do {	\
	if(!varname)		\
		minix_panic(#varname " flag off", __LINE__); \
	if(!intr_disabled())	\
		minix_panic(#varname " interrupts on", __LINE__); \
	varname = 0;	\
	if(mustunlock)	{ unlock;	} \
	return v;	\
	} while(0)


#if DEBUG_VMASSERT
#define vmassert(t) { \
	if(!(t)) { minix_panic("vm: assert " #t " failed\n", __LINE__); } }
#else
#define vmassert(t) { }
#endif

#endif /* DEBUG_H */
