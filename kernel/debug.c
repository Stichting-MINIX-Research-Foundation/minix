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

#define MAX_LOOP (NR_PROCS + NR_TASKS)

PUBLIC int
runqueues_ok(void)
{
  int q, l = 0;
  register struct proc *xp;

  for (xp = BEG_PROC_ADDR; xp < END_PROC_ADDR; ++xp) {
	xp->p_found = 0;
	if (l++ > MAX_LOOP) panic("check error");
  }

  for (q=l=0; q < NR_SCHED_QUEUES; q++) {
    if (rdy_head[q] && !rdy_tail[q]) {
	printf("head but no tail in %d\n", q);
	return 0;
    }
    if (!rdy_head[q] && rdy_tail[q]) {
	printf("tail but no head in %d\n", q);
	return 0;
    }
    if (rdy_tail[q] && rdy_tail[q]->p_nextready) {
	printf("tail and tail->next not null in %d\n", q);
	return 0;
    }
    for(xp = rdy_head[q]; xp; xp = xp->p_nextready) {
	const vir_bytes vxp = (vir_bytes) xp;
	vir_bytes dxp;
	if(vxp < (vir_bytes) BEG_PROC_ADDR || vxp >= (vir_bytes) END_PROC_ADDR) {
  		printf("xp out of range\n");
		return 0;
	}
	dxp = vxp - (vir_bytes) BEG_PROC_ADDR;
	if(dxp % sizeof(struct proc)) {
  		printf("xp not a real pointer");
		return 0;
	}
	if(!proc_ptr_ok(xp)) {
  		printf("xp bogus pointer");
		return 0;
	}
	if (RTS_ISSET(xp, RTS_SLOT_FREE)) {
		printf("scheduling error: dead proc q %d %d\n",
			q, xp->p_endpoint);
		return 0;
	}
        if (!proc_is_runnable(xp)) {
		printf("scheduling error: unready on runq %d proc %d\n",
			q, xp->p_nr);
		return 0;
        }
        if (xp->p_priority != q) {
		printf("scheduling error: wrong priority q %d proc %d ep %d name %s\n",
			q, xp->p_nr, xp->p_endpoint, xp->p_name);
		return 0;
	}
	if (xp->p_found) {
		printf("scheduling error: double sched q %d proc %d\n",
			q, xp->p_nr);
		return 0;
	}
	xp->p_found = 1;
	if (!xp->p_nextready && rdy_tail[q] != xp) {
		printf("sched err: last element not tail q %d proc %d\n",
			q, xp->p_nr);
		return 0;
	}
	if (l++ > MAX_LOOP) {
		printf("loop in schedule queue?");
		return 0;
	}
    }
  }	

  l = 0;
  for (xp = BEG_PROC_ADDR; xp < END_PROC_ADDR; ++xp) {
	if(!proc_ptr_ok(xp)) {
		printf("xp bogus pointer in proc table\n");
		return 0;
	}
	if (isemptyp(xp))
		continue;
	if(proc_is_runnable(xp) && !xp->p_found) {
		printf("sched error: ready proc %d not on queue\n", xp->p_nr);
		return 0;
		if (l++ > MAX_LOOP) {
			printf("loop in debug.c?\n"); 
			return 0;
		}
	}
  }

  /* All is ok. */
  return 1;
}

PUBLIC char *
rtsflagstr(const int flags)
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
miscflagstr(const int flags)
{
	static char str[100];
	str[0] = '\0';

	FLAG(MF_REPLY_PEND);
	FLAG(MF_ASYNMSG);
	FLAG(MF_FULLVM);
	FLAG(MF_DELIVERMSG);
	FLAG(MF_KCALL_RESUME);

	return str;
}

PUBLIC char *
schedulerstr(struct proc *scheduler)
{
	if (scheduler != NULL)
	{
		return scheduler->p_name;
	}

	return "KERNEL";
}

