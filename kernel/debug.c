/* This file implements kernel debugging functionality that is not included
 * in the standard kernel. Available functionality includes timing of lock
 * functions and sanity checking of the scheduling queues.
 */

#include "kernel.h"
#include "proc.h"
#include "debug.h"

#include <minix/sysutil.h>
#include <limits.h>
#include <string.h>


/* Data structures to store lock() timing data. */
static unsigned long starttimes[TIMING_CATEGORIES][2];

#define HIGHCOUNT	0
#define LOWCOUNT	1

void timer_start(int cat, char *name)
{
	static int init = 0;
	unsigned long h, l;
	int i;

	if (cat < 0 || cat >= TIMING_CATEGORIES) return;

	for(i = 0; i < sizeof(timingdata[0].names) && *name; i++)
		timingdata[cat].names[i] = *name++;
	timingdata[cat].names[sizeof(timingdata[0].names)-1] = '\0';

	if (starttimes[cat][HIGHCOUNT]) {  return; }

	if (!init) {
		int t, f;
		init = 1;
		for(t = 0; t < TIMING_CATEGORIES; t++) {
			timingdata[t].lock_timings_range[0] = 0;
			timingdata[t].resets = timingdata[t].misses = 
				timingdata[t].measurements = 0;
		}
	}

	read_tsc(&starttimes[cat][HIGHCOUNT], &starttimes[cat][LOWCOUNT]);
}

void timer_end(int cat)
{
	unsigned long h, l, d = 0, binsize;
	int bin;

	read_tsc(&h, &l);
	if (cat < 0 || cat >= TIMING_CATEGORIES) return;
	if (!starttimes[cat][HIGHCOUNT]) {
		timingdata[cat].misses++;
		return;
	}
	if (starttimes[cat][HIGHCOUNT] == h) {
		d = (l - starttimes[cat][1]);
	} else if (starttimes[cat][HIGHCOUNT] == h-1 &&
		starttimes[cat][LOWCOUNT] > l) {
		d = ((ULONG_MAX - starttimes[cat][LOWCOUNT]) + l);
	} else {
		timingdata[cat].misses++;
		return;
	}
	starttimes[cat][HIGHCOUNT] = 0;
	if (!timingdata[cat].lock_timings_range[0] ||
		d < timingdata[cat].lock_timings_range[0] ||
		d > timingdata[cat].lock_timings_range[1]) {
		int t;
		if (!timingdata[cat].lock_timings_range[0] ||
			d < timingdata[cat].lock_timings_range[0])
			timingdata[cat].lock_timings_range[0] = d;
		if (!timingdata[cat].lock_timings_range[1] ||
			d > timingdata[cat].lock_timings_range[1])
			timingdata[cat].lock_timings_range[1] = d;
		for(t = 0; t < TIMING_POINTS; t++)
			timingdata[cat].lock_timings[t] = 0;
		timingdata[cat].binsize =
			(timingdata[cat].lock_timings_range[1] -
			timingdata[cat].lock_timings_range[0])/(TIMING_POINTS+1);
		if (timingdata[cat].binsize < 1)
		  timingdata[cat].binsize = 1;
		timingdata[cat].resets++;
	}
	bin = (d-timingdata[cat].lock_timings_range[0]) /
		timingdata[cat].binsize;
	if (bin < 0 || bin >= TIMING_POINTS) {
		int t;
		/* this indicates a bug, but isn't really serious */
		for(t = 0; t < TIMING_POINTS; t++)
			timingdata[cat].lock_timings[t] = 0;
		timingdata[cat].misses++;
	} else {
		timingdata[cat].lock_timings[bin]++;
		timingdata[cat].measurements++;
	}

	return;
}

#if DEBUG_SCHED_CHECK		/* only include code if enabled */

#define MAX_LOOP (NR_PROCS + NR_TASKS)

PUBLIC void
check_runqueues_f(char *file, int line)
{
  int q, l = 0;
  register struct proc *xp;
#define MYPANIC(msg) {		\
	static char buf[100];	\
	strcpy(buf, file);	\
	strcat(buf, ": ");	\
	util_nstrcat(buf, line);\
	strcat(buf, ": ");	\
	strcat(buf, msg);	\
	minix_panic(buf, NO_NUM);	\
	}

  for (xp = BEG_PROC_ADDR; xp < END_PROC_ADDR; ++xp) {
	xp->p_found = 0;
	if (l++ > MAX_LOOP) {  MYPANIC("check error"); }
  }

  for (q=l=0; q < NR_SCHED_QUEUES; q++) {
    if (rdy_head[q] && !rdy_tail[q]) {
	kprintf("head but no tail in %d\n", q);
		 MYPANIC("scheduling error");
    }
    if (!rdy_head[q] && rdy_tail[q]) {
	kprintf("tail but no head in %d\n", q);
		 MYPANIC("scheduling error");
    }
    if (rdy_tail[q] && rdy_tail[q]->p_nextready != NIL_PROC) {
	kprintf("tail and tail->next not null in %d\n", q);
		 MYPANIC("scheduling error");
    }
    for(xp = rdy_head[q]; xp != NIL_PROC; xp = xp->p_nextready) {
        if (!xp->p_ready) {
		kprintf("scheduling error: unready on runq %d proc %d\n",
			q, xp->p_nr);
  		MYPANIC("found unready process on run queue");
        }
        if (xp->p_priority != q) {
		kprintf("scheduling error: wrong priority q %d proc %d\n",
			q, xp->p_nr);
		MYPANIC("wrong priority");
	}
	if (xp->p_found) {
		kprintf("scheduling error: double sched q %d proc %d\n",
			q, xp->p_nr);
		MYPANIC("proc more than once on scheduling queue");
	}
	xp->p_found = 1;
	if (xp->p_nextready == NIL_PROC && rdy_tail[q] != xp) {
		kprintf("sched err: last element not tail q %d proc %d\n",
			q, xp->p_nr);
		MYPANIC("scheduling error");
	}
	if (l++ > MAX_LOOP) MYPANIC("loop in schedule queue?");
    }
  }	

  l = 0;
  for (xp = BEG_PROC_ADDR; xp < END_PROC_ADDR; ++xp) {
	if (! isemptyp(xp) && xp->p_ready && ! xp->p_found) {
		kprintf("sched error: ready proc %d not on queue\n", xp->p_nr);
		MYPANIC("ready proc not on scheduling queue");
		if (l++ > MAX_LOOP) { MYPANIC("loop in debug.c?"); }
	}
  }
}

#endif /* DEBUG_SCHED_CHECK */
