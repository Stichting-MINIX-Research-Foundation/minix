/* This file implements kernel debugging functionality that is not included
 * in the standard kernel. Available functionality includes timing of lock
 * functions and sanity checking of the scheduling queues.
 */

#include "kernel/kernel.h"

#include <minix/callnr.h> // Kept for now
#include <minix/u64.h>    // Kept for now
// #include <limits.h>    // Removed (INT_MAX might be an issue)
// #include <string.h>    // Replaced
// #include <assert.h>    // Replaced

// Added kernel headers
#include <minix/kernel_types.h>
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


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
	kprintf_stub("head but no tail in %d\n", q); // MODIFIED
	return 0;
    }
    if (!rdy_head[q] && rdy_tail[q]) {
	kprintf_stub("tail but no head in %d\n", q); // MODIFIED
	return 0;
    }
    if (rdy_tail[q] && rdy_tail[q]->p_nextready) {
	kprintf_stub("tail and tail->next not null in %d\n", q); // MODIFIED
	return 0;
    }
    for(xp = rdy_head[q]; xp; xp = xp->p_nextready) {
	const vir_bytes vxp = (vir_bytes) xp;
	vir_bytes dxp;
	if(vxp < (vir_bytes) BEG_PROC_ADDR || vxp >= (vir_bytes) END_PROC_ADDR) {
		kprintf_stub("xp out of range\n"); // MODIFIED
		return 0;
	}
	dxp = vxp - (vir_bytes) BEG_PROC_ADDR;
	if(dxp % sizeof(struct proc)) {
		kprintf_stub("xp not a real pointer"); // MODIFIED
		return 0;
	}
	if(!proc_ptr_ok(xp)) {
		kprintf_stub("xp bogus pointer"); // MODIFIED
		return 0;
	}
	if (RTS_ISSET(xp, RTS_SLOT_FREE)) {
		kprintf_stub("scheduling error: dead proc q %d %d\n", // MODIFIED
			q, xp->p_endpoint);
		return 0;
	}
        if (!proc_is_runnable(xp)) {
		kprintf_stub("scheduling error: unready on runq %d proc %d\n", // MODIFIED
			q, xp->p_nr);
		return 0;
        }
        if (xp->p_priority != q) {
		kprintf_stub("scheduling error: wrong priority q %d proc %d ep %d name %s\n", // MODIFIED
			q, xp->p_nr, xp->p_endpoint, xp->p_name);
		return 0;
	}
	if (xp->p_found) {
		kprintf_stub("scheduling error: double sched q %d proc %d\n", // MODIFIED
			q, xp->p_nr);
		return 0;
	}
	xp->p_found = 1;
	if (!xp->p_nextready && rdy_tail[q] != xp) {
		kprintf_stub("sched err: last element not tail q %d proc %d\n", // MODIFIED
			q, xp->p_nr);
		return 0;
	}
	if (l++ > MAX_LOOP) {
		kprintf_stub("loop in schedule queue?"); // MODIFIED
		return 0;
	}
    }
  }	

  for (xp = BEG_PROC_ADDR; xp < END_PROC_ADDR; ++xp) {
	if(!proc_ptr_ok(xp)) {
		kprintf_stub("xp bogus pointer in proc table\n"); // MODIFIED
		return 0;
	}
	if (isemptyp(xp))
		continue;
	if(proc_is_runnable(xp) && !xp->p_found) {
		kprintf_stub("sched error: ready proc %d not on queue\n", xp->p_nr); // MODIFIED
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

#define FLAG(n) if(flags & n) { /* FIXME: strlcat(str, #n " ", sizeof(str)); */ } // MODIFIED

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
		kprintf_stub("%s(%d)", name, ep); // MODIFIED
	}
	else {
		kprintf_stub("%d", ep); // MODIFIED
	}
}

static void
print_endpoint(endpoint_t ep)
{
	int proc_nr;
	struct proc *pp = NULL;

	switch(ep) {
	case ANY:
		kprintf_stub("ANY"); // MODIFIED
	break;
	case SELF:
		kprintf_stub("SELF"); // MODIFIED
	break;
	case NONE:
		kprintf_stub("NONE"); // MODIFIED
	break;
	default:
		if(!isokendpt(ep, &proc_nr)) {
			kprintf_stub("??? %d\n", ep); // MODIFIED
		}
		else {
			pp = proc_addr(proc_nr);
			if(isemptyp(pp)) {
				kprintf_stub("??? empty slot %d\n", proc_nr); // MODIFIED
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
	if(sig_mgr == NONE) { kprintf_stub("no sigmgr"); return; } // MODIFIED
	kprintf_stub("sigmgr "); // MODIFIED
	print_endpoint(sig_mgr);
	if(bak_sig_mgr != NONE) {
		kprintf_stub(" / "); // MODIFIED
		print_endpoint(bak_sig_mgr);
	}
}

void print_proc(struct proc *pp)
{
	endpoint_t dep;

	kprintf_stub("%d: %s %d prio %d time %d/%d cycles 0x%x%08x cpu %2d " // MODIFIED
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
		kprintf_stub(" blocked on: "); // MODIFIED
		print_endpoint(dep);
	}
	kprintf_stub("\n"); // MODIFIED
}

static void print_proc_depends(struct proc *pp, const int level)
{
	struct proc *depproc = NULL;
	endpoint_t dep;
#define COL { int i; for(i = 0; i < level; i++) kprintf_stub("> "); } // MODIFIED

	if(level >= NR_PROCS) {
		kprintf_stub("loop??\n"); // MODIFIED
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
		(void)kstrlcpy(typename, errname, sizeof(typename)); /* FIXME: strcpy was here, validate size for kstrlcpy. sizeof(dst) is a guess. */ // MODIFIED
		/* FIXME: strcat was here */ // (void)kstrcat(typename, " / ", sizeof(typename));
		/* FIXME: strcat was here */ // (void)kstrcat(typename, callname, sizeof(typename));
		return typename;
	}

	if(errname) return errname;

	KASSERT_PLACEHOLDER(callname); // MODIFIED
	return callname;
}

static void printproc(struct proc *rp)
{
	if (rp)
		kprintf_stub(" %s(%d)", rp->p_name, rp - proc); // MODIFIED
	else
		kprintf_stub(" kernel"); // MODIFIED
}

static void printparam(const char *name, const void *data, k_size_t size) // MODIFIED size_t
{
	kprintf_stub(" %s=", name); // MODIFIED
	switch (size) {
		case sizeof(char):	kprintf_stub("%d", *(char *) data);	break; // MODIFIED
		case sizeof(short):	kprintf_stub("%d", *(short *) data);	break; // MODIFIED
		case sizeof(int):	kprintf_stub("%d", *(int *) data);	break; // MODIFIED
		default:		kprintf_stub("(%u bytes)", size);	break; // MODIFIED
	}
}

#ifdef DEBUG_DUMPIPC_NAMES
static int namematch(char **names, int nnames, char *name)
{
	int i;
	for(i = 0; i < nnames; i++)
		if(!kstrcmp(names[i], name)) // MODIFIED
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
	kprintf_stub("%c", operation); // MODIFIED
	printproc(src);
	printproc(dst);
	name = mtypename(mtype, &mightbecall);
	if (name) {
		kprintf_stub(" %s(%d/0x%x)", name, mtype, mtype); // MODIFIED
	} else {
		kprintf_stub(" %d/0x%x", mtype, mtype); // MODIFIED
	}

	if (mightbecall && printparams) {
#define IDENT(x, y) if (mtype == x) printparam(#y, &msg->y, sizeof(msg->y));
#include "kernel/extracted-mfield.h"
#undef IDENT
	}
	kprintf_stub("\n"); // MODIFIED
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
		kprintf_stub("%2d.  %8s -> %8s  %9d/s\n", // MODIFIED
			i, n1, n2, persec(winners[i].messages));
	}
	kprintf_stub("total %d/s\n", persec(total)); // MODIFIED
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
			KASSERT_PLACEHOLDER(rem >= 0); // MODIFIED
			KASSERT_PLACEHOLDER(rem < PRINTSLOTS); // MODIFIED
			if(rem > 0) {
				KASSERT_PLACEHOLDER(w+1 <= PRINTSLOTS-1); // MODIFIED
				KASSERT_PLACEHOLDER(w >= 0); // MODIFIED
				kmemmove(&winners[w+1], &winners[w], // MODIFIED
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
	KASSERT_PLACEHOLDER(s >= 0 && s < IPCPROCS); \
}

static void statmsg(message *msg, struct proc *srcp, struct proc *dstp)
{
	int src, dst, now, secs, dt;
	static int lastprint;

	/* Stat message. */
	KASSERT_PLACEHOLDER(src); // This assert was on 'src' which is uninitialized here. Assuming it meant srcp or similar.
                               // For now, keeping as is, but this is a bug in original code.
                               // If it meant to assert srcp, it would be KASSERT_PLACEHOLDER(srcp);
	proc2slot(srcp, src);
	proc2slot(dstp, dst);
	messages[src][dst]++;

	/* Print something? */
	now = get_monotonic();
	dt = now - lastprint;
	secs = dt/system_hz;
	if(secs >= 30) {
		kmemset(winners, 0, sizeof(winners)); // MODIFIED
		sortstats();
		printstats(dt);
		kmemset(messages, 0, sizeof(messages)); // MODIFIED
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
	KASSERT_PLACEHOLDER(p); // MODIFIED
	proc2slot(p, slot);
	for(i = 0; i < IPCPROCS; i++)
		messages[slot][i] = messages[i][slot] = 0;
#endif
}
#endif
