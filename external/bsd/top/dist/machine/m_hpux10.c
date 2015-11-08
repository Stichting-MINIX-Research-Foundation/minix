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
 * SYNOPSIS:  any hp9000 running hpux version 10.x
 *
 * DESCRIPTION:
 * This is the machine-dependent module for HPUX 10/11 that uses pstat.
 * It has been tested on HP/UX 10.01, 10.20, and 11.00.  It is presumed
 * to work also on 10.10.
 * Idle processes are marked by being either runnable or having a %CPU
 * of at least 0.1%.  This fraction is defined by CPU_IDLE_THRESH and
 * can be adjusted at compile time.
 *
 * CFLAGS: -DHAVE_GETOPT
 *
 * LIBS: 
 *
 * AUTHOR: John Haxby <john_haxby@hp.com>
 * AUTHOR: adapted from Rich Holland <holland@synopsys.com>
 * AUTHOR: adapted from Kevin Schmidt <kevin@mcl.ucsb.edu> 
 */

#include "config.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <nlist.h>
#include <fcntl.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/pstat.h>
#include <sys/dk.h>
#include <sys/stat.h>
#include <sys/dirent.h>

#include "top.h"
#include "machine.h"
#include "utils.h"

/*
 * The idle threshold (CPU_IDLE_THRESH) is an extension to the normal
 * idle process check.  Basically, we regard a process as idle if it is
 * both asleep and using less that CPU_IDLE_THRESH percent cpu time.  I
 * believe this makes the "i" option more useful, but if you don't, add
 * "-DCPU_IDLE_THRESH=0.0" to the CFLAGS.
 */
#ifndef CPU_IDLE_THRESH
#define CPU_IDLE_THRESH 0.1
#endif

# define P_RSSIZE(p) (p)->pst_rssize
# define P_TSIZE(p) (p)->pst_tsize
# define P_DSIZE(p) (p)->pst_dsize
# define P_SSIZE(p) (p)->pst_ssize

#define VMUNIX	"/stand/vmunix"
#define KMEM	"/dev/kmem"
#define MEM	"/dev/mem"
#ifdef DOSWAP
#define SWAP	"/dev/dmem"
#endif

/* what we consider to be process size: */
#define PROCSIZE(pp) (P_TSIZE(pp) + P_DSIZE(pp) + P_SSIZE(pp))

/* definitions for indices in the nlist array */
#define X_MPID		0

static struct nlist nlst[] = {
    { "mpid" },
    { 0 }
};

/*
 *  These definitions control the format of the per-process area
 */

static char header[] =
  "     TTY   PID X         PRI NICE  SIZE   RES STATE   TIME    CPU COMMAND";
/* 0123456789.12345 -- field to fill in starts at header+6 */
#define UNAME_START 15

#define Proc_format \
	"%8.8s %5d %-8.8s %4d %4d %5s %5s %-5s %6s %5.2f%% %s"

/* process state names for the "STATE" column of the display */

char *state_abbrev[] =
{
    "", "sleep", "run", "stop", "zomb", "trans", "start"
};


/* values that we stash away in _init and use in later routines */
static int kmem;
static struct pst_status *pst;

/* these are retrieved from the OS in _init */
static int nproc;
static int ncpu = 0;

/* these are offsets obtained via nlist and used in the get_ functions */
static unsigned long mpid_offset;

/* these are for calculating cpu state percentages */
static long cp_time[PST_MAX_CPUSTATES];
static long cp_old[PST_MAX_CPUSTATES];
static long cp_diff[PST_MAX_CPUSTATES];

/* these are for detailing the process states */
int process_states[7];
char *procstatenames[] = {
    "", " sleeping, ", " running, ", " stopped, ", " zombie, ",
    " trans, ", " starting, ",
    NULL
};

/* these are for detailing the cpu states */
int cpu_states[PST_MAX_CPUSTATES];
char *cpustatenames[] = {
    /* roll "swait" into "block" and "ssys" into "sys" */
    "usr", "nice", "sys", "idle", "", "block", "\0swait", "intr", "\0ssys",
    NULL
};

/* these are for detailing the memory statistics */
long memory_stats[8];
char *memorynames[] = {
    "Real: ", "K act, ", "K tot  ", "Virtual: ", "K act, ",
    "K tot, ", "K free", NULL
};

/* these are for getting the memory statistics */
static int pageshift;		/* log base 2 of the pagesize */

/* define pagetok in terms of pageshift */
#define pagetok(size) ((size) << pageshift)

/* Mapping TTY major/minor numbers is done through this structure */
struct ttymap {
    dev_t dev;
    char name [9];
};
static struct ttymap *ttynames = NULL;
static int nttys = 0;
static get_tty_names ();

/* comparison routine for qsort */

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
    6,	/* run			*/
    4,	/* stop			*/
    2,	/* zombie		*/
    5,	/* start		*/
    1,  /* other                */
};
 
proc_compare(p1, p2)
struct pst_status *p1;
struct pst_status *p2;

{
    int result;
    float lresult;

    /* compare percent cpu (pctcpu) */
    if ((lresult = p2->pst_pctcpu - p1->pst_pctcpu) == 0)
    {
	/* use cpticks to break the tie */
	if ((result = p2->pst_cpticks - p1->pst_cpticks) == 0)
	{
	    /* use process state to break the tie */
	    if ((result = sorted_state[p2->pst_stat] -
			  sorted_state[p1->pst_stat])  == 0)
	    {
		/* use priority to break the tie */
		if ((result = p2->pst_pri - p1->pst_pri) == 0)
		{
		    /* use resident set size (rssize) to break the tie */
		    if ((result = P_RSSIZE(p2) - P_RSSIZE(p1)) == 0)
		    {
			/* use total memory to break the tie */
			result = PROCSIZE(p2) - PROCSIZE(p1);
		    }
		}
	    }
	}
    }
    else
    {
	result = lresult < 0 ? -1 : 1;
    }

    return(result);
}

machine_init(statics)

struct statics *statics;

{
    struct pst_static info;
    int i = 0;
    int pagesize;

    /* If we can get mpid from the kernel, we'll use it, otherwise    */
    /* we'll guess from the most recently started proces              */
    if ((kmem = open (KMEM, O_RDONLY)) < 0 ||
	(nlist (VMUNIX, nlst)) < 0 ||
	(nlst[X_MPID].n_type) == 0)
	mpid_offset = 0;
    else
	mpid_offset = nlst[X_MPID].n_value;

    if (pstat_getstatic (&info, sizeof (info), 1, 0) < 0)
    {
	perror ("pstat_getstatic");
	return -1;
    }

    /*
     * Allocate space for the per-process structures (pst_status).  To
     * make life easier, simply allocate enough storage to hold all the
     * process information at once.  This won't normally be a problem
     * since machines with lots of processes configured will also have
     * lots of memory.
     */
    nproc = info.max_proc;
    pst = (struct pst_status *) malloc (nproc * sizeof (struct pst_status));
    if (pst == NULL)
    {
	fprintf (stderr, "out of memory\n");
	return -1;
    }

    /*
     * Calculate pageshift -- the value needed to convert pages to Kbytes.
     * This will usually be 2.
     */
    pageshift = 0;
    for (pagesize = info.page_size; pagesize > 1; pagesize >>= 1)
	pageshift += 1;
    pageshift -= LOG1024;

    /* get tty name information */
    i = 0;
    get_tty_names ("/dev", &i);

    /* fill in the statics information */
    statics->procstate_names = procstatenames;
    statics->cpustate_names = cpustatenames;
    statics->memory_names = memorynames;

    /* all done! */
    return(0);
}

char *format_header(uname_field)
char *uname_field;
{
    char *ptr = header + UNAME_START;
    while (*uname_field != '\0')
	*ptr++ = *uname_field++;

    return header;
}

void
get_system_info(si)

struct system_info *si;

{
    static struct pst_dynamic dynamic;
    int i, n;
    long total;

    pstat_getdynamic (&dynamic, sizeof (dynamic), 1, 0);
    ncpu = dynamic.psd_proc_cnt;  /* need this later */

    /* Load average */
    si->load_avg[0] = dynamic.psd_avg_1_min;
    si->load_avg[1] = dynamic.psd_avg_5_min;
    si->load_avg[2] = dynamic.psd_avg_15_min;

    /*
     * CPU times
     * to avoid space problems, we roll SWAIT (kernel semaphore block)
     * into BLOCK (spin lock block) and SSYS (kernel process) into SYS
     * (system time) Ideally, all screens would be wider :-)
     */
    dynamic.psd_cpu_time [CP_BLOCK] += dynamic.psd_cpu_time [CP_SWAIT];
    dynamic.psd_cpu_time [CP_SWAIT] = 0;
    dynamic.psd_cpu_time [CP_SYS] += dynamic.psd_cpu_time [CP_SSYS];
    dynamic.psd_cpu_time [CP_SSYS] = 0;
    for (i = 0; i < PST_MAX_CPUSTATES; i++)
	cp_time [i] = dynamic.psd_cpu_time [i];
    percentages(PST_MAX_CPUSTATES, cpu_states, cp_time, cp_old, cp_diff);
    si->cpustates = cpu_states;

    /*
     * VM statistics
     */	
    memory_stats[0] = -1;
    memory_stats[1] = pagetok (dynamic.psd_arm);
    memory_stats[2] = pagetok (dynamic.psd_rm);
    memory_stats[3] = -1;
    memory_stats[4] = pagetok (dynamic.psd_avm);
    memory_stats[5] = pagetok (dynamic.psd_vm);
    memory_stats[6] = pagetok (dynamic.psd_free);
    si->memory = memory_stats;

    /*
     * If we can get mpid from the kernel, then we will do so now. 
     * Otherwise we'll guess at mpid from the most recently started
     * process time.  Note that this requires us to get the pst array
     * now rather than in get_process_info().  We rely on
     * get_system_info() being called before get_system_info() for this
     * to work reliably.
     */
    for (i = 0; i < nproc; i++)
	pst[i].pst_pid = -1;
    n = pstat_getproc (pst, sizeof (*pst), nproc, 0);

    if (kmem >= 0 && mpid_offset > 0)
	(void) getkval(mpid_offset, &(si->last_pid), sizeof(si->last_pid), "mpid");
    else
    {
	static int last_start_time = 0;
	int pid = 0;

	for (i = 0; i < n; i++)
	{
	    if (last_start_time <= pst[i].pst_start) 
	    {
	    	last_start_time = pst[i].pst_start;
		if (pid <= pst[i].pst_pid)
		    pid = pst[i].pst_pid;
	    }
	}
	if (pid != 0)
	    si->last_pid = pid;
    }
}

caddr_t get_process_info(si, sel, compare_index)

struct system_info *si;
struct process_select *sel;
int compare_index;

{
    static int handle;
    int i, active, total;

    /*
     * Eliminate unwanted processes
     * and tot up all the wanted processes by state
     */
    for (i = 0; i < sizeof (process_states)/sizeof (process_states[0]); i++)
	process_states [i] = 0;

    for (total = 0, active = 0, i = 0; pst[i].pst_pid >= 0; i++)
    {
	int state = pst[i].pst_stat;

	process_states [state] += 1;
	total += 1;

	if (!sel->system && (pst[i].pst_flag & PS_SYS))
	{
	    pst[i].pst_stat = -1;
	    continue;
	}

	/*
	 * If we are eliminating idle processes, then a process is regarded
	 * as idle if it is in a short term sleep and not using much
	 * CPU, or stopped, or simple dead.
	 */
	if (!sel->idle
	    && (state == PS_SLEEP || state == PS_STOP || state == PS_ZOMBIE)
	    && (state != PS_SLEEP && pst[i].pst_pctcpu < CPU_IDLE_THRESH/100.0))
	    pst[i].pst_stat = -1;
		
	if (sel->uid > 0 && sel->uid != pst[i].pst_uid)
	    pst[i].pst_stat = -1;

	if (sel->command != NULL &&
	    strncmp (sel->command, pst[i].pst_ucomm, strlen (pst[i].pst_ucomm)) != 0)
	    pst[i].pst_stat = -1;

	if (pst[i].pst_stat >= 0)
	    active += 1;
    }
    si->procstates = process_states;
    si->p_total = total;
    si->p_active = active;

    qsort ((char *)pst, i, sizeof(*pst), proc_compare);

    /* handle is simply an index into the process structures */
    handle = 0;
    return (caddr_t) &handle;
}

/*
 * Find the terminal name associated with a particular
 * major/minor number pair
 */
static char *term_name (term)
struct psdev *term;
{
    dev_t dev;
    int i;

    if (term->psd_major == -1 && term->psd_minor == -1)
	return "?";

    dev = makedev (term->psd_major, term->psd_minor);
    for (i = 0; i < nttys && ttynames[i].name[0] != '\0'; i++)
    {
	if (dev == ttynames[i].dev)
	    return ttynames[i].name;
    }
    return "<unk>";
}

char *format_next_process(handle, get_userid)

caddr_t handle;
char *(*get_userid)();

{
    static char fmt[MAX_COLS];	/* static area where result is built */
    char run [sizeof ("runNN")];
    int idx;
    struct pst_status *proc;
    char *state;
    int size;

    register long cputime;
    register double pct;
    int where;
    struct handle *hp;
    struct timeval time;
    struct timezone timezone;

    /* sanity check */
    if (handle == NULL)
	return "";

    idx = *((int *) handle);
    while (idx < nproc && pst[idx].pst_stat < 0)
	idx += 1;
    if (idx >= nproc || pst[idx].pst_stat < 0)
	return "";
    proc = &pst[idx];
    *((int *) handle) = idx+1;

    /* set ucomm for system processes, although we shouldn't need to */
    if (proc->pst_ucomm[0] == '\0')
    {
	if (proc->pst_pid == 0)
	    strcpy (proc->pst_ucomm, "Swapper");
	else if (proc->pst_pid == 2)
	    strcpy (proc->pst_ucomm, "Pager");
    }

    size = proc->pst_tsize + proc->pst_dsize + proc->pst_ssize;

    if (ncpu > 1 && proc->pst_stat == PS_RUN)
    {
	sprintf (run, "run%02d", proc->pst_procnum);
	state = run;
    }
    else if (proc->pst_stat == PS_SLEEP)
    {
	switch (proc->pst_pri+PTIMESHARE) {
	case PSWP:	state = "SWP"; break; /* also PMEM */
	case PRIRWLOCK:	state = "RWLOCK"; break;
	case PRIBETA:	state = "BETA"; break;
	case PRIALPHA:	state = "ALPHA"; break;
	case PRISYNC:	state = "SYNC"; break;
	case PINOD:	state = "INOD"; break;
	case PRIBIO:	state = "BIO"; break;
	case PLLIO:	state = "LLIO"; break; /* also PRIUBA  */
	case PZERO:	state = "ZERO"; break;
	case PPIPE:	state = "pipe"; break;
	case PVFS:	state = "vfs"; break;
	case PWAIT:	state = "wait"; break;
	case PLOCK:	state = "lock"; break;
	case PSLEP:	state = "slep"; break;
	case PUSER:	state = "user"; break;
	default:
	    if (proc->pst_pri < PZERO-PTIMESHARE)
		state = "SLEEP";
	    else
		state = "sleep";
	}
    }
    else
	state = state_abbrev [proc->pst_stat];

    /* format this entry */
    sprintf(fmt,
	    Proc_format,
	    term_name (&proc->pst_term),
	    proc->pst_pid,
	    (*get_userid)(proc->pst_uid),
	    proc->pst_pri,
	    proc->pst_nice - NZERO,
	    format_k(size),
	    format_k(proc->pst_rssize),
	    state,
	    format_time(proc->pst_utime + proc->pst_stime),
	    100.0 * proc->pst_pctcpu,
	    printable(proc->pst_ucomm));

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
    if (lseek(kmem, (long)offset, SEEK_SET) == -1) {
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
    
void (*signal(sig, func))()
    int sig;
    void (*func)();
{
    struct sigaction act;
    struct sigaction oact;

    memset (&act, 0, sizeof (act));
    act.sa_handler = func;

    if (sigaction (sig, &act, &oact) < 0)
	return BADSIG;
    return oact.sa_handler;
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
    int i;

    for (i = 0;  i < nproc; i++)
    {
	if (pst[i].pst_pid == pid)
	    return pst[i].pst_uid;
    }
    return -1;
}


static get_tty_names (dir, m)
char *dir;
int *m;
{
    char name [MAXPATHLEN+1];
    struct dirent **namelist;
    int i, n;

    if ((n = scandir (dir, &namelist, NULL, NULL)) < 0)
	return;

    if (ttynames == NULL)
    {
	nttys = n;
	ttynames = malloc (n*sizeof (*ttynames));
    }
    else
    {
	nttys += n;
	ttynames = realloc (ttynames, nttys*sizeof (*ttynames));
    }

    for (i = 0; i < n; i++)
    {
	struct stat statbuf;
	char *str = namelist[i]->d_name;
	if (*str == '.')
	    continue;
	sprintf (name, "%s/%s", dir, str);
	if (stat (name, &statbuf) < 0)
	    continue;
	
	if (!isalpha (*str))
	    str = name + sizeof ("/dev");
	if (S_ISCHR (statbuf.st_mode))
	{
	    ttynames [*m].dev = statbuf.st_rdev;
	    strncpy (ttynames[*m].name, str, 8);
	    ttynames[*m].name[9] = '\0';
	    *m += 1;
	}
	else if (S_ISDIR (statbuf.st_mode))
	    get_tty_names (name, m);
    }
    if (*m < nttys)
	ttynames[*m].name[0] = '\0';
    free (namelist);
}

