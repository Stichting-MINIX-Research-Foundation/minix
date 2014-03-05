
#ifndef _MINIX_ENDPOINT_H
#define _MINIX_ENDPOINT_H 1

#ifdef _MINIX_SYSTEM

#include <minix/sys_config.h>
#include <minix/com.h>
#include <limits.h>
#include <minix/type.h>

/*
 * Endpoints are split into two parts: a process slot and a generation number.
 *
 * The process slot number identifies the slot in various process tables.
 * It is positive or zero for user processes, and negative for kernel tasks.
 * Constants dictate that with the current endpoint layout, the maximum range
 * of process slot numbers is [-MAX_NR_TASKS,MAX_NR_PROCS>.  The used part of
 * the range is currently [-NR_TASKS,NR_PROCS> -- these two constants may be
 * changed within the maximum range without changing the endpoint layout.
 *
 * The generation number is a per-slot number that gets increased by one every
 * time a slot is reused for a new process.  The generation number minimizes
 * the chance that the endpoint of a dead process can (accidentially) be used
 * to communicate with a different, live process.  Preventing such accidents
 * is essential for proper system service restartability support.
 *
 * The split between the two parts of the endpoint is such that when the
 * generation number is zero, the endpoint number equals the process slot
 * number, even for negative task numbers.  This is required for the endpoint
 * numbers hardcoded in <minix/com.h>, and it makes endpoint numbers easy to
 * read in general.
 *
 * There are three special endpoint numbers: ANY, NONE, and SELF.  These
 * numbers are used to identify "any process", "no process at all", and
 * "own process", respectively.  They fall outside the normal range of
 * process slot numbers, and are always of generation zero.
 */

/*
 * The following constant defines the split between the two parts of the
 * endpoint numbers.  It can be adjusted to allow for either more processes
 * or more per-process generation numbers.  Changing it will change the
 * endpoint number layout, and thus break binary compatibility with existing
 * processes.
 */
#define _ENDPOINT_GENERATION_SHIFT	15

/* Derived constants. */
#define _ENDPOINT_GENERATION_SIZE	(1 << _ENDPOINT_GENERATION_SHIFT)
/* INT_MAX is used here to prevent signedness issues with the macros below. */
#define _ENDPOINT_MAX_GENERATION	(INT_MAX/_ENDPOINT_GENERATION_SIZE-1)
#define _ENDPOINT_SLOT_TOP	(_ENDPOINT_GENERATION_SIZE-MAX_NR_TASKS)

/* The special endpoint numbers, and the resulting maximum slot number. */
#define ANY		((endpoint_t) (_ENDPOINT_SLOT_TOP - 1))
#define NONE		((endpoint_t) (_ENDPOINT_SLOT_TOP - 2))
#define SELF		((endpoint_t) (_ENDPOINT_SLOT_TOP - 3))
#define MAX_NR_PROCS		      (_ENDPOINT_SLOT_TOP - 3)	/* (int)SELF */

/* Sanity check. */
#if NR_PROCS > MAX_NR_PROCS
#error "NR_PROCS exceeds MAX_NR_PROCS, increase _ENDPOINT_GENERATION_SHIFT"
#endif

/* Generation + Process slot number <-> endpoint. */
#define _ENDPOINT(g, p) \
	((endpoint_t)(((g) << _ENDPOINT_GENERATION_SHIFT) + (p)))
#define _ENDPOINT_G(e) (((e)+MAX_NR_TASKS) >> _ENDPOINT_GENERATION_SHIFT)
#define _ENDPOINT_P(e) \
	((((e)+MAX_NR_TASKS) & (_ENDPOINT_GENERATION_SIZE - 1)) - MAX_NR_TASKS)

#endif /* _MINIX_SYSTEM */

#endif
