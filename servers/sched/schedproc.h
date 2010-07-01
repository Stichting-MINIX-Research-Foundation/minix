/* This table has one slot per process.  It contains scheduling information
 * for each process.
 */
#include <limits.h>

/* EXTERN should be extern except in main.c, where we want to keep the struct */
#ifdef _MAIN
#undef EXTERN
#define EXTERN
#endif

/**
 * We might later want to add more information to this table, such as the
 * process owner, process group or cpumask.
 */

EXTERN struct schedproc {
	endpoint_t endpoint;	/* process endpoint id */
	endpoint_t parent;	/* parent endpoint id */
	unsigned flags;		/* flag bits */

	/* User space scheduling */
	unsigned max_priority;	/* this process' highest allowed priority */
	unsigned priority;		/* the process' current priority */
	unsigned time_slice;		/* this process's time slice */
} schedproc[NR_PROCS];

/* Flag values */
#define IN_USE		0x00001	/* set when 'schedproc' slot in use */
