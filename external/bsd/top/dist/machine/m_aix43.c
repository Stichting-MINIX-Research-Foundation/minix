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
 * SYNOPSIS:  PowerPC running AIX 4.2 or higher
 *
 * DESCRIPTION:
 * This is the machine-dependent module for AIX 4.2 and higher
 * It is currenlty only tested on PowerPC architectures.
 *
 * TERMCAP: -lcurses
 *
 * CFLAGS: -DORDER -DHAVE_GETOPT
 *
 * LIBS: -bD:0x18000000
 *
 * AUTHOR:  Joep Vesseur <joep@fwi.uva.nl>
 *
 * PATCHES: Antoine Tabary <tabary@bruyeres.cea.fr>
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <nlist.h>
#include <sys/sysinfo.h>
#include <procinfo.h>
#include <sys/proc.h>
#include <sys/times.h>
#include <sys/param.h>
#include <pwd.h>
#include "top.h"
#include "machine.h"
#include "utils.h"


#define PROCRESS(p) (((p)->pi_trss + (p)->pi_drss)*4)
#define PROCSIZE(p) (((p)->pi_tsize/1024+(p)->pi_dvm)*4)
#define PROCTIME(pi) (pi->pi_ru.ru_utime.tv_sec + pi->pi_ru.ru_stime.tv_sec)


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
#define X_PROC          3
#define X_V             4

static struct nlist nlst[] = {
    { "avenrun", 0, 0, 0, 0, 0 }, /* 0 */
    { "sysinfo", 0, 0, 0, 0, 0 }, /* 1 */
    { "vmker",   0, 0, 0, 0, 0 }, /* 2 */
    { "proc",    0, 0, 0, 0, 0 }, /* 3 */
    { "v",       0, 0, 0, 0, 0 }, /* 4 */
    {  NULL, 0, 0, 0, 0, 0 }
};


/* get_process_info returns handle. definition is here */
struct handle
{
	struct procsinfo **next_proc;
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
int cpu_states[4];
char *cpustatenames[] = {
    "idle", "user", "kernel", "wait",
    NULL
};

/* these are for detailing the memory statistics */
long memory_stats[4];
char *memorynames[] = {
    "K Total, ", "K Free, ", "K Buffers", NULL
};
#define M_REAL     0
#define M_REALFREE 1
#define M_BUFFERS  2

long swap_stats[3];
char *swapnames[] = {
    "K Total, ", "K Free", NULL
};

#define M_VIRTUAL  0
#define M_VIRTFREE 1

char *state_abbrev[] = {
    "", "sleep", "", "", "sleep", "zomb", "stop", "run", "swap"
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
extern int errno;
extern char *sys_errlist[];
long lseek();
long time();
long percentages();


/* useful globals */
int kmem;			/* file descriptor */

/* offsets in kernel */
static unsigned long avenrun_offset;
static unsigned long sysinfo_offset;
static unsigned long vmker_offset;
static unsigned long proc_offset;
static unsigned long v_offset;

/* used for calculating cpu state percentages */
static long cp_time[CPU_NTIMES];
static long cp_old[CPU_NTIMES];
static long cp_diff[CPU_NTIMES];

/* the runqueue length is a cumulative value. keep old value */
long old_runque;

/* process info */
struct var v_info;		/* to determine nprocs */
int nprocs;			/* maximum nr of procs in proctab */
int ncpus;			/* nr of cpus installed */

int ptsize;			/* size of process table in bytes */
struct proc *p_proc;		/* a copy of the process table */
struct procsinfo *p_info;	/* needed for vm and ru info */
struct procsinfo **pref;	/* processes selected for display */
int pref_len;			/* number of processes selected */

/* needed to calculate WCPU */
unsigned long curtime;


/*
 * Initialize globals, get kernel offsets and stuff...
 */
machine_init(struct statics *statics)

{
    time_t uptime, now;
    struct tms tbuf;

    if ((kmem = open(KMEM, O_RDONLY)) == -1) {
	perror(KMEM);
	return -1;
    }

    /* get kernel symbol offsets */
    if (knlist(nlst, 5, sizeof(struct nlist)) != 0) {
	perror("knlist");
	return -1;
    }
    avenrun_offset = nlst[X_AVENRUN].n_value;
    sysinfo_offset = nlst[X_SYSINFO].n_value;
    vmker_offset   = nlst[X_VMKER].n_value;
    proc_offset    = nlst[X_PROC].n_value;
    v_offset       = nlst[X_V].n_value;

    getkval(v_offset, (caddr_t)&v_info, sizeof v_info, "v");

    ncpus = v_info.v_ncpus;	/* number of cpus */
    nprocs = PROCMASK(PIDMAX);
    if (nprocs > 1024) nprocs = 1024;

    ptsize = nprocs * sizeof (struct proc);
    p_proc = (struct proc *)malloc(ptsize);
    p_info = (struct procsinfo *)malloc(nprocs * sizeof (struct procsinfo));
    pref = (struct procsinfo **)malloc(nprocs * sizeof (struct procsinfo *));

    if (!p_proc || !p_info || !pref) {
	fprintf(stderr, "top: not enough memory\n");
	return -1;
    }

    /* set boot time */
    now = time(NULL);
    uptime = times(&tbuf) / HZ;
    statics->boottime = now - uptime;

    statics->procstate_names = procstatenames;
    statics->cpustate_names = cpustatenames;
    statics->memory_names = memorynames;
    statics->order_names = ordernames;
    statics->swap_names = swapnames;

    return(0);
}



char *format_header(char *uname_field)

{
    register char *ptr;

    ptr = header + UNAME_START;
    while (*uname_field != '\0')
    {
	*ptr++ = *uname_field++;
    }

    return(header);
}



void
get_system_info(struct system_info *si)

{
    int load_avg[3];
    struct sysinfo s_info;
    struct vmker m_info;
    int i;
    double total = 0;

    /* get the load avarage array */
    getkval(avenrun_offset, (caddr_t)load_avg, sizeof load_avg, "avenrun");

    /* get the sysinfo structure */
    getkval(sysinfo_offset, (caddr_t)&s_info, sizeof s_info, "sysinfo");

    /* get vmker structure */
    getkval(vmker_offset, (caddr_t)&m_info, sizeof m_info, "vmker");

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

    total = total/1000.0;  /* top itself will correct this */
    for (i = 0; i < CPU_NTIMES; i++) {
        cpu_states[i] = cp_diff[i] / total;
    }

    /* calculate memory statistics, scale 4K pages to megabytes */
#define PAGE_TO_MB(a) ((a)*4/1024)
    memory_stats[M_REAL]     = PAGE_TO_MB(m_info.totalmem);
    memory_stats[M_REALFREE] = PAGE_TO_MB(m_info.freemem);
    memory_stats[M_BUFFERS]  = PAGE_TO_MB(m_info.numperm);
    swap_stats[M_VIRTUAL]  = PAGE_TO_MB(m_info.totalvmem);
    swap_stats[M_VIRTFREE] = PAGE_TO_MB(m_info.freevmem);

    /* runnable processes */
    process_states[0] = s_info.runque - old_runque;
    old_runque = s_info.runque;

    si->cpustates = cpu_states;
    si->memory = memory_stats;
    si->swap = swap_stats;
}

static struct handle handle;

caddr_t
get_process_info(struct system_info *si, struct process_select *sel, int compare_index)

{
    int i, nproc;
    int ptsize_util;
    int active_procs = 0, total_procs = 0;
    struct procsinfo *pp, **p_pref = pref;
    unsigned long pctcpu;
    pid_t procsindex = 0;
    struct proc *p;

    si->procstates = process_states;

    curtime = time(0);

    /* get the procsinfo structures of all running processes */
    nproc = getprocs(p_info, sizeof (struct procsinfo), NULL, 0, 
		     &procsindex, nprocs);
    if (nproc < 0) {
	perror("getprocs");
	quit(1);
    }

    /* the swapper has no cmd-line attached */
    strcpy(p_info[0].pi_comm, "swapper");
    
    /* get proc table */
    ptsize_util = (PROCMASK(p_info[nproc-1].pi_pid)+1) * sizeof(struct proc);
    getkval(proc_offset, (caddr_t)p_proc, ptsize_util, "proc");

    memset(process_states, 0, sizeof process_states);

    /* build a list of pointers to processes to show. walk through the
     * list of procsinfo structures instead of the proc table since the
     * mapping of procsinfo -> proctable is easy, the other way around
     * is cumbersome
     */
    for (pp = p_info, i = 0; i < nproc; pp++, i++) {

	p = &p_proc[PROCMASK(pp->pi_pid)];

	/* AIX marks all runnable processes as ACTIVE. We want to know
	   which processes are sleeping, so check used cpu ticks and adjust
	   status field accordingly
	 */
	if (p->p_stat == SACTIVE && p->p_cpticks == 0)
	    p->p_stat = SIDL;

        if (pp->pi_state && (sel->system || ((pp->pi_flags & SKPROC) == 0))) {
	    total_procs++;
	    process_states[p->p_stat]++;
	    if ( (pp->pi_state != SZOMB) &&
		(sel->idle || p->p_cpticks != 0 || (p->p_stat == SACTIVE))
		&& (sel->uid == -1 || pp->pi_uid == (uid_t)sel->uid)) {
                *p_pref++ = pp;
		active_procs++;
	    }
	}
    }   

    /* the pref array now holds pointers to the procsinfo structures in
     * the p_info array that were selected for display
     */

    /* sort if requested */
    if (si->p_active)
	qsort((char *)pref, active_procs, sizeof (struct procsinfo *), 
	      proc_compares[compare_index]);
    
    si->last_pid = -1;		/* no way to figure out last used pid */
    si->p_total = total_procs;
    si->p_active = pref_len = active_procs;

    handle.next_proc = pref;
    handle.remaining = active_procs;

    return((caddr_t)&handle);
}

char fmt[MAX_COLS];		/* static area where result is built */

/* define what weighted cpu is. use definition of %CPU from 'man ps(1)' */
#define weighted_cpu(pp) (PROCTIME(pp) == 0 ? 0.0 : \
                        (((PROCTIME(pp)*100.0)/(curtime-pi->pi_start)/ncpus)))
#define double_pctcpu(p) ((double)p->p_pctcpu/(double)FLT_MODULO)

char *
format_next_process(caddr_t handle, char *(*get_userid)())

{
    register struct handle *hp;
    register struct procsinfo *pi;
    register struct proc *p;
    char *uname;
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
    p = &p_proc[PROCMASK(pi->pi_pid)];

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
            getpriority(PRIO_PROCESS, pi->pi_pid),
	    EXTRACT_NICE(p),				  /* fixed or vari */
            proc_size,					  /* size */
            size_unit,					  /* K or M */
            proc_ress,					  /* resident */
            ress_unit,					  /* K or M */
            state_abbrev[p->p_stat],			  /* process state */
            format_time(cpu_time),			  /* time used */
	    weighted_cpu(pi),	                          /* WCPU */
	    100.0 * double_pctcpu(p),                     /* CPU */
            printable(pi->pi_comm),                       /* COMM */
	    (pi->pi_flags & SKPROC) == 0 ? "" : " (sys)"  /* kernel process? */
	    );
    return(fmt);
}


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

int
getkval(unsigned long offset, caddr_t ptr, int size, char *refstr)

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
    
/* comparison routine for qsort */
/*
 * The following code is taken from the solaris module and adjusted
 * for AIX -- JV .
 */

#define ORDERKEY_PCTCPU \
           if (lresult = p2->p_pctcpu - p1->p_pctcpu, \
               (result = lresult > 0 ? 1 : lresult < 0 ? -1 : 0) == 0)

#define ORDERKEY_CPTICKS \
           if ((result = PROCTIME(pi2) - PROCTIME(pi1)) == 0)


#define ORDERKEY_STATE \
           if ((result = sorted_state[p2->p_stat]  \
                         - sorted_state[p1->p_stat])  == 0)

/* Nice values directly reflect the process' priority, and are always >0 ;-) */
#define ORDERKEY_PRIO \
	   if ((result = EXTRACT_NICE(p1) - EXTRACT_NICE(p2)) == 0) 

#define ORDERKEY_RSSIZE \
           if ((result = PROCRESS(pi2) - PROCRESS(pi1)) == 0)
#define ORDERKEY_MEM \
           if ((result = PROCSIZE(pi2) - PROCSIZE(pi1)) == 0)

static unsigned char sorted_state[] =
{
    0, /* not used */
    0,
    0,
    0,
    3,                          /* sleep */
    1,				/* zombie */
    4,				/* stop */
    6,				/* run */
    2,				/* swap */
};

/* compare_cpu - the comparison function for sorting by cpu percentage */

int
compare_cpu(struct procsinfo **ppi1, struct procsinfo **ppi2)

{
    register struct procsinfo *pi1 = *ppi1, *pi2 = *ppi2;
    register struct proc *p1;
    register struct proc *p2;
    register int result;
    register long lresult;

    p1 = &p_proc[PROCMASK(pi1->pi_pid)];
    p2 = &p_proc[PROCMASK(pi2->pi_pid)];

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
compare_size(struct procsinfo **ppi1, struct procsinfo **ppi2)

{
    register struct procsinfo *pi1 = *ppi1, *pi2 = *ppi2;
    register struct proc *p1;
    register struct proc *p2;
    register int result;
    register long lresult;

    p1 = &p_proc[PROCMASK(pi1->pi_pid)];
    p2 = &p_proc[PROCMASK(pi2->pi_pid)];

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
compare_res(struct procsinfo **ppi1, struct procsinfo **ppi2)

{
    register struct procsinfo *pi1 = *ppi1, *pi2 = *ppi2;
    register struct proc *p1;
    register struct proc *p2;
    register int result;
    register long lresult;

    p1 = &p_proc[PROCMASK(pi1->pi_pid)];
    p2 = &p_proc[PROCMASK(pi2->pi_pid)];

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
compare_time(struct procsinfo **ppi1, struct procsinfo **ppi2)

{
    register struct procsinfo *pi1 = *ppi1, *pi2 = *ppi2;
    register struct proc *p1;
    register struct proc *p2;
    register int result;
    register long lresult;

    p1 = &p_proc[PROCMASK(pi1->pi_pid)];
    p2 = &p_proc[PROCMASK(pi2->pi_pid)];

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
compare_prio(struct procsinfo **ppi1, struct procsinfo **ppi2)

{
    register struct procsinfo *pi1 = *ppi1, *pi2 = *ppi2;
    register struct proc *p1;
    register struct proc *p2;
    register int result;
    register long lresult;

    p1 = &p_proc[PROCMASK(pi1->pi_pid)];
    p2 = &p_proc[PROCMASK(pi2->pi_pid)];

    ORDERKEY_PRIO
    ORDERKEY_PCTCPU
    ORDERKEY_CPTICKS
    ORDERKEY_STATE
    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ;

    return result;
}
    
int
proc_owner(int pid)

{
   int uid;
   register struct procsinfo **prefp = pref;
   register int cnt = pref_len;

   while (--cnt >= 0) {
       if ((*prefp)->pi_pid == pid)
	   return (*prefp)->pi_uid;
       prefp++;
   }
   
   return(-1);
}


