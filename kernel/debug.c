/* This file implements kernel debugging functionality that is not included
 * in the standard kernel. Available functionality includes timing of lock
 * functions and sanity checking of the scheduling queues.
 */

#include "kernel.h"
#include "proc.h"
#include "debug.h"
#include <limits.h>

#if DEBUG_TIME_LOCKS		/* only include code if enabled */

/* Data structures to store lock() timing data. */
struct lock_timingdata timingdata[TIMING_CATEGORIES];
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
	timingdata[0].names[sizeof(timingdata[0].names)-1] = '\0';

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

#endif /* DEBUG_TIME_LOCKS */

#if DEBUG_SCHED_CHECK		/* only include code if enabled */

#define PROCLIMIT 10000

PUBLIC void
check_runqueues(char *when)
{
  int q, l = 0;
  register struct proc *xp;

  for (xp = BEG_PROC_ADDR; xp < END_PROC_ADDR; ++xp) {
	xp->p_found = 0;
	if (l++ > PROCLIMIT) {  panic("check error", NO_NUM); }
  }

  for (q=0; q < NR_SCHED_QUEUES; q++) {
    if (rdy_head[q] && !rdy_tail[q]) {
	kprintf("head but no tail: %s", when);
		 panic("scheduling error", NO_NUM);
    }
    if (!rdy_head[q] && rdy_tail[q]) {
	kprintf("tail but no head: %s", when);
		 panic("scheduling error", NO_NUM);
    }
    if (rdy_tail[q] && rdy_tail[q]->p_nextready != NIL_PROC) {
	kprintf("tail and tail->next not null; %s", when);
		 panic("scheduling error", NO_NUM);
    }
    for(xp = rdy_head[q]; xp != NIL_PROC; xp = xp->p_nextready) {
        if (!xp->p_ready) {
		kprintf("scheduling error: unready on runq: %s\n", when);
		
  		panic("found unready process on run queue", NO_NUM);
        }
        if (xp->p_priority != q) {
		kprintf("scheduling error: wrong priority: %s\n", when);
		
		panic("wrong priority", NO_NUM);
	}
	if (xp->p_found) {
		kprintf("scheduling error: double scheduling: %s\n", when);
		panic("proc more than once on scheduling queue", NO_NUM);
	}
	xp->p_found = 1;
	if (xp->p_nextready == NIL_PROC && rdy_tail[q] != xp) {
		kprintf("scheduling error: last element not tail: %s\n", when);
		panic("scheduling error", NO_NUM);
	}
	if (l++ > PROCLIMIT) panic("loop in schedule queue?", NO_NUM);
    }
  }	

  for (xp = BEG_PROC_ADDR; xp < END_PROC_ADDR; ++xp) {
	if (! isemptyp(xp) && xp->p_ready && ! xp->p_found) {
		kprintf("scheduling error: ready not on queue: %s\n", when);
		panic("ready proc not on scheduling queue", NO_NUM);
		if (l++ > PROCLIMIT) { panic("loop in proc.t?", NO_NUM); }
	}
  }
}

#endif /* DEBUG_SCHED_CHECK */
