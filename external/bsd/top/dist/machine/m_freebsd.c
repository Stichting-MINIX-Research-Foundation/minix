/*
 * Copyright (c) 1984 through 2008, William LeFebvre
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 * 
 *     * Neither the name of William LeFebvre nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * top - a top users display for Unix
 *
 * SYNOPSIS:  For FreeBSD 5.x, 6.x, 7.x, 8.x
 *
 * DESCRIPTION:
 * Originally written for BSD4.4 system by Christos Zoulas.
 * Ported to FreeBSD 2.x by Steven Wallace && Wolfram Schneider
 * Order support hacked in from top-3.5beta6/machine/m_aix41.c
 *   by Monte Mitzelfelt
 * Ported to FreeBSD 5.x and higher by William LeFebvre
 *
 * AUTHOR:  Christos Zoulas <christos@ee.cornell.edu>
 *          Steven Wallace  <swallace@freebsd.org>
 *          Wolfram Schneider <wosch@FreeBSD.org>
 */


#include <sys/time.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/param.h>

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <nlist.h>
#include <math.h>
#include <kvm.h>
#include <pwd.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <sys/dkstat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/vmmeter.h>
#include <sys/resource.h>
#include <sys/rtprio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* Swap */
#include <stdlib.h>
#include <sys/conf.h>

#include <osreldate.h> /* for changes in kernel structures */

#include "top.h"
#include "machine.h"
#include "utils.h"
#include "username.h"
#include "hash.h"
#include "display.h"

extern char* printable __P((char *));
int swapmode __P((int *retavail, int *retfree));
static int smpmode;
static int namelength;

/*
 * Versions prior to 5.x do not track threads in kinfo_proc, so we
 * simply do not display any information about them.
 * Versions 5.x, 6.x, and 7.x track threads but the data reported
 * as runtime for each thread is actually per-process and is just
 * duplicated across all threads.  It would be very wrong to show
 * this data individually for each thread.  Therefore we will show
 * a THR column (number of threads) but not provide any sort of
 * per-thread display.  We distinguish between these three ways of
 * handling threads as follows:  HAS_THREADS indicates that the
 * system has and tracks kernel threads (a THR column will appear
 * in the display).  HAS_SHOWTHREADS indicates that the system 
 * reports correct per-thread information and we will provide a
 * per-thread display (the 'H' and 't' command) upon request.
 * HAS_SHOWTHREADS implies HAS_THREADS.
 */

/* HAS_THREADS for anything 5.x and up */
#if OSMAJOR >= 5
#define HAS_THREADS
#endif

/* HAS_SHOWTHREADS for anything 8.x and up */
#if OSMAJOR >=8
#define HAS_SHOWTHREADS
#endif

/* get_process_info passes back a handle.  This is what it looks like: */

struct handle
{
    struct kinfo_proc **next_proc;	/* points to next valid proc pointer */
    int remaining;		/* number of pointers remaining */
};

/* declarations for load_avg */
#include "loadavg.h"

/*
 * Macros to access process information:
 * In versions 4.x and earlier the kinfo_proc structure was a collection of
 * substructures (kp_proc and kp_eproc).  Starting with 5.0 kinfo_proc was
 * redesigned and "flattene" so that most of the information was available
 * in a single structure.  We use macros to access the various types of
 * information and define these macros according to the OS revision.  The
 * names PP, EP, and VP are due to the fact that information was originally
 * contained in the different substructures.  We retain these names in the
 * code for backward compatibility.  These macros use ANSI concatenation.
 * PP: proc
 * EP: extented proc
 * VP: vm (virtual memory information)
 * PRUID: Real uid
 * RP: rusage
 * PPCPU: where we store calculated cpu% data
 * SPPTR: where we store pointer to extra calculated data
 * SP: access to the extra calculated data pointed to by SPPTR
 */
#if OSMAJOR <= 4
#define PP(pp, field) ((pp)->kp_proc . p_##field)
#define EP(pp, field) ((pp)->kp_eproc . e_##field)
#define VP(pp, field) ((pp)->kp_eproc.e_vm . vm_##field)
#define PRUID(pp) ((pp)->kp_eproc.e_pcred.p_ruid)
#else
#define PP(pp, field) ((pp)->ki_##field)
#define EP(pp, field) ((pp)->ki_##field)
#define VP(pp, field) ((pp)->ki_##field)
#define PRUID(pp) ((pp)->ki_ruid)
#define RP(pp, field) ((pp)->ki_rusage.ru_##field)
#define PPCPU(pp) ((pp)->ki_sparelongs[0])
#define SPPTR(pp) ((pp)->ki_spareptrs[0])
#define SP(pp, field) (((struct save_proc *)((pp)->ki_spareptrs[0]))->sp_##field)
#endif

/* what we consider to be process size: */
#if OSMAJOR <= 4
#define PROCSIZE(pp) (VP((pp), map.size) / 1024)
#else
#define PROCSIZE(pp) (((pp)->ki_size) / 1024)
#endif

/* calculate a per-second rate using milliseconds */
#define per_second(n, msec)   (((n) * 1000) / (msec))

/* process state names for the "STATE" column of the display */
/* the extra nulls in the string "run" are for adding a slash and
   the processor number when needed */

char *state_abbrev[] =
{
    "?", "START", "RUN", "SLEEP", "STOP", "ZOMB", "WAIT", "LOCK"
};
#define NUM_STATES 8

/* kernel access */
static kvm_t *kd;

/* these are for dealing with sysctl-based data */
#define MAXMIBLEN 8
struct sysctl_mib {
    char *name;
    int mib[MAXMIBLEN];
    size_t miblen;
};
static struct sysctl_mib mibs[] = {
    { "vm.stats.sys.v_swtch" },
#define V_SWTCH 0
    { "vm.stats.sys.v_trap" },
#define V_TRAP 1
    { "vm.stats.sys.v_intr" },
#define V_INTR 2
    { "vm.stats.sys.v_soft" },
#define V_SOFT 3
    { "vm.stats.vm.v_forks" },
#define V_FORKS 4
    { "vm.stats.vm.v_vforks" },
#define V_VFORKS 5
    { "vm.stats.vm.v_rforks" },
#define V_RFORKS 6
    { "vm.stats.vm.v_vm_faults" },
#define V_VM_FAULTS 7
    { "vm.stats.vm.v_swapin" },
#define V_SWAPIN 8
    { "vm.stats.vm.v_swapout" },
#define V_SWAPOUT 9
    { "vm.stats.vm.v_tfree" },
#define V_TFREE 10
    { "vm.stats.vm.v_vnodein" },
#define V_VNODEIN 11
    { "vm.stats.vm.v_vnodeout" },
#define V_VNODEOUT 12
    { "vm.stats.vm.v_active_count" },
#define V_ACTIVE_COUNT 13
    { "vm.stats.vm.v_inactive_count" },
#define V_INACTIVE_COUNT 14
    { "vm.stats.vm.v_wire_count" },
#define V_WIRE_COUNT 15
    { "vm.stats.vm.v_cache_count" },
#define V_CACHE_COUNT 16
    { "vm.stats.vm.v_free_count" },
#define V_FREE_COUNT 17
    { "vm.stats.vm.v_swappgsin" },
#define V_SWAPPGSIN 18
    { "vm.stats.vm.v_swappgsout" },
#define V_SWAPPGSOUT 19
    { "vfs.bufspace" },
#define VFS_BUFSPACE 20
    { "kern.cp_time" },
#define K_CP_TIME 21
#ifdef HAS_SHOWTHREADS
    { "kern.proc.all" },
#else
    { "kern.proc.proc" },
#endif
#define K_PROC 22
    { NULL }
};
    

/* these are for calculating cpu state percentages */

static long cp_time[CPUSTATES];
static long cp_old[CPUSTATES];
static long cp_diff[CPUSTATES];

/* these are for detailing the process states */

int process_states[8];
char *procstatenames[] = {
    "", " starting, ", " running, ", " sleeping, ", " stopped, ", " zombie, ",
    " waiting, ", " locked, ",
    NULL
};

/* these are for detailing the cpu states */

int cpu_states[CPUSTATES];
char *cpustatenames[] = {
    "user", "nice", "system", "interrupt", "idle", NULL
};

/* these are for detailing the kernel information */

int kernel_stats[9];
char *kernelnames[] = {
    " ctxsw, ", " trap, ", " intr, ", " soft, ", " fork, ",
    " flt, ", " pgin, ", " pgout, ", " fr",
    NULL
};

/* these are for detailing the memory statistics */

long memory_stats[7];
char *memorynames[] = {
    "K Active, ", "K Inact, ", "K Wired, ", "K Cache, ", "K Buf, ", "K Free",
    NULL
};

long swap_stats[7];
char *swapnames[] = {
/*   0           1            2           3            4       5 */
    "K Total, ", "K Used, ", "K Free, ", "% Inuse, ", "K In, ", "K Out",
    NULL
};


/*
 * pbase points to the array that holds the kinfo_proc structures.  pref
 * (pronounced p-ref) points to an array of kinfo_proc pointers and is where
 * we build up a list of processes we wish to display.  Both pbase and pref are
 * potentially resized on every call to get_process_info.  psize is the number
 * of procs for which we currently have space allocated.  pref_len is the number
 * of valid pointers in pref (this is used by proc_owner).  We start psize off
 * at -1 to ensure that space gets allocated on the first call to
 * get_process_info.
 */

static int psize = -1;
static int pref_len;
static struct kinfo_proc *pbase = NULL;
static struct kinfo_proc **pref = NULL;

/* this structure retains information from the proc array between samples */
struct save_proc {
    pid_t sp_pid;
    u_int64_t sp_runtime;
    long sp_vcsw;
    long sp_ivcsw;
    long sp_inblock;
    long sp_oublock;
    long sp_majflt;
    long sp_totalio;
    long sp_old_nvcsw;
    long sp_old_nivcsw;
    long sp_old_inblock;
    long sp_old_oublock;
    long sp_old_majflt;
};
hash_table *procs;

struct proc_field {
    char *name;
    int width;
    int rjust;
    int min_screenwidth;
    int (*format)(char *, int, struct kinfo_proc *);
};

/* these are for getting the memory statistics */

static int pagesize;		/* kept from getpagesize */
static int pageshift;		/* log base 2 of the pagesize */

/* define pagetok in terms of pageshift */

#define pagetok(size) ((size) << pageshift)

/* things that we track between updates */
static u_int ctxsws = 0;
static u_int traps = 0;
static u_int intrs = 0;
static u_int softs = 0;
static u_int64_t forks = 0;
static u_int pfaults;
static u_int pagein;
static u_int pageout;
static u_int tfreed;
static int swappgsin = -1;
static int swappgsout = -1;
extern struct timeval timeout;
static struct timeval lasttime = { 0, 0 };
static long elapsed_time;
static long elapsed_msecs;

/* things that we track during an update */
static long total_io;
static int show_fullcmd;
static struct handle handle;
static int username_length;
static int show_usernames;
static int display_mode;
static int *display_fields;
#ifdef HAS_SHOWTHREADS
static int show_threads = 0;
#endif


/* sorting orders. first is default */
char *ordernames[] = {
    "cpu", "size", "res", "time", "pri", "io", "pid", NULL
};

/* compare routines */
int proc_compare(), compare_size(), compare_res(), compare_time(),
    compare_prio(), compare_io(), compare_pid();

int (*proc_compares[])() = {
    proc_compare,
    compare_size,
    compare_res,
    compare_time,
    compare_prio,
    compare_io,
    compare_pid,
    NULL
};

/* swap related calculations */

static int mib_swapinfo[16];
static int *mib_swapinfo_idx;
static int mib_swapinfo_size = 0;

void
swap_init()

{
    size_t m;

    m = sizeof(mib_swapinfo) / sizeof(mib_swapinfo[0]);
    if (sysctlnametomib("vm.swap_info", mib_swapinfo, &m) != -1)
    {
	mib_swapinfo_size = m + 1;
	mib_swapinfo_idx = &(mib_swapinfo[m]);
    }
}

int
swap_getdata(long long *retavail, long long *retfree)

{
    int n;
    size_t size;
    long long total = 0;
    long long used = 0;
    struct xswdev xsw;

    n = 0;
    if (mib_swapinfo_size > 0)
    {
	*mib_swapinfo_idx = 0;
	while (size = sizeof(xsw),
	       sysctl(mib_swapinfo, mib_swapinfo_size, &xsw, &size, NULL, 0) != -1)
	{
	    dprintf("swap_getdata: swaparea %d: nblks %d, used %d\n",
		    n, xsw.xsw_nblks, xsw.xsw_used);
	    total += (long long)xsw.xsw_nblks;
	    used += (long long)xsw.xsw_used;
	    *mib_swapinfo_idx = ++n;
	}

	*retavail = pagetok(total);
	*retfree = pagetok(total) - pagetok(used);

	if (total > 0)
	{
	    n = (int)((double)used * 100.0 / (double)total);
	}
	else
	{
	    n = 0;
	}
    }
    else
    {
	*retavail = 0;
	*retfree = 0;
    }

    dprintf("swap_getdata: avail %lld, free %lld, %d%%\n",
	    *retavail, *retfree, n);
    return(n);
}

/*
 *  getkval(offset, ptr, size) - get a value out of the kernel.
 *	"offset" is the byte offset into the kernel for the desired value,
 *  	"ptr" points to a buffer into which the value is retrieved,
 *  	"size" is the size of the buffer (and the object to retrieve).
 *      Return 0 on success, -1 on any kind of failure.
 */

static int
getkval(unsigned long offset, int *ptr, int size)

{
    if (kd != NULL)
    {
	if (kvm_read(kd, offset, (char *) ptr, size) == size)
	{
	    return(0);
	}
    }
    return(-1);
}

int
get_sysctl_mibs()

{
    struct sysctl_mib *mp;
    size_t len;

    mp = mibs;
    while (mp->name != NULL)
    {
	len = MAXMIBLEN;
	if (sysctlnametomib(mp->name, mp->mib, &len) == -1)
	{
	    message_error(" sysctlnametomib: %s", strerror(errno));
	    return -1;
	}
	mp->miblen = len;
	mp++;
    }
    return 0;
}

int
get_sysctl(int idx, void *v, size_t l)

{
    struct sysctl_mib *m;
    size_t len;

    m = &(mibs[idx]);
    len = l;
    if (sysctl(m->mib, m->miblen, v, &len, NULL, 0) == -1)
    {
	message_error(" sysctl: %s", strerror(errno));
	return -1;
    }
    return len;
}

size_t
get_sysctlsize(int idx)

{
    size_t len;
    struct sysctl_mib *m;

    m = &(mibs[idx]);
    if (sysctl(m->mib, m->miblen, NULL, &len, NULL, 0) == -1)
    {
	message_error(" sysctl (size): %s", strerror(errno));
	len = 0;
    }
    return len;
}
    
int
fmt_pid(char *buf, int sz, struct kinfo_proc *pp)

{
    return snprintf(buf, sz, "%6d", PP(pp, pid));
}

int
fmt_username(char *buf, int sz, struct kinfo_proc *pp)

{
    return snprintf(buf, sz, "%-*.*s",
		    username_length, username_length, username(PRUID(pp)));
}

int
fmt_uid(char *buf, int sz, struct kinfo_proc *pp)

{
    return snprintf(buf, sz, "%6d", PRUID(pp));
}

int
fmt_thr(char *buf, int sz, struct kinfo_proc *pp)

{
    return snprintf(buf, sz, "%3d", PP(pp, numthreads));
}

int
fmt_pri(char *buf, int sz, struct kinfo_proc *pp)

{
#if OSMAJOR <= 4
    return snprintf(buf, sz, "%3d", PP(pp, priority));
#else
    return snprintf(buf, sz, "%3d", PP(pp, pri.pri_level));
#endif
}

int
fmt_nice(char *buf, int sz, struct kinfo_proc *pp)

{
    return snprintf(buf, sz, "%4d", PP(pp, nice) - NZERO);
}

int
fmt_size(char *buf, int sz, struct kinfo_proc *pp)

{
    return snprintf(buf, sz, "%5s", format_k(PROCSIZE(pp)));
}

int
fmt_res(char *buf, int sz, struct kinfo_proc *pp)

{
    return snprintf(buf, sz, "%5s", format_k(pagetok(VP(pp, rssize))));
}

int
fmt_state(char *buf, int sz, struct kinfo_proc *pp)

{
    int state;
    char status[16];

    state = PP(pp, stat);
    switch(state)
    {
    case SRUN:
	if (smpmode && PP(pp, oncpu) != 0xff)
	    sprintf(status, "CPU%d", PP(pp, oncpu));
	else
	    strcpy(status, "RUN");
	break;

    case SSLEEP:
	if (EP(pp, wmesg) != NULL) {
	    sprintf(status, "%.6s", EP(pp, wmesg));
	    break;
	}
	/* fall through */
    default:
	if (state >= 0 && state < NUM_STATES)
	    sprintf(status, "%.6s", state_abbrev[(unsigned char) state]);
	else
	    sprintf(status, "?%-5d", state);
	break;
    }

    return snprintf(buf, sz, "%-6.6s", status);
}

int
fmt_flags(char *buf, int sz, struct kinfo_proc *pp)

{
    long flag;
    char chrs[12];
    char *p;

    flag = PP(pp, flag);
    p = chrs;
    if (PP(pp, nice) < NZERO)
	*p++ = '<';
    else if (PP(pp, nice) > NZERO)
	*p++ = 'N';
    if (flag & P_TRACED)
	*p++ = 'X';
    if (flag & P_WEXIT && PP(pp, stat) != SZOMB)
	*p++ = 'E';
    if (flag & P_PPWAIT)
	*p++ = 'V';
    if (flag & P_SYSTEM || PP(pp, lock) > 0)
	*p++ = 'L';
    if (PP(pp, kiflag) & KI_SLEADER)
	*p++ = 's';
    if (flag & P_CONTROLT)
	*p++ = '+';
    if (flag & P_JAILED)
	*p++ = 'J';
    *p = '\0';

    return snprintf(buf, sz, "%-3.3s", chrs);
}

int
fmt_c(char *buf, int sz, struct kinfo_proc *pp)

{
    return snprintf(buf, sz, "%1x", PP(pp, lastcpu));
}

int
fmt_time(char *buf, int sz, struct kinfo_proc *pp)

{
    return snprintf(buf, sz, "%6s",
		    format_time((PP(pp, runtime) + 500000) / 1000000));
}

int
fmt_cpu(char *buf, int sz, struct kinfo_proc *pp)

{
    return snprintf(buf, sz, "%5.2f%%", (double)PPCPU(pp) / 100.0);
}

int
fmt_command(char *buf, int sz, struct kinfo_proc *pp)

{
    int inmem;
    char cmd[MAX_COLS];
    char *bufp;
    struct pargs pargs;
    int len;

#if OSMAJOR <= 4
    inmem = (PP(pp, flag) & P_INMEM);
#else
    inmem = (PP(pp, sflag) & PS_INMEM);
#endif

    if (show_fullcmd && inmem)
    {
        /* get the pargs structure */
        if (getkval((unsigned long)PP(pp, args), (int *)&pargs, sizeof(pargs)) != -1)
        {
            /* determine workable length */
            if ((len = pargs.ar_length) >= MAX_COLS)
            {
                len = MAX_COLS - 1;
            }

            /* get the string from that */
            if (len > 0 && getkval((unsigned long)PP(pp, args) +
				   sizeof(pargs.ar_ref) +
				   sizeof(pargs.ar_length),
				   (int *)cmd, len) != -1)
            {
                /* successfull retrieval: now convert nulls in to spaces */
                bufp = cmd;
                while (len-- > 0)
                {
                    if (*bufp == '\0')
                    {
                        *bufp = ' ';
                    }
                    bufp++;
                }

                /* null terminate cmd */
                *--bufp = '\0';

		/* format cmd as our answer */
		return snprintf(buf, sz, "%s", cmd);
            }
        }
    }

    /* for anything else we just display comm */
    return snprintf(buf, sz, inmem ? "%s" : "<%s>", printable(PP(pp, comm)));
}

int
fmt_vcsw(char *buf, int sz, struct kinfo_proc *pp)

{
    return snprintf(buf, sz, "%6ld", per_second(SP(pp, vcsw), elapsed_msecs));
}

int
fmt_ivcsw(char *buf, int sz, struct kinfo_proc *pp)

{
    return snprintf(buf, sz, "%6ld", per_second(SP(pp, ivcsw), elapsed_msecs));
}

int
fmt_read(char *buf, int sz, struct kinfo_proc *pp)

{
    return snprintf(buf, sz, "%6ld", per_second(SP(pp, inblock), elapsed_msecs));
}

int
fmt_write(char *buf, int sz, struct kinfo_proc *pp)

{
    return snprintf(buf, sz, "%6ld", per_second(SP(pp, oublock), elapsed_msecs));
}

int
fmt_fault(char *buf, int sz, struct kinfo_proc *pp)

{
    return snprintf(buf, sz, "%6ld", per_second(SP(pp, majflt), elapsed_msecs));
}

int
fmt_iototal(char *buf, int sz, struct kinfo_proc *pp)

{
    return snprintf(buf, sz, "%6ld", per_second(SP(pp, totalio), elapsed_msecs));
}

int
fmt_iopct(char *buf, int sz, struct kinfo_proc *pp)

{
    return snprintf(buf, sz, "%6.2f", (SP(pp, totalio) * 100.) / total_io);
}


struct proc_field proc_field[] = {
    { "PID", 6, 1, 0, fmt_pid },
    { "USERNAME", 8, 0, 0, fmt_username },
#define FIELD_USERNAME 1
    { "UID", 6, 1, 0, fmt_uid },
#define FIELD_UID 2
    { "THR", 3, 1, 0, fmt_thr },
    { "PRI", 3, 1, 0, fmt_pri },
    { "NICE", 4, 1, 0, fmt_nice },
    { "SIZE", 5, 1, 0, fmt_size },
    { "RES", 5, 1, 0, fmt_res },
    { "STATE", 6, 0, 0, fmt_state },
    { "FLG", 3, 0, 84, fmt_flags },
    { "C", 1, 0, 0, fmt_c },
    { "TIME", 6, 1, 0, fmt_time },
    { "CPU", 6, 1, 0, fmt_cpu },
    { "COMMAND", 7, 0, 0, fmt_command },
    { "VCSW", 6, 1, 0, fmt_vcsw },
    { "IVCSW", 6, 1, 0, fmt_ivcsw },
    { "READ", 6, 1, 0, fmt_read },
    { "WRITE", 6, 1, 0, fmt_write },
    { "FAULT", 6, 1, 0, fmt_fault },
    { "TOTAL", 6, 1, 0, fmt_iototal },
    { "PERCENT", 7, 1, 0, fmt_iopct },
    { NULL, 0, 0, 0, NULL }
};
#define MAX_FIELDS 24

static int mode0_display[MAX_FIELDS];
static int mode0thr_display[MAX_FIELDS];
static int mode1_display[MAX_FIELDS];

int
field_index(char *col)

{
    struct proc_field *fp;
    int i = 0;

    fp = proc_field;
    while (fp->name != NULL)
    {
	if (strcmp(col, fp->name) == 0)
	{
	    return i;
	}
	fp++;
	i++;
    }

    return -1;
}

void
field_subst(int *fp, int old, int new)

{
    while (*fp != -1)
    {
	if (*fp == old)
	{
	    *fp = new;
	}
	fp++;
    }
}

int
machine_init(struct statics *statics)

{
    int i = 0;
    size_t len;
    int *ip;

    struct timeval boottime;

    len = sizeof(smpmode);
    if ((sysctlbyname("machdep.smp_active", &smpmode, &len, NULL, 0) < 0 &&
         sysctlbyname("smp.smp_active", &smpmode, &len, NULL, 0) < 0) ||
	len != sizeof(smpmode))
    {
	smpmode = 0;
    }
    smpmode = smpmode != 0;

    /* kvm_open the active kernel: its okay if this fails */
    kd = kvm_open(NULL, NULL, NULL, O_RDONLY, NULL);

    /* get boot time */
    len = sizeof(boottime);
    if (sysctlbyname("kern.boottime", &boottime, &len, NULL, 0) == -1)
    {
	/* we have no boottime to report */
	boottime.tv_sec = -1;
    }

    pbase = NULL;
    pref = NULL;

    /* get the page size with "getpagesize" and calculate pageshift from it */
    i = pagesize = getpagesize();
    pageshift = 0;
    while (i > 1)
    {
	pageshift++;
	i >>= 1;
    }

    /* translate sysctl paths to mibs for faster access */
    get_sysctl_mibs();

    /* initialize swap stuff */
    swap_init();

    /* create the hash table that remembers proc data */
    procs = hash_create(2039);

    /* we only need the amount of log(2)1024 for our conversion */
    pageshift -= LOG1024;

    /* fill in the statics information */
    statics->procstate_names = procstatenames;
    statics->cpustate_names = cpustatenames;
    statics->memory_names = memorynames;
    statics->kernel_names = kernelnames;
    statics->boottime = boottime.tv_sec;
    statics->swap_names = swapnames;
    statics->order_names = ordernames;
    statics->flags.warmup = 1;
    statics->modemax = 2;
#ifdef HAS_SHOWTHREADS
    statics->flags.threads = 1;
#endif

    /* we need kvm descriptor in order to show full commands */
    statics->flags.fullcmds = kd != NULL;

    /* set up the display indices for mode0 */
    ip = mode0_display;
    *ip++ = field_index("PID");
    *ip++ = field_index("USERNAME");
#ifdef HAS_THREADS
    *ip++ = field_index("THR");
#endif
    *ip++ = field_index("PRI");
    *ip++ = field_index("NICE");
    *ip++ = field_index("SIZE");
    *ip++ = field_index("RES");
    *ip++ = field_index("STATE");
    *ip++ = field_index("FLG");
    if (smpmode)
	*ip++ = field_index("C");
    *ip++ = field_index("TIME");
    *ip++ = field_index("CPU");
    *ip++ = field_index("COMMAND");
    *ip = -1;

#ifdef HAS_SHOWTHREADS
    /* set up the display indices for mode0 showing threads */
    ip = mode0thr_display;
    *ip++ = field_index("PID");
    *ip++ = field_index("USERNAME");
    *ip++ = field_index("PRI");
    *ip++ = field_index("NICE");
    *ip++ = field_index("SIZE");
    *ip++ = field_index("RES");
    *ip++ = field_index("STATE");
    *ip++ = field_index("FLG");
    if (smpmode)
	*ip++ = field_index("C");
    *ip++ = field_index("TIME");
    *ip++ = field_index("CPU");
    *ip++ = field_index("COMMAND");
    *ip = -1;
#endif

    /* set up the display indices for mode1 */
    ip = mode1_display;
    *ip++ = field_index("PID");
    *ip++ = field_index("USERNAME");
    *ip++ = field_index("VCSW");
    *ip++ = field_index("IVCSW");
    *ip++ = field_index("READ");
    *ip++ = field_index("WRITE");
    *ip++ = field_index("FAULT");
    *ip++ = field_index("TOTAL");
    *ip++ = field_index("PERCENT");
    *ip++ = field_index("COMMAND");
    *ip = -1;

    /* all done! */
    return(0);
}

char *format_header(char *uname_field)

{
    return "";
}

void
get_vm_sum(struct vmmeter *sum)

{
#define GET_VM_STAT(v, s)  (void)get_sysctl(v, &(sum->s), sizeof(sum->s))

    GET_VM_STAT(V_SWTCH, v_swtch);
    GET_VM_STAT(V_TRAP, v_trap);
    GET_VM_STAT(V_INTR, v_intr);
    GET_VM_STAT(V_SOFT, v_soft);
    GET_VM_STAT(V_VFORKS, v_vforks);
    GET_VM_STAT(V_FORKS, v_forks);
    GET_VM_STAT(V_RFORKS, v_rforks);
    GET_VM_STAT(V_VM_FAULTS, v_vm_faults);
    GET_VM_STAT(V_SWAPIN, v_swapin);
    GET_VM_STAT(V_SWAPOUT, v_swapout);
    GET_VM_STAT(V_TFREE, v_tfree);
    GET_VM_STAT(V_VNODEIN, v_vnodein);
    GET_VM_STAT(V_VNODEOUT, v_vnodeout);
    GET_VM_STAT(V_ACTIVE_COUNT, v_active_count);
    GET_VM_STAT(V_INACTIVE_COUNT, v_inactive_count);
    GET_VM_STAT(V_WIRE_COUNT, v_wire_count);
    GET_VM_STAT(V_CACHE_COUNT, v_cache_count);
    GET_VM_STAT(V_FREE_COUNT, v_free_count);
    GET_VM_STAT(V_SWAPPGSIN, v_swappgsin);
    GET_VM_STAT(V_SWAPPGSOUT, v_swappgsout);
}

void
get_system_info(struct system_info *si)

{
    long total;
    struct timeval thistime;
    struct timeval timediff;

    /* timestamp and time difference */
    gettimeofday(&thistime, 0);
    timersub(&thistime, &lasttime, &timediff);
    elapsed_time = timediff.tv_sec * 1000000 + timediff.tv_usec;
    elapsed_msecs = timediff.tv_sec * 1000 + timediff.tv_usec / 1000;

    /* get the load averages */
    if (getloadavg(si->load_avg, NUM_AVERAGES) == -1)
    {
	/* failed: fill in with zeroes */
	(void) memset(si->load_avg, 0, sizeof(si->load_avg));
    }

    /* get the cp_time array */
    (void)get_sysctl(K_CP_TIME, &cp_time, sizeof(cp_time));

    /* convert cp_time counts to percentages */
    total = percentages(CPUSTATES, cpu_states, cp_time, cp_old, cp_diff);

    /* sum memory & swap statistics */
    {
	struct vmmeter sum;
	static unsigned int swap_delay = 0;
	static long long swapavail = 0;
	static long long swapfree = 0;
	static int bufspace = 0;

	get_vm_sum(&sum);

	/* get bufspace */
	bufspace = 0;
	(void) get_sysctl(VFS_BUFSPACE, &bufspace, sizeof(bufspace));

	/* kernel stats */
	dprintf("kernel: swtch %d, trap %d, intr %d, soft %d, vforks %d\n",
		sum.v_swtch, sum.v_trap, sum.v_intr, sum.v_soft, sum.v_vforks);
	kernel_stats[0] = per_second(sum.v_swtch - ctxsws, elapsed_msecs);
	kernel_stats[1] = per_second(sum.v_trap - traps, elapsed_msecs);
	kernel_stats[2] = per_second(sum.v_intr - intrs, elapsed_msecs);
	kernel_stats[3] = per_second(sum.v_soft - softs, elapsed_msecs);
	kernel_stats[4] = per_second(sum.v_vforks + sum.v_forks +
				     sum.v_rforks - forks, elapsed_msecs);
	kernel_stats[5] = per_second(sum.v_vm_faults - pfaults, elapsed_msecs);
	kernel_stats[6] = per_second(sum.v_swapin + sum.v_vnodein - pagein, elapsed_msecs);
	kernel_stats[7] = per_second(sum.v_swapout + sum.v_vnodeout - pageout, elapsed_msecs);
	kernel_stats[8] = per_second(sum.v_tfree - tfreed, elapsed_msecs);
	ctxsws = sum.v_swtch;
	traps = sum.v_trap;
	intrs = sum.v_intr;
	softs = sum.v_soft;
	forks = (u_int64_t)sum.v_vforks + sum.v_forks + sum.v_rforks;
	pfaults = sum.v_vm_faults;
	pagein = sum.v_swapin + sum.v_vnodein;
	pageout = sum.v_swapout + sum.v_vnodeout;
	tfreed = sum.v_tfree;

	/* convert memory stats to Kbytes */
	memory_stats[0] = pagetok(sum.v_active_count);
	memory_stats[1] = pagetok(sum.v_inactive_count);
	memory_stats[2] = pagetok(sum.v_wire_count);
	memory_stats[3] = pagetok(sum.v_cache_count);
	memory_stats[4] = bufspace / 1024;
	memory_stats[5] = pagetok(sum.v_free_count);
	memory_stats[6] = -1;

	/* first interval */
        if (swappgsin < 0)
	{
	    swap_stats[4] = 0;
	    swap_stats[5] = 0;
	} 

	/* compute differences between old and new swap statistic */
	else
	{
	    swap_stats[4] = pagetok(sum.v_swappgsin - swappgsin);
	    swap_stats[5] = pagetok(sum.v_swappgsout - swappgsout);
	}

        swappgsin = sum.v_swappgsin;
	swappgsout = sum.v_swappgsout;

	/* call CPU heavy swap_getdata() only for changes */
        if (swap_stats[4] > 0 || swap_stats[5] > 0 || swap_delay == 0)
	{
	    swap_stats[3] = swap_getdata(&swapavail, &swapfree);
	    swap_stats[0] = swapavail;
	    swap_stats[1] = swapavail - swapfree;
	    swap_stats[2] = swapfree;
	}
        swap_delay = 1;
	swap_stats[6] = -1;
    }

    /* set arrays and strings */
    si->cpustates = cpu_states;
    si->kernel = kernel_stats;
    si->memory = memory_stats;
    si->swap = swap_stats;

    si->last_pid = -1;

    lasttime = thistime;
}

caddr_t
get_process_info(struct system_info *si,
			 struct process_select *sel,
			 int compare_index)

{
    int i;
    int total_procs;
    int active_procs;
    struct kinfo_proc **prefp;
    struct kinfo_proc *pp;
    struct kinfo_proc *prev_pp = NULL;
    struct save_proc *savep;
    long proc_io;
    pid_t pid;
    size_t size;
    int nproc;

    /* these are copied out of sel for speed */
    int show_idle;
    int show_self;
    int show_system;
    int show_uid;
    char *show_command;

    /* get proc table size and give it a boost */
    nproc = (int)get_sysctlsize(K_PROC) / sizeof(struct kinfo_proc);
    nproc += nproc >> 4;
    size = nproc * sizeof(struct kinfo_proc);
    dprintf("get_process_info: nproc %d, psize %d, size %d\n", nproc, psize, size);

    /* make sure we have enough space allocated */
    if (nproc > psize)
    {
	/* reallocate both pbase and pref */
	pbase = (struct kinfo_proc *)realloc(pbase, size);
	pref  = (struct kinfo_proc **)realloc(pref,
		    sizeof(struct kinfo_proc *) * nproc);
	psize = nproc;
    }

    /* make sure we got the space we asked for */
    if (pref == NULL || pbase == NULL)
    {
	/* abandon all hope */
	message_error(" Out of memory!");
	nproc = psize = 0;
	si->p_total = 0;
	si->p_active = 0;
	return NULL;
    }

    /* get all process information (threads, too) */
    if (size > 0)
    {
	nproc = get_sysctl(K_PROC, pbase, size);
	if (nproc == -1)
	{
	    nproc = 0;
	}
	else
	{
	    nproc /= sizeof(struct kinfo_proc);
	}
    }

    /* get a pointer to the states summary array */
    si->procstates = process_states;

    /* set up flags which define what we are going to select */
    show_idle = sel->idle;
    show_self = 0;
    show_system = sel->system;
    show_uid = sel->uid != -1;
    show_fullcmd = sel->fullcmd;
    show_command = sel->command;
    show_usernames = sel->usernames;
    display_mode = sel->mode;
#ifdef HAS_SHOWTHREADS
    show_threads = sel->threads;
#endif

    /* count up process states and get pointers to interesting procs */
    total_procs = 0;
    active_procs = 0;
    total_io = 0;
    memset((char *)process_states, 0, sizeof(process_states));
    prefp = pref;
    for (pp = pbase, i = 0; i < nproc; pp++, i++)
    {
	/*
	 *  Place pointers to each valid proc structure in pref[].
	 *  Process slots that are actually in use have a non-zero
	 *  status field.  Processes with P_SYSTEM set are system
	 *  processes---these get ignored unless show_sysprocs is set.
	 */
	pid = PP(pp, pid);
	if (PP(pp, stat) != 0)
	{
#ifdef HAS_SHOWTHREADS
	    int is_thread;
	    lwpid_t tid;

	    /* get thread id */
	    tid = PP(pp, tid);

	    /* is this just a thread? */
	    is_thread = (prev_pp != NULL && PP(prev_pp, pid) == pid);

	    /* count this process and its state */
	    /* only count threads if we are showing them */
	    if (show_threads || !is_thread)
	    {
		total_procs++;
		process_states[(unsigned char) PP(pp, stat)]++;
	    }

	    /* grab old data from hash */
	    if ((savep = hash_lookup_lwpid(procs, tid)) != NULL)
	    {
		/* verify that this is not a new or different thread */
		/* (freebsd reuses thread ids fairly quickly) */
		/* pids must match and time can't have gone backwards */
		if (pid != savep->sp_pid || PP(pp, runtime) < savep->sp_runtime)
		{
		    /* not the same thread -- reuse the save_proc structure */
		    memset(savep, 0, sizeof(struct save_proc));
		    savep->sp_pid = pid;
		}
	    }
	    else
	    {
		/* havent seen this one before */
		savep = (struct save_proc *)calloc(1, sizeof(struct save_proc));
		savep->sp_pid = pid;
		hash_add_lwpid(procs, tid, savep);
	    }

#else /* !HAS_SHOWTHREADS */
	    total_procs++;
	    process_states[(unsigned char) PP(pp, stat)]++;

	    /* grab old data from hash */
	    if ((savep = hash_lookup_pid(procs, pid)) == NULL)
	    {
		/* havent seen this one before */
		savep = (struct save_proc *)calloc(1, sizeof(struct save_proc));
		savep->sp_pid = pid;
		hash_add_pid(procs, pid, savep);
	    }
#endif

	    /* save the pointer to the sp struct */
	    SPPTR(pp) = (void *)savep;

	    /* calculate %cpu */
	    PPCPU(pp) = ((PP(pp, runtime) - savep->sp_runtime) * 10000) /
		elapsed_time;
	    dprintf("%d (%d): runtime %lld, saved_pid %d, saved_runtime %lld, elapsed_time %d, ppcpu %d\n",
		    pid, PP(pp, tid), PP(pp, runtime), savep->sp_pid, savep->sp_runtime,
		    elapsed_time, PPCPU(pp));

	    /* calculate io differences */
	    proc_io = 0;
	    savep->sp_vcsw = (RP(pp, nvcsw) - savep->sp_old_nvcsw);
	    savep->sp_ivcsw = (RP(pp, nivcsw) - savep->sp_old_nivcsw);
	    proc_io += (savep->sp_inblock = (RP(pp, inblock) - savep->sp_old_inblock));
	    proc_io += (savep->sp_oublock = (RP(pp, oublock) - savep->sp_old_oublock));
	    proc_io += (savep->sp_majflt = (RP(pp, majflt) - savep->sp_old_majflt));
	    total_io += proc_io;
	    savep->sp_totalio = proc_io;

	    /* save data for next time */
	    savep->sp_runtime = PP(pp, runtime);
	    savep->sp_old_nvcsw = RP(pp, nvcsw);
	    savep->sp_old_nivcsw = RP(pp, nivcsw);
	    savep->sp_old_inblock = RP(pp, inblock);
	    savep->sp_old_oublock = RP(pp, oublock);
	    savep->sp_old_majflt = RP(pp, majflt);

	    /* is this one selected for viewing? */
	    if ((PP(pp, stat) != SZOMB) &&
		(show_system || ((PP(pp, flag) & P_SYSTEM) == 0)) &&
		(show_idle || (PP(pp, pctcpu) != 0) || 
		 (PP(pp, stat) == SRUN)) &&
		(!show_uid || PRUID(pp) == (uid_t)sel->uid) &&
		(show_command == NULL ||
		 strcasestr(PP(pp, comm), show_command) != NULL))
	    {
#ifdef HAS_SHOWTHREADS
		/* yes, but make sure it isn't just a thread */
		if (show_threads || !is_thread)
		{
		    /* we will be showing this thread */
		    *prefp++ = pp;
		    active_procs++;
		}
		else
		{
		    /* we will not be showing this thread, but we need to roll
		       up its cpu usage in to its process */
		    PP(prev_pp, pctcpu) += PP(pp, pctcpu);
		}
#else /* !HAS_SHOWTHREADS */
		/* we will be showing this process */
		*prefp++ = pp;
		active_procs++;
#endif
	    }
	    prev_pp = pp;
	}
    }

    dprintf("total_io: %d\n", total_io);
    if (total_io == 0) total_io = 1;

    /* if requested, sort the "interesting" processes */
    if (active_procs > 1)
    {
	qsort((char *)pref, active_procs, sizeof(struct kinfo_proc *),
	      proc_compares[compare_index]);
    }

    /* remember active and total counts */
    si->p_total = total_procs;
    si->p_active = pref_len = active_procs;

    /* pass back a handle */
    handle.next_proc = pref;
    handle.remaining = active_procs;
    return((caddr_t)&handle);
}

static char p_header[MAX_COLS];

char *
format_process_header(struct process_select *sel, caddr_t handle, int count)

{
    int cols;
    int n;
    int w;
    char *p;
    int *fi;
    struct kinfo_proc **kip;
    struct proc_field *fp;

    /* check for null handle */
    if (handle == NULL)
    {
	return("");
    }

    /* remember how many columns there are on the display */
    cols = display_columns();

    /* mode & threads dictate format */
    fi = display_fields =
	sel->mode == 0 ?
	(sel->threads == 0 ? mode0_display : mode0thr_display) :
	mode1_display;

    /* set username field correctly */
    if (!sel->usernames)
    {
	/* display uids */
	field_subst(fi, FIELD_USERNAME, FIELD_UID);
    }
    else
    {
	/* display usernames */
	field_subst(fi, FIELD_UID, FIELD_USERNAME);

	/* we also need to determine the longest username for column width */
	/* calculate namelength from first "count" processes */
	kip = ((struct handle *)handle)->next_proc;
	n = ((struct handle *)handle)->remaining;
	if (n > count)
	    n = count;
	namelength = 0;
	while (n-- > 0)
	{
	    w = strlen(username(PRUID(*kip)));
	    if (w > namelength) namelength = w;
	    kip++;
	}
	dprintf("format_process_header: namelength %d\n", namelength);

	/* place it in bounds */
	if (namelength < 8)
	{
	    namelength = 8;
	}

	/* set the column width */
	proc_field[FIELD_USERNAME].width = username_length = namelength;
    }

    /* walk thru fields and construct header */
    /* are we worried about overflow??? */
    p = p_header;
    while (*fi != -1)
    {
	fp = &(proc_field[*fi++]);
	if (fp->min_screenwidth <= cols)
	{
	    p += sprintf(p, fp->rjust ? "%*s" : "%-*s", fp->width, fp->name);
	    *p++ = ' ';
	}
    }
    *--p = '\0';

    return p_header;
}

static char fmt[MAX_COLS];		/* static area where result is built */

char *
format_next_process(caddr_t handle, char *(*get_userid)(int))

{
    struct kinfo_proc *pp;
    struct handle *hp;
    struct proc_field *fp;
    int *fi;
    int i;
    int cols;
    char *p;
    int len;
    int x;

    /* find and remember the next proc structure */
    hp = (struct handle *)handle;
    pp = *(hp->next_proc++);
    hp->remaining--;
    
    /* mode & threads dictate format */
    fi = display_fields;

    /* screen width is a consideration, too */
    cols = display_columns();

    /* build output by field */
    p = fmt;
    len = MAX_COLS;
    while ((i = *fi++) != -1)
    {
	fp = &(proc_field[i]);
	if (len > 0 && fp->min_screenwidth <= cols)
	{
	    x = (*(fp->format))(p, len, pp);
	    if (x >= len)
	    {
		dprintf("format_next_process: formatter overflow: x %d, len %d, p %08x => %08x, fmt %08x - %08x\n",
			x, len, p, p + len, fmt, fmt + sizeof(fmt));
		p += len;
		len = 0;
	    }
	    else
	    {
		p += x;
		*p++ = ' ';
		len -= x + 1;
	    }
	}
    }
    *--p = '\0';

    /* return the result */
    return(fmt);
}

/* comparison routines for qsort */

/*
 *  proc_compare - comparison function for "qsort"
 *	Compares the resource consumption of two processes using five
 *  	distinct keys.  The keys (in descending order of importance) are:
 *  	percent cpu, cpu ticks, state, resident set size, total virtual
 *  	memory usage.  The process states are ordered as follows (from least
 *  	to most important):  WAIT, zombie, sleep, stop, start, run.  The
 *  	array declaration below maps a process state index into a number
 *  	that reflects this ordering.
 */

static unsigned char sorted_state[] =
{
    0,	/* not used		*/
    3,	/* sleep		*/
    1,	/* ABANDONED (WAIT)	*/
    6,	/* run			*/
    5,	/* start		*/
    2,	/* zombie		*/
    4	/* stop			*/
};
 

#define ORDERKEY_PCTCPU \
  if (lresult = (long) PPCPU(p2) - (long) PPCPU(p1), \
     (result = lresult > 0 ? 1 : lresult < 0 ? -1 : 0) == 0)

#define ORDERKEY_CPTICKS \
  if ((result = PP(p2, runtime) > PP(p1, runtime) ? 1 : \
                PP(p2, runtime) < PP(p1, runtime) ? -1 : 0) == 0)

#define ORDERKEY_STATE \
  if ((result = sorted_state[(unsigned char) PP(p2, stat)] - \
                sorted_state[(unsigned char) PP(p1, stat)]) == 0)

#if OSMAJOR <= 4
#define ORDERKEY_PRIO \
  if ((result = PP(p2, priority) - PP(p1, priority)) == 0)
#else
#define ORDERKEY_PRIO \
  if ((result = PP(p2, pri.pri_level) - PP(p1, pri.pri_level)) == 0)
#endif

#define ORDERKEY_RSSIZE \
  if ((result = VP(p2, rssize) - VP(p1, rssize)) == 0) 

#define ORDERKEY_MEM \
  if ( (result = PROCSIZE(p2) - PROCSIZE(p1)) == 0 )

#define ORDERKEY_IO \
  if ( (result = SP(p2, totalio) - SP(p1, totalio)) == 0)

#define ORDERKEY_PID \
  if ( (result = PP(p1, pid) - PP(p2, pid)) == 0)

/* compare_cpu - the comparison function for sorting by cpu percentage */

int
proc_compare(struct proc **pp1, struct proc **pp2)

{
    struct kinfo_proc *p1;
    struct kinfo_proc *p2;
    int result;
    pctcpu lresult;

    /* remove one level of indirection */
    p1 = *(struct kinfo_proc **) pp1;
    p2 = *(struct kinfo_proc **) pp2;

    ORDERKEY_PCTCPU
    ORDERKEY_CPTICKS
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ;

    return(result);
}

/* compare_size - the comparison function for sorting by total memory usage */

int
compare_size(struct proc **pp1, struct proc **pp2)

{
    struct kinfo_proc *p1;
    struct kinfo_proc *p2;
    int result;
    pctcpu lresult;

    /* remove one level of indirection */
    p1 = *(struct kinfo_proc **) pp1;
    p2 = *(struct kinfo_proc **) pp2;

    ORDERKEY_MEM
    ORDERKEY_RSSIZE
    ORDERKEY_PCTCPU
    ORDERKEY_CPTICKS
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ;

    return(result);
}

/* compare_res - the comparison function for sorting by resident set size */

int
compare_res(struct proc **pp1, struct proc **pp2)

{
    struct kinfo_proc *p1;
    struct kinfo_proc *p2;
    int result;
    pctcpu lresult;

    /* remove one level of indirection */
    p1 = *(struct kinfo_proc **) pp1;
    p2 = *(struct kinfo_proc **) pp2;

    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ORDERKEY_PCTCPU
    ORDERKEY_CPTICKS
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ;

    return(result);
}

/* compare_time - the comparison function for sorting by total cpu time */

int
compare_time(struct proc **pp1, struct proc **pp2)

{
    struct kinfo_proc *p1;
    struct kinfo_proc *p2;
    int result;
    pctcpu lresult;
  
    /* remove one level of indirection */
    p1 = *(struct kinfo_proc **) pp1;
    p2 = *(struct kinfo_proc **) pp2;

    ORDERKEY_CPTICKS
    ORDERKEY_PCTCPU
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ;

      return(result);
  }
  
/* compare_prio - the comparison function for sorting by priority */

int
compare_prio(struct proc **pp1, struct proc **pp2)

{
    struct kinfo_proc *p1;
    struct kinfo_proc *p2;
    int result;
    pctcpu lresult;

    /* remove one level of indirection */
    p1 = *(struct kinfo_proc **) pp1;
    p2 = *(struct kinfo_proc **) pp2;

    ORDERKEY_PRIO
    ORDERKEY_CPTICKS
    ORDERKEY_PCTCPU
    ORDERKEY_STATE
    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ;

    return(result);
}

/* compare_io - the comparison function for sorting by io count */

int
compare_io(struct proc **pp1, struct proc **pp2)

{
    struct kinfo_proc *p1;
    struct kinfo_proc *p2;
    int result;
    pctcpu lresult;

    /* remove one level of indirection */
    p1 = *(struct kinfo_proc **) pp1;
    p2 = *(struct kinfo_proc **) pp2;

    ORDERKEY_IO
    ORDERKEY_PCTCPU
    ORDERKEY_CPTICKS
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ;

    return(result);
}

/* compare_pid - the comparison function for sorting by process id */

int
compare_pid(struct proc **pp1, struct proc **pp2)

{
    struct kinfo_proc *p1;
    struct kinfo_proc *p2;
    int result;

    /* remove one level of indirection */
    p1 = *(struct kinfo_proc **) pp1;
    p2 = *(struct kinfo_proc **) pp2;

    ORDERKEY_PID
    ;

    return(result);
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
proc_owner(int pid)

{
    int cnt;
    struct kinfo_proc **prefp;
    struct kinfo_proc *pp;

    prefp = pref;
    cnt = pref_len;
    while (--cnt >= 0)
    {
	pp = *prefp++;	
	if (PP(pp, pid) == (pid_t)pid)
	{
	    return((int)PRUID(pp));
	}
    }
    return(-1);
}

