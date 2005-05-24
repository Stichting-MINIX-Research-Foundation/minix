/* The system call implemented in this file:
 *   m_type:	SYS_DEBUG
 *
 * The parameters for this system call are:
 */

#include "../kernel.h"
#include "../system.h"
#include "../proc.h"

#if ENABLE_K_DEBUGGING		/* only include code if enabled */

#define PROCLIMIT 10000

PUBLIC void
check_runqueues(char *when)
{
  int q, l = 0;
  register struct proc *xp;

  for (xp = BEG_PROC_ADDR; xp < END_PROC_ADDR; ++xp) {
	xp->p_found = 0;
	if(l++ > PROCLIMIT) {  panic("check error", NO_NUM); }
  }

  for (q=0; q < NR_SCHED_QUEUES; q++) {
    if(rdy_head[q] && !rdy_tail[q]) {
	kprintf("head but no tail: %s", (karg_t) when);
		 panic("scheduling error", NO_NUM);
    }
    if(!rdy_head[q] && rdy_tail[q]) {
	kprintf("tail but no head: %s", (karg_t) when);
		 panic("scheduling error", NO_NUM);
    }
    if(rdy_tail[q] && rdy_tail[q]->p_nextready != NIL_PROC) {
	kprintf("tail and tail->next not null; %s", (karg_t) when);
		 panic("scheduling error", NO_NUM);
    }
    for(xp = rdy_head[q]; xp != NIL_PROC; xp = xp->p_nextready) {
        if (!xp->p_ready) {
		kprintf("scheduling error: unready on runq: %s\n", (karg_t) when);
		
  		panic("found unready process on run queue", NO_NUM);
        }
        if(xp->p_priority != q) {
		kprintf("scheduling error: wrong priority: %s\n", (karg_t) when);
		
		panic("wrong priority", NO_NUM);
	}
	if(xp->p_found) {
		kprintf("scheduling error: double scheduling: %s\n", (karg_t) when);
		panic("proc more than once on scheduling queue", NO_NUM);
	}
	xp->p_found = 1;
	if(xp->p_nextready == NIL_PROC && rdy_tail[q] != xp) {
		kprintf("scheduling error: last element not tail: %s\n", (karg_t) when);
		panic("scheduling error", NO_NUM);
	}
	if(l++ > PROCLIMIT) panic("loop in schedule queue?", NO_NUM);
    }
  }	

  for (xp = BEG_PROC_ADDR; xp < END_PROC_ADDR; ++xp) {
	if(isalivep(xp) && xp->p_ready && !xp->p_found) {
		kprintf("scheduling error: ready not on queue: %s\n", (karg_t) when);
		panic("ready proc not on scheduling queue", NO_NUM);
		if(l++ > PROCLIMIT) { panic("loop in proc.t?", NO_NUM); }
	}
  }
}

/*==========================================================================*
 *				do_debug 				    *
 *==========================================================================*/

#endif	/* ENABLE_K_DEBUGGING */
