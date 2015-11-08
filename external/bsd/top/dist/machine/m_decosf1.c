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
 * SYNOPSIS:  OSF/1, Digital Unix 4.0, Compaq Tru64 5.0
 *
 * DESCRIPTION:
 * This is the machine-dependent module for DEC OSF/1 and its descendents
 * It is known to work on OSF/1 1.2, 1.3, 2.0-T3, 3.0, Digital Unix V4.0,
 * Digital Unix 5.0, and Tru64 5.0.
 * WARNING: if you use optimization with the standard "cc" compiler that
 * .        comes with V3.0 the resulting executable may core dump.  If
 * .        this happens, recompile without optimization.
 *
 * LIBS: -lmld -lmach
 *
 * CFLAGS: -DHAVE_GETOPT -DORDER
 *
 * AUTHOR:  Anthony Baxter, <anthony@aaii.oz.au>
 * Derived originally from m_ultrix, by David S. Comay <dsc@seismo.css.gov>, 
 * although by now there is hardly any of the code from m_ultrix left.
 * Helped a lot by having the source for syd(1), by Claus Kalle, and
 * from several people at DEC who helped with providing information on
 * some of the less-documented bits of the kernel interface.
 *
 * Modified: 31-Oct-94, Pat Welch, tpw@physics.orst.edu
 *	changed _mpid to pidtab for compatibility with OSF/1 version 3.0
 *
 * Modified: 13-Dec-94, William LeFebvre, lefebvre@dis.anl.gov
 *	removed used of pidtab (that was bogus) and changed things to
 *	automatically detect the absence of _mpid in the nlist and
 *	recover gracefully---this appears to be the only difference
 *	with 3.0.
 *
 * Modified: 3-Mar-00, Rainer Orth <ro@TechFak.Uni-Bielefeld.DE>
 *	added support for sort ordering.
 */
/* 
 * Theory of operation: 
 * 
 * Use Mach calls to build up a structure that contains all the sorts
 * of stuff normally found in a struct proc in a BSD system. Then
 * everything else uses this structure. This has major performance wins,
 * and also should work for future versions of the O/S.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/param.h>

#include <string.h>
#include <sys/user.h>
#include <stdio.h>
#include <nlist.h>
#include <math.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/dk.h>
#include <sys/vm.h>
#include <sys/file.h>
#include <sys/time.h>
/* #include <machine/pte.h> */
/* forward declarations, needed by <net/if.h> included from <sys/table.h> */
struct rtentry;
struct mbuf;
#include <sys/table.h>
#include <mach.h>
#include <mach/mach_types.h>
#include <mach/vm_statistics.h>
#include <sys/syscall.h> /* for SYS_setpriority, in setpriority(), below */


#include "top.h"
#include "machine.h"
#include "utils.h"

extern int errno, sys_nerr;
extern char *sys_errlist[];
#define strerror(e) (((e) >= 0 && (e) < sys_nerr) ? sys_errlist[(e)] : "Unknown error")

#define VMUNIX	"/vmunix"
#define KMEM	"/dev/kmem"
#define MEM	"/dev/mem"

/* get_process_info passes back a handle.  This is what it looks like: */

struct handle
{
    struct osf1_top_proc **next_proc;	/* points to next valid proc pointer */
    int remaining;		/* number of pointers remaining */
};

/* declarations for load_avg */
#include "loadavg.h"

/* definitions for indices in the nlist array */
#define X_MPID		0

static struct nlist nlst[] = {
    { "_mpid" },		/* 0 */
    { 0 }
};

/* Some versions of OSF/1 don't support reporting of the last PID.
   This flag indicates whether or not we are reporting the last PID. */
static int do_last_pid = 1;

/*
 *  These definitions control the format of the per-process area
 */

static char header[] =
  "   PID X        PRI NICE  SIZE   RES STATE   TIME    CPU COMMAND";
/* 01234567   -- field to fill in starts at header+7 */
#define UNAME_START 7

#define Proc_format \
	"%6d %-8.8s %3d %4d %5s %5s %-5s %-6s %5.2f%% %s"


/* process state names for the "STATE" column of the display */
/* the extra nulls in the string "run" are for adding a slash and
 * the processor number when needed. Although OSF/1 doesnt support
 * multiple processors yet, (and this module _certainly_ doesnt
 * support it, either, we may as well plan for the future. :-)
 */

char *state_abbrev[] =
{
    "", "run\0\0\0", "WAIT", "sleep", "sleep", "stop", "halt", "???", "zomb"
};


static int kmem, mem;

/* values that we stash away in _init and use in later routines */

static double logcpu;

/* these are retrieved from the kernel in _init */

static unsigned long proc;
static          int  nproc;
static load_avg  ccpu;

typedef long mtime_t;

/* these are offsets obtained via nlist and used in the get_ functions */

static unsigned long mpid_offset;

/* these are for detailing the process states */

int process_states[9];
char *procstatenames[] = {
    "", " running, ", " waiting, ", " sleeping, ", " idle, ",
    " stopped, ", " halted, ", "", " zombie",
    NULL
};

/* these are for detailing the cpu states */

int cpu_states[5];
char *cpustatenames[] = {
    "user", "nice", "system", "wio", "idle", NULL
};

long old_cpu_ticks[5];

/* these are for detailing the memory statistics */

long memory_stats[5];
char *memorynames[] = {
    "K active, ", "K inactive, ", "K total, ", "K free", NULL
};

long swap_stats[3];
char *swapnames[] = {
    "K in use, ", "K total", NULL
};

/* these are names given to allowed sorting orders -- first is default */
char *ordernames[] = {
    "cpu", "size", "res", "time", NULL
};

/* forward definitions for comparison functions */
int compare_cpu();
int compare_size();
int compare_res();
int compare_time();

int (*proc_compares[])() = {
    compare_cpu,
    compare_size,
    compare_res,
    compare_time,
    NULL
};

/* these are for getting the memory statistics */

static int pageshift;		/* log base 2 of the pagesize */

/* define pagetok in terms of pageshift */

#define pagetok(size) ((size) << pageshift)

/* take a process, make it a mach task, and grab all the info out */
void do_threads_calculations();

/*
 * Because I dont feel like repeatedly grunging through the kernel with
 * Mach calls, and I also dont want the horrid performance hit this
 * would give, I read the stuff I need out, and put in into my own
 * structure, for later use.
 */

struct osf1_top_proc {
    size_t p_mach_virt_size;
    char p_mach_state;
    int p_flag;
    fixpt_t p_mach_pct_cpu; /* aka p_pctcpu */
    int used_ticks;
    size_t process_size;
    pid_t p_pid;
    uid_t p_ruid;
    char p_pri;
    char p_nice;
    size_t p_rssize;
    char u_comm[PI_COMLEN + 1];
} ;

/* these are for keeping track of the proc array */

static int bytes;
static int pref_len;
static struct osf1_top_proc *pbase;
static struct osf1_top_proc **pref;

/* useful externals */
extern int errno;
extern char *sys_errlist[];

long percentages();

machine_init(statics)
struct statics *statics;
{
    register int i = 0;
    register int pagesize;
    struct tbl_sysinfo sibuf;

    if ((kmem = open(KMEM, O_RDONLY)) == -1) {
	perror(KMEM);
	return(-1);
    }
    if ((mem = open(MEM, O_RDONLY)) == -1) {
	perror(MEM);
	return(-1);
    }

    /* get the list of symbols we want to access in the kernel */
    if (nlist(VMUNIX, nlst) == -1)
    {
	perror("TOP(nlist)");
	return (-1);
    }

    if (nlst[X_MPID].n_type == 0)
    {
	/* this kernel has no _mpid, so go without */
	do_last_pid = 0;
    }
    else
    {
	/* stash away mpid pointer for later use */
	mpid_offset = nlst[X_MPID].n_value;
    }

    /* get the symbol values out of kmem */
    nproc  = table(TBL_PROCINFO, 0, (struct tbl_procinfo *)NULL, INT_MAX, 0);

    /* allocate space for proc structure array and array of pointers */
    bytes = nproc * sizeof(struct osf1_top_proc);
    pbase = (struct osf1_top_proc *)malloc(bytes);
    pref  = (struct osf1_top_proc **)malloc(nproc * 
                                              sizeof(struct osf1_top_proc *));

    /* Just in case ... */
    if (pbase == (struct osf1_top_proc *)NULL || 
                                  pref == (struct osf1_top_proc **)NULL)
    {
	fprintf(stderr, "top: cannot allocate sufficient memory\n");
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

    /* fill in the statics information */
    statics->procstate_names = procstatenames;
    statics->cpustate_names = cpustatenames;
    statics->memory_names = memorynames;
    statics->order_names = ordernames;
    statics->swap_names = swapnames;

    /* initialise this, for calculating cpu time */
    if (table(TBL_SYSINFO,0,&sibuf,1,sizeof(struct tbl_sysinfo))<0) {
	perror("TBL_SYSINFO");
	return(-1);
    }
    old_cpu_ticks[0] = sibuf.si_user;
    old_cpu_ticks[1] = sibuf.si_nice;
    old_cpu_ticks[2] = sibuf.si_sys;
    old_cpu_ticks[3] = sibuf.wait;
    old_cpu_ticks[4] = sibuf.si_idle;

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

void get_system_info(si)
struct system_info *si;
{
    struct tbl_loadavg labuf;
    struct tbl_sysinfo sibuf;
    struct tbl_swapinfo swbuf;
    vm_statistics_data_t vmstats;
    int swap_pages=0,swap_free=0,i;
    long new_ticks[5],diff_ticks[5];
    long delta_ticks;

    if (do_last_pid)
    {
	/* last pid assigned */
	(void) getkval(mpid_offset, &(si->last_pid), sizeof(si->last_pid), 
		       "_mpid");
    }
    else
    {
	si->last_pid = -1;
    }

    /* get load averages */
    if (table(TBL_LOADAVG,0,&labuf,1,sizeof(struct tbl_loadavg))<0) {
	perror("TBL_LOADAVG");
	return;
    }
    if (labuf.tl_lscale)   /* scaled */
	for(i=0;i<3;i++) 
	    si->load_avg[i] = ((double)labuf.tl_avenrun.l[i] / 
                                            (double)labuf.tl_lscale );
    else                   /* not scaled */
	for(i=0;i<3;i++) 
	    si->load_avg[i] = labuf.tl_avenrun.d[i];

    /* array of cpu state counters */
    if (table(TBL_SYSINFO,0,&sibuf,1,sizeof(struct tbl_sysinfo))<0) {
	perror("TBL_SYSINFO");
	return;
    }
    new_ticks[0] = sibuf.si_user ; new_ticks[1] = sibuf.si_nice;
    new_ticks[2] = sibuf.si_sys  ; new_ticks[3] = sibuf.wait;
    new_ticks[4] = sibuf.si_idle;
    delta_ticks=0;
    for(i=0;i<5;i++) {
	diff_ticks[i] = new_ticks[i] - old_cpu_ticks[i];
	delta_ticks += diff_ticks[i];
	old_cpu_ticks[i] = new_ticks[i];
    }
    si->cpustates = cpu_states;
    if(delta_ticks)
	for(i=0;i<5;i++) 
	    si->cpustates[i] = (int)( ( (double)diff_ticks[i] / 
                                           (double)delta_ticks ) * 1000 );
    
    /* memory information */
    /* this is possibly bogus - we work out total # pages by */
    /* adding up the free, active, inactive, wired down, and */
    /* zero filled. Anyone who knows a better way, TELL ME!  */
    /* Change: dont use zero filled. */
    (void) vm_statistics(task_self(),&vmstats);

    /* thanks DEC for the table() command. No thanks at all for   */
    /* omitting the man page for it from OSF/1 1.2, and failing   */
    /* to document SWAPINFO in the 1.3 man page. Lets hear it for */
    /* include files. */
    i=0;
    while(table(TBL_SWAPINFO,i,&swbuf,1,sizeof(struct tbl_swapinfo))>0) {
	swap_pages += swbuf.size;
	swap_free  += swbuf.free;
	i++;
    }
    memory_stats[0] = pagetok(vmstats.active_count);
    memory_stats[1] = pagetok(vmstats.inactive_count);
    memory_stats[2] = pagetok((vmstats.free_count + vmstats.active_count +
	vmstats.inactive_count + vmstats.wire_count));
    memory_stats[3] = pagetok(vmstats.free_count);
    swap_stats[0] = pagetok(swap_pages - swap_free);
    swap_stats[1] = pagetok(swap_pages);
    si->memory = memory_stats;
    si->swap = swap_stats;
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
    register struct osf1_top_proc **prefp;
    register struct osf1_top_proc *pp;
    struct tbl_procinfo p_i[8];
    int j,k,r;

    /* these are copied out of sel for speed */
    int show_idle;
    int show_uid;
    int show_command;

    /* get a pointer to the states summary array */
    si->procstates = process_states;

    /* set up flags which define what we are going to select */
    show_idle = sel->idle;
    show_uid = sel->uid != -1;
    show_command = sel->command != NULL;

    /* count up process states and get pointers to interesting procs */
    total_procs = 0;
    active_procs = 0;
    memset((char *)process_states, 0, sizeof(process_states));
    prefp = pref;
    pp=pbase;
    for (j=0; j<nproc; j += 8) 
    {
	r = table(TBL_PROCINFO, j, (struct tbl_procinfo *)p_i, 8, 
                                               sizeof(struct tbl_procinfo));
	for (k=0; k < r; k++ , pp++) 
	{
	    if(p_i[k].pi_pid == 0) 
	    {
		pp->p_pid = 0;
	    }
	    else
	    {
		pp->p_pid = p_i[k].pi_pid;
		pp->p_ruid = p_i[k].pi_ruid;
		pp->p_flag = p_i[k].pi_flag;
		pp->p_nice = getpriority(PRIO_PROCESS,p_i[k].pi_pid);
		/* Load useful values into the proc structure */
		do_threads_calculations(pp);
		/*
		 *  Place pointers to each valid proc structure in pref[].
		 *  Process slots that are actually in use have a non-zero
		 *  status field.  
		 */
#ifdef DEBUG
		/*
		 *  Emit debug info about all processes before selection.
		 */
		fprintf(stderr, "pid = %d ruid = %d comm = %s p_mach_state = %d p_stat = %d p_flag = 0x%x\n",
			pp->p_pid, pp->p_ruid, p_i[k].pi_comm,
			pp->p_mach_state, p_i[k].pi_status, pp->p_flag);
#endif
		if (pp->p_mach_state != 0)
		{
		    total_procs++;
		    process_states[pp->p_mach_state]++;
		    if ((pp->p_mach_state != 8) &&
			(show_idle || (pp->p_mach_pct_cpu != 0) || 
			 (pp->p_mach_state == 1)) &&
			(!show_uid || pp->p_ruid == (uid_t)sel->uid)) {
			*prefp++ = pp;
			active_procs++;
		    }
		}
	    }
	}
    }

    /* if requested, sort the "interesting" processes */
    if (proc_compares[compare_index] != NULL)
    {
	qsort((char *)pref, active_procs, sizeof(struct osf1_top_proc *), 
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

char fmt[MAX_COLS];		/* static area where result is built */

char *format_next_process(handle, get_userid)
caddr_t handle;
char *(*get_userid)();
{
    register struct osf1_top_proc *pp;
    register long cputime;
    register double pct;
    struct user u;
    struct handle *hp;

    /* find and remember the next proc structure */
    hp = (struct handle *)handle;
    pp = *(hp->next_proc++);
    hp->remaining--;

    /* get the process's user struct and set cputime */
    
    if (table(TBL_UAREA,pp->p_pid,&u,1,sizeof(struct user))<0) {
    /* whoops, it must have died between the read of the proc area
     * and now. Oh well, lets just dump some meaningless thing out
     * to keep the rest of the program happy
     */
	sprintf(fmt,
		Proc_format,
		pp->p_pid,
		(*get_userid)(pp->p_ruid),
		0,
		0,
		"",
		"",
		"dead",
		"",
		0.0,
		"<dead>");
	    return(fmt);
    }

    /* set u_comm for system processes */
    if (u.u_comm[0] == '\0')
    {
	if (pp->p_pid == 0)
	{
	    (void) strcpy(u.u_comm, "[idle]");
	}
	else if (pp->p_pid == 2)
	{
	    (void) strcpy(u.u_comm, "[execpt.hndlr]");
	}
    }

    /* Check if process is in core */
    if (!(pp->p_flag & SLOAD)) {
	/*
	 * Print swapped processes as <pname>
	 */
	char buf[sizeof(u.u_comm)];
	(void) strncpy(buf, u.u_comm, sizeof(u.u_comm));
	u.u_comm[0] = '<';
	(void) strncpy(&u.u_comm[1], buf, sizeof(u.u_comm) - 2);
	u.u_comm[sizeof(u.u_comm) - 2] = '\0';
	(void) strncat(u.u_comm, ">", sizeof(u.u_comm) - 1);
	u.u_comm[sizeof(u.u_comm) - 1] = '\0';
    }

    cputime = u.u_ru.ru_utime.tv_sec + u.u_ru.ru_stime.tv_sec;

    /* calculate the base for cpu percentages */
    pct = pctdouble(pp->p_mach_pct_cpu);

    /* format this entry */
    sprintf(fmt,
	    Proc_format,
	    pp->p_pid,
	    (*get_userid)(pp->p_ruid),
	    pp->p_pri,
	    pp->p_nice,
            format_k(pp->p_mach_virt_size/1024),
            format_k(pp->p_rssize/1000),
	    state_abbrev[pp->p_mach_state],
	    format_time(cputime),
	    100.0 * ((double)pp->p_mach_pct_cpu / 10000.0),
	    printable(u.u_comm));

    /* return the result */
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

getkval(offset, ptr, size, refstr)

unsigned long offset;
int *ptr;
int size;
char *refstr;

{
    if (lseek(kmem, (long)offset, L_SET) == -1) {
        if (*refstr == '!')
            refstr++;
        (void) fprintf(stderr, "%s: lseek to %s: %s\n", KMEM, 
		       refstr, strerror(errno));
        quit(23);
    }
    if (read(kmem, (char *) ptr, size) == -1) {
        if (*refstr == '!') 
            return(0);
        else {
            (void) fprintf(stderr, "%s: reading %s: %s\n", KMEM, 
			   refstr, strerror(errno));
            quit(23);
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
 * (from least to most important):  WAIT, zomb, ???, halt, idle, sleep,
 * stop, run.  The array declaration below maps a process state index into
 * a number that reflects this ordering.
 */

/* First, the possible comparison keys.  These are defined in such a way
   that they can be merely listed in the source code to define the actual
   desired ordering.
 */

#define ORDERKEY_PCTCPU  if (lresult = p2->p_mach_pct_cpu - p1->p_mach_pct_cpu,\
                           (result = lresult > 0 ? 1 : lresult < 0 ? -1 : 0) == 0)
#define ORDERKEY_CPTICKS if ((result = p2->used_ticks - p1->used_ticks) == 0)
#define ORDERKEY_STATE   if ((result = sorted_state[p2->p_mach_state] - \
                            sorted_state[p1->p_mach_state])  == 0)
#define ORDERKEY_PRIO    if ((result = p2->p_pri - p1->p_pri) == 0)
#define ORDERKEY_RSSIZE  if ((result = p2->p_rssize - p1->p_rssize) == 0)
#define ORDERKEY_MEM     if ((result = p2->p_mach_virt_size - p1->p_mach_virt_size) == 0)

/* Now the array that maps process state to a weight */

static unsigned char sorted_state[] =
{
   0, /*""*/
   8, /*"run"*/
   1, /*"WAIT"*/
   6, /*"sleep"*/
   5, /*"idle"*/
   7, /*"stop"*/
   4, /*"halt"*/
   3, /*"???"*/
   2, /*"zomb"*/
};
 
/* compare_cpu - the comparison function for sorting by cpu percentage */

compare_cpu(pp1, pp2)

struct osf1_top_proc **pp1;
struct osf1_top_proc **pp2;

{
    register struct osf1_top_proc *p1;
    register struct osf1_top_proc *p2;
    register long result;
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

struct osf1_top_proc **pp1;
struct osf1_top_proc **pp2;

{
    register struct osf1_top_proc *p1;
    register struct osf1_top_proc *p2;
    register long result;
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

struct osf1_top_proc **pp1;
struct osf1_top_proc **pp2;

{
    register struct osf1_top_proc *p1;
    register struct osf1_top_proc *p2;
    register long result;
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

/* compare_time - the comparison function for sorting by total cpu time */

compare_time(pp1, pp2)

struct osf1_top_proc **pp1;
struct osf1_top_proc **pp2;

{
    register struct osf1_top_proc *p1;
    register struct osf1_top_proc *p2;
    register long result;
    register pctcpu lresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    ORDERKEY_CPTICKS
    ORDERKEY_PCTCPU
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_RSSIZE
    ORDERKEY_MEM
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
    register struct osf1_top_proc **prefp;
    register struct osf1_top_proc *pp;

    prefp = pref;
    cnt = pref_len;
    while (--cnt >= 0)
    {
	if ((pp = *prefp++)->p_pid == (pid_t)pid)
	{
	    return((int)pp->p_ruid);
	}
    }
    return(-1);
}


/*
 * We use the Mach interface, as well as the table(UAREA,,,) call to
 * get some more information, then put it into unused fields in our
 * copy of the proc structure, to make it faster and easier to get at
 * later.
 */
void do_threads_calculations(thisproc)
struct osf1_top_proc *thisproc; 
{
  int j;
  task_t  thistask;
  task_basic_info_data_t   taskinfo;
  unsigned int taskinfo_l;
  thread_array_t    threadarr;
  unsigned int threadarr_l;
  thread_basic_info_t     threadinfo;
  thread_basic_info_data_t threadinfodata;
  unsigned int threadinfo_l;
  int task_tot_cpu=0;  /* total cpu usage of threads in a task */
  struct user u;

  thisproc->p_pri=0; 
  thisproc->p_rssize=0; 
  thisproc->p_mach_virt_size=0; 
  thisproc->p_mach_state=0; 
  thisproc->p_mach_pct_cpu=0;

  if(task_by_unix_pid(task_self(), thisproc->p_pid, &thistask) 
                                                != KERN_SUCCESS){
      thisproc->p_mach_state=8; /* (zombie) */
  } else {
    taskinfo_l=TASK_BASIC_INFO_COUNT;
    if(task_info(thistask, TASK_BASIC_INFO, (task_info_t) &taskinfo, 
                                      &taskinfo_l)
       != KERN_SUCCESS) {
      thisproc->p_mach_state=8; /* (zombie) */
    } else {
      int minim_state=99,mcurp=1000,mbasp=1000,mslpt=999;

      thisproc->p_rssize=taskinfo.resident_size;
      thisproc->p_mach_virt_size=taskinfo.virtual_size;

      if (task_threads(thistask, &threadarr, &threadarr_l) != KERN_SUCCESS)
	  return;
      threadinfo= &threadinfodata;
      for(j=0; j < threadarr_l; j++) {
	threadinfo_l=THREAD_BASIC_INFO_COUNT;
	if(thread_info(threadarr[j],THREAD_BASIC_INFO,
	       (thread_info_t) threadinfo, &threadinfo_l) == KERN_SUCCESS) {
	    
	  task_tot_cpu += threadinfo->cpu_usage;
	  if(minim_state>threadinfo->run_state) 
              minim_state=threadinfo->run_state;
	  if(mcurp>threadinfo->cur_priority) 
              mcurp=threadinfo->cur_priority;
	  if(mbasp>threadinfo->base_priority) 
              mbasp=threadinfo->base_priority;
	  if(mslpt>threadinfo->sleep_time) 
              mslpt=threadinfo->sleep_time;
	}
      }
      switch (minim_state) {
      case TH_STATE_RUNNING:      
	    thisproc->p_mach_state=1;  break;
      case TH_STATE_UNINTERRUPTIBLE: 
	    thisproc->p_mach_state=2; break;
      case TH_STATE_WAITING:      
	    thisproc->p_mach_state=(threadinfo->sleep_time > 20) ? 4 : 3; break;
      case TH_STATE_STOPPED:      
	    thisproc->p_mach_state=5; break;
      case TH_STATE_HALTED:       
	    thisproc->p_mach_state=6; break;
      default:                    
	    thisproc->p_mach_state=7; break;
      }

      thisproc->p_pri=mcurp;
      thisproc->p_mach_pct_cpu=(fixpt_t)(task_tot_cpu*10);
      vm_deallocate(task_self(),(vm_address_t)threadarr,threadarr_l);
    }
  }
  if (table(TBL_UAREA,thisproc->p_pid,&u,1,sizeof(struct user))>=0) {
    thisproc->used_ticks=(u.u_ru.ru_utime.tv_sec + u.u_ru.ru_stime.tv_sec);
    thisproc->process_size=u.u_tsize + u.u_dsize + u.u_ssize;
  }
}

/* The reason for this function is that the system call will let
 * someone lower their own processes priority (because top is setuid :-(
 * Yes, using syscall() is a hack, if you can come up with something 
 * better, then I'd be thrilled to hear it. I'm not holding my breath,
 * though.  
 *             Anthony.
 */
int setpriority(int dummy, int procnum, int niceval)
{

    int uid, curprio;

    uid=getuid();
    if ( (curprio=getpriority(PRIO_PROCESS,procnum) ) == -1) 
    {
	return(-1); /* errno goes back to renice_process() */
    }
    /* check for not-root - if so, dont allow users to decrease priority */
    else if ( uid && (niceval<curprio) )
    {
	errno=EACCES;
	return(-1);
    }
    return(syscall(SYS_setpriority,PRIO_PROCESS,procnum,niceval));
}

