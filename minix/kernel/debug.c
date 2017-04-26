/* This file implements kernel debugging functionality that is not included
 * in the standard kernel. Available functionality includes timing of lock
 * functions and sanity checking of the scheduling queues.
 */

#include "kernel/kernel.h"

#include <minix/callnr.h>
#include <minix/u64.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

#define MAX_LOOP (NR_PROCS + NR_TASKS)

int runqueues_ok_cpu(unsigned cpu)
{
  int q, l = 0;
  register struct proc *xp;
  struct proc **rdy_head, **rdy_tail;

  rdy_head = get_cpu_var(cpu, run_q_head);
  rdy_tail = get_cpu_var(cpu, run_q_tail);

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
	}
  }

  /* All is ok. */
  return 1;
}

#ifdef CONFIG_SMP
static int runqueues_ok_all(void)
{
	unsigned c;

	for (c = 0 ; c < ncpus; c++) {
		if (!runqueues_ok_cpu(c))
			return 0;
	}
	return 1;	
}

int runqueues_ok(void)
{
	return runqueues_ok_all();
}

#else

int runqueues_ok(void)
{
	return runqueues_ok_cpu(0);
}


#endif

char *
rtsflagstr(const u32_t flags)
{
	static char str[100];
	str[0] = '\0';

#define FLAG(n) if(flags & n) { strlcat(str, #n " ", sizeof(str)); }

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

char *
miscflagstr(const u32_t flags)
{
	static char str[100];
	str[0] = '\0';

	FLAG(MF_REPLY_PEND);
	FLAG(MF_DELIVERMSG);
	FLAG(MF_KCALL_RESUME);

	return str;
}

char *
schedulerstr(struct proc *scheduler)
{
	if (scheduler != NULL)
	{
		return scheduler->p_name;
	}

	return "KERNEL";
}

static void
print_proc_name(struct proc *pp)
{
	char *name = pp->p_name;
	endpoint_t ep = pp->p_endpoint;

	if(name) {
		printf("%s(%d)", name, ep);
	}
	else {
		printf("%d", ep);
	}
}

static void
print_endpoint(endpoint_t ep)
{
	int proc_nr;
	struct proc *pp = NULL;

	switch(ep) {
	case ANY:
		printf("ANY");
	break;
	case SELF:
		printf("SELF");
	break;
	case NONE:
		printf("NONE");
	break;
	default:
		if(!isokendpt(ep, &proc_nr)) {
			printf("??? %d\n", ep);
		}
		else {
			pp = proc_addr(proc_nr);
			if(isemptyp(pp)) {
				printf("??? empty slot %d\n", proc_nr);
			}
			else {
				print_proc_name(pp);
			}
		}
	break;
	}
}

static void
print_sigmgr(struct proc *pp)
{
	endpoint_t sig_mgr, bak_sig_mgr;
	sig_mgr = priv(pp) ? priv(pp)->s_sig_mgr : NONE;
	bak_sig_mgr = priv(pp) ? priv(pp)->s_bak_sig_mgr : NONE;
	if(sig_mgr == NONE) { printf("no sigmgr"); return; }
	printf("sigmgr ");
	print_endpoint(sig_mgr);
	if(bak_sig_mgr != NONE) {
		printf(" / ");
		print_endpoint(bak_sig_mgr);
	}
}

void print_proc(struct proc *pp)
{
	endpoint_t dep;

	printf("%d: %s %d prio %d time %d/%d cycles 0x%x%08x cpu %2d "
			"pdbr 0x%lx rts %s misc %s sched %s ",
		proc_nr(pp), pp->p_name, pp->p_endpoint, 
		pp->p_priority, pp->p_user_time,
		pp->p_sys_time, ex64hi(pp->p_cycles),
		ex64lo(pp->p_cycles), pp->p_cpu,
#if defined(__i386__)
		pp->p_seg.p_cr3,
#elif defined(__arm__)
		pp->p_seg.p_ttbr,
#endif
		rtsflagstr(pp->p_rts_flags), miscflagstr(pp->p_misc_flags),
		schedulerstr(pp->p_scheduler));

	print_sigmgr(pp);

	dep = P_BLOCKEDON(pp);
	if(dep != NONE) {
		printf(" blocked on: ");
		print_endpoint(dep);
	}
	printf("\n");
}

static void print_proc_depends(struct proc *pp, const int level)
{
	struct proc *depproc = NULL;
	endpoint_t dep;
#define COL { int i; for(i = 0; i < level; i++) printf("> "); }

	if(level >= NR_PROCS) {
		printf("loop??\n");
		return;
	}

	COL

	print_proc(pp);

	COL
	proc_stacktrace(pp);


	dep = P_BLOCKEDON(pp);
	if(dep != NONE && dep != ANY) {
		int procno;
		if(isokendpt(dep, &procno)) {
			depproc = proc_addr(procno);
			if(isemptyp(depproc))
				depproc = NULL;
		}
		if (depproc)
			print_proc_depends(depproc, level+1);
	}
}

void print_proc_recursive(struct proc *pp)
{
	print_proc_depends(pp, 0);
}

#if DEBUG_DUMPIPC || DEBUG_DUMPIPCF
static const char *mtypename(int mtype, int *possible_callname)
{
	char *callname = NULL, *errname = NULL;
	/* use generated file to recognize message types
	 *
	 * we try to match both error numbers and call numbers, as the
	 * reader can probably decide from context what's going on.
	 *
	 * whenever it might be a call number we tell the caller so the
	 * call message fields can be decoded if known.
	 */
	switch(mtype) {
#define IDENT(x) case x: callname = #x; *possible_callname = 1; break;
#include "kernel/extracted-mtype.h"
#undef IDENT
	}
	switch(mtype) {
#define IDENT(x) case x: errname = #x; break;
#include "kernel/extracted-errno.h"
#undef IDENT
	}

	/* no match */
	if(!errname && !callname)
		return NULL;

	/* 2 matches */
	if(errname && callname) {
		static char typename[100];
		strcpy(typename, errname);
		strcat(typename, " / ");
		strcat(typename, callname);
		return typename;
	}

	if(errname) return errname;

	assert(callname);
	return callname;
}

static void printproc(struct proc *rp)
{
	if (rp)
		printf(" %s(%d)", rp->p_name, rp - proc);
	else
		printf(" kernel");
}

static void printparam(const char *name, const void *data, size_t size)
{
	printf(" %s=", name);
	switch (size) {
		case sizeof(char):	printf("%d", *(char *) data);	break;
		case sizeof(short):	printf("%d", *(short *) data);	break;
		case sizeof(int):	printf("%d", *(int *) data);	break;
		default:		printf("(%u bytes)", size);	break;
	}
}

#ifdef DEBUG_DUMPIPC_NAMES
static int namematch(char **names, int nnames, char *name)
{
	int i;
	for(i = 0; i < nnames; i++)
		if(!strcmp(names[i], name))
			return 1;
	return 0;
}
#endif

void printmsg(message *msg, struct proc *src, struct proc *dst,
	char operation, int printparams)
{
	const char *name;
	int mtype = msg->m_type, mightbecall = 0;

#ifdef DEBUG_DUMPIPC_NAMES
  {
	char *names[] = DEBUG_DUMPIPC_NAMES;
	int nnames = sizeof(names)/sizeof(names[0]);

	/* skip printing messages for messages neither to
	 * or from DEBUG_DUMPIPC_EP if it is defined; either
	 * can be NULL to indicate kernel
	 */
	if(!(src && namematch(names, nnames, src->p_name)) &&
	   !(dst && namematch(names, nnames, dst->p_name))) {
		return;
	}
  }
#endif

	/* source, destination and message type */
	printf("%c", operation);
	printproc(src);
	printproc(dst);
	name = mtypename(mtype, &mightbecall);
	if (name) {
		printf(" %s(%d/0x%x)", name, mtype, mtype);
	} else {
		printf(" %d/0x%x", mtype, mtype);
	}

	if (mightbecall && printparams) {
#define IDENT(x, y) if (mtype == x) printparam(#y, &msg->y, sizeof(msg->y));
#include "kernel/extracted-mfield.h"
#undef IDENT
	}
	printf("\n");
}
#endif

#if DEBUG_IPCSTATS
#define IPCPROCS (NR_PROCS+1)	/* number of slots we need */
#define KERNELIPC NR_PROCS	/* slot number to use for kernel calls */
static int messages[IPCPROCS][IPCPROCS];

#define PRINTSLOTS 20
static struct {
	int src, dst, messages;
} winners[PRINTSLOTS];
static int total, goodslots;

static void printstats(int ticks)
{
	int i;
	for(i = 0; i < goodslots; i++) {
#define name(s) (s == KERNELIPC ? "kernel" : proc_addr(s)->p_name)
#define persec(n) (system_hz*(n)/ticks)
		char	*n1 = name(winners[i].src),
			*n2 = name(winners[i].dst);
		printf("%2d.  %8s -> %8s  %9d/s\n",
			i, n1, n2, persec(winners[i].messages));
	}
	printf("total %d/s\n", persec(total));
}

static void sortstats(void)
{
	/* Print top message senders/receivers. */
	int src_slot, dst_slot;
	total = goodslots = 0;
	for(src_slot = 0; src_slot < IPCPROCS; src_slot++) {
		for(dst_slot = 0; dst_slot < IPCPROCS; dst_slot++) {
			int w = PRINTSLOTS, rem,
				n = messages[src_slot][dst_slot];
			total += n;
			while(w > 0 && n > winners[w-1].messages)
				w--;
			if(w >= PRINTSLOTS) continue;

			/* This combination has beaten the current winners
			 * and should be inserted at position 'w.'
			 */
			rem = PRINTSLOTS-w-1;
			assert(rem >= 0);
			assert(rem < PRINTSLOTS);
			if(rem > 0) {
				assert(w+1 <= PRINTSLOTS-1);
				assert(w >= 0);
				memmove(&winners[w+1], &winners[w],
					rem*sizeof(winners[0]));
			}
			winners[w].src = src_slot;
			winners[w].dst = dst_slot;
			winners[w].messages = n;
			if(goodslots < PRINTSLOTS) goodslots++;
		}
	}
}

#define proc2slot(p, s) { \
	if(p) { s = p->p_nr; } \
	else { s = KERNELIPC; } \
	assert(s >= 0 && s < IPCPROCS); \
}

static void statmsg(message *msg, struct proc *srcp, struct proc *dstp)
{
	int src, dst, now, secs, dt;
	static int lastprint;

	/* Stat message. */
	assert(src);
	proc2slot(srcp, src);
	proc2slot(dstp, dst);
	messages[src][dst]++;

	/* Print something? */
	now = get_monotonic();
	dt = now - lastprint;
	secs = dt/system_hz;
	if(secs >= 30) {
		memset(winners, 0, sizeof(winners));
		sortstats();
		printstats(dt);
		memset(messages, 0, sizeof(messages));
		lastprint = now;
	}
}
#endif

#if DEBUG_IPC_HOOK
void hook_ipc_msgkcall(message *msg, struct proc *proc)
{
#if DEBUG_DUMPIPC
	printmsg(msg, proc, NULL, 'k', 1);
#endif
}

void hook_ipc_msgkresult(message *msg, struct proc *proc)
{
#if DEBUG_DUMPIPC
	printmsg(msg, NULL, proc, 'k', 0);
#endif
#if DEBUG_IPCSTATS
	statmsg(msg, proc, NULL);
#endif
}

void hook_ipc_msgrecv(message *msg, struct proc *src, struct proc *dst)
{
#if DEBUG_DUMPIPC
	printmsg(msg, src, dst, 'r', 0);
#endif
#if DEBUG_IPCSTATS
	statmsg(msg, src, dst);
#endif
}

void hook_ipc_msgsend(message *msg, struct proc *src, struct proc *dst)
{
#if DEBUG_DUMPIPC
	printmsg(msg, src, dst, 's', 1);
#endif
}

void hook_ipc_clear(struct proc *p)
{
#if DEBUG_IPCSTATS
	int slot, i;
	assert(p);
	proc2slot(p, slot);
	for(i = 0; i < IPCPROCS; i++)
		messages[slot][i] = messages[i][slot] = 0;
#endif
}
#endif
