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
 * SYNOPSIS:  any Sun running SunOS version 4.x
 *
 * DESCRIPTION:
 * This is the machine-dependent module for SunOS 4.x.
 * This makes top work on the following systems:
 *	SunOS 4.0
 *	SunOS 4.0.1
 *	SunOS 4.0.2 (including 386i architecture)
 *	SunOS 4.0.3
 *	SunOS 4.1
 *	SunOS 4.1.1
 *	SunOS 4.1.2 (including MP architectures)
 *	SunOS 4.1.3 (including MP architectures)
 *      SunOS 4.1.3_U1 (including MP architectures)
 *      SunOS 4.1.4 (including MP architectures)
 *	Solbourne OS/MP PRIOR to 4.1A
 *
 * LIBS:  -lkvm
 *
 * CFLAGS: -DHAVE_GETOPT -DORDER
 *
 * AUTHOR:  William LeFebvre <wnl@groupsys.com>
 * Solbourne support by David MacKenzie <djm@eng.umd.edu>
 */

/*
 * #ifdef MULTIPROCESSOR means Sun MP.
 * #ifdef solbourne is for Solbourne.
 */

#include "config.h"
#include <sys/types.h>
#include <sys/signal.h>

/* make sure param.h gets loaded with KERNEL defined to get PZERO & NZERO */
#define KERNEL
#include <sys/param.h>
#undef KERNEL

#include <stdio.h>
#include <kvm.h>
#include <nlist.h>
#include <math.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/dk.h>
#include <sys/vm.h>
#include <sys/file.h>
#include <sys/time.h>
#include <vm/page.h>

#ifdef solbourne
#include <sys/syscall.h>
#endif

/* Older versions of SunOS don't have a typedef for pid_t.
   Hopefully this will catch all those cases without causing other problems.
 */
#ifndef __sys_stdtypes_h
typedef int pid_t;
#endif

#include "top.h"
#include "machine.h"
#include "utils.h"

/* declarations for load_avg */
#include "loadavg.h"

/* get_process_info passes back a handle.  This is what it looks like: */

struct handle
{
    struct proc **next_proc;	/* points to next valid proc pointer */
    int remaining;		/* number of pointers remaining */
};

/* define what weighted cpu is.  */
#define weighted_cpu(pct, pp) ((pp)->p_time == 0 ? 0.0 : \
			 ((pct) / (1.0 - exp((pp)->p_time * logcpu))))

/* what we consider to be process size: */
#define PROCSIZE(pp) ((pp)->p_tsize + (pp)->p_dsize + (pp)->p_ssize)

/* definitions for indices in the nlist array */
#define X_AVENRUN	0
#define X_CCPU		1
#define X_MPID		2
#define X_NPROC		3
#define X_PROC		4
#define X_TOTAL		5
#define X_CP_TIME	6
#define X_PAGES		7
#define X_EPAGES	8

static struct nlist nlst[] = {
#ifdef i386
    { "avenrun" },		/* 0 */
    { "ccpu" },			/* 1 */
    { "mpid" },			/* 2 */
    { "nproc" },		/* 3 */
    { "proc" },			/* 4 */
    { "total" },		/* 5 */
    { "cp_time" },		/* 6 */
    { "pages" },		/* 7 */
    { "epages" },		/* 8 */
#else
    { "_avenrun" },		/* 0 */
    { "_ccpu" },		/* 1 */
    { "_mpid" },		/* 2 */
    { "_nproc" },		/* 3 */
    { "_proc" },		/* 4 */
    { "_total" },		/* 5 */
    { "_cp_time" },		/* 6 */
    { "_pages" },		/* 7 */
    { "_epages" },		/* 8 */
#ifdef MULTIPROCESSOR
    { "_ncpu" },
#define X_NCPU		9
    { "_xp_time" },
#define X_XP_TIME	10
#endif
#endif
    { 0 }
};

/*
 *  These definitions control the format of the per-process area
 */

static char header[] =
  "  PID X        PRI NICE  SIZE   RES STATE   TIME   WCPU    CPU COMMAND";
/* 0123456   -- field to fill in starts at header+6 */
#define UNAME_START 6

#define Proc_format \
	"%5d %-8.8s %3d %4d %5s %5s %-5s %-6s %5.2f%% %5.2f%% %s"


/* process state names for the "STATE" column of the display */
/* the extra nulls in the string "run" are for adding a slash and
   the processor number when needed */

char *state_abbrev[] =
{
    "", "sleep", "WAIT", "run\0\0\0", "start", "zomb", "stop"
};

/* values that we stash away in _init and use in later routines */

static double logcpu;
kvm_t *kd;

/* these are retrieved from the kernel in _init */

static unsigned long proc;
static          int  nproc;
static load_avg ccpu;
static unsigned long pages;
static unsigned long epages;
static          int  ncpu = 0;

/* these are offsets obtained via nlist and used in the get_ functions */

static unsigned long mpid_offset;
static unsigned long avenrun_offset;
static unsigned long total_offset;
static unsigned long cp_time_offset;
#ifdef MULTIPROCESSOR
static unsigned long xp_time_offset;
#endif

/* these are for calculating cpu state percentages */

static long cp_time[CPUSTATES];
static long cp_old[CPUSTATES];
static long cp_diff[CPUSTATES];
#ifdef MULTIPROCESSOR
static long xp_time[NCPU][XPSTATES];
/* for now we only accumulate spin time, but extending this to pick up
   other stuff in xp_time is trivial.  */
static long xp_old[NCPU];
#endif

/* these are for detailing the process states */

int process_states[7];
char *procstatenames[] = {
    "", " sleeping, ", " ABANDONED, ", " running, ", " starting, ",
    " zombie, ", " stopped, ",
    NULL
};

/* these are for detailing the cpu states */

int cpu_states[5];
char *cpustatenames[] = {
    "user", "nice", "system", "idle",
#ifdef MULTIPROCESSOR
    "spin",
#define XCP_SPIN 4
#endif
    NULL
};

/* these are for detailing the memory statistics */

long memory_stats[4];
char *memorynames[] = {
    "K available, ", "K in use, ", "K free, ", "K locked", NULL
};

/* these are names given to allowed sorting orders -- first is default */
char *ordernames[] = 
{"cpu", "size", "res", NULL};

/* forward definitions for comparison functions */
int compare_cpu();
int compare_size();
int compare_res();

int (*proc_compares[])() = {
    compare_cpu,
    compare_size,
    compare_res,
    NULL };


/* these are for keeping track of the proc array */

static int bytes;
static int pref_len;
static struct proc *pbase;
static struct proc **pref;

/* these are for getting the memory statistics */

static struct page *physpage;
static int bytesize;
static int count;
static int pageshift;		/* log base 2 of the pagesize */

/* define pagetok in terms of pageshift */

#define pagetok(size) ((size) << pageshift)

/* useful externals */
extern int errno;
extern char *sys_errlist[];

long lseek();
long time();

machine_init(statics)

struct statics *statics;

{
    register int i;
    register int pagesize;

    /* initialize the kernel interface */
    if ((kd = kvm_open(NULL, NULL, NULL, O_RDONLY, "top")) == NULL)
    {
	perror("kvm_open");
	return(-1);
    }

    /* get the list of symbols we want to access in the kernel */
    if ((i = kvm_nlist(kd, nlst)) < 0)
    {
	fprintf(stderr, "top: nlist failed\n");
	return(-1);
    }

#ifdef MULTIPROCESSOR
    /* were ncpu and xp_time not found in the nlist? */
    if (i > 0 && nlst[X_NCPU].n_type == 0 && nlst[X_XP_TIME].n_type == 0)
    {
	/* we were compiled on an MP system but we are not running on one */
	/* so we will pretend this didn't happen and set ncpu = 1 */
	i -= 2;
	ncpu = 1;
    }
#endif

#ifdef solbourne
    {
	unsigned int status, type;

	/* Get the number of CPUs on this system.  */
	syscall(SYS_getcpustatus, &status, &ncpu, &type);
    }
#endif

    /* make sure they were all found */
    if (i > 0 && check_nlist(nlst) > 0)
    {
	return(-1);
    }

    /* get the symbol values out of kmem */
    (void) getkval(nlst[X_PROC].n_value,   (int *)(&proc),	sizeof(proc),
	    nlst[X_PROC].n_name);
    (void) getkval(nlst[X_NPROC].n_value,  &nproc,		sizeof(nproc),
	    nlst[X_NPROC].n_name);
    (void) getkval(nlst[X_CCPU].n_value,   (int *)(&ccpu),	sizeof(ccpu),
	    nlst[X_CCPU].n_name);
    (void) getkval(nlst[X_PAGES].n_value,  (int *)(&pages),	sizeof(pages),
	    nlst[X_PAGES].n_name);
    (void) getkval(nlst[X_EPAGES].n_value, (int *)(&epages),	sizeof(epages),
	    nlst[X_EPAGES].n_name);
#ifdef MULTIPROCESSOR
    if (ncpu == 0)
    {
	/* if ncpu > 0 then we are not really on an MP system */
	(void) getkval(nlst[X_NCPU].n_value,   (int *)(&ncpu),	sizeof(ncpu),
		       nlst[X_NCPU].n_name);
    }
#endif

    /* stash away certain offsets for later use */
    mpid_offset = nlst[X_MPID].n_value;
    avenrun_offset = nlst[X_AVENRUN].n_value;
    total_offset = nlst[X_TOTAL].n_value;
    cp_time_offset = nlst[X_CP_TIME].n_value;
#ifdef MULTIPROCESSOR
    xp_time_offset = nlst[X_XP_TIME].n_value;
#endif

    /* this is used in calculating WCPU -- calculate it ahead of time */
    logcpu = log(loaddouble(ccpu));

    /* allocate space for proc structure array and array of pointers */
    bytes = nproc * sizeof(struct proc);
    pbase = (struct proc *)malloc(bytes);
    pref  = (struct proc **)malloc(nproc * sizeof(struct proc *));

    /* Just in case ... */
    if (pbase == (struct proc *)NULL || pref == (struct proc **)NULL)
    {
	fprintf(stderr, "top: can't allocate sufficient memory\n");
	return(-1);
    }

    /* allocate a table to hold all the page structs */
    bytesize = epages - pages;
    count = bytesize / sizeof(struct page);
    physpage = (struct page *)malloc(epages - pages);
    if (physpage == NULL)
    {
	fprintf(stderr, "top: can't allocate sufficient memory\n");
	return(-1);
    }
   
    /* get the page size with "getpagesize" and calculate pageshift from it */
    pagesize = getpagesize();
    pageshift = 0;
    while (pagesize > 1)
    {
	pageshift++;
	pagesize >>= 1;
    }

    /* we only need the amount of log(2)1024 for our conversion */
    pageshift -= LOG1024;

#if defined(MULTIPROCESSOR) || defined(solbourne)
    /* add a slash to the "run" state abbreviation */
    if (ncpu > 1)
    {
	state_abbrev[SRUN][3] = '/';
    }
#endif

    /* fill in the statics information */
    statics->procstate_names = procstatenames;
    statics->cpustate_names = cpustatenames;
    statics->memory_names = memorynames;
    statics->order_names = ordernames;

    /* all done! */
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

void
get_system_info(si)

struct system_info *si;

{
    load_avg avenrun[3];
    long total;
#ifdef MULTIPROCESSOR
    long half_total;
#endif

    /* get the cp_time array */
    (void) getkval(cp_time_offset, (int *)cp_time, sizeof(cp_time),
		   "_cp_time");

#ifdef MULTIPROCESSOR
    /* get the xp_time array as well */
    if (ncpu > 1)
    {
	(void) getkval(xp_time_offset, (int *)xp_time, sizeof(xp_time),
		       "_xp_time");
    }
#endif

    /* get load average array */
    (void) getkval(avenrun_offset, (int *)avenrun, sizeof(avenrun),
		   "_avenrun");

    /* get mpid -- process id of last process */
    (void) getkval(mpid_offset, &(si->last_pid), sizeof(si->last_pid),
		   "_mpid");

    /* get the array of physpage descriptors */
    (void) getkval(pages, (int *)physpage, bytesize, "array _page");

    /* convert load averages to doubles */
    {
	register int i;
	register double *infoloadp;
	register load_avg *sysloadp;

	infoloadp = si->load_avg;
	sysloadp = avenrun;
	for (i = 0; i < 3; i++)
	{
	    *infoloadp++ = loaddouble(*sysloadp++);
	}
    }

    /* convert cp_time counts to percentages */
    total = percentages(CPUSTATES, cpu_states, cp_time, cp_old, cp_diff);

#ifdef MULTIPROCESSOR
    /* calculate spin time from all processors */
    if (ncpu > 1)
    {
	register int c;
	register int i;
	register long sum;
	register long change;

	/* collect differences for each processor and add them */
	sum = 0;
	for (i = 0; i < ncpu; i++)
	{
	    c = xp_time[i][XP_SPIN];
	    change = c - xp_old[i];
	    if (change < 0)
	    {
		/* counter wrapped */
		change = (long)((unsigned long)c -
				(unsigned long)xp_old[i]);
	    }
	    sum += change;
	    xp_old[i] = c;
	}

	/*
	 *  NOTE:  I am assuming that the ticks found in xp_time are
	 *  already included in the ticks accumulated in cp_time.  To
	 *  get an accurate reflection, therefore, we have to subtract
	 *  the spin time from the system time and recompute those two
	 *  percentages.
	 */
	half_total = total / 2l;
	cp_diff[CP_SYS] -= sum;
	cpu_states[CP_SYS] = (int)((cp_diff[CP_SYS] * 1000 + half_total) /
				   total);
	cpu_states[XCP_SPIN] = (int)((sum * 1000 + half_total) / total);
    }
#endif

    /* sum memory statistics */
    {
	register struct page *pp;
	register int cnt;
	register int inuse;
	register int free;
	register int locked;

	/* bop thru the array counting page types */
	pp = physpage;
	inuse = free = locked = 0;
	for (cnt = count; --cnt >= 0; pp++)
	{
	    if (pp->p_free)
	    	free++;
	    else if (pp->p_lock || pp->p_keepcnt > 0)
	    	locked++;
	    else
	    	inuse++;
	}

	/* convert memory stats to Kbytes */
	memory_stats[0] = pagetok(inuse + free);
	memory_stats[1] = pagetok(inuse);
	memory_stats[2] = pagetok(free);
	memory_stats[3] = pagetok(locked);
    }

    /* set arrays and strings */
    si->cpustates = cpu_states;
    si->memory = memory_stats;
}

static struct handle handle;

caddr_t get_process_info(si, sel, compare_index)

struct system_info *si;
struct process_select *sel;
int compare_index;

{
    register int i;
    register int total_procs;
    register int active_procs;
    register struct proc **prefp;
    register struct proc *pp;

    /* these are copied out of sel for speed */
    int show_idle;
    int show_system;
    int show_uid;
    int show_command;

    /* read all the proc structures in one fell swoop */
    (void) getkval(proc, (int *)pbase, bytes, "proc array");

    /* get a pointer to the states summary array */
    si->procstates = process_states;

    /* set up flags which define what we are going to select */
    show_idle = sel->idle;
    show_system = sel->system;
    show_uid = sel->uid != -1;
    show_command = sel->command != NULL;

    /* count up process states and get pointers to interesting procs */
    total_procs = 0;
    active_procs = 0;
    bzero((char *)process_states, sizeof(process_states));
    prefp = pref;
    for (pp = pbase, i = 0; i < nproc; pp++, i++)
    {
	/*
	 *  Place pointers to each valid proc structure in pref[].
	 *  Process slots that are actually in use have a non-zero
	 *  status field.  Processes with SSYS set are system
	 *  processes---these get ignored unless show_sysprocs is set.
	 */
	if (pp->p_stat != 0 &&
	    (show_system || ((pp->p_flag & SSYS) == 0)))
	{
	    total_procs++;
	    process_states[pp->p_stat]++;
	    if ((pp->p_stat != SZOMB) &&
		(show_idle || (pp->p_pctcpu != 0) || (pp->p_stat == SRUN)) &&
		(!show_uid || pp->p_uid == (uid_t)sel->uid))
	    {
		*prefp++ = pp;
		active_procs++;
	    }
	}
    }

    /* if requested, sort the "interesting" processes */
    qsort((char *)pref, active_procs, sizeof(struct proc *),
	  proc_compares[compare_index]);

    /* remember active and total counts */
    si->p_total = total_procs;
    si->p_active = pref_len = active_procs;

    /* pass back a handle */
    handle.next_proc = pref;
    handle.remaining = active_procs;
    return((caddr_t)&handle);
}

char fmt[MAX_COLS];		/* static area where result is built */

char *format_next_process(handle, get_userid)

caddr_t handle;
char *(*get_userid)();

{
    register struct proc *pp;
    register long cputime;
    register double pct;
    struct user u;
    struct handle *hp;

    /* find and remember the next proc structure */
    hp = (struct handle *)handle;
    pp = *(hp->next_proc++);
    hp->remaining--;
    
    /* get the process's user struct and set cputime */
    if (getu(pp, &u) == -1)
    {
	(void) strcpy(u.u_comm, "<swapped>");
	cputime = 0;
    }
    else
    {
	/* set u_comm for system processes */
	if (u.u_comm[0] == '\0')
	{
	    if (pp->p_pid == 0)
	    {
		(void) strcpy(u.u_comm, "Swapper");
	    }
	    else if (pp->p_pid == 2)
	    {
		(void) strcpy(u.u_comm, "Pager");
	    }
	}

	cputime = u.u_ru.ru_utime.tv_sec + u.u_ru.ru_stime.tv_sec;
    }

    /* calculate the base for cpu percentages */
    pct = pctdouble(pp->p_pctcpu);

#ifdef MULTIPROCESSOR
    /*
     *  If there is more than one cpu then add the processor number to
     *  the "run/" string.  Note that this will only show up if the
     *  process is in the run state.  Also note:  when they
     *  start making Suns with more than 9 processors this will break
     *  since the string will then be more than 5 characters.
     */
    if (ncpu > 1)
    {
	state_abbrev[SRUN][4] = (pp->p_cpuid & 0xf) + '0';
    }
#endif
#ifdef solbourne
    if (ncpu > 1)
      {
	state_abbrev[SRUN][4] = (pp->p_lastcpu) + '0';
      }
#endif

    /* format this entry */
    sprintf(fmt,
	    Proc_format,
	    pp->p_pid,
	    (*get_userid)(pp->p_uid),
	    pp->p_pri - PZERO,
	    pp->p_nice - NZERO,
	    format_k(pagetok(PROCSIZE(pp))),
	    format_k(pagetok(pp->p_rssize)),
	    state_abbrev[pp->p_stat],
	    format_time(cputime),
	    100.0 * weighted_cpu(pct, pp),
	    100.0 * pct,
	    printable(u.u_comm));

    /* return the result */
    return(fmt);
}

/*
 *  getu(p, u) - get the user structure for the process whose proc structure
 *	is pointed to by p.  The user structure is put in the buffer pointed
 *	to by u.  Return 0 if successful, -1 on failure (such as the process
 *	being swapped out).
 */

getu(p, u)

register struct proc *p;
struct user *u;

{
    register struct user *lu;

    lu = kvm_getu(kd, p);
    if (lu == NULL)
    {
	return(-1);
    }
    else
    {
	*u = *lu;
	return(0);
    }
}

/*
 * check_nlist(nlst) - checks the nlist to see if any symbols were not
 *		found.  For every symbol that was not found, a one-line
 *		message is printed to stderr.  The routine returns the
 *		number of symbols NOT found.
 */

int check_nlist(nlst)

register struct nlist *nlst;

{
    register int i;

    /* check to see if we got ALL the symbols we requested */
    /* this will write one line to stderr for every symbol not found */

    i = 0;
    while (nlst->n_name != NULL)
    {
#ifdef i386
	if (nlst->n_value == 0)
#else
	if (nlst->n_type == 0)
#endif
	{
	    /* this one wasn't found */
	    fprintf(stderr, "kernel: no symbol named `%s'\n", nlst->n_name);
	    i = 1;
	}
	nlst++;
    }

    return(i);
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

getkval(offset, ptr, size, refstr)

unsigned long offset;
int *ptr;
int size;
char *refstr;

{
    if (kvm_read(kd, offset, ptr, size) != size)
    {
	if (*refstr == '!')
	{
	    return(0);
	}
	else
	{
	    fprintf(stderr, "top: kvm_read for %s: %s\n",
		refstr, sys_errlist[errno]);
	    quit(23);
	    /*NOTREACHED*/
	}
    }
    return(1);
}
    
/* comparison routines for qsort */

/*
 * There are currently four possible comparison routines.  main selects
 * one of these by indexing in to the array proc_compares.
 *
 * Possible keys are defined as macros below.  Currently these keys are
 * defined:  percent cpu, cpu ticks, process state, resident set size,
 * total virtual memory usage.  The process states are ordered as follows
 * (from least to most important):  WAIT, zombie, sleep, stop, start, run.
 * The array declaration below maps a process state index into a number
 * that reflects this ordering.
 */

/* First, the possible comparison keys.  These are defined in such a way
   that they can be merely listed in the source code to define the actual
   desired ordering.
 */

#define ORDERKEY_PCTCPU  if (lresult = p2->p_pctcpu - p1->p_pctcpu,\
			     (result = lresult > 0 ? 1 : lresult < 0 ? -1 : 0) == 0)
#define ORDERKEY_CPTICKS if ((result = p2->p_cpticks - p1->p_cpticks) == 0)
#define ORDERKEY_STATE   if ((result = sorted_state[p2->p_stat] - \
			      sorted_state[p1->p_stat])  == 0)
#define ORDERKEY_PRIO    if ((result = p2->p_pri - p1->p_pri) == 0)
#define ORDERKEY_RSSIZE  if ((result = p2->p_rssize - p1->p_rssize) == 0)
#define ORDERKEY_MEM     if ((result = PROCSIZE(p2) - PROCSIZE(p1)) == 0)

/* Now the array that maps process state to a weight */

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
 
/* compare_cpu - the comparison function for sorting by cpu percentage */

compare_cpu(pp1, pp2)

struct proc **pp1;
struct proc **pp2;

{
    register struct proc *p1;
    register struct proc *p2;
    register int result;
    register pctcpu lresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

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

compare_size(pp1, pp2)

struct proc **pp1;
struct proc **pp2;

{
    register struct proc *p1;
    register struct proc *p2;
    register int result;
    register pctcpu lresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

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

compare_res(pp1, pp2)

struct proc **pp1;
struct proc **pp2;

{
    register struct proc *p1;
    register struct proc *p2;
    register int result;
    register pctcpu lresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ORDERKEY_PCTCPU
    ORDERKEY_CPTICKS
    ORDERKEY_STATE
    ORDERKEY_PRIO
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

int proc_owner(pid)

int pid;

{
    register int cnt;
    register struct proc **prefp;
    register struct proc *pp;

    prefp = pref;
    cnt = pref_len;
    while (--cnt >= 0)
    {
	if ((pp = *prefp++)->p_pid == (pid_t)pid)
	{
	    return((int)pp->p_uid);
	}
    }
    return(-1);
}
