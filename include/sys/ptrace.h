/* <sys/ptrace.h>
 * definitions for ptrace(2) 
 */

#ifndef _PTRACE_H
#define _PTRACE_H

#define T_STOP	       -1	/* stop the process */
#define T_OK		0	/* enable tracing by parent for this process */
#define T_GETINS	1	/* return value from instruction space */
#define T_GETDATA	2	/* return value from data space */
#define T_GETUSER	3	/* return value from user process table */
#define	T_SETINS	4	/* set value from instruction space */
#define T_SETDATA	5	/* set value from data space */
#define T_SETUSER	6	/* set value in user process table */
#define T_RESUME	7	/* resume execution */
#define T_EXIT		8	/* exit */
#define T_STEP		9	/* set trace bit */

/* Function Prototypes. */
#ifndef _ANSI_H
#include <ansi.h>
#endif

_PROTOTYPE( long ptrace, (int _req, pid_t _pid, long _addr, long _data) );

#endif /* _PTRACE_H */
