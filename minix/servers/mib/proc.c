/* MIB service - proc.c - functionality based on service process tables */
/* Eventually, the CTL_PROC subtree might end up here as well. */

#include "mib.h"

#include <sys/exec.h>
#include <minix/sysinfo.h>

#include <machine/archtypes.h>
#include "kernel/proc.h"
#include "servers/pm/mproc.h"
#include "servers/vfs/const.h"
#include "servers/vfs/fproc.h"

typedef struct proc ixfer_proc_t;
typedef struct mproc ixfer_mproc_t;

static ixfer_proc_t proc_tab[NR_TASKS + NR_PROCS];
static ixfer_mproc_t mproc_tab[NR_PROCS];
static struct fproc_light fproc_tab[NR_PROCS];

/*
 * The number of processes added to the current number of processes when doing
 * a size estimation, so that the actual data retrieval does not end up with
 * too little space if new processes have forked between the two calls.  We do
 * a process table update only once per clock tick, which means that typically
 * no update will take place between the user process's size estimation request
 * and its subsequent data retrieval request.  On the other hand, if we do
 * update process tables in between, quite a bit might have changed.
 */
#define EXTRA_PROCS	8

#define HASH_SLOTS 	(NR_PROCS / 4)	/* expected nr. of processes in use */
#define NO_SLOT		(-1)
static int hash_tab[HASH_SLOTS];	/* hash table mapping from PID.. */
static int hnext_tab[NR_PROCS];		/* ..to PM process slot */

static clock_t tabs_updated = 0;	/* when the tables were last updated */
static int tabs_valid = TRUE;		/* FALSE if obtaining tables failed */

/*
 * Update the process tables by pulling in new copies from the kernel, PM, and
 * VFS, but only every so often and only if it has not failed before.  Return
 * TRUE iff the tables are now valid.
 */
static int
update_tables(void)
{
	clock_t now;
	pid_t pid;
	int r, kslot, mslot, hslot;

	/*
	 * If retrieving the tables failed at some point, do not keep trying
	 * all the time.  Such a failure is very unlikely to be transient.
	 */
	if (tabs_valid == FALSE)
		return FALSE;

	/*
	 * Update the tables once per clock tick at most.  The update operation
	 * is rather heavy, transferring several hundreds of kilobytes between
	 * servers.  Userland should be able to live with information that is
	 * outdated by at most one clock tick.
	 */
	now = getticks();

	if (tabs_updated != 0 && tabs_updated == now)
		return TRUE;

	/* Perform an actual update now. */
	tabs_valid = FALSE;

	/* Retrieve and check the kernel process table. */
	if ((r = sys_getproctab(proc_tab)) != OK) {
		printf("MIB: unable to obtain kernel process table (%d)\n", r);

		return FALSE;
	}

	for (kslot = 0; kslot < NR_TASKS + NR_PROCS; kslot++) {
		if (proc_tab[kslot].p_magic != PMAGIC) {
			printf("MIB: kernel process table mismatch\n");

			return FALSE;
		}
	}

	/* Retrieve and check the PM process table. */
	r = getsysinfo(PM_PROC_NR, SI_PROC_TAB, mproc_tab, sizeof(mproc_tab));
	if (r != OK) {
		printf("MIB: unable to obtain PM process table (%d)\n", r);

		return FALSE;
	}

	for (mslot = 0; mslot < NR_PROCS; mslot++) {
		if (mproc_tab[mslot].mp_magic != MP_MAGIC) {
			printf("MIB: PM process table mismatch\n");

			return FALSE;
		}
	}

	/* Retrieve an extract of the VFS process table. */
	r = getsysinfo(VFS_PROC_NR, SI_PROCLIGHT_TAB, fproc_tab,
	    sizeof(fproc_tab));
	if (r != OK) {
		printf("MIB: unable to obtain VFS process table (%d)\n", r);

		return FALSE;
	}

	tabs_valid = TRUE;
	tabs_updated = now;

	/*
	 * Build a hash table mapping from process IDs to slot numbers, for
	 * fast access.  TODO: decide if this is better done on demand only.
	 */
	for (hslot = 0; hslot < HASH_SLOTS; hslot++)
		hash_tab[hslot] = NO_SLOT;

	for (mslot = 0; mslot < NR_PROCS; mslot++) {
		if (mproc_tab[mslot].mp_flags & IN_USE) {
			if ((pid = mproc_tab[mslot].mp_pid) <= 0)
				continue;

			hslot = mproc_tab[mslot].mp_pid % HASH_SLOTS;

			hnext_tab[mslot] = hash_tab[hslot];
			hash_tab[hslot] = mslot;
		}
	}

	return TRUE;
}

/*
 * Return the PM slot number for the given PID, or NO_SLOT if the PID is not in
 * use by a process.
 */
static int
get_mslot(pid_t pid)
{
	int mslot;

	/* PID 0 identifies the kernel; checking this is up to the caller. */
	if (pid <= 0)
		return NO_SLOT;

	for (mslot = hash_tab[pid % HASH_SLOTS]; mslot != NO_SLOT;
	    mslot = hnext_tab[mslot])
		if (mproc_tab[mslot].mp_pid == pid)
			break;

	return mslot;
}

/*
 * Store the given number of clock ticks as a timeval structure.
 */
static void
ticks_to_timeval(struct timeval * tv, clock_t ticks)
{
	clock_t hz;

	hz = sys_hz();

	tv->tv_sec = ticks / hz;
	tv->tv_usec = (long)((ticks % hz) * 1000000ULL / hz);
}

/*
 * Generate a wchan message text for the cases that the process is blocked on
 * IPC with another process, of which the endpoint is given as 'endpt' here.
 * The name of the other process is to be stored in 'wmesg', which is a buffer
 * of size 'wmsz'.  The result should be null terminated.  If 'ipc' is set, the
 * process is blocked on a direct IPC call, in which case the name of the other
 * process is enclosed in parentheses.  If 'ipc' is not set, the call is made
 * indirectly through VFS, and the name of the other process should not be
 * enclosed in parentheses.  If no name can be obtained, we use the endpoint of
 * the other process instead.
 */
static void
fill_wmesg(char * wmesg, size_t wmsz, endpoint_t endpt, int ipc)
{
	const char *name;
	int mslot;

	switch (endpt) {
	case ANY:
		name = "any";
		break;
	case SELF:
		name = "self";
		break;
	case NONE:
		name = "none";
		break;
	default:
		mslot = _ENDPOINT_P(endpt);
		if (mslot >= -NR_TASKS && mslot < NR_PROCS &&
		    (mslot < 0 || (mproc_tab[mslot].mp_flags & IN_USE)))
			name = proc_tab[NR_TASKS + mslot].p_name;
		else
			name = NULL;
	}

	if (name != NULL)
		snprintf(wmesg, wmsz, "%s%s%s",
		    ipc ? "(" : "", name, ipc ? ")" : "");
	else
		snprintf(wmesg, wmsz, "%s%d%s",
		    ipc ? "(" : "", endpt, ipc ? ")" : "");
}

/*
 * Return the LWP status of a process, along with additional information in
 * case the process is sleeping (LSSLEEP): a wchan value and text to indicate
 * what the process is sleeping on, and possibly a flag field modification to
 * indicate that the sleep is interruptible.
 */
static int
get_lwp_stat(int mslot, uint64_t * wcptr, char * wmptr, size_t wmsz,
	int32_t * flag)
{
	struct mproc *mp;
	struct fproc_light *fp;
	struct proc *kp;
	const char *wmesg;
	uint64_t wchan;
	endpoint_t endpt;

	mp = &mproc_tab[mslot];
	fp = &fproc_tab[mslot];
	kp = &proc_tab[NR_TASKS + mslot];

	/*
	 * First cover all the cases that the process is not sleeping.  In
	 * those cases, we need not return additional sleep information either.
	 */
	if (mp->mp_flags & (TRACE_ZOMBIE | ZOMBIE))
		return LSZOMB;

	if (mp->mp_flags & EXITING)
		return LSDEAD;

	if ((mp->mp_flags & TRACE_STOPPED) || RTS_ISSET(kp, RTS_P_STOP))
		return LSSTOP;

	if (proc_is_runnable(kp))
		return LSRUN;

	/*
	 * The process is sleeping.  In that case, we must also figure out why,
	 * and return an appropriate wchan value and human-readable wmesg text.
	 *
	 * The process can be blocked on either a known sleep state in PM or
	 * VFS, or otherwise on IPC communication with another process, or
	 * otherwise on a kernel RTS flag.  In each case, decide what to use as
	 * wchan value and wmesg text, and whether the sleep is interruptible.
	 *
	 * The wchan value should be unique for the sleep reason.  We use its
	 * lower eight bits to indicate a class:
	 *   0x00 = kernel task
	 *   0x01 = kerel RTS block
	 *   0x02 = PM call
	 *   0x03 = VFS call
	 *   0x04 = MIB call
	 *   0xff = blocked on process
	 * The upper bits are used for class-specific information.  The actual
	 * value does not really matter, as long as it is nonzero and there is
	 * no overlap between the different values.
	 */
	wchan = 0;
	wmesg = NULL;

	/*
	 * First see if the process is marked as blocked in the tables of PM or
	 * VFS.  Such a block reason is always an interruptible sleep.  Note
	 * that we do not use the kernel table at all in this case: each of the
	 * three tables is consistent within itself, but not necessarily
	 * consistent with any of the other tables, so we avoid internal
	 * mismatches if we can.
	 */
	if (mp->mp_flags & WAITING) {
		wchan = 0x102;
		wmesg = "wait";
	} else if (mp->mp_flags & SIGSUSPENDED) {
		wchan = 0x202;
		wmesg = "pause";
	} else if (fp->fpl_blocked_on != FP_BLOCKED_ON_NONE) {
		wchan = (fp->fpl_blocked_on << 8) | 0x03;
		switch (fp->fpl_blocked_on) {
		case FP_BLOCKED_ON_PIPE:
			wmesg = "pipe";
			break;
		case FP_BLOCKED_ON_FLOCK:
			wmesg = "flock";
			break;
		case FP_BLOCKED_ON_POPEN:
			wmesg = "popen";
			break;
		case FP_BLOCKED_ON_SELECT:
			wmesg = "select";
			break;
		case FP_BLOCKED_ON_CDEV:
		case FP_BLOCKED_ON_SDEV:
			/*
			 * Add the task (= character or socket driver) endpoint
			 * to the wchan value, and use the driver's process
			 * name, without parentheses, as wmesg text.
			 */
			wchan |= (uint64_t)fp->fpl_task << 16;
			fill_wmesg(wmptr, wmsz, fp->fpl_task, FALSE /*ipc*/);
			break;
		default:
			/* A newly added flag we don't yet know about? */
			wmesg = "???";
			break;
		}
	}
	if (wchan != 0) {
		*wcptr = wchan;
		if (wmesg != NULL) /* NULL means "already set" here */
			strlcpy(wmptr, wmesg, wmsz);
		*flag |= L_SINTR;
	}

	/*
	 * See if the process is blocked on sending or receiving.  If not, then
	 * use one of the kernel RTS flags as reason.
	 */
	endpt = P_BLOCKEDON(kp);

	switch (endpt) {
	case MIB_PROC_NR:
		/* This is really just aesthetics. */
		wchan = 0x04;
		wmesg = "sysctl";
		break;
	case NONE:
		/*
		 * The process is not running, but also not blocked on IPC with
		 * another process.  This means it must be stopped on a kernel
		 * RTS flag.
		 */
		wchan = ((uint64_t)kp->p_rts_flags << 8) | 0x01;
		if (RTS_ISSET(kp, RTS_PROC_STOP))
			wmesg = "kstop";
		else if (RTS_ISSET(kp, RTS_SIGNALED) ||
		    RTS_ISSET(kp, RTS_SIGNALED))
			wmesg = "ksignal";
		else if (RTS_ISSET(kp, RTS_NO_PRIV))
			wmesg = "knopriv";
		else if (RTS_ISSET(kp, RTS_PAGEFAULT) ||
		    RTS_ISSET(kp, RTS_VMREQTARGET))
			wmesg = "fault";
		else if (RTS_ISSET(kp, RTS_NO_QUANTUM))
			wmesg = "sched";
		else
			wmesg = "kflag";
		break;
	case ANY:
		/*
		 * If the process is blocked receiving from ANY, mark it as
		 * being in an interruptible sleep.  This looks nicer, even
		 * though "interruptible" is not applicable to services at all.
		 */
		*flag |= L_SINTR;
		break;
	}

	/*
	 * If at this point wchan is still zero, the process is blocked sending
	 * or receiving.  Use a wchan value based on the target endpoint, and
	 * use "(procname)" as wmesg text.
	 */
	if (wchan == 0) {
		*wcptr = ((uint64_t)endpt << 8) | 0xff;
		fill_wmesg(wmptr, wmsz, endpt, TRUE /*ipc*/);
	} else {
		*wcptr = wchan;
		if (wmesg != NULL) /* NULL means "already set" here */
			strlcpy(wmptr, wmesg, wmsz);
	}

	return LSSLEEP;
}


/*
 * Fill the part of a LWP structure that is common between kernel tasks and
 * user processes.  Also return a CPU estimate in 'estcpu', because we generate
 * the value as a side effect here, and the LWP structure has no estcpu field.
 */
static void
fill_lwp_common(struct kinfo_lwp * l, int kslot, uint32_t * estcpu)
{
	struct proc *kp;
	struct timeval tv;
	clock_t uptime;
	uint32_t hz;

	kp = &proc_tab[kslot];

	uptime = getticks();
	hz = sys_hz();

	/*
	 * We use the process endpoint as the LWP ID.  Not only does this allow
	 * users to obtain process endpoints with "ps -s" (thus replacing the
	 * MINIX3 ps(1)'s "ps -E"), but if we ever do implement kernel threads,
	 * this is probably still going to be accurate.
	 */
	l->l_lid = kp->p_endpoint;

	/*
	 * The time during which the process has not been swapped in or out is
	 * not applicable for us, and thus, we set it to the time the process
	 * has been running (in seconds).  This value is relevant mostly for
	 * ps(1)'s CPU usage correction for processes that have just started.
	 */
	if (kslot >= NR_TASKS)
		l->l_swtime = uptime - mproc_tab[kslot - NR_TASKS].mp_started;
	else
		l->l_swtime = uptime;
	l->l_swtime /= hz;

	/*
	 * Sleep (dequeue) times are not maintained for kernel tasks, so
	 * pretend they are never asleep (which is pretty accurate).
	 */
	if (kslot < NR_TASKS)
		l->l_slptime = 0;
	else
		l->l_slptime = (uptime - kp->p_dequeued) / hz;

	l->l_priority = kp->p_priority;
	l->l_usrpri = kp->p_priority;
	l->l_cpuid = kp->p_cpu;
	ticks_to_timeval(&tv, kp->p_user_time + kp->p_sys_time);
	l->l_rtime_sec = tv.tv_sec;
	l->l_rtime_usec = tv.tv_usec;

	/*
	 * Obtain CPU usage percentages and estimates through library code
	 * shared between the kernel and this service; see its source for
	 * details.  We note that the produced estcpu value is rather different
	 * from the one produced by NetBSD, but this should not be a problem.
	 */
	l->l_pctcpu = cpuavg_getstats(&kp->p_cpuavg, &l->l_cpticks, estcpu,
	    uptime, hz);
}

/*
 * Fill a LWP structure for a kernel task.  Each kernel task has its own LWP,
 * and all of them have negative PIDs.
 */
static void
fill_lwp_kern(struct kinfo_lwp * l, int kslot)
{
	uint32_t estcpu;

	memset(l, 0, sizeof(*l));

	l->l_flag = L_INMEM | L_SINTR | L_SYSTEM;
	l->l_stat = LSSLEEP;
	l->l_pid = kslot - NR_TASKS;

	/*
	 * When showing LWP entries, ps(1) uses the process name rather than
	 * the LWP name.  All kernel tasks are therefore shown as "[kernel]"
	 * anyway.  We use the wmesg field to show the actual kernel task name.
	 */
	l->l_wchan = ((uint64_t)(l->l_pid) << 8) | 0x00;
	strlcpy(l->l_wmesg, proc_tab[kslot].p_name, sizeof(l->l_wmesg));
	strlcpy(l->l_name, "kernel", sizeof(l->l_name));

	fill_lwp_common(l, kslot, &estcpu);
}

/*
 * Fill a LWP structure for a user process.
 */
static void
fill_lwp_user(struct kinfo_lwp * l, int mslot)
{
	struct mproc *mp;
	uint32_t estcpu;

	memset(l, 0, sizeof(*l));

	mp = &mproc_tab[mslot];

	l->l_flag = L_INMEM;
	l->l_stat = get_lwp_stat(mslot, &l->l_wchan, l->l_wmesg,
	    sizeof(l->l_wmesg), &l->l_flag);
	l->l_pid = mp->mp_pid;
	strlcpy(l->l_name, mp->mp_name, sizeof(l->l_name));

	fill_lwp_common(l, NR_TASKS + mslot, &estcpu);
}

/*
 * Implementation of CTL_KERN KERN_LWP.
 */
ssize_t
mib_kern_lwp(struct mib_call * call, struct mib_node * node __unused,
	struct mib_oldp * oldp, struct mib_newp * newp __unused)
{
	struct kinfo_lwp lwp;
	struct mproc *mp;
	size_t copysz;
	ssize_t off;
	pid_t pid;
	int r, elsz, elmax, kslot, mslot, last_mslot;

	if (call->call_namelen != 3)
		return EINVAL;

	pid = (pid_t)call->call_name[0];
	elsz = call->call_name[1];
	elmax = call->call_name[2]; /* redundant with the given oldlen.. */

	if (pid < -1 || elsz <= 0 || elmax < 0)
		return EINVAL;

	if (!update_tables())
		return EINVAL;

	off = 0;
	copysz = MIN((size_t)elsz, sizeof(lwp));

	/*
	 * We model kernel tasks as LWP threads of the kernel (with PID 0).
	 * Modeling the kernel tasks as processes with negative PIDs, like
	 * ProcFS does, conflicts with the KERN_LWP API here: a PID of -1
	 * indicates that the caller wants a full listing of LWPs.
	 */
	if (pid <= 0) {
		for (kslot = 0; kslot < NR_TASKS; kslot++) {
			if (mib_inrange(oldp, off) && elmax > 0) {
				fill_lwp_kern(&lwp, kslot);
				if ((r = mib_copyout(oldp, off, &lwp,
				    copysz)) < 0)
					return r;
				elmax--;
			}
			off += elsz;
		}

		/* No need to add extra space here: NR_TASKS is static. */
		if (pid == 0)
			return off;
	}

	/*
	 * With PID 0 out of the way: the user requested the LWP for either a
	 * specific user process (pid > 0), or for all processes (pid < 0).
	 */
	if (pid > 0) {
		if ((mslot = get_mslot(pid)) == NO_SLOT ||
		    (mproc_tab[mslot].mp_flags & (TRACE_ZOMBIE | ZOMBIE)))
			return ESRCH;
		last_mslot = mslot;
	} else {
		mslot = 0;
		last_mslot = NR_PROCS - 1;
	}

	for (; mslot <= last_mslot; mslot++) {
		mp = &mproc_tab[mslot];

		if ((mp->mp_flags & (IN_USE | TRACE_ZOMBIE | ZOMBIE)) !=
		    IN_USE)
			continue;

		if (mib_inrange(oldp, off) && elmax > 0) {
			fill_lwp_user(&lwp, mslot);
			if ((r = mib_copyout(oldp, off, &lwp, copysz)) < 0)
				return r;
			elmax--;
		}
		off += elsz;
	}

	if (oldp == NULL && pid < 0)
		off += EXTRA_PROCS * elsz;

	return off;
}


/*
 * Fill the part of a process structure that is common between kernel tasks and
 * user processes.
 */
static void
fill_proc2_common(struct kinfo_proc2 * p, int kslot)
{
	struct vm_usage_info vui;
	struct timeval tv;
	struct proc *kp;
	struct kinfo_lwp l;

	kp = &proc_tab[kslot];

	/*
	 * Much of the information in the LWP structure also ends up in the
	 * process structure.  In order to avoid duplication of some important
	 * code, first generate LWP values and then copy it them into the
	 * process structure.
	 */
	memset(&l, 0, sizeof(l));
	fill_lwp_common(&l, kslot, &p->p_estcpu);

	/* Obtain memory usage information from VM.  Ignore failures. */
	memset(&vui, 0, sizeof(vui));
	(void)vm_info_usage(kp->p_endpoint, &vui);

	ticks_to_timeval(&tv, kp->p_user_time + kp->p_sys_time);
	p->p_rtime_sec = l.l_rtime_sec;
	p->p_rtime_usec = l.l_rtime_usec;
	p->p_cpticks = l.l_cpticks;
	p->p_pctcpu = l.l_pctcpu;
	p->p_swtime = l.l_swtime;
	p->p_slptime = l.l_slptime;
	p->p_uticks = kp->p_user_time;
	p->p_sticks = kp->p_sys_time;
	/* TODO: p->p_iticks */
	ticks_to_timeval(&tv, kp->p_user_time);
	p->p_uutime_sec = tv.tv_sec;
	p->p_uutime_usec = tv.tv_usec;
	ticks_to_timeval(&tv, kp->p_sys_time);
	p->p_ustime_sec = tv.tv_sec;
	p->p_ustime_usec = tv.tv_usec;

	p->p_priority = l.l_priority;
	p->p_usrpri = l.l_usrpri;

	p->p_vm_rssize = howmany(vui.vui_total, PAGE_SIZE);
	p->p_vm_vsize = howmany(vui.vui_virtual, PAGE_SIZE);
	p->p_vm_msize = howmany(vui.vui_mvirtual, PAGE_SIZE);

	p->p_uru_maxrss = vui.vui_maxrss;
	p->p_uru_minflt = vui.vui_minflt;
	p->p_uru_majflt = vui.vui_majflt;

	p->p_cpuid = l.l_cpuid;
}

/*
 * Fill a process structure for the kernel pseudo-process (with PID 0).
 */
static void
fill_proc2_kern(struct kinfo_proc2 * p)
{

	memset(p, 0, sizeof(*p));

	p->p_flag = L_INMEM | L_SYSTEM | L_SINTR;
	p->p_pid = 0;
	p->p_stat = LSSLEEP;
	p->p_nice = NZERO;

	/* Use the KERNEL task wchan, for consistency between ps and top. */
	p->p_wchan = ((uint64_t)KERNEL << 8) | 0x00;
	strlcpy(p->p_wmesg, "kernel", sizeof(p->p_wmesg));

	strlcpy(p->p_comm, "kernel", sizeof(p->p_comm));
	p->p_realflag = P_INMEM | P_SYSTEM | P_SINTR;
	p->p_realstat = SACTIVE;
	p->p_nlwps = NR_TASKS;

	/*
	 * By using the KERNEL slot here, the kernel process will get a proper
	 * CPU usage average.
	 */
	fill_proc2_common(p, KERNEL + NR_TASKS);
}

/*
 * Fill a process structure for a user process.
 */
static void
fill_proc2_user(struct kinfo_proc2 * p, int mslot)
{
	struct mproc *mp;
	struct fproc_light *fp;
	time_t boottime;
	dev_t tty;
	struct timeval tv;
	int i, r, kslot, zombie;

	memset(p, 0, sizeof(*p));

	if ((r = getuptime(NULL, NULL, &boottime)) != OK)
		panic("getuptime failed: %d", r);

	kslot = NR_TASKS + mslot;
	mp = &mproc_tab[mslot];
	fp = &fproc_tab[mslot];

	zombie = (mp->mp_flags & (TRACE_ZOMBIE | ZOMBIE));
	tty = (!zombie) ? fp->fpl_tty : NO_DEV;

	p->p_eflag = 0;
	if (tty != NO_DEV)
		p->p_eflag |= EPROC_CTTY;
	if (mp->mp_pid == mp->mp_procgrp) /* TODO: job control support */
		p->p_eflag |= EPROC_SLEADER;

	p->p_exitsig = SIGCHLD; /* TODO */

	p->p_flag = P_INMEM;
	if (mp->mp_flags & TAINTED)
		p->p_flag |= P_SUGID;
	if (mp->mp_tracer != NO_TRACER)
		p->p_flag |= P_TRACED;
	if (tty != NO_DEV)
		p->p_flag |= P_CONTROLT;
	p->p_pid = mp->mp_pid;
	if (mp->mp_parent >= 0 && mp->mp_parent < NR_PROCS)
		p->p_ppid = mproc_tab[mp->mp_parent].mp_pid;
	p->p_sid = mp->mp_procgrp; /* TODO: job control supported */
	p->p__pgid = mp->mp_procgrp;
	p->p_tpgid = (tty != NO_DEV) ? mp->mp_procgrp : 0;
	p->p_uid = mp->mp_effuid;
	p->p_ruid = mp->mp_realuid;
	p->p_gid = mp->mp_effgid;
	p->p_rgid = mp->mp_realgid;
	p->p_ngroups = MIN(mp->mp_ngroups, KI_NGROUPS);
	for (i = 0; i < p->p_ngroups; i++)
		p->p_groups[i] = mp->mp_sgroups[i];
	p->p_tdev = tty;
	memcpy(&p->p_siglist, &mp->mp_sigpending, sizeof(p->p_siglist));
	memcpy(&p->p_sigmask, &mp->mp_sigmask, sizeof(p->p_sigmask));
	memcpy(&p->p_sigcatch, &mp->mp_catch, sizeof(p->p_sigcatch));
	memcpy(&p->p_sigignore, &mp->mp_ignore, sizeof(p->p_sigignore));
	p->p_nice = mp->mp_nice + NZERO;
	strlcpy(p->p_comm, mp->mp_name, sizeof(p->p_comm));
	p->p_uvalid = 1;
	ticks_to_timeval(&tv, mp->mp_started);
	p->p_ustart_sec = boottime + tv.tv_sec;
	p->p_ustart_usec = tv.tv_usec;
	/* TODO: other rusage fields */
	ticks_to_timeval(&tv, mp->mp_child_utime + mp->mp_child_stime);
	p->p_uctime_sec = tv.tv_sec;
	p->p_uctime_usec = tv.tv_usec;
	p->p_realflag = p->p_flag;
	p->p_nlwps = (zombie) ? 0 : 1;
	p->p_svuid = mp->mp_svuid;
	p->p_svgid = mp->mp_svgid;

	p->p_stat = get_lwp_stat(mslot, &p->p_wchan, p->p_wmesg,
	    sizeof(p->p_wmesg), &p->p_flag);

	switch (p->p_stat) {
	case LSRUN:
		p->p_realstat = SACTIVE;
		p->p_nrlwps = 1;
		break;
	case LSSLEEP:
		p->p_realstat = SACTIVE;
		if (p->p_flag & L_SINTR)
			p->p_realflag |= P_SINTR;
		break;
	case LSSTOP:
		p->p_realstat = SSTOP;
		break;
	case LSZOMB:
		p->p_realstat = SZOMB;
		break;
	case LSDEAD:
		p->p_stat = LSZOMB; /* ps(1) STAT does not know LSDEAD */
		p->p_realstat = SDEAD;
		break;
	default:
		assert(0);
	}

	if (!zombie)
		fill_proc2_common(p, kslot);
}

/*
 * Implementation of CTL_KERN KERN_PROC2.
 */
ssize_t
mib_kern_proc2(struct mib_call * call, struct mib_node * node __unused,
	struct mib_oldp * oldp, struct mib_newp * newp __unused)
{
	struct kinfo_proc2 proc2;
	struct mproc *mp;
	size_t copysz;
	ssize_t off;
	dev_t tty;
	int r, req, arg, elsz, elmax, kmatch, zombie, mslot;

	if (call->call_namelen != 4)
		return EINVAL;

	req = call->call_name[0];
	arg = call->call_name[1];
	elsz = call->call_name[2];
	elmax = call->call_name[3]; /* redundant with the given oldlen.. */

	/*
	 * The kernel is special, in that it does not have a slot in the PM or
	 * VFS tables.  As such, it is dealt with separately.  While checking
	 * arguments, we might as well check whether the kernel is matched.
	 */
	switch (req) {
	case KERN_PROC_ALL:
		kmatch = TRUE;
		break;
	case KERN_PROC_PID:
	case KERN_PROC_SESSION:
	case KERN_PROC_PGRP:
	case KERN_PROC_UID:
	case KERN_PROC_RUID:
	case KERN_PROC_GID:
	case KERN_PROC_RGID:
		kmatch = (arg == 0);
		break;
	case KERN_PROC_TTY:
		kmatch = ((dev_t)arg == KERN_PROC_TTY_NODEV);
		break;
	default:
		return EINVAL;
	}

	if (elsz <= 0 || elmax < 0)
		return EINVAL;

	if (!update_tables())
		return EINVAL;

	off = 0;
	copysz = MIN((size_t)elsz, sizeof(proc2));

	if (kmatch) {
		if (mib_inrange(oldp, off) && elmax > 0) {
			fill_proc2_kern(&proc2);
			if ((r = mib_copyout(oldp, off, &proc2, copysz)) < 0)
				return r;
			elmax--;
		}
		off += elsz;
	}

	for (mslot = 0; mslot < NR_PROCS; mslot++) {
		mp = &mproc_tab[mslot];

		if (!(mp->mp_flags & IN_USE))
			continue;

		switch (req) {
		case KERN_PROC_PID:
			if ((pid_t)arg != mp->mp_pid)
				continue;
			break;
		case KERN_PROC_SESSION: /* TODO: job control support */
		case KERN_PROC_PGRP:
			if ((pid_t)arg != mp->mp_procgrp)
				continue;
			break;
		case KERN_PROC_TTY:
			if ((dev_t)arg == KERN_PROC_TTY_REVOKE)
				continue; /* TODO: revoke(2) support */
			/* Do not access the fproc_tab slot of zombies. */
			zombie = (mp->mp_flags & (TRACE_ZOMBIE | ZOMBIE));
			tty = (zombie) ? fproc_tab[mslot].fpl_tty : NO_DEV;
			if ((dev_t)arg == KERN_PROC_TTY_NODEV) {
				if (tty != NO_DEV)
					continue;
			} else if ((dev_t)arg == NO_DEV || (dev_t)arg != tty)
				continue;
			break;
		case KERN_PROC_UID:
			if ((uid_t)arg != mp->mp_effuid)
				continue;
			break;
		case KERN_PROC_RUID:
			if ((uid_t)arg != mp->mp_realuid)
				continue;
			break;
		case KERN_PROC_GID:
			if ((gid_t)arg != mp->mp_effgid)
				continue;
			break;
		case KERN_PROC_RGID:
			if ((gid_t)arg != mp->mp_realgid)
				continue;
			break;
		}

		if (mib_inrange(oldp, off) && elmax > 0) {
			fill_proc2_user(&proc2, mslot);
			if ((r = mib_copyout(oldp, off, &proc2, copysz)) < 0)
				return r;
			elmax--;
		}
		off += elsz;
	}

	if (oldp == NULL && req != KERN_PROC_PID)
		off += EXTRA_PROCS * elsz;

	return off;
}

/*
 * Implementation of CTL_KERN KERN_PROC_ARGS.
 */
ssize_t
mib_kern_proc_args(struct mib_call * call, struct mib_node * node __unused,
	struct mib_oldp * oldp, struct mib_newp * newp __unused)
{
	char vbuf[PAGE_SIZE], sbuf[PAGE_SIZE], obuf[PAGE_SIZE];
	struct ps_strings pss;
	struct mproc *mp;
	char *buf, *p, *q, *pptr;
	vir_bytes vaddr, vpage, spage, paddr, ppage;
	size_t max, off, olen, oleft, oldlen, bytes, pleft;
	unsigned int copybudget;
	pid_t pid;
	int req, mslot, count, aborted, ended;
	ssize_t r;

	if (call->call_namelen != 2)
		return EINVAL;

	pid = call->call_name[0];
	req = call->call_name[1];

	switch (req) {
	case KERN_PROC_ARGV:
	case KERN_PROC_ENV:
	case KERN_PROC_NARGV:
	case KERN_PROC_NENV:
		break;
	default:
		return EOPNOTSUPP;
	}

	if (!update_tables())
		return EINVAL;

	if ((mslot = get_mslot(pid)) == NO_SLOT)
		return ESRCH;
	mp = &mproc_tab[mslot];
	if (mp->mp_flags & (TRACE_ZOMBIE | ZOMBIE))
		return ESRCH;

	/* We can return the count field size without copying in any data. */
	if (oldp == NULL && (req == KERN_PROC_NARGV || req == KERN_PROC_NENV))
		return sizeof(count);

	if (sys_datacopy(mp->mp_endpoint,
	    mp->mp_frame_addr + mp->mp_frame_len - sizeof(pss),
	    SELF, (vir_bytes)&pss, sizeof(pss)) != OK)
		return EINVAL;

	/*
	 * Determine the upper size limit of the requested data.  Not only may
	 * the size never exceed ARG_MAX, it may also not exceed the frame
	 * length as given in its original exec call.  In fact, the frame
	 * length should be substantially larger: all strings for both the
	 * arguments and the environment are in there, along with other stuff,
	 * and there must be no overlap between strings.  It is possible that
	 * the application called setproctitle(3), in which case the ps_strings
	 * pointers refer to data outside the frame altogether.  However, this
	 * data should not exceed 2048 bytes, and we cover this by rounding up
	 * the frame length to a multiple of the page size.  Anyhow, NetBSD
	 * blindly returns ARG_MAX when asked for a size estimate, so with this
	 * maximum we are already quite a bit more accurate.
	 */
	max = roundup(MIN(mp->mp_frame_len, ARG_MAX), PAGE_SIZE);

	switch (req) {
	case KERN_PROC_NARGV:
		count = pss.ps_nargvstr;
		return mib_copyout(oldp, 0, &count, sizeof(count));
	case KERN_PROC_NENV:
		count = pss.ps_nenvstr;
		return mib_copyout(oldp, 0, &count, sizeof(count));
	case KERN_PROC_ARGV:
		if (oldp == NULL)
			return max;
		vaddr = (vir_bytes)pss.ps_argvstr;
		count = pss.ps_nargvstr;
		break;
	case KERN_PROC_ENV:
		if (oldp == NULL)
			return max;
		vaddr = (vir_bytes)pss.ps_envstr;
		count = pss.ps_nenvstr;
		break;
	}

	/*
	 * Go through the strings.  Copy in entire, machine-aligned pages at
	 * once, in the hope that all data is stored consecutively, which it
	 * should be: we expect that the vector is followed by the strings, and
	 * that the strings are stored in order of vector reference.  We keep
	 * up to two pages with copied-in data: one for the vector, and
	 * optionally one for string data.  In addition, we keep one page with
	 * data to be copied out, so that we do not cause a lot of copy
	 * overhead for short strings.
	 *
	 * We stop whenever any of the following conditions are met:
	 * - copying in data from the target process fails for any reason;
	 * - we have processed the last index ('count') into the vector;
	 * - the current vector element is a NULL pointer;
	 * - the requested number of output bytes ('oldlen') has been reached;
	 * - the maximum number of output bytes ('max') has been reached;
	 * - the number of page copy-ins exceeds an estimated threshold;
	 * - copying out data fails for any reason (we then return the error).
	 *
	 * We limit the number of page copy-ins because otherwise a rogue
	 * process could create an argument vector consisting of only two-byte
	 * strings that all span two pages, causing us to copy up to 1GB of
	 * data with the current ARG_MAX value of 256K.  No reasonable vector
	 * should cause more than (ARG_MAX / PAGE_SIZE) page copies for
	 * strings; we are nice enough to allow twice that.  Vector copies do
	 * not count, as they are linear anyway.
	 *
	 * Unlike every other sysctl(2) call, we are supposed to truncate the
	 * resulting size (the returned 'oldlen') to the requested size (the
	 * given 'oldlen') *and* return the resulting size, rather than ENOMEM
	 * and the real size.  Unfortunately, libkvm actually relies on this.
	 *
	 * Generally speaking, upon failure we just return a truncated result.
	 * In case of truncation, the data we copy out need not be null
	 * terminated.  It is up to userland to process the data correctly.
	 */
	if (trunc_page(vaddr) == 0 || vaddr % sizeof(char *) != 0)
		return 0;

	off = 0;
	olen = 0;
	aborted = FALSE;

	oldlen = mib_getoldlen(oldp);
	if (oldlen > max)
		oldlen = max;

	copybudget = (ARG_MAX / PAGE_SIZE) * 2;

	vpage = 0;
	spage = 0;

	while (count > 0 && off + olen < oldlen && !aborted) {
		/*
		 * Start by fetching the page containing the current vector
		 * element, if needed.  We could limit the fetch to the vector
		 * size, but our hope is that for the simple cases, the strings
		 * are on the remainder of the same page, so we save a copy
		 * call.  TODO: since the strings should follow the vector, we
		 * could start the copy at the base of the vector.
		 */
		if (trunc_page(vaddr) != vpage) {
			vpage = trunc_page(vaddr);
			if (sys_datacopy(mp->mp_endpoint, vpage, SELF,
			    (vir_bytes)vbuf, PAGE_SIZE) != OK)
				break;
		}

		/* Get the current vector element, pointing to a string. */
		memcpy(&pptr, &vbuf[vaddr - vpage], sizeof(pptr));
		paddr = (vir_bytes)pptr;
		ppage = trunc_page(paddr);
		if (ppage == 0)
			break;

		/* Fetch the string itself, one page at a time at most. */
		do {
			/*
			 * See if the string pointer falls inside either the
			 * vector page or the previously fetched string page
			 * (if any).  If not, fetch a string page.
			 */
			if (ppage == vpage) {
				buf = vbuf;
			} else if (ppage == spage) {
				buf = sbuf;
			} else {
				if (--copybudget == 0) {
					aborted = TRUE;
					break;
				}
				spage = ppage;
				if (sys_datacopy(mp->mp_endpoint, spage, SELF,
				    (vir_bytes)sbuf, PAGE_SIZE) != OK) {
					aborted = TRUE;
					break;
				}
				buf = sbuf;
			}

			/*
			 * We now have a string fragment in a buffer.  See if
			 * the string is null terminated.  If not, all the data
			 * up to the buffer end is part of the string, and the
			 * string continues on the next page.
			 */
			p = &buf[paddr - ppage];
			pleft = PAGE_SIZE - (paddr - ppage);
			assert(pleft > 0);

			if ((q = memchr(p, '\0', pleft)) != NULL) {
				bytes = (size_t)(q - p + 1);
				assert(bytes <= pleft);
				ended = TRUE;
			} else {
				bytes = pleft;
				ended = FALSE;
			}

			/* Limit the result to the requested length. */
			if (off + olen + bytes > oldlen)
				bytes = oldlen - off - olen;

			/*
			 * Add 'bytes' bytes from string pointer 'p' to the
			 * output buffer, copying out its contents to userland
			 * if it has filled up.
			 */
			if (olen + bytes > sizeof(obuf)) {
				oleft = sizeof(obuf) - olen;
				memcpy(&obuf[olen], p, oleft);

				if ((r = mib_copyout(oldp, off, obuf,
				    sizeof(obuf))) < 0)
					return r;
				off += sizeof(obuf);
				olen = 0;

				p += oleft;
				bytes -= oleft;
			}
			if (bytes > 0) {
				memcpy(&obuf[olen], p, bytes);
				olen += bytes;
			}

			/*
			 * Continue as long as we have not yet found the string
			 * end, and we have not yet filled the output buffer.
			 */
			paddr += pleft;
			assert(trunc_page(paddr) == paddr);
			ppage = paddr;
		} while (!ended && off + olen < oldlen);

		vaddr += sizeof(char *);
		count--;
	}

	/* Copy out any remainder of the output buffer. */
	if (olen > 0) {
		if ((r = mib_copyout(oldp, off, obuf, olen)) < 0)
			return r;
		off += olen;
	}

	assert(off <= oldlen);
	return off;
}

/*
 * Implementation of CTL_MINIX MINIX_PROC PROC_LIST.
 */
ssize_t
mib_minix_proc_list(struct mib_call * call __unused,
	struct mib_node * node __unused, struct mib_oldp * oldp,
	struct mib_newp * newp __unused)
{
	struct minix_proc_list mpl[NR_PROCS];
	struct minix_proc_list *mplp;
	struct mproc *mp;
	unsigned int mslot;

	if (oldp == NULL)
		return sizeof(mpl);

	if (!update_tables())
		return EINVAL;

	memset(&mpl, 0, sizeof(mpl));

	mplp = mpl;
	mp = mproc_tab;

	for (mslot = 0; mslot < NR_PROCS; mslot++, mplp++, mp++) {
		if (!(mp->mp_flags & IN_USE) || mp->mp_pid <= 0)
			continue;

		mplp->mpl_flags = MPLF_IN_USE;
		if (mp->mp_flags & (TRACE_ZOMBIE | ZOMBIE))
			mplp->mpl_flags |= MPLF_ZOMBIE;
		mplp->mpl_pid = mp->mp_pid;
		mplp->mpl_uid = mp->mp_effuid;
		mplp->mpl_gid = mp->mp_effgid;
	}

	return mib_copyout(oldp, 0, &mpl, sizeof(mpl));
}

/*
 * Implementation of CTL_MINIX MINIX_PROC PROC_DATA.
 */
ssize_t
mib_minix_proc_data(struct mib_call * call, struct mib_node * node __unused,
	struct mib_oldp * oldp, struct mib_newp * newp __unused)
{
	struct minix_proc_data mpd;
	struct proc *kp;
	int kslot, mslot = 0;
	unsigned int mflags;
	pid_t pid;

	/*
	 * It is currently only possible to retrieve the process data for a
	 * particular PID, which must be given as the last name component.
	 */
	if (call->call_namelen != 1)
		return EINVAL;

	pid = (pid_t)call->call_name[0];

	if (!update_tables())
		return EINVAL;

	/*
	 * Unlike the CTL_KERN nodes, we use the ProcFS semantics here: if the
	 * given PID is negative, it is a kernel task; otherwise, it identifies
	 * a user process.  A request for PID 0 will result in ESRCH.
	 */
	if (pid < 0) {
		if (pid < -NR_TASKS)
			return ESRCH;

		kslot = pid + NR_TASKS;
		assert(kslot < NR_TASKS);
	} else {
		if ((mslot = get_mslot(pid)) == NO_SLOT)
			return ESRCH;

		kslot = NR_TASKS + mslot;
	}

	if (oldp == NULL)
		return sizeof(mpd);

	kp = &proc_tab[kslot];

	mflags = (pid > 0) ? mproc_tab[mslot].mp_flags : 0;

	memset(&mpd, 0, sizeof(mpd));
	mpd.mpd_endpoint = kp->p_endpoint;
	if (mflags & PRIV_PROC)
		mpd.mpd_flags |= MPDF_SYSTEM;
	if (mflags & (TRACE_ZOMBIE | ZOMBIE))
		mpd.mpd_flags |= MPDF_ZOMBIE;
	else if ((mflags & TRACE_STOPPED) || RTS_ISSET(kp, RTS_P_STOP))
		mpd.mpd_flags |= MPDF_STOPPED;
	else if (proc_is_runnable(kp))
		mpd.mpd_flags |= MPDF_RUNNABLE;
	mpd.mpd_blocked_on = P_BLOCKEDON(kp);
	mpd.mpd_priority = kp->p_priority;
	mpd.mpd_user_time = kp->p_user_time;
	mpd.mpd_sys_time = kp->p_sys_time;
	mpd.mpd_cycles = kp->p_cycles;
	mpd.mpd_kipc_cycles = kp->p_kipc_cycles;
	mpd.mpd_kcall_cycles = kp->p_kcall_cycles;
	if (kslot >= NR_TASKS) {
		mpd.mpd_nice = mproc_tab[mslot].mp_nice;
		strlcpy(mpd.mpd_name, mproc_tab[mslot].mp_name,
		    sizeof(mpd.mpd_name));
	} else
		strlcpy(mpd.mpd_name, kp->p_name, sizeof(mpd.mpd_name));

	return mib_copyout(oldp, 0, &mpd, sizeof(mpd));
}
