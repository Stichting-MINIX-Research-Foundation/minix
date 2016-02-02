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
 * SYNOPSIS:  PowerPC running AIX 5.1 or higher
 *
 * DESCRIPTION:
 * This is the machine-dependent module for AIX 5.1 and higher (may work on
 * older releases too).  It is currently only tested on PowerPC
 * architectures.
 *
 * TERMCAP: -lcurses
 *
 * CFLAGS: -DORDER -DHAVE_GETOPT -DHAVE_STRERROR -DMAXPROCS=10240
 *
 * LIBS: -lperfstat
 *
 * AUTHOR:  Joep Vesseur <joep@fwi.uva.nl>
 *
 * PATCHES: Antoine Tabary <tabary@bruyeres.cea.fr>, Dan Nelson <dnelson@allantgroup.com>
 */

#define MAXPROCS 10240

#include "config.h"

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <nlist.h>
#include <procinfo.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/sysinfo.h> 
#include <sys/sysconfig.h>
#include <pwd.h>
#include <errno.h>
#include <libperfstat.h>
#include "top.h"
#include "machine.h"
#include "utils.h"


#define PROCRESS(p) (((p)->pi_trss + (p)->pi_drss)*4)
#define PROCSIZE(p) (((p)->pi_tsize/1024+(p)->pi_dvm)*4)
#define PROCTIME(pi) (pi->pi_ru.ru_utime.tv_sec + pi->pi_ru.ru_stime.tv_sec)

#ifdef OLD
/*
 * structure definition taken from 'monitor' by Jussi Maki (jmaki@hut.fi)
 */
struct vmker {
    uint n0,n1,n2,n3,n4,n5,n6,n7,n8;
    uint totalmem;
    uint badmem; /* this is used in RS/6000 model 220 */
    uint freemem;
    uint n12;
    uint numperm;   /* this seems to keep other than text and data segment 
                       usage; name taken from /usr/lpp/bos/samples/vmtune.c */
    uint totalvmem,freevmem;
    uint n15, n16, n17, n18, n19;
};

#define KMEM "/dev/kmem"

/* Indices in the nlist array */
#define X_AVENRUN       0
#define X_SYSINFO       1
#define X_VMKER         2
#define X_V             3

static struct nlist nlst[] = {
    { "avenrun", 0, 0, 0, 0, 0 }, /* 0 */
    { "sysinfo", 0, 0, 0, 0, 0 }, /* 1 */
    { "vmker",   0, 0, 0, 0, 0 }, /* 2 */
    { "v",       0, 0, 0, 0, 0 }, /* 3 */
    {  NULL, 0, 0, 0, 0, 0 }
};

#endif

/* get_process_info returns handle. definition is here */
struct handle
{
	struct procentry64 **next_proc;
	int remaining;
};

/*
 *  These definitions control the format of the per-process area
 */
static char header[] =
  "   PID X        PRI NICE   SIZE   RES STATE   TIME   WCPU    CPU COMMAND";
/* 0123456   -- field to fill in starts at header+6 */
#define UNAME_START 7

#define Proc_format \
	"%6d %-8.8s %3d %4d %5d%c %4d%c %-5s %6s %5.2f%% %5.2f%% %.14s%s"


/* these are for detailing the process states */
int process_states[9];
char *procstatenames[] = {
    " none, ", " sleeping, ", " state2, ", " runnable, ",
    " idle, ", " zombie, ", " stopped, ", " running, ", " swapped, ",
    NULL
};

/* these are for detailing the cpu states */
int cpu_states[CPU_NTIMES];
char *cpustatenames[] = {
    "idle", "user", "kernel", "wait",
    NULL
};

/* these are for detailing the memory statistics */
long memory_stats[7];
char *memorynames[] = {
    "K total, ", "K buf, ", "K sys, ", "K free", NULL
};
#define M_REAL		0
#define M_BUFFERS	1
#define M_SYSTEM	2
#define M_REALFREE	3

long swap_stats[3];
char *swapnames[] = {
    "K total, ", "K free", NULL
};
#define M_VIRTUAL 0
#define M_VIRTFREE 1

char *state_abbrev[] = {
    NULL, NULL, NULL, NULL, "idle", "zomb", "stop", "run", "swap"
};

/* sorting orders. first is default */
char *ordernames[] = {
    "cpu", "size", "res", "time", "pri", NULL
};

/* compare routines */
int compare_cpu(), compare_size(), compare_res(), compare_time(), 
    compare_prio();

int (*proc_compares[])() = {
    compare_cpu,
    compare_size,
    compare_res,
    compare_time,
    compare_prio,
    NULL
};

/* useful externals */
long percentages(int cnt, int *out, long *new, long *old, long *diffs);
char *format_time(long seconds);

#ifdef OLD
/* useful globals */
int kmem;			/* file descriptor */

/* offsets in kernel */
static unsigned long avenrun_offset;
static unsigned long sysinfo_offset;
static unsigned long vmker_offset;
static unsigned long v_offset;
#endif

/* used for calculating cpu state percentages */
static long cp_time[CPU_NTIMES];
static long cp_old[CPU_NTIMES];
static long cp_diff[CPU_NTIMES];

/* the runqueue length is a cumulative value. keep old value */
long old_runque;

/* process info */
struct kernvars v_info;		/* to determine nprocs */
int nprocs;			/* maximum nr of procs in proctab */
int ncpus;			/* nr of cpus installed */

struct procentry64 *p_info;	/* needed for vm and ru info */
struct procentry64 **pref;	/* processes selected for display */
struct timeval64 *cpu_proc, *old_cpu_proc; /* total cpu used by each process */
int pref_len;			/* number of processes selected */

/* needed to calculate WCPU */
unsigned long curtime;

/* needed to calculate CPU */
struct timeval curtimeval;
struct timeval lasttimeval;

#ifdef OLD
int getkval(unsigned long offset, caddr_t ptr, int size, char *refstr);
#endif

void *xmalloc(long size)
{
	void *p = malloc(size);
	if (!p)
	{
		fprintf(stderr,"Could not allocate %ld bytes: %s\n", size, strerror(errno));
		exit(1);
	}
	return p;
}

/*
 * Initialize globals, get kernel offsets and stuff...
 */
int machine_init(statics)
    struct statics *statics;
{
#ifdef OLD
    if ((kmem = open(KMEM, O_RDONLY)) == -1) {
	perror(KMEM);
	return -1;
    }

    /* get kernel symbol offsets */
    if (knlist(nlst, 4, sizeof(struct nlist)) != 0) {
	perror("knlist");
	return -1;
    }
    avenrun_offset = nlst[X_AVENRUN].n_value;
    sysinfo_offset = nlst[X_SYSINFO].n_value;
    vmker_offset   = nlst[X_VMKER].n_value;
    v_offset       = nlst[X_V].n_value;

    getkval(v_offset, (caddr_t)&v_info, sizeof v_info, "v"); 
#else
	sysconfig(SYS_GETPARMS, &v_info, sizeof v_info);
#endif
    ncpus = v_info.v_ncpus;	/* number of cpus */

/* procentry64 is 4912 bytes, and PROCMASK(PIDMAX) is 262144.  That'd
   require 1.2gb for the p_info array, which is way overkill.  Raise
   MAXPROCS if you have more than 10240 active processes in the system.
*/

#if 0
    nprocs = PROCMASK(PIDMAX);
#else
    nprocs = MAXPROCS;
#endif

    cpu_proc = (struct timeval64 *)xmalloc(PROCMASK(PIDMAX) * sizeof (struct timeval64));
    old_cpu_proc = (struct timeval64 *)xmalloc(PROCMASK(PIDMAX) * sizeof (struct timeval64));
    p_info = (struct procentry64 *)xmalloc(nprocs * sizeof (struct procentry64));
    pref = (struct procentry64 **)xmalloc(nprocs * sizeof (struct procentry64 *));

    statics->procstate_names = procstatenames;
    statics->cpustate_names = cpustatenames;
    statics->memory_names = memorynames;
    statics->swap_names = swapnames;
    statics->order_names = ordernames;

    return(0);
}

char *format_header(uname_field)
    register char *uname_field;
{
    register char *ptr;

    ptr = header + UNAME_START;
    while (*uname_field != '\0')
    {
	*ptr++ = *uname_field++;
    }

    return(header);
}




void get_system_info(si)
    struct system_info *si;
{
#ifdef OLD
    long long load_avg[3];
    struct sysinfo64 s_info;
    struct vmker m_info;
#else
    perfstat_memory_total_t m_info1;
    perfstat_cpu_total_t s_info1;
#endif
    int i;
    int total = 0;

#ifdef OLD
    /* get the load avarage array */
    getkval(avenrun_offset, (caddr_t)load_avg, sizeof load_avg, "avenrun");

    /* get the sysinfo structure */
    getkval(sysinfo_offset, (caddr_t)&s_info, sizeof s_info, "sysinfo64");

    /* get vmker structure */
    getkval(vmker_offset, (caddr_t)&m_info, sizeof m_info, "vmker");
#else
    /* cpu stats */
    perfstat_cpu_total(NULL, &s_info1, sizeof s_info1, 1);

    /* memory stats */
    perfstat_memory_total(NULL, &m_info1, sizeof m_info1, 1);
#endif


#ifdef OLD
    /* convert load avarages to doubles */
    for (i = 0; i < 3; i++)
	si->load_avg[i] = (double)load_avg[i]/65536.0;

    /* calculate cpu state in percentages */
    for (i = 0; i < CPU_NTIMES; i++) {
	cp_old[i] = cp_time[i];
	cp_time[i] = s_info.cpu[i];
	cp_diff[i] = cp_time[i] - cp_old[i];
	total += cp_diff[i];
    }

#else
    /* convert load avarages to doubles */
    for (i = 0; i < 3; i++)
        si->load_avg[i] = (double)s_info1.loadavg[i]/(1<<SBITS);

    /* calculate cpu state in percentages */
    for (i = 0; i < CPU_NTIMES; i++) {
	cp_old[i] = cp_time[i];
	cp_time[i] = (	i==CPU_IDLE?s_info1.idle:
			i==CPU_USER?s_info1.user:
			i==CPU_KERNEL?s_info1.sys:
			i==CPU_WAIT?s_info1.wait:0);
	cp_diff[i] = cp_time[i] - cp_old[i];
	total += cp_diff[i];
    }
#endif
    for (i = 0; i < CPU_NTIMES; i++) {
        cpu_states[i] = 1000 * cp_diff[i] / total; 
    }

    /* calculate memory statistics, scale 4K pages */
#ifdef OLD
#define PAGE_TO_MB(a) ((a)*4/1024)
    memory_stats[M_TOTAL]    = PAGE_TO_MB(m_info.totalmem+m_info.totalvmem);
    memory_stats[M_REAL]     = PAGE_TO_MB(m_info.totalmem);
    memory_stats[M_REALFREE] = PAGE_TO_MB(m_info.freemem);
    memory_stats[M_BUFFERS]  = PAGE_TO_MB(m_info.numperm);
    swap_stats[M_VIRTUAL]  = PAGE_TO_MB(m_info.totalvmem);
    swap_stats[M_VIRTFREE] = PAGE_TO_MB(m_info.freevmem);
#else
#define PAGE_TO_KB(a) ((a)*4)
    memory_stats[M_REAL] = PAGE_TO_KB(m_info1.real_total);
    memory_stats[M_BUFFERS] = PAGE_TO_KB(m_info1.numperm);
#ifdef _AIXVERSION_520
    memory_stats[M_SYSTEM] = PAGE_TO_KB(m_info1.real_system);
#endif
    memory_stats[M_REALFREE] = PAGE_TO_KB(m_info1.real_free);
    swap_stats[M_VIRTUAL] = PAGE_TO_KB(m_info1.pgsp_total);
    swap_stats[M_VIRTFREE] = PAGE_TO_KB(m_info1.pgsp_free);
#endif

    /* runnable processes */
#ifdef OLD
    process_states[0] = s_info.runque - old_runque;
    old_runque = s_info.runque;
#else
    process_states[0] = s_info1.runque - old_runque;
    old_runque = s_info1.runque;
#endif

    si->cpustates = cpu_states;
    si->memory = memory_stats;
    si->swap = swap_stats;
}

static struct handle handle;

caddr_t get_process_info(si, sel, compare_index)
    struct system_info *si;
    struct process_select *sel;
    int compare_index;
{
    int i, nproc;
    int active_procs = 0, total_procs = 0;
    struct procentry64 *pp, **p_pref = pref;
    struct timeval64 *cpu_proc_temp;
    double timediff;
    pid_t procsindex = 0;

    si->procstates = process_states;

    curtime = time(0);
    lasttimeval = curtimeval;
    gettimeofday(&curtimeval, NULL);

    /* get the procentry64 structures of all running processes */
    nproc = getprocs64(p_info, sizeof (struct procentry64), NULL, 0, 
                       &procsindex, nprocs);
    if (nproc < 0) {
	perror("getprocs64");
	quit(1);
    }

    /* the swapper has no cmd-line attached */
    strcpy(p_info[0].pi_comm, "swapper");

    if (lasttimeval.tv_sec)
    {
        timediff = (curtimeval.tv_sec - lasttimeval.tv_sec) +
                   1.0*(curtimeval.tv_usec - lasttimeval.tv_usec) / uS_PER_SECOND;
    }

    /* The pi_cpu value is wildly inaccurate.  The maximum value is 120, but
       when the scheduling timer fires, the field is zeroed for all
       processes and ramps up over a short period of time.  Instead of using
       this weird number, manually calculate an accurate value from the
       rusage data.  Store this run's rusage in cpu_proc[pid], and subtract
       from old_cpu_proc.
    */
    for (pp = p_info, i = 0; i < nproc; pp++, i++) {
        pid_t pid = PROCMASK(pp->pi_pid);
        
        /* total system and user time into cpu_proc */
        cpu_proc[pid] = pp->pi_ru.ru_utime;
        cpu_proc[pid].tv_sec += pp->pi_ru.ru_stime.tv_sec;
        cpu_proc[pid].tv_usec += pp->pi_ru.ru_stime.tv_usec;
        if (cpu_proc[pid].tv_usec > NS_PER_SEC) {
            cpu_proc[pid].tv_sec++;
            cpu_proc[pid].tv_usec -= NS_PER_SEC;
        }

        /* If this process was around during the previous update, calculate
           a true %CPU.  If not, convert the kernel's cpu value from its
           120-max value to a 10000-max one.
        */ 
        if (old_cpu_proc[pid].tv_sec == 0 && old_cpu_proc[pid].tv_usec == 0)
            pp->pi_cpu = pp->pi_cpu * 10000 / 120;
        else
            pp->pi_cpu = ((cpu_proc[pid].tv_sec - old_cpu_proc[pid].tv_sec) +
                         1.0*(cpu_proc[pid].tv_usec - old_cpu_proc[pid].tv_usec) / NS_PER_SEC) / timediff * 10000;
    }
    
    /* remember our current values as old_cpu_proc, and zero out cpu_proc
       for the next update cycle */
    memset(old_cpu_proc, 0, sizeof(struct timeval64) * nprocs);
    cpu_proc_temp = cpu_proc;
    cpu_proc = old_cpu_proc;
    old_cpu_proc = cpu_proc_temp;

    memset(process_states, 0, sizeof process_states);

    /* build a list of pointers to processes to show. */
    for (pp = p_info, i = 0; i < nproc; pp++, i++) {

	/* AIX marks all runnable processes as ACTIVE. We want to know
	   which processes are sleeping, so check used cpu and adjust status
	   field accordingly
	 */
	if (pp->pi_state == SACTIVE && pp->pi_cpu == 0)
	    pp->pi_state = SIDL;

        if (pp->pi_state && (sel->system || ((pp->pi_flags & SKPROC) == 0))) {
	    total_procs++;
	    process_states[pp->pi_state]++;
	    if ( (pp->pi_state != SZOMB) &&
		(sel->idle || pp->pi_cpu != 0 || (pp->pi_state == SACTIVE))
		&& (sel->uid == -1 || pp->pi_uid == (uid_t)sel->uid)) {
                *p_pref++ = pp;
		active_procs++;
	    }
	}
    }   

    /* the pref array now holds pointers to the procentry64 structures in
     * the p_info array that were selected for display
     */

    /* sort if requested */
    if ( proc_compares[compare_index] != NULL)
	qsort((char *)pref, active_procs, sizeof (struct procentry64 *), 
	      proc_compares[compare_index]);
    
    si->last_pid = -1;		/* no way to figure out last used pid */
    si->p_total = total_procs;
    si->p_active = pref_len = active_procs;

    handle.next_proc = pref;
    handle.remaining = active_procs;

    return((caddr_t)&handle);
}

char fmt[128];		/* static area where result is built */

/* define what weighted cpu is. use definition of %CPU from 'man ps(1)' */
#define weighted_cpu(pp) (PROCTIME(pp) == 0 ? 0.0 : \
                        (((PROCTIME(pp)*100.0)/(curtime-pi->pi_start))))

char *format_next_process(handle, get_userid)
    caddr_t handle;
    char *(*get_userid)();
{
    register struct handle *hp;
    register struct procentry64 *pi;
    long cpu_time;
    int proc_size, proc_ress;
    char size_unit = 'K';
    char ress_unit = 'K';

    hp = (struct handle *)handle;
    if (hp->remaining == 0) {	/* safe guard */
	fmt[0] = '\0';
	return fmt;
    }
    pi = *(hp->next_proc++);
    hp->remaining--;

    cpu_time = PROCTIME(pi);

    /* we disply sizes up to 10M in KiloBytes, beyond 10M in MegaBytes */
    if ((proc_size = (pi->pi_tsize/1024+pi->pi_dvm)*4) > 10240) {
	proc_size /= 1024;
	size_unit = 'M';
    }
    if ((proc_ress = (pi->pi_trss + pi->pi_drss)*4) > 10240) {
	proc_ress /= 1024;
	ress_unit = 'M';
    }

    sprintf(fmt, Proc_format ,
            pi->pi_pid,					  /* PID */
            (*get_userid)(pi->pi_uid),			  /* login name */
	        pi->pi_nice,				  /* fixed or vari */
            getpriority(PRIO_PROCESS, pi->pi_pid),
            proc_size,					  /* size */
            size_unit,					  /* K or M */
            proc_ress,					  /* resident */
            ress_unit,					  /* K or M */
            state_abbrev[pi->pi_state],			  /* process state */
            format_time(cpu_time),			  /* time used */
            weighted_cpu(pi),	                          /* WCPU */
            pi->pi_cpu / 100.0,                     /* CPU */
            printable(pi->pi_comm),                       /* COMM */
            (pi->pi_flags & SKPROC) == 0 ? "" : " (sys)"  /* kernel process? */
	    );
    return(fmt);
}

#ifdef OLD
/*
 *  getkval(offset, ptr, size, refstr) - get a value out of the kernel.
 *	"offset" is the byte offset into the kernel for the desired value,
 *  	"ptr" points to a buffer into which the value is retrieved,
 *  	"size" is the size of the buffer (and the object to retrieve),
 *  	"refstr" is a reference string used when printing error meessages,
 *	    if "refstr" starts with a '!', then a failure on read will not
 *  	    be fatal (this may seem like a silly way to do things, but I
 *  	    really didn't want the overhead of another argument).
 *  	
 */
int getkval(offset, ptr, size, refstr)
    unsigned long offset;
    caddr_t ptr;
    int size;
    char *refstr;
{
    int upper_2gb = 0;

    /* reads above 2Gb are done by seeking to offset%2Gb, and supplying
     * 1 (opposed to 0) as fourth parameter to readx (see 'man kmem')
     */
    if (offset > 1<<31) {
	upper_2gb = 1;
	offset &= 0x7fffffff;
    }

    if (lseek(kmem, offset, SEEK_SET) != offset) {
	fprintf(stderr, "top: lseek failed\n");
	quit(2);
    }

    if (readx(kmem, ptr, size, upper_2gb) != size) {
	if (*refstr == '!')
	    return 0;
	else {
	    fprintf(stderr, "top: kvm_read for %s: %s\n", refstr,
		    sys_errlist[errno]);
	    quit(2);
	}
    }

    return 1 ;
}
#endif
    
/* comparison routine for qsort */
/*
 * The following code is taken from the solaris module and adjusted
 * for AIX -- JV .
 */

#define ORDERKEY_PCTCPU \
           if ((result = pi2->pi_cpu - pi1->pi_cpu) == 0)

#define ORDERKEY_CPTICKS \
           if ((result = PROCTIME(pi2) - PROCTIME(pi1)) == 0)

#define ORDERKEY_STATE \
           if ((result = sorted_state[pi2->pi_state]  \
                         - sorted_state[pi1->pi_state])  == 0)

/* Nice values directly reflect the process' priority, and are always >0 ;-) */
#define ORDERKEY_PRIO \
	   if ((result = pi1->pi_nice - pi2->pi_nice) == 0)
#define ORDERKEY_RSSIZE \
           if ((result = PROCRESS(pi2) - PROCRESS(pi1)) == 0)
#define ORDERKEY_MEM \
           if ((result = PROCSIZE(pi2) - PROCSIZE(pi1)) == 0)

static unsigned char sorted_state[] =
{
    0,	/* not used */
    0,
    0,
    0,
    3,	/* sleep */
    1,	/* zombie */
    4,	/* stop */
    6,	/* run */
    2,	/* swap */
};

/* compare_cpu - the comparison function for sorting by cpu percentage */

int
compare_cpu(ppi1, ppi2)
    struct procentry64 **ppi1;
    struct procentry64 **ppi2;
{
    register struct procentry64 *pi1 = *ppi1, *pi2 = *ppi2;
    register int result;

    ORDERKEY_PCTCPU
    ORDERKEY_CPTICKS
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ;

    return result;
}
    

/* compare_size - the comparison function for sorting by total memory usage */

int
compare_size(ppi1, ppi2)
    struct procentry64 **ppi1;
    struct procentry64 **ppi2;
{
    register struct procentry64 *pi1 = *ppi1, *pi2 = *ppi2;
    register int result;

    ORDERKEY_MEM
    ORDERKEY_RSSIZE
    ORDERKEY_PCTCPU
    ORDERKEY_CPTICKS
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ;

    return result;
}
    

/* compare_res - the comparison function for sorting by resident set size */

int
compare_res(ppi1, ppi2)
    struct procentry64 **ppi1;
    struct procentry64 **ppi2;
{
    register struct procentry64 *pi1 = *ppi1, *pi2 = *ppi2;
    register int result;

    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ORDERKEY_PCTCPU
    ORDERKEY_CPTICKS
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ;

    return result;
}
    

/* compare_time - the comparison function for sorting by total cpu time */

int
compare_time(ppi1, ppi2)
    struct procentry64 **ppi1;
    struct procentry64 **ppi2;
{
    register struct procentry64 *pi1 = *ppi1, *pi2 = *ppi2;
    register int result;

    ORDERKEY_CPTICKS
    ORDERKEY_PCTCPU
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_MEM
    ORDERKEY_RSSIZE
    ;

    return result;
}
    

/* compare_prio - the comparison function for sorting by cpu percentage */

int
compare_prio(ppi1, ppi2)
    struct procentry64 **ppi1;
    struct procentry64 **ppi2;
{
    register struct procentry64 *pi1 = *ppi1, *pi2 = *ppi2;
    register int result;

    ORDERKEY_PRIO
    ORDERKEY_PCTCPU
    ORDERKEY_CPTICKS
    ORDERKEY_STATE
    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ;

    return result;
}
    

int proc_owner(pid)
int pid;
{
   register struct procentry64 **prefp = pref;
   register int cnt = pref_len;

   while (--cnt >= 0) {
       if ((*prefp)->pi_pid == pid)
	   return (*prefp)->pi_uid;
       prefp++;
   }
   
   return(-1);
}
