/*	$NetBSD: m_netbsd.c,v 1.18 2013/10/20 03:02:27 christos Exp $	*/

/*
 * top - a top users display for Unix
 *
 * SYNOPSIS:  For a NetBSD-1.5 (or later) system
 *
 * DESCRIPTION:
 * Originally written for BSD4.4 system by Christos Zoulas.
 * Based on the FreeBSD 2.0 version by Steven Wallace and Wolfram Schneider.
 * NetBSD-1.0 port by Arne Helme. Process ordering by Luke Mewburn.
 * NetBSD-1.3 port by Luke Mewburn, based on code by Matthew Green.
 * NetBSD-1.4/UVM port by matthew green.
 * NetBSD-1.5 port by Simon Burge.
 * NetBSD-1.6/UBC port by Tomas Svensson.
 * -
 * This is the machine-dependent module for NetBSD-1.5 and later
 * works for:
 *	NetBSD-1.6ZC
 * and should work for:
 *	NetBSD-2.0	(when released)
 * -
 * top does not need to be installed setuid or setgid with this module.
 *
 * LIBS: -lkvm
 *
 * CFLAGS: -DHAVE_GETOPT -DORDER -DHAVE_STRERROR
 *
 * AUTHORS:	Christos Zoulas <christos@ee.cornell.edu>
 *		Steven Wallace <swallace@freebsd.org>
 *		Wolfram Schneider <wosch@cs.tu-berlin.de>
 *		Arne Helme <arne@acm.org>
 *		Luke Mewburn <lukem@NetBSD.org>
 *		matthew green <mrg@eterna.com.au>
 *		Simon Burge <simonb@NetBSD.org>
 *		Tomas Svensson <ts@unix1.net>
 *		Andrew Doran <ad@NetBSD.org>
 *
 *
 * $Id: m_netbsd.c,v 1.18 2013/10/20 03:02:27 christos Exp $
 */
#include <sys/cdefs.h>

#ifndef lint
__RCSID("$NetBSD: m_netbsd.c,v 1.18 2013/10/20 03:02:27 christos Exp $");
#endif

#include <sys/param.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/sched.h>
#include <sys/swap.h>

#include <uvm/uvm_extern.h>

#include <err.h>
#include <errno.h>
#include <kvm.h>
#include <math.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "os.h"
#include "top.h"
#include "machine.h"
#include "utils.h"
#include "display.h"
#include "loadavg.h"
#include "username.h"

static void percentages64(int, int *, u_int64_t *, u_int64_t *,
    u_int64_t *);

/* get_process_info passes back a handle.  This is what it looks like: */

struct handle {
	struct process_select *sel;
	struct kinfo_proc2 **next_proc;	/* points to next valid proc pointer */
	int remaining;		/* number of pointers remaining */
};

/* define what weighted CPU is. */
#define weighted_cpu(pfx, pct, pp) ((pp)->pfx ## swtime == 0 ? 0.0 : \
			 ((pct) / (1.0 - exp((pp)->pfx ## swtime * logcpu))))

/* what we consider to be process size: */
/* NetBSD introduced p_vm_msize with RLIMIT_AS */
#ifdef RLIMIT_AS
#define PROCSIZE(pp) \
    ((pp)->p_vm_msize)
#else
#define PROCSIZE(pp) \
    ((pp)->p_vm_tsize + (pp)->p_vm_dsize + (pp)->p_vm_ssize)
#endif


/*
 * These definitions control the format of the per-process area
 */

static char Proc_header[] =
  "  PID X        PRI NICE   SIZE   RES STATE      TIME   WCPU    CPU COMMAND";
/* 0123456   -- field to fill in starts at header+6 */
#define PROC_UNAME_START 6
#define Proc_format \
	"%5d %-8.8s %3d %4d%7s %5s %-8.8s%7s %5.*f%% %5.*f%% %s"

static char Thread_header[] =
  "  PID   LID X        PRI STATE      TIME   WCPU    CPU NAME      COMMAND";
/* 0123456   -- field to fill in starts at header+6 */
#define THREAD_UNAME_START 12
#define Thread_format \
        "%5d %5d %-8.8s %3d %-8.8s%7s %5.2f%% %5.2f%% %-9.9s %s"

/* 
 * Process state names for the "STATE" column of the display.
 */

const char *state_abbrev[] = {
	"", "IDLE", "RUN", "SLEEP", "STOP", "ZOMB", "DEAD", "CPU"
};

static kvm_t *kd;

static char *(*userprint)(int);

/* these are retrieved from the kernel in _init */

static double logcpu;
static int hz;
static int ccpu;

/* these are for calculating CPU state percentages */

static int ncpu = 0;
static u_int64_t *cp_time;
static u_int64_t *cp_old;
static u_int64_t *cp_diff;

/* these are for detailing the process states */

int process_states[8];
const char *procstatenames[] = {
	"", " idle, ", " runnable, ", " sleeping, ", " stopped, ",
	" zombie, ", " dead, ", " on CPU, ",
	NULL
};

/* these are for detailing the CPU states */

int *cpu_states;
const char *cpustatenames[] = {
#ifndef __minix
	"user", "nice", "system", "interrupt", "idle", NULL
#else /* __minix */
	"user", "nice", "system", "kernel", "idle", NULL
#endif /* __minix */
};

/* these are for detailing the memory statistics */

long memory_stats[7];
const char *memorynames[] = {
#ifndef __minix
	"K Act, ", "K Inact, ", "K Wired, ", "K Exec, ", "K File, ",
	"K Free, ",
#else /* __minix */
	"K Total, ", "K Free, ", "K Contig, ", "K Cached, ", "K ???, ",
	"K ???, ",
#endif /* __minix */
	NULL
};

long swap_stats[4];
const char *swapnames[] = {
#ifndef __minix
	"K Total, ", "K Used, ", "K Free, ",
#endif /* __minix */
	NULL
};


/* these are names given to allowed sorting orders -- first is default */
const char *ordernames[] = {
	"cpu",
	"pri",
	"res",
	"size",
	"state",
	"time",
	"pid",
	"command",
	"username",
	NULL
};

/* forward definitions for comparison functions */
static int compare_cpu(struct proc **, struct proc **);
static int compare_prio(struct proc **, struct proc **);
static int compare_res(struct proc **, struct proc **);
static int compare_size(struct proc **, struct proc **);
static int compare_state(struct proc **, struct proc **);
static int compare_time(struct proc **, struct proc **);
static int compare_pid(struct proc **, struct proc **);
static int compare_command(struct proc **, struct proc **);
static int compare_username(struct proc **, struct proc **);

int (*proc_compares[])(struct proc **, struct proc **) = {
	compare_cpu,
	compare_prio,
	compare_res,
	compare_size,
	compare_state,
	compare_time,
	compare_pid,
	compare_command,
	compare_username,
	NULL
};

static char *format_next_lwp(caddr_t, char *(*)(int));
static char *format_next_proc(caddr_t, char *(*)(int));

static caddr_t get_proc_info(struct system_info *, struct process_select *,
			     int (*)(struct proc **, struct proc **));
static caddr_t get_lwp_info(struct system_info *, struct process_select *,
			    int (*)(struct proc **, struct proc **));

/* these are for keeping track of the proc array */

static int nproc;
static int onproc = -1;
static int nlwp;
static int onlwp = -1;
static int pref_len;
static int lref_len;
static struct kinfo_proc2 *pbase;
static struct kinfo_lwp *lbase;
static struct kinfo_proc2 **pref;
static struct kinfo_lwp **lref;
static int maxswap;
static void *swapp;
static int procgen;
static int thread_nproc;
static int thread_onproc = -1;
static struct kinfo_proc2 *thread_pbase;

/* these are for getting the memory statistics */

static int pageshift;		/* log base 2 of the pagesize */

int threadmode;

/* define pagetok in terms of pageshift */

#define pagetok(size) ((size) << pageshift)

/*
 * Print swapped processes as <pname> and
 * system processes as [pname]
 */
static const char *
get_pretty(const struct kinfo_proc2 *pp)
{
	if ((pp->p_flag & P_SYSTEM) != 0)
		return "[]";
	if ((pp->p_flag & P_INMEM) == 0)
		return "<>";
	return "";
}

static const char *
get_command(const struct process_select *sel, struct kinfo_proc2 *pp)
{
	static char cmdbuf[128];
	const char *pretty;
	char **argv;
	if (pp == NULL)
		return "<gone>";
	pretty = get_pretty(pp);

	if (sel->fullcmd == 0 || kd == NULL || (argv = kvm_getargv2(kd, pp,
	    sizeof(cmdbuf))) == NULL) {
		if (pretty[0] != '\0' && pp->p_comm[0] != pretty[0])
			snprintf(cmdbuf, sizeof(cmdbuf), "%c%s%c", pretty[0],
			    printable(pp->p_comm), pretty[1]);
		else
			strlcpy(cmdbuf, printable(pp->p_comm), sizeof(cmdbuf));
	} else {
		char *d = cmdbuf;
		if (pretty[0] != '\0' && argv[0][0] != pretty[0]) 
			*d++ = pretty[0];
		while (*argv) {
			const char *s = printable(*argv++);
			while (d < cmdbuf + sizeof(cmdbuf) - 2 &&
			    (*d++ = *s++) != '\0')
				continue;
			if (d > cmdbuf && d < cmdbuf + sizeof(cmdbuf) - 2 &&
			    d[-1] == '\0')
				d[-1] = ' ';
		}
		if (pretty[0] != '\0' && pretty[0] == cmdbuf[0])
			*d++ = pretty[1];
		*d++ = '\0';
	}
	return cmdbuf;
}

int
machine_init(statics)
	struct statics *statics;
{
	int pagesize;
	int mib[2];
	size_t size;
	struct clockinfo clockinfo;
	struct timeval boottime;

	if ((kd = kvm_open(NULL, NULL, NULL, KVM_NO_FILES, "kvm_open")) == NULL)
		return -1;

	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	size = sizeof(ncpu);
	if (sysctl(mib, 2, &ncpu, &size, NULL, 0) == -1) {
		fprintf(stderr, "top: sysctl hw.ncpu failed: %s\n",
		    strerror(errno));
		return(-1);
	}
	statics->ncpu = ncpu;
	cp_time = malloc(sizeof(cp_time[0]) * CPUSTATES * ncpu);
	mib[0] = CTL_KERN;
	mib[1] = KERN_CP_TIME;
	size = sizeof(cp_time[0]) * CPUSTATES * ncpu;
	if (sysctl(mib, 2, cp_time, &size, NULL, 0) < 0) {
		fprintf(stderr, "top: sysctl kern.cp_time failed: %s\n",
		    strerror(errno));
		return(-1);
	}

	/* Handle old call that returned only aggregate */
	if (size == sizeof(cp_time[0]) * CPUSTATES)
		ncpu = 1;

	cpu_states = malloc(sizeof(cpu_states[0]) * CPUSTATES * ncpu);
	cp_old = malloc(sizeof(cp_old[0]) * CPUSTATES * ncpu);
	cp_diff = malloc(sizeof(cp_diff[0]) * CPUSTATES * ncpu);
	if (cpu_states == NULL || cp_time == NULL || cp_old == NULL ||
	    cp_diff == NULL) {
		fprintf(stderr, "top: machine_init: %s\n",
		    strerror(errno));
		return(-1);
	}

	mib[0] = CTL_KERN;
	mib[1] = KERN_CCPU;
	size = sizeof(ccpu);
	if (sysctl(mib, 2, &ccpu, &size, NULL, 0) == -1) {
		fprintf(stderr, "top: sysctl kern.ccpu failed: %s\n",
		    strerror(errno));
		return(-1);
	}

	mib[0] = CTL_KERN;
	mib[1] = KERN_CLOCKRATE;
	size = sizeof(clockinfo);
	if (sysctl(mib, 2, &clockinfo, &size, NULL, 0) == -1) {
		fprintf(stderr, "top: sysctl kern.clockrate failed: %s\n",
		    strerror(errno));
		return(-1);
	}
	hz = clockinfo.stathz;

	/* this is used in calculating WCPU -- calculate it ahead of time */
	logcpu = log(loaddouble(ccpu));

	pbase = NULL;
	lbase = NULL;
	pref = NULL;
	nproc = 0;
	onproc = -1;
	nlwp = 0;
	onlwp = -1;
	/* get the page size with "getpagesize" and calculate pageshift from it */
	pagesize = getpagesize();
	pageshift = 0;
	while (pagesize > 1) {
		pageshift++;
		pagesize >>= 1;
	}

	/* we only need the amount of log(2)1024 for our conversion */
	pageshift -= LOG1024;

	/* fill in the statics information */
#ifdef notyet
	statics->ncpu = ncpu;
#endif
	statics->procstate_names = procstatenames;
	statics->cpustate_names = cpustatenames;
	statics->memory_names = memorynames;
	statics->swap_names = swapnames;
	statics->order_names = ordernames;
	statics->flags.threads = 1;
	statics->flags.fullcmds = 1;

	mib[0] = CTL_KERN;
	mib[1] = KERN_BOOTTIME;
	size = sizeof(boottime);
	if (sysctl(mib, 2, &boottime, &size, NULL, 0) != -1 &&
    	    boottime.tv_sec != 0)
		statics->boottime = boottime.tv_sec;
	else
		statics->boottime = 0;
	/* all done! */
	return(0);
}

char *
format_process_header(struct process_select *sel, caddr_t handle, int count)

{
	char *header;
	char *ptr;
	const char *uname_field = sel->usernames ? "USERNAME" : "    UID ";

	if (sel->threads) {
		header = Thread_header;
		ptr = header + THREAD_UNAME_START;
	} else {
		header = Proc_header;
		ptr = header + PROC_UNAME_START;
	}

	while (*uname_field != '\0') {
		*ptr++ = *uname_field++;
	}

	return(header);
}

char *
format_header(char *uname_field)
{
	char *header = Proc_header;
	char *ptr = header + PROC_UNAME_START;

	while (*uname_field != '\0') {
		*ptr++ = *uname_field++;
	}

	return(header);
}

void
get_system_info(struct system_info *si)
{
	size_t ssize;
	int mib[2];
	struct uvmexp_sysctl uvmexp;
	struct swapent *sep;
	u_int64_t totalsize, totalinuse;
	int size, inuse, ncounted, i;
	int rnswap, nswap;

	mib[0] = CTL_KERN;
	mib[1] = KERN_CP_TIME;
	ssize = sizeof(cp_time[0]) * CPUSTATES * ncpu;
	if (sysctl(mib, 2, cp_time, &ssize, NULL, 0) < 0) {
		fprintf(stderr, "top: sysctl kern.cp_time failed: %s\n",
		    strerror(errno));
		quit(23);
	}

	if (getloadavg(si->load_avg, NUM_AVERAGES) < 0) {
		int j;

		warn("can't getloadavg");
		for (j = 0; j < NUM_AVERAGES; j++)
			si->load_avg[j] = 0.0;
	}

	/* convert cp_time counts to percentages */
	for (i = 0; i < ncpu; i++) {
	    int j = i * CPUSTATES;
	    percentages64(CPUSTATES, cpu_states + j, cp_time + j, cp_old + j,
		cp_diff + j);
	}

	mib[0] = CTL_VM;
	mib[1] = VM_UVMEXP2;
	ssize = sizeof(uvmexp);
	if (sysctl(mib, 2, &uvmexp, &ssize, NULL, 0) < 0) {
		fprintf(stderr, "top: sysctl vm.uvmexp2 failed: %s\n",
		    strerror(errno));
		quit(23);
	}

	/* convert memory stats to Kbytes */
#ifndef __minix
	memory_stats[0] = pagetok(uvmexp.active);
	memory_stats[1] = pagetok(uvmexp.inactive);
	memory_stats[2] = pagetok(uvmexp.wired);
	memory_stats[3] = pagetok(uvmexp.execpages);
	memory_stats[4] = pagetok(uvmexp.filepages);
	memory_stats[5] = pagetok(uvmexp.free);
#else /* __minix */
	memory_stats[0] = pagetok(uvmexp.npages);
	memory_stats[1] = pagetok(uvmexp.free);
	memory_stats[2] = pagetok(uvmexp.unused1); /* largest phys contig */
	memory_stats[3] = pagetok(uvmexp.filepages);
	memory_stats[4] = 0;
	memory_stats[5] = 0;
#endif /* __minix */

	swap_stats[0] = swap_stats[1] = swap_stats[2] = 0;

	do {
#ifndef __minix
		nswap = swapctl(SWAP_NSWAP, 0, 0);
#else /* __minix */
		nswap = 0;
#endif /* __minix */
		if (nswap < 1)
			break;
		if (nswap > maxswap) {
			if (swapp)
				free(swapp);
			swapp = sep = malloc(nswap * sizeof(*sep));
			if (sep == NULL)
				break;
			maxswap = nswap;
		} else
			sep = swapp;
#ifndef __minix
		rnswap = swapctl(SWAP_STATS, (void *)sep, nswap);
#else /* __minix */
		rnswap = 0;
#endif /* __minix */
		if (nswap != rnswap)
			break;

		totalsize = totalinuse = ncounted = 0;
		for (; rnswap-- > 0; sep++) {
			ncounted++;
			size = sep->se_nblks;
			inuse = sep->se_inuse;
			totalsize += size;
			totalinuse += inuse;
		}
		swap_stats[0] = dbtob(totalsize) / 1024;
		swap_stats[1] = dbtob(totalinuse) / 1024;
		swap_stats[2] = dbtob(totalsize) / 1024 - swap_stats[1];
	} while (0);

	memory_stats[6] = -1;
	swap_stats[3] = -1;

	/* set arrays and strings */
	si->cpustates = cpu_states;
	si->memory = memory_stats;
	si->swap = swap_stats;
	si->last_pid = -1;

}

static struct kinfo_proc2 *
proc_from_thread(struct kinfo_lwp *pl)
{
	struct kinfo_proc2 *pp = thread_pbase;
	int i;

	for (i = 0; i < thread_nproc; i++, pp++)
		if ((pid_t)pp->p_pid == (pid_t)pl->l_pid)
			return pp;
	return NULL;
}

static int
uid_from_thread(struct kinfo_lwp *pl)
{
	struct kinfo_proc2 *pp;

	if ((pp = proc_from_thread(pl)) == NULL)
		return -1;
	return pp->p_ruid;
}

caddr_t
get_process_info(struct system_info *si, struct process_select *sel, int c)
{
	userprint = sel->usernames ? username : itoa7;

	if ((threadmode = sel->threads) != 0)
		return get_lwp_info(si, sel, proc_compares[c]);
	else
		return get_proc_info(si, sel, proc_compares[c]);
}

static caddr_t
get_proc_info(struct system_info *si, struct process_select *sel,
	      int (*compare)(struct proc **, struct proc **))
{
	int i;
	int total_procs;
	int active_procs;
	struct kinfo_proc2 **prefp, **n;
	struct kinfo_proc2 *pp;
	int op, arg;

	/* these are copied out of sel for speed */
	int show_idle;
	int show_system;
	int show_uid;

	static struct handle handle;

	procgen++;

	if (sel->pid == (pid_t)-1) {
		op = KERN_PROC_ALL;
		arg = 0;
	} else {
		op = KERN_PROC_PID;
		arg = sel->pid;
	}

	pbase = kvm_getproc2(kd, op, arg, sizeof(struct kinfo_proc2), &nproc);
	if (pbase == NULL) {
		if (sel->pid != (pid_t)-1) {
			nproc = 0;
		} else {
			(void) fprintf(stderr, "top: Out of memory.\n");
			quit(23);
		}
	}
	if (nproc > onproc) {
		n = (struct kinfo_proc2 **) realloc(pref,
		    sizeof(struct kinfo_proc2 *) * nproc);
		if (n == NULL) {
			(void) fprintf(stderr, "top: Out of memory.\n");
			quit(23);
		}
		pref = n;
		onproc = nproc;
	}
	/* get a pointer to the states summary array */
	si->procstates = process_states;

	/* set up flags which define what we are going to select */
	show_idle = sel->idle;
	show_system = sel->system;
	show_uid = sel->uid != -1;

	/* count up process states and get pointers to interesting procs */
	total_procs = 0;
	active_procs = 0;
	memset((char *)process_states, 0, sizeof(process_states));
	prefp = pref;
	for (pp = pbase, i = 0; i < nproc; pp++, i++) {

		/*
		 * Place pointers to each valid proc structure in pref[].
		 * Process slots that are actually in use have a non-zero
		 * status field.  Processes with P_SYSTEM set are system
		 * processes---these get ignored unless show_sysprocs is set.
		 */
		if (pp->p_stat != 0 && (show_system || ((pp->p_flag & P_SYSTEM) == 0))) {
			total_procs++;
			process_states[(unsigned char) pp->p_stat]++;
			if (pp->p_stat != LSZOMB &&
			    (show_idle || (pp->p_pctcpu != 0) || 
			    (pp->p_stat == LSRUN || pp->p_stat == LSONPROC)) &&
			    (!show_uid || pp->p_ruid == (uid_t)sel->uid)) {
				*prefp++ = pp;
				active_procs++;
			}
		}
	}

	/* if requested, sort the "interesting" processes */
	if (compare != NULL) {
		qsort((char *)pref, active_procs, sizeof(struct kinfo_proc2 *), 
		    (int (*)(const void *, const void *))compare);
	}

	/* remember active and total counts */
	si->p_total = total_procs;
	si->p_active = pref_len = active_procs;

	/* pass back a handle */
	handle.next_proc = pref;
	handle.remaining = active_procs;
	handle.sel = sel;
	return((caddr_t)&handle);
}

static caddr_t
get_lwp_info(struct system_info *si, struct process_select *sel,
	     int (*compare)(struct proc **, struct proc **))
{
	int i;
	int total_lwps;
	int active_lwps;
	struct kinfo_lwp **lrefp, **n;
	struct kinfo_lwp *lp;
	struct kinfo_proc2 *pp;

	/* these are copied out of sel for speed */
	int show_idle;
	int show_system;
	int show_uid;

	static struct handle handle;

	pp = kvm_getproc2(kd, KERN_PROC_ALL, 0, sizeof(struct kinfo_proc2),
	    &thread_nproc);
	if (pp == NULL) {
		(void) fprintf(stderr, "top: Out of memory.\n");
		quit(23);
	}
	if (thread_pbase == NULL || thread_nproc != thread_onproc) {
		free(thread_pbase);
		thread_onproc = thread_nproc;
		thread_pbase = calloc(sizeof(struct kinfo_proc2), thread_nproc);
		if (thread_pbase == NULL) {
			(void) fprintf(stderr, "top: Out of memory.\n");
			quit(23);
		}
	}
	memcpy(thread_pbase, pp, sizeof(struct kinfo_proc2) * thread_nproc);

	lbase = kvm_getlwps(kd, -1, 0, sizeof(struct kinfo_lwp), &nlwp);
	if (lbase == NULL) {
#ifdef notyet
		if (sel->pid != (pid_t)-1) {
			nproc = 0;
			nlwp = 0;
		}
		else
#endif
		{
			(void) fprintf(stderr, "top: Out of memory.\n");
			quit(23);
		}
	}
	if (nlwp > onlwp) {
		n = (struct kinfo_lwp **) realloc(lref,
		    sizeof(struct kinfo_lwp *) * nlwp);
		if (n == NULL) {
			(void) fprintf(stderr, "top: Out of memory.\n");
			quit(23);
		}
		lref = n;
		onlwp = nlwp;
	}
	/* get a pointer to the states summary array */
	si->procstates = process_states;

	/* set up flags which define what we are going to select */
	show_idle = sel->idle;
	show_system = sel->system;
	show_uid = sel->uid != -1;

	/* count up thread states and get pointers to interesting threads */
	total_lwps = 0;
	active_lwps = 0;
	memset((char *)process_states, 0, sizeof(process_states));
	lrefp = lref;
	for (lp = lbase, i = 0; i < nlwp; lp++, i++) {
		if (sel->pid != (pid_t)-1 && sel->pid != (pid_t)lp->l_pid)
			continue;

		/*
		 * Place pointers to each valid lwp structure in lref[].
		 * thread slots that are actually in use have a non-zero
		 * status field.  threads with L_SYSTEM set are system
		 * threads---these get ignored unless show_sysprocs is set.
		 */
		if (lp->l_stat != 0 && (show_system || ((lp->l_flag & LW_SYSTEM) == 0))) {
			total_lwps++;
			process_states[(unsigned char) lp->l_stat]++;
			if (lp->l_stat != LSZOMB &&
			    (show_idle || (lp->l_pctcpu != 0) || 
			    (lp->l_stat == LSRUN || lp->l_stat == LSONPROC)) &&
			    (!show_uid || uid_from_thread(lp) == sel->uid)) {
				*lrefp++ = lp;
				active_lwps++;
			}
		}
	}

	/* if requested, sort the "interesting" threads */
	if (compare != NULL) {
		qsort((char *)lref, active_lwps, sizeof(struct kinfo_lwp *),
		    (int (*)(const void *, const void *))compare);
	}

	/* remember active and total counts */
	si->p_total = total_lwps;
	si->p_active = lref_len = active_lwps;

	/* pass back a handle */
	handle.next_proc = (struct kinfo_proc2 **)lref;
	handle.remaining = active_lwps;
	handle.sel = sel;

	return((caddr_t)&handle);
}

char *
format_next_process(caddr_t handle, char *(*get_userid)(int))
{

	if (threadmode)
		return format_next_lwp(handle, get_userid);
	else
		return format_next_proc(handle, get_userid);
}


char *
format_next_proc(caddr_t handle, char *(*get_userid)(int))
{
	struct kinfo_proc2 *pp;
	long cputime;
	double pct, wcpu, cpu;
	struct handle *hp;
	const char *statep;
#ifdef KI_NOCPU
	char state[10];
#endif
	char wmesg[KI_WMESGLEN + 1];
	static char fmt[MAX_COLS];		/* static area where result is built */

	/* find and remember the next proc structure */
	hp = (struct handle *)handle;
	pp = *(hp->next_proc++);
	hp->remaining--;

	/* get the process's user struct and set cputime */

#if 0
	/* This does not produce the correct results */
	cputime = pp->p_uticks + pp->p_sticks + pp->p_iticks;
#else
	cputime = pp->p_rtime_sec;	/* This does not count interrupts */
#endif

	/* calculate the base for CPU percentages */
	pct = pctdouble(pp->p_pctcpu);

	if (pp->p_stat == LSSLEEP) {
		strlcpy(wmesg, pp->p_wmesg, sizeof(wmesg));
		statep = wmesg;
	} else
		statep = state_abbrev[(unsigned)pp->p_stat];

#ifdef KI_NOCPU
	/* Post-1.5 change: add CPU number if appropriate */
	if (pp->p_cpuid != KI_NOCPU && ncpu > 1) {
		switch (pp->p_stat) {
		case LSONPROC:
		case LSRUN:
		case LSSLEEP:
		case LSIDL:
			(void)snprintf(state, sizeof(state), "%.6s/%u", 
			     statep, (unsigned int)pp->p_cpuid);
			statep = state;
			break;
		}
	}
#endif
	wcpu = 100.0 * weighted_cpu(p_, pct, pp);
	cpu = 100.0 * pct;

	/* format this entry */
	sprintf(fmt,
	    Proc_format,
	    pp->p_pid,
	    (*userprint)(pp->p_ruid),
	    pp->p_priority,
	    pp->p_nice - NZERO,
	    format_k(pagetok(PROCSIZE(pp))),
	    format_k(pagetok(pp->p_vm_rssize)),
	    statep,
	    format_time(cputime),
	    (wcpu >= 100.0) ? 0 : 2, wcpu,
	    (cpu >= 100.0) ? 0 : 2, cpu,
	    get_command(hp->sel, pp));

	/* return the result */
	return(fmt);
}

static char *
format_next_lwp(caddr_t handle, char *(*get_userid)(int))
{
	struct kinfo_proc2 *pp;
	struct kinfo_lwp *pl;
	long cputime;
	double pct;
	struct handle *hp;
	const char *statep;
#ifdef KI_NOCPU
	char state[10];
#endif
	char wmesg[KI_WMESGLEN + 1];
	static char fmt[MAX_COLS];		/* static area where result is built */
	int uid;

	/* find and remember the next proc structure */
	hp = (struct handle *)handle;
	pl = (struct kinfo_lwp *)*(hp->next_proc++);
	hp->remaining--;
	pp = proc_from_thread(pl);

	/* get the process's user struct and set cputime */
	uid = pp ? pp->p_ruid : 0;

	cputime = pl->l_rtime_sec;

	/* calculate the base for CPU percentages */
	pct = pctdouble(pl->l_pctcpu);

	if (pl->l_stat == LSSLEEP) {
		strlcpy(wmesg, pl->l_wmesg, sizeof(wmesg));
		statep = wmesg;
	} else
		statep = state_abbrev[(unsigned)pl->l_stat];

#ifdef KI_NOCPU
	/* Post-1.5 change: add CPU number if appropriate */
	if (pl->l_cpuid != KI_NOCPU && ncpu > 1) {
		switch (pl->l_stat) {
		case LSONPROC:
		case LSRUN:
		case LSSLEEP:			
		case LSIDL:
			(void)snprintf(state, sizeof(state), "%.6s/%u", 
			     statep, (unsigned int)pl->l_cpuid);
			statep = state;
			break;
		}
	}
#endif

	if (pl->l_name[0] == '\0') {
		pl->l_name[0] = '-';
		pl->l_name[1] = '\0';
	}

	/* format this entry */
	sprintf(fmt,
	    Thread_format,
	    pl->l_pid,
	    pl->l_lid,
	    (*userprint)(uid),
	    pl->l_priority,
	    statep,
	    format_time(cputime),
	    100.0 * weighted_cpu(l_, pct, pl),
	    100.0 * pct,
	    printable(pl->l_name),
	    get_command(hp->sel, pp));

	/* return the result */
	return(fmt);
}

/* comparison routines for qsort */

/*
 * There are currently four possible comparison routines.  main selects
 * one of these by indexing in to the array proc_compares.
 *
 * Possible keys are defined as macros below.  Currently these keys are
 * defined:  percent CPU, CPU ticks, process state, resident set size,
 * total virtual memory usage.  The process states are ordered as follows
 * (from least to most important):  WAIT, zombie, sleep, stop, start, run.
 * The array declaration below maps a process state index into a number
 * that reflects this ordering.
 */

/*
 * First, the possible comparison keys.  These are defined in such a way
 * that they can be merely listed in the source code to define the actual
 * desired ordering.
 */

#define ORDERKEY_PCTCPU(pfx) \
	if (lresult = (pctcpu)(p2)->pfx ## pctcpu - (pctcpu)(p1)->pfx ## pctcpu,\
	    (result = lresult > 0 ? 1 : lresult < 0 ? -1 : 0) == 0)

#define ORDERKEY_CPTICKS(pfx) \
	if (lresult = (pctcpu)(p2)->pfx ## rtime_sec \
		    - (pctcpu)(p1)->pfx ## rtime_sec,\
	    (result = lresult > 0 ? 1 : lresult < 0 ? -1 : 0) == 0)

#define ORDERKEY_STATE(pfx) \
	if ((result = sorted_state[(int)(p2)->pfx ## stat] - \
		      sorted_state[(int)(p1)->pfx ## stat] ) == 0)

#define ORDERKEY_PRIO(pfx) \
	if ((result = (p2)->pfx ## priority - (p1)->pfx ## priority) == 0)

#define ORDERKEY_RSSIZE \
	if ((result = p2->p_vm_rssize - p1->p_vm_rssize) == 0)

#define ORDERKEY_MEM	\
	if ((result = (PROCSIZE(p2) - PROCSIZE(p1))) == 0)
#define ORDERKEY_SIZE(v1, v2)	\
	if ((result = (v2 - v1)) == 0)

/*
 * Now the array that maps process state to a weight.
 * The order of the elements should match those in state_abbrev[]
 */

static int sorted_state[] = {
	0,	/*  (not used)	  ?	*/
	1,	/* "start"	SIDL	*/
	4,	/* "run"	SRUN	*/
	3,	/* "sleep"	SSLEEP	*/
	3,	/* "stop"	SSTOP	*/
	2,	/* "dead"	SDEAD	*/
	1,	/* "zomb"	SZOMB	*/
	5,	/* "onproc"	SONPROC	*/
};

/* compare_cpu - the comparison function for sorting by CPU percentage */

static int
compare_cpu(pp1, pp2)
	struct proc **pp1, **pp2;
{
	int result;
	pctcpu lresult;

	if (threadmode) {
		struct kinfo_lwp *p1 = *(struct kinfo_lwp **) pp1;
		struct kinfo_lwp *p2 = *(struct kinfo_lwp **) pp2;

		ORDERKEY_PCTCPU(l_)
		ORDERKEY_CPTICKS(l_)
		ORDERKEY_STATE(l_)
		ORDERKEY_PRIO(l_)
		return result;
	} else {
		struct kinfo_proc2 *p1 = *(struct kinfo_proc2 **) pp1;
		struct kinfo_proc2 *p2 = *(struct kinfo_proc2 **) pp2;

		ORDERKEY_PCTCPU(p_)
		ORDERKEY_CPTICKS(p_)
		ORDERKEY_STATE(p_)
		ORDERKEY_PRIO(p_)
		ORDERKEY_RSSIZE
		ORDERKEY_MEM
		return result;
	}

	return (result);
}

/* compare_prio - the comparison function for sorting by process priority */

static int
compare_prio(pp1, pp2)
	struct proc **pp1, **pp2;
{
	int result;
	pctcpu lresult;

	if (threadmode) {
		struct kinfo_lwp *p1 = *(struct kinfo_lwp **) pp1;
		struct kinfo_lwp *p2 = *(struct kinfo_lwp **) pp2;

		ORDERKEY_PRIO(l_)
		ORDERKEY_PCTCPU(l_)
		ORDERKEY_CPTICKS(l_)
		ORDERKEY_STATE(l_)
		return result;
	} else {
		struct kinfo_proc2 *p1 = *(struct kinfo_proc2 **) pp1;
		struct kinfo_proc2 *p2 = *(struct kinfo_proc2 **) pp2;

		ORDERKEY_PRIO(p_)
		ORDERKEY_PCTCPU(p_)
		ORDERKEY_CPTICKS(p_)
		ORDERKEY_STATE(p_)
		ORDERKEY_RSSIZE
		ORDERKEY_MEM
		return result;
	}

	return (result);
}

/* compare_res - the comparison function for sorting by resident set size */

static int
compare_res(pp1, pp2)
	struct proc **pp1, **pp2;
{
	int result;
	pctcpu lresult;

	if (threadmode) {
		struct kinfo_lwp *p1 = *(struct kinfo_lwp **) pp1;
		struct kinfo_lwp *p2 = *(struct kinfo_lwp **) pp2;

		ORDERKEY_PCTCPU(l_)
		ORDERKEY_CPTICKS(l_)
		ORDERKEY_STATE(l_)
		ORDERKEY_PRIO(l_)
		return result;
	} else {
		struct kinfo_proc2 *p1 = *(struct kinfo_proc2 **) pp1;
		struct kinfo_proc2 *p2 = *(struct kinfo_proc2 **) pp2;

		ORDERKEY_RSSIZE
		ORDERKEY_MEM
		ORDERKEY_PCTCPU(p_)
		ORDERKEY_CPTICKS(p_)
		ORDERKEY_STATE(p_)
		ORDERKEY_PRIO(p_)
		return result;
	}

	return (result);
}

static int
compare_pid(pp1, pp2)
	struct proc **pp1, **pp2;
{
	if (threadmode) {
		struct kinfo_lwp *l1 = *(struct kinfo_lwp **) pp1;
		struct kinfo_lwp *l2 = *(struct kinfo_lwp **) pp2;
		struct kinfo_proc2 *p1 = proc_from_thread(l1);
		struct kinfo_proc2 *p2 = proc_from_thread(l2);
		return p2->p_pid - p1->p_pid;
	} else {
		struct kinfo_proc2 *p1 = *(struct kinfo_proc2 **) pp1;
		struct kinfo_proc2 *p2 = *(struct kinfo_proc2 **) pp2;
		return p2->p_pid - p1->p_pid;
	}
}

static int
compare_command(pp1, pp2)
	struct proc **pp1, **pp2;
{
	if (threadmode) {
		struct kinfo_lwp *l1 = *(struct kinfo_lwp **) pp1;
		struct kinfo_lwp *l2 = *(struct kinfo_lwp **) pp2;
		struct kinfo_proc2 *p1 = proc_from_thread(l1);
		struct kinfo_proc2 *p2 = proc_from_thread(l2);
		return strcmp(p2->p_comm, p1->p_comm);
	} else {
		struct kinfo_proc2 *p1 = *(struct kinfo_proc2 **) pp1;
		struct kinfo_proc2 *p2 = *(struct kinfo_proc2 **) pp2;
		return strcmp(p2->p_comm, p1->p_comm);
	}
}

static int
compare_username(pp1, pp2)
	struct proc **pp1, **pp2;
{
	if (threadmode) {
		struct kinfo_lwp *l1 = *(struct kinfo_lwp **) pp1;
		struct kinfo_lwp *l2 = *(struct kinfo_lwp **) pp2;
		struct kinfo_proc2 *p1 = proc_from_thread(l1);
		struct kinfo_proc2 *p2 = proc_from_thread(l2);
		return strcmp(p2->p_login, p1->p_login);
	} else {
		struct kinfo_proc2 *p1 = *(struct kinfo_proc2 **) pp1;
		struct kinfo_proc2 *p2 = *(struct kinfo_proc2 **) pp2;
		return strcmp(p2->p_login, p1->p_login);
	}
}
/* compare_size - the comparison function for sorting by total memory usage */

static int
compare_size(pp1, pp2)
	struct proc **pp1, **pp2;
{
	int result;
	pctcpu lresult;

	if (threadmode) {
		struct kinfo_lwp *p1 = *(struct kinfo_lwp **) pp1;
		struct kinfo_lwp *p2 = *(struct kinfo_lwp **) pp2;

		ORDERKEY_PCTCPU(l_)
		ORDERKEY_CPTICKS(l_)
		ORDERKEY_STATE(l_)
		ORDERKEY_PRIO(l_)
		return result;
	} else {
		struct kinfo_proc2 *p1 = *(struct kinfo_proc2 **) pp1;
		struct kinfo_proc2 *p2 = *(struct kinfo_proc2 **) pp2;

		ORDERKEY_MEM
		ORDERKEY_RSSIZE
		ORDERKEY_PCTCPU(p_)
		ORDERKEY_CPTICKS(p_)
		ORDERKEY_STATE(p_)
		ORDERKEY_PRIO(p_)
		return result;
	}

	return (result);
}

/* compare_state - the comparison function for sorting by process state */

static int
compare_state(pp1, pp2)
	struct proc **pp1, **pp2;
{
	int result;
	pctcpu lresult;

	if (threadmode) {
		struct kinfo_lwp *p1 = *(struct kinfo_lwp **) pp1;
		struct kinfo_lwp *p2 = *(struct kinfo_lwp **) pp2;

		ORDERKEY_STATE(l_)
		ORDERKEY_PCTCPU(l_)
		ORDERKEY_CPTICKS(l_)
		ORDERKEY_PRIO(l_)
		return result;
	} else {
		struct kinfo_proc2 *p1 = *(struct kinfo_proc2 **) pp1;
		struct kinfo_proc2 *p2 = *(struct kinfo_proc2 **) pp2;

		ORDERKEY_STATE(p_)
		ORDERKEY_PCTCPU(p_)
		ORDERKEY_CPTICKS(p_)
		ORDERKEY_PRIO(p_)
		ORDERKEY_RSSIZE
		ORDERKEY_MEM
		return result;
	}

	return (result);
}

/* compare_time - the comparison function for sorting by total CPU time */

static int
compare_time(pp1, pp2)
	struct proc **pp1, **pp2;
{
	int result;
	pctcpu lresult;

	if (threadmode) {
		struct kinfo_lwp *p1 = *(struct kinfo_lwp **) pp1;
		struct kinfo_lwp *p2 = *(struct kinfo_lwp **) pp2;

		ORDERKEY_CPTICKS(l_)
		ORDERKEY_PCTCPU(l_)
		ORDERKEY_STATE(l_)
		ORDERKEY_PRIO(l_)
		return result;
	} else {
		struct kinfo_proc2 *p1 = *(struct kinfo_proc2 **) pp1;
		struct kinfo_proc2 *p2 = *(struct kinfo_proc2 **) pp2;

		ORDERKEY_CPTICKS(p_)
		ORDERKEY_PCTCPU(p_)
		ORDERKEY_STATE(p_)
		ORDERKEY_PRIO(p_)
		ORDERKEY_MEM
		ORDERKEY_RSSIZE
		return result;
	}

	return (result);
}


/*
 * proc_owner(pid) - returns the uid that owns process "pid", or -1 if
 *		the process does not exist.
 *		It is EXTREMLY IMPORTANT that this function work correctly.
 *		If top runs setuid root (as in SVR4), then this function
 *		is the only thing that stands in the way of a serious
 *		security problem.  It validates requests for the "kill"
 *		and "renice" commands.
 */

int
proc_owner(pid)
	int pid;
{
	int cnt;
	struct kinfo_proc2 **prefp;
	struct kinfo_proc2 *pp;

	if (threadmode)
		return(-1);

	prefp = pref;
	cnt = pref_len;
	while (--cnt >= 0) {
		pp = *prefp++;	
		if (pp->p_pid == (pid_t)pid)
			return(pp->p_ruid);
	}
	return(-1);
}

/*
 *  percentages(cnt, out, new, old, diffs) - calculate percentage change
 *	between array "old" and "new", putting the percentages i "out".
 *	"cnt" is size of each array and "diffs" is used for scratch space.
 *	The array "old" is updated on each call.
 *	The routine assumes modulo arithmetic.  This function is especially
 *	useful on BSD mchines for calculating CPU state percentages.
 */

static void
percentages64(cnt, out, new, old, diffs)
	int cnt;
	int *out;
	u_int64_t *new;
	u_int64_t *old;
	u_int64_t *diffs;
{
	int i;
	u_int64_t change;
	u_int64_t total_change;
	u_int64_t *dp;
	u_int64_t half_total;

	/* initialization */
	total_change = 0;
	dp = diffs;

	/* calculate changes for each state and the overall change */
	for (i = 0; i < cnt; i++) {
		/*
		 * Don't worry about wrapping - even at hz=1GHz, a
		 * u_int64_t will last at least 544 years.
		 */
		change = *new - *old;
		total_change += (*dp++ = change);
		*old++ = *new++;
	}

	/* avoid divide by zero potential */
	if (total_change == 0)
		total_change = 1;

	/* calculate percentages based on overall change, rounding up */
	half_total = total_change / 2;
	for (i = 0; i < cnt; i++)
		*out++ = (int)((*diffs++ * 1000 + half_total) / total_change);
}
