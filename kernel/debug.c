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

#if DEBUG_SCHED_CHECK		/* only include code if enabled */

#define MAX_LOOP (NR_PROCS + NR_TASKS)

PUBLIC void
check_runqueues_f(char *file, int line)
{
  int q, l = 0;
  register struct proc *xp;

  if(!intr_disabled()) {
	minix_panic("check_runqueues called with interrupts enabled", NO_NUM);
  }

  FIXME("check_runqueues being done");

#define MYPANIC(msg) {		\
	kprintf("check_runqueues:%s:%d: %s\n", file, line, msg); \
	minix_panic("check_runqueues failed", NO_NUM);	\
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
	vir_bytes vxp = (vir_bytes) xp, dxp;
	if(vxp < (vir_bytes) BEG_PROC_ADDR || vxp >= (vir_bytes) END_PROC_ADDR) {
  		MYPANIC("xp out of range");
	}
	dxp = vxp - (vir_bytes) BEG_PROC_ADDR;
	if(dxp % sizeof(struct proc)) {
  		MYPANIC("xp not a real pointer");
	}
	if(xp->p_magic != PMAGIC) {
  		MYPANIC("magic wrong in xp");
	}
	if (RTS_ISSET(xp, RTS_SLOT_FREE)) {
		kprintf("scheduling error: dead proc q %d %d\n",
			q, xp->p_endpoint);
  		MYPANIC("dead proc on run queue");
	}
        if (!xp->p_ready) {
		kprintf("scheduling error: unready on runq %d proc %d\n",
			q, xp->p_nr);
  		MYPANIC("found unready process on run queue");
        }
        if (xp->p_priority != q) {
		kprintf("scheduling error: wrong priority q %d proc %d ep %d name %s\n",
			q, xp->p_nr, xp->p_endpoint, xp->p_name);
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
	if(xp->p_magic != PMAGIC) 
		MYPANIC("p_magic wrong in proc table");
	if (isemptyp(xp))
		continue;
	if(xp->p_ready && ! xp->p_found) {
		kprintf("sched error: ready proc %d not on queue\n", xp->p_nr);
		MYPANIC("ready proc not on scheduling queue");
		if (l++ > MAX_LOOP) { MYPANIC("loop in debug.c?"); }
	}
  }
}

#endif /* DEBUG_SCHED_CHECK */

PUBLIC char *
rtsflagstr(int flags)
{
	static char str[100];
	str[0] = '\0';

#define FLAG(n) if(flags & n) { strcat(str, #n " "); }

	FLAG(RTS_SLOT_FREE);
	FLAG(RTS_PROC_STOP);
	FLAG(RTS_SENDING);
	FLAG(RTS_RECEIVING);
	FLAG(RTS_SIGNALED);
	FLAG(RTS_SIG_PENDING);
	FLAG(RTS_P_STOP);
	FLAG(RTS_NO_PRIV);
	FLAG(RTS_NO_ENDPOINT);
	FLAG(RTS_VMINHIBIT);
	FLAG(RTS_PAGEFAULT);
	FLAG(RTS_VMREQUEST);
	FLAG(RTS_VMREQTARGET);
	FLAG(RTS_PREEMPTED);
	FLAG(RTS_NO_QUANTUM);

	return str;
}

PUBLIC char *
miscflagstr(int flags)
{
	static char str[100];
	str[0] = '\0';

	FLAG(MF_REPLY_PEND);
	FLAG(MF_ASYNMSG);
	FLAG(MF_FULLVM);
	FLAG(MF_DELIVERMSG);

	return str;
}

