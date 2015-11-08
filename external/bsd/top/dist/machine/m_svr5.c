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
 * SYNOPSIS:  For Intel based System V Release 5 (Unixware7)
 * 
 * DESCRIPTION:
 * System V release 5 for i[3456]86
 * Works for:
 * i586-sco-sysv5uw7  i386 SCO UNIX_SVR5 (UnixWare 7)
 * 
 * LIBS:  -lelf -lmas
 * 
 * CFLAGS: -DHAVE_GETOPT -DORDER
 * 
 * AUTHORS: Mike Hopkirk       <hops@sco.com>
 *          David Cutter       <dpc@grail.com>
 *          Andrew Herbert     <andrew@werple.apana.org.au>
 *          Robert Boucher     <boucher@sofkin.ca>
 */

/* build config
 *  SHOW_NICE - process nice fields don't seem to be being updated so changed
 *     default to display # of threads in use instead.
 *     define this to display nice fields (values always 0)
 * #define SHOW_NICE 1 
 */

#define _KMEMUSER
#define prpsinfo psinfo
#include <sys/procfs.h>

#define pr_state pr_lwp.pr_state
#define pr_nice pr_lwp.pr_nice
#define pr_pri pr_lwp.pr_pri
#define pr_onpro pr_lwp.pr_onpro
#define ZOMBIE(p)	((p)->pr_nlwp == 0)
#define SIZE_K(p)	pagetok((p)->pr_size)
#define RSS_K(p)	pagetok((p)->pr_rssize)


#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <nlist.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sysmacros.h>
#include <vm/anon.h> 
#include <sys/priocntl.h>
#include <sys/tspriocntl.h> 
#include <sys/var.h>

#include "top.h"
#include "machine.h"
#include "utils.h"

#define UNIX "/stand/unix"
#define KMEM "/dev/kmem"
#define PROCFS "/proc"
#define CPUSTATES	5

#ifndef PRIO_MAX
#define PRIO_MAX	20
#endif
#ifndef PRIO_MIN
#define PRIO_MIN	-20
#endif

#ifndef FSCALE
#define FSHIFT  8		/* bits to right of fixed binary point */
#define FSCALE  (1<<FSHIFT)
#endif

#define loaddouble(x) ((double)x/FSCALE)
#define pagetok(size) ((size) * pagesz) >> LOG1024

/* definitions for the index in the nlist array */
#define X_AVENRUN	0
#define X_V		1
#define X_MPID		2

static struct nlist nlst[] =
{
   {"avenrun"},		        /* 0 */
   {"v"},			/* 1 */
   {"nextpid"},                 /* 2 */
  {NULL}
};

static unsigned long avenrun_offset;
static unsigned long mpid_offset;

static unsigned int pagesz;

static void reallocproc(int n);
static int maxprocs;

/* get_process_info passes back a handle.  This is what it looks like: */

struct handle
{
    struct prpsinfo **next_proc;/* points to next valid proc pointer */
    int remaining;		/* number of pointers remaining */
};

/*
 *  These definitions control the format of the per-process area
 */

static char header[] =
#ifdef SHOW_NICE
"  PID X        PRI NICE  SIZE   RES STATE   TIME      CPU  COMMAND";
#else
"  PID X        PRI  THR  SIZE   RES STATE   TIME      CPU  COMMAND";
#endif
/* 0123456   -- field to fill in starts at header+6 */
#define UNAME_START 6
#define Proc_format \
	"%5d %-8.8s %3d %4d %5s %5s %-5s %6s %8.4f%% %.16s"

char *state_abbrev[] =
{"oncpu", "run", "sleep",  "stop", "idle", "zombie"};

#define sZOMB 5
int process_states[8];
char *procstatenames[] =
{
  " on cpu, ", " running, ", " sleeping, ", " stopped, ",
  " idling ",  " zombie, ", 
  NULL
};

int cpu_states[CPUSTATES];
char *cpustatenames[] =
{"idle", "user", "kernel", "wait", NULL};


/* these are for detailing the memory statistics */
long memory_stats[5];
char *memorynames[] =
{"K phys, ", "K used, ", "K free, ", "K swapUsed, ", "K swapFree", NULL};

/* these are names given to allowed sorting orders -- first is default */
char *ordernames[] = 
{"state", "cpu", "size", "res", "time", "pid", "uid", "rpid", "ruid", NULL};

/* forward definitions for comparison functions */
int proc_compare();
int compare_cpu();
int compare_size();
int compare_res();
int compare_time();
int compare_pid();
int compare_uid();
int compare_rpid();
int compare_ruid();

int (*proc_compares[])() = {
    proc_compare,
    compare_cpu,
    compare_size,
    compare_res,
    compare_time,
    compare_pid,
    compare_uid,
    compare_rpid,
    compare_ruid,
    NULL };


static int kmem = -1;
static int nproc;
static int bytes;
static struct prpsinfo *pbase;
static struct prpsinfo **pref;
static DIR *procdir;

/* useful externals */
extern int errno;
extern char *sys_errlist[];
extern char *myname;
extern long percentages ();
extern int check_nlist ();
extern int getkval ();
extern void perror ();
extern void getptable ();
extern void quit ();
extern int nlist ();

/* fwd dcls */
static int kmet_init(void );
static int get_cpustates(int *new);


int
machine_init (struct statics *statics)
  {
    static struct var v;
    int i;

    /* fill in the statics information */
    statics->procstate_names = procstatenames;
    statics->cpustate_names = cpustatenames;
    statics->memory_names = memorynames;
    statics->order_names = ordernames;

    /* get the list of symbols we want to access in the kernel */
    if (nlist (UNIX, nlst))
      {
	(void) fprintf (stderr, "Unable to nlist %s\n", UNIX);
	return (-1);
      }

    /* make sure they were all found */
    if (check_nlist (nlst) > 0)
      return (-1);

    /* open kernel memory */
    if ((kmem = open (KMEM, O_RDONLY)) == -1)
      {
	perror (KMEM);
	return (-1);
      }

    v.v_proc=200;   /* arbitrary default */
    /* get the symbol values out of kmem */
    /* NPROC Tuning parameter for max number of processes */
    (void) getkval (nlst[X_V].n_value, &v, sizeof (struct var), nlst[X_V].n_name);
    nproc = v.v_proc;
    maxprocs = nproc;

    /* stash away certain offsets for later use */
    mpid_offset = nlst[X_MPID].n_value;
    avenrun_offset = nlst[X_AVENRUN].n_value;

    /* allocate space for proc structure array and array of pointers */
    bytes = nproc * sizeof (struct prpsinfo);
    pbase = (struct prpsinfo *) malloc (bytes);
    pref = (struct prpsinfo **) malloc (nproc * sizeof (struct prpsinfo *));

    pagesz = sysconf(_SC_PAGESIZE);


    /* Just in case ... */
    if (pbase == (struct prpsinfo *) NULL || pref == (struct prpsinfo **) NULL)
      {
	(void) fprintf (stderr, "%s: can't allocate sufficient memory\n", myname);
	return (-1);
      }

    if (!(procdir = opendir (PROCFS)))
      {
	(void) fprintf (stderr, "Unable to open %s\n", PROCFS);
	return (-1);
      }

    if (chdir (PROCFS))
    {				/* handy for later on when we're reading it */
	(void) fprintf (stderr, "Unable to chdir to %s\n", PROCFS);
	return (-1);
    }


    kmet_init();

    /* all done! */
    return (0);
  }

char *
format_header (char *uname_field)
{
  register char *ptr;

  ptr = header + UNAME_START;
  while (*uname_field != '\0')
    *ptr++ = *uname_field++;

  return (header);
}

void
get_system_info (struct system_info *si)
{
  long avenrun[3];
  long mem;
  static time_t cp_old[CPUSTATES];
  static time_t cp_diff[CPUSTATES];	/* for cpu state percentages */
  register int i;
  static long swap_total;
  static long swap_free;
  int new_states[CPUSTATES];

  get_cpustates(new_states);

  /* convert cp_time counts to percentages */
  (void) percentages (CPUSTATES, cpu_states, new_states, cp_old, cp_diff);


  si->last_pid = -1;
  /* get mpid -- process id of last process
   * svr5 is nextpid - next pid to be assigned (already incremented)
   */
   (void) getkval (mpid_offset, &(si->last_pid), sizeof (si->last_pid),
   		  "nextpid");
   (si->last_pid)--;    /* so we shld decrement for display */


  /* get load average array */
  (void) getkval (avenrun_offset, (int *) avenrun, sizeof (avenrun), "avenrun");
  /* convert load averages to doubles */
  for (i = 0; i < 3; i++)
    si->load_avg[i] = loaddouble(avenrun[i]);

  mem = sysconf(_SC_TOTAL_MEMORY);      /* physical mem */
  memory_stats[0] = pagetok (mem);

  mem = kmet_get_freemem();             /* free mem */
  memory_stats[2] = pagetok (mem);

  /* mem = sysconf(_SC_GENERAL_MEMORY);    */
  memory_stats[1] = memory_stats[0] - memory_stats[2]; /* active */

  get_swapinfo(&swap_total, &swap_free);
  memory_stats[3] = pagetok(swap_total - swap_free);
  memory_stats[4] = pagetok(swap_free);
 

  /* set arrays and strings */
  si->cpustates = cpu_states;
  si->memory = memory_stats;
}

static struct handle handle;

caddr_t
get_process_info (
		   struct system_info *si,
		   struct process_select *sel,
		   int idx)
{
  register int i;
  register int total_procs;
  register int active_procs;
  register struct prpsinfo **prefp;
  register struct prpsinfo *pp;

  /* these are copied out of sel for speed */
  int show_idle;
  int show_system;
  int show_uid;

  /* Get current number of processes */

  /* read all the proc structures */
  getptable (pbase);

  /* get a pointer to the states summary array */
  si->procstates = process_states;

  /* set up flags which define what we are going to select */
  show_idle   = sel->idle;
  show_system = sel->system;
  show_uid    = sel->uid != -1;

  nproc = kmet_get_nproc();

  /* count up process states and get pointers to interesting procs */
  total_procs = 0;
  active_procs = 0;
  (void) memset (process_states, 0, sizeof (process_states));
  prefp = pref;

  for (pp = pbase, i = 0; i < nproc; pp++, i++)
  {
      /*
	 *  Place pointers to each valid proc structure in pref[].
	 *  Process slots that are actually in use have a non-zero
	 *  status field.  Processes with PR_ISSYS set are system
	 *  processes---these get ignored unless show_sysprocs is set.
	 */
      if ((pp->pr_state >= SONPROC && pp->pr_state <= SIDL)  &&
	  (show_system || ((pp->pr_flag & PR_ISSYS) == 0)))
      {
	  total_procs++;
	  process_states[pp->pr_state]++;
	  if ((!ZOMBIE(pp)) &&
	      (show_idle || (pp->pr_state == SRUN) || (pp->pr_state == SONPROC)) &&
	      (!show_uid || pp->pr_uid == (uid_t) sel->uid))
	  {
	      *prefp++ = pp;
	      active_procs++;
	  }
	  if (ZOMBIE(pp))
    	    process_states[sZOMB]++;    /* invented */

      }
  }

  /* if requested, sort the "interesting" processes */
  qsort ((char *) pref, active_procs, sizeof (struct prpsinfo *),
	 proc_compares[idx]);

  /* remember active and total counts */
  si->p_total = total_procs;
  si->P_ACTIVE = active_procs;

  /* pass back a handle */
  handle.next_proc = pref;
  handle.remaining = active_procs;
  return ((caddr_t) & handle);
}

/*
 * cpu percentage calculation is as fm ps.c
 * seems to be ratio of (sys+user time used)/(elapsed time)
 * i.e percent of cpu utilised when on cpu
 */
static double percent_cpu( struct prpsinfo *pp)
{
    static time_t tim = 0L;   
    time_t starttime;
    time_t ctime;
    time_t etime;

    /* if (tim == 0L) */
        tim = time((time_t *) 0);
    starttime = pp->pr_start.tv_sec;
    if (pp->pr_start.tv_nsec > 500000000)
            starttime++;
    etime = (tim - starttime);
    ctime = pp->pr_time.tv_sec;
    if (pp->pr_time.tv_nsec > 500000000)
    ctime++;
    if (etime) 
    {
        /* return  (float)(ctime * 100) / (unsigned)etime; */
        /* this was ocasionally giving vals >100 for some
         * unknown reason so the below normalises it
         */
        
        double pct;
        pct = (float)(ctime * 100) / (unsigned)etime;
        return (pct < 100.0) ? pct : 100.00;
    }
    return 0.00;
}


char fmt[MAX_COLS];			/* static area where result is built */

char *
format_next_process (
		      caddr_t handle,
		      char *(*get_userid) ())
{
  register struct prpsinfo *pp;
  struct handle *hp;
  register long cputime;
  register double pctcpu;

  /* find and remember the next proc structure */
  hp = (struct handle *) handle;
  pp = *(hp->next_proc++);
  hp->remaining--;

  /* get the cpu usage and calculate the cpu percentages */
  cputime = pp->pr_time.tv_sec;
  pctcpu = percent_cpu(pp);


  /* format this entry */
  (void) sprintf (fmt,
		  Proc_format,
		  pp->pr_pid,
		  (*get_userid) (pp->pr_uid),
                  pp->pr_pri,
#ifdef SHOW_NICE
		  pp->pr_nice,
#else
	          (u_short)pp->pr_nlwp < 999 ? (u_short)pp->pr_nlwp : 999,
#endif
	          format_k(SIZE_K(pp)), 
                  format_k(RSS_K(pp)),  
	          (ZOMBIE(pp))  ? state_abbrev[sZOMB] 
                                : state_abbrev[pp->pr_state],
		  format_time(cputime),
		  /* 100.0 * */ pctcpu,
		  printable(pp->pr_fname));

  /* return the result */
  return (fmt);
}

/*
 * check_nlist(nlst) - checks the nlist to see if any symbols were not
 *		found.  For every symbol that was not found, a one-line
 *		message is printed to stderr.  The routine returns the
 *		number of symbols NOT found.
 */
int
check_nlist (register struct nlist *nlst)
{
  register int i;

  /* check to see if we got ALL the symbols we requested */
  /* this will write one line to stderr for every symbol not found */

  i = 0;
  while (nlst->n_name != NULL)
    {
      if (nlst->n_value == 0)
	{
	  /* this one wasn't found */
	  (void) fprintf (stderr, "kernel: no symbol named `%s'\n", nlst->n_name);
	  i = 1;
	}
      nlst++;
    }
  return (i);
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
getkval (
	  unsigned long offset,
	  int *ptr,
	  int size,
	  char *refstr)
{
  if (lseek (kmem, (long) offset, 0) == -1)
    {
      if (*refstr == '!')
	refstr++;
      (void) fprintf (stderr, "%s: lseek to %s: %s\n",
		      myname, refstr, sys_errlist[errno]);
      quit (22);
    }
  if (read (kmem, (char *) ptr, size) == -1)
    if (*refstr == '!')
      /* we lost the race with the kernel, process isn't in memory */
      return (0);
    else
      {
	(void) fprintf (stderr, "%s: reading %s: %s\n",
			myname, refstr, sys_errlist[errno]);
	quit (23);
      }
  return (1);
}

/* ----------------- comparison routines for qsort ---------------- */

/* First, the possible comparison keys.  These are defined in such a way
   that they can be merely listed in the source code to define the actual
   desired ordering.
 */

#define ORDERKEY_PCTCPU  if (dresult = percent_cpu (p2) - percent_cpu (p1),\
			     (result = dresult > 0.0 ? 1 : \
			     dresult < 0.0 ? -1 : 0) == 0)

#define ORDERKEY_CPTICKS if ((result = p2->pr_time.tv_sec - p1->pr_time.tv_sec) == 0)
#define ORDERKEY_STATE   if ((result = (long) (sorted_state[p2->pr_state] - \
			       sorted_state[p1->pr_state])) == 0)

#define ORDERKEY_PRIO    if ((result = p2->pr_pri    - p1->pr_pri)    == 0)
#define ORDERKEY_RSSIZE  if ((result = p2->pr_rssize - p1->pr_rssize) == 0)
#define ORDERKEY_MEM     if ((result = (p2->pr_size  - p1->pr_size))  == 0)

#define ORDERKEY_PID     if ((result = (p2->pr_pid  - p1->pr_pid))  == 0)
#define ORDERKEY_UID     if ((result = (p2->pr_uid  - p1->pr_uid))  == 0)
#define ORDERKEY_RPID    if ((result = (p1->pr_pid  - p2->pr_pid))  == 0)
#define ORDERKEY_RUID    if ((result = (p1->pr_uid  - p2->pr_uid))  == 0)

/* states enum {SONPROC, SRUN, SSLEEP, SSTOP, SIDL}  */
unsigned char sorted_state[] =
{
  7,				/* onproc		*/
  6,				/* run		        */
  5,				/* sleep		*/
  4,				/* stop		        */
  3,				/* idle			*/
  2,				/* zombie		*/
  0,				/* unused               */
  0				/* unused	        */
};

#if 0
/*
 *  proc_compare - original singleton comparison function for "qsort"
 *	Compares the resource consumption of two processes using five
 *  	distinct keys.  The keys (in descending order of importance) are:
 *  	percent cpu, cpu ticks, state, resident set size, total virtual
 *  	memory usage.  The process states are ordered as follows (from least
 *  	to most important):  WAIT, zombie, sleep, stop, start, run.  The
 *  	array declaration below maps a process state index into a number
 *  	that reflects this ordering.
 */
 /* default comparison rtn */
int
original_proc_compare (
	       struct prpsinfo **pp1,
	       struct prpsinfo **pp2)
  {
    register struct prpsinfo *p1;
    register struct prpsinfo *p2;
    register long result;
    double dresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    /* compare percent cpu (pctcpu) */
    dresult = percent_cpu(p2) - percent_cpu (p1);
    result = dresult > 0.0 ?  1 :
             dresult < 0.0 ? -1 : 0;
    if (result)
    {
	/* use cpticks to break the tie */
	if ((result = p2->pr_time.tv_sec - p1->pr_time.tv_sec) == 0)
	  {
	    /* use process state to break the tie */
	    if ((result = (long) (sorted_state[p2->pr_state] -
				  sorted_state[p1->pr_state])) == 0)
	      {
		/* use priority to break the tie */
		if ((result = p2->pr_pri - p1->pr_pri) == 0)
		  {
		    /* use resident set size (rssize) to break the tie */
		    if ((result = p2->pr_rssize - p1->pr_rssize) == 0)
		      {
			/* use total memory to break the tie */
			result = (p2->pr_size - p1->pr_size);
		      }
		  }
	      }
	  }
    }
    return (result);
  }
#endif  /* original comparison rtn */

/* compare_state - comparison function for sorting by state,pri,time,size */
int
proc_compare (
	       struct prpsinfo **pp1,
	       struct prpsinfo **pp2)
  {
    register struct prpsinfo *p1;
    register struct prpsinfo *p2;
    register long result;
    double dresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_CPTICKS
    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ORDERKEY_PCTCPU
    ;

    return (result);
  }


/* compare_cpu - the comparison function for sorting by cpu % (deflt) */
int
compare_cpu (
	       struct prpsinfo **pp1,
	       struct prpsinfo **pp2)
  {
    register struct prpsinfo *p1;
    register struct prpsinfo *p2;
    register long result;
    double dresult;

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

    return (result);
  }

/* compare_size - the comparison function for sorting by total memory usage */
int
compare_size (
	       struct prpsinfo **pp1,
	       struct prpsinfo **pp2)
  {
    register struct prpsinfo *p1;
    register struct prpsinfo *p2;
    register long result;
    double dresult;

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

    return (result);
  }

/* compare_res - the comparison function for sorting by resident set size */
int
compare_res (
	       struct prpsinfo **pp1,
	       struct prpsinfo **pp2)
  {
    register struct prpsinfo *p1;
    register struct prpsinfo *p2;
    register long result;
    double dresult;

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

    return (result);
  }

/* compare_time - the comparison function for sorting by total cpu time */
int
compare_time (
	       struct prpsinfo **pp1,
	       struct prpsinfo **pp2)
  {
    register struct prpsinfo *p1;
    register struct prpsinfo *p2;
    register long result;
    double dresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    ORDERKEY_CPTICKS
    ORDERKEY_PCTCPU
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_MEM
    ORDERKEY_RSSIZE
    ;

    return (result);
  }

/* compare_pid - the comparison function for sorting by pid */
int
compare_pid (
	       struct prpsinfo **pp1,
	       struct prpsinfo **pp2)
  {
    register struct prpsinfo *p1;
    register struct prpsinfo *p2;
    register long result;
    double dresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    ORDERKEY_PID
    ORDERKEY_CPTICKS
    ORDERKEY_PCTCPU
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_MEM
    ORDERKEY_RSSIZE
    ;

    return (result);
  }

/* compare_uid - the comparison function for sorting by user ID */
int
compare_uid (
	       struct prpsinfo **pp1,
	       struct prpsinfo **pp2)
  {
    register struct prpsinfo *p1;
    register struct prpsinfo *p2;
    register long result;
    double dresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    ORDERKEY_UID
    ORDERKEY_CPTICKS
    ORDERKEY_PCTCPU
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_MEM
    ORDERKEY_RSSIZE
    ;

    return (result);
  }

/* compare_rpid - the comparison function for sorting by pid ascending */
int
compare_rpid (
	       struct prpsinfo **pp1,
	       struct prpsinfo **pp2)
  {
    register struct prpsinfo *p1;
    register struct prpsinfo *p2;
    register long result;
    double dresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    ORDERKEY_RPID
    ORDERKEY_CPTICKS
    ORDERKEY_PCTCPU
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_MEM
    ORDERKEY_RSSIZE
    ;

    return (result);
  }

/* compare_uid - the comparison function for sorting by user ID ascending */
int
compare_ruid (
	       struct prpsinfo **pp1,
	       struct prpsinfo **pp2)
  {
    register struct prpsinfo *p1;
    register struct prpsinfo *p2;
    register long result;
    double dresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    ORDERKEY_RUID
    ORDERKEY_CPTICKS
    ORDERKEY_PCTCPU
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_MEM
    ORDERKEY_RSSIZE
    ;

    return (result);
  }


/* ---------------- helper rtns ---------------- */

/*
 * get process table
 */
void
getptable (struct prpsinfo *baseptr)
{
  struct prpsinfo *currproc;	/* pointer to current proc structure	*/
  int numprocs = 0;
  struct dirent *direntp;

  currproc = baseptr;
  for (rewinddir (procdir); direntp = readdir (procdir);)
    {
      int fd;
      char buf[30];

      sprintf(buf,"%s/psinfo", direntp->d_name);

      if ((fd = open (buf, O_RDONLY)) < 0)
	continue;

      if (read(fd, currproc, sizeof(psinfo_t)) != sizeof(psinfo_t))
      {
	  (void) close (fd);
	  continue;
      }
       
      numprocs++;
      currproc++;
      
      (void) close (fd);

      /* Atypical place for growth */
      if (numprocs >= maxprocs) 
      {
	    reallocproc(2 * numprocs);
	    currproc = (struct prpsinfo *)
		    ((char *)baseptr + sizeof(psinfo_t) * numprocs);
      }

    }

  if (nproc != numprocs)
    nproc = numprocs;
}

/* return the owner of the specified process, for use in commands.c as we're
   running setuid root */
int
proc_owner (int pid)
{
  register struct prpsinfo *p;
  int i;
  for (i = 0, p = pbase; i < nproc; i++, p++)
    if (p->pr_pid == (pid_t)pid)
      return ((int)(p->pr_uid));

  return (-1);
}

int
setpriority (int dummy, int who, int niceval)
{
  int scale;
  int prio;
  pcinfo_t pcinfo;
  pcparms_t pcparms;
  tsparms_t *tsparms;

  strcpy (pcinfo.pc_clname, "TS");
  if (priocntl (0, 0, PC_GETCID, (caddr_t) & pcinfo) == -1)
    return (-1);

  prio = niceval;
  if (prio > PRIO_MAX)
    prio = PRIO_MAX;
  else if (prio < PRIO_MIN)
    prio = PRIO_MIN;

  tsparms = (tsparms_t *) pcparms.pc_clparms;
  scale = ((tsinfo_t *) pcinfo.pc_clinfo)->ts_maxupri;
  tsparms->ts_uprilim = tsparms->ts_upri = -(scale * prio) / 20;
  pcparms.pc_cid = pcinfo.pc_cid;

  if (priocntl (P_PID, who, PC_SETPARMS, (caddr_t) & pcparms) == -1)
    return (-1);

  return (0);
}


get_swapinfo(long *total, long *fr)
{
    register int cnt, i;
    register long t, f;
    struct swaptable *swt;
    struct swapent *ste;
    static char path[256];

    /* get total number of swap entries */
    cnt = swapctl(SC_GETNSWP, 0);

    /* allocate enough space to hold count + n swapents */
    swt = (struct swaptable *)malloc(sizeof(int) +
				     cnt * sizeof(struct swapent));
    if (swt == NULL)
    {
	*total = 0;
	*fr = 0;
	return;
    }
    swt->swt_n = cnt;

    /* fill in ste_path pointers: we don't care about the paths, so we point
       them all to the same buffer */
    ste = &(swt->swt_ent[0]);
    i = cnt;
    while (--i >= 0)
    {
	ste++->ste_path = path;
    }

    /* grab all swap info */
    swapctl(SC_LIST, swt);

    /* walk thru the structs and sum up the fields */
    t = f = 0;
    ste = &(swt->swt_ent[0]);
    i = cnt;
    while (--i >= 0)
    {
	/* dont count slots being deleted */
	if (!(ste->ste_flags & ST_INDEL) )
	{
	    t += ste->ste_pages;
	    f += ste->ste_free;
	}
	ste++;
    }

    /* fill in the results */
    *total = t;
    *fr = f;
    free(swt);
}


/*
 * When we reach a proc limit, we need to realloc the stuff.
 */
static void reallocproc(int n)
{
    int bytes;
    struct oldproc *op, *endbase;

    if (n < maxprocs)
	return;

    maxprocs = n;

    /* allocate space for proc structure array and array of pointers */
    bytes = maxprocs * sizeof(psinfo_t) ;
    pbase = (struct prpsinfo *) realloc(pbase, bytes);
    pref = (struct prpsinfo **) realloc(pref,
			maxprocs * sizeof(struct prpsinfo *));

    /* Just in case ... */
    if (pbase == (struct prpsinfo *) NULL || pref == (struct prpsinfo **) NULL)
    {
	fprintf (stderr, "%s: can't allocate sufficient memory\n", myname);
	quit(1);
    }
}

/* ---------------------------------------------------------------- */
/* Access kernel Metrics 
 * SVR5 uses metreg inteface to Kernel statistics (metrics)
 *  see /usr/include/mas.h, /usr/include/metreg.h
 */

#include <sys/mman.h>
#include <sys/dl.h>
#include <mas.h>
#include <metreg.h>
 
static int md;         /* metric descriptor handle */   
static  uint32 ncpu;   /* number of processors in system */

/* fwd dcls */
static uint32 kmet_get_cpu( int type, char *desc);
static void kmet_verify( 
    uint32 md,    metid_t id,  units_t units, type_t mettype, 
    uint32 metsz, uint32 nobj, uint32 nlocs,  resource_t res_id, 
    uint32 ressz ) ;


static int get_cpustates(int *new)
{
    new[0] = (int)kmet_get_cpu( MPC_CPU_IDLE, "idle");
    new[1] = (int)kmet_get_cpu( MPC_CPU_USR,  "usr");
    new[2] = (int)kmet_get_cpu( MPC_CPU_SYS,  "sys");
    new[3] = (int)kmet_get_cpu( MPC_CPU_WIO,  "wio");
}


/* initialises kernel metrics access and gets #cpus */
static int kmet_init()
{
    uint32 *ncpu_p;

    /*  open (and map in) the metric access file and assoc data structures */
    if( ( md = mas_open( MAS_FILE, MAS_MMAP_ACCESS ) ) < 0 ) 
    {
        (void)fprintf(stderr,"mas_open failed\n");
        mas_perror();
        quit(10);
    }

    /* verify the NCPU metric is everything we expect */
    kmet_verify(md, NCPU, CPUS, CONFIGURABLE, sizeof(short),
                   1, 1, MAS_SYSTEM, sizeof(uint32) );

    /* get the number of cpu's on the system */
    if( (ncpu_p = (uint32 *)mas_get_met( md, NCPU, 0 )) == NULL ) 
    {
        (void)fprintf(stderr,"mas_get_met of ncpu failed\n");  
        mas_perror();
        quit(12);
    }
    ncpu = (uint32)(*(short *)ncpu_p);

    /* check that MPC_CPU_IDLE is of the form we expect
     *      ( paranoically we should check the rest as well but ... )
     */
    kmet_verify( md, MPC_CPU_IDLE, TIX, PROFILE, sizeof(uint32),
                    1,  ncpu, NCPU, sizeof(short) );

    kmet_verify( md, PROCUSE, PROCESSES, COUNT, sizeof(uint32),
                    1,  1, MAS_SYSTEM, sizeof(uint32) );
    nproc = kmet_get_nproc();

    return 0;
}

/* done with kernel metrics access */
static int
kmet_done()
{
    if ( mas_close( md ) < 0 )
    {
        (void)fprintf(stderr,"mas_close failed\n");
        mas_perror();
        quit(14);
    }
}


static uint32
kmet_get_cpu( int type, char *desc)
{
    int i;
    uint32 r=0, rtot=0 ;

    for (i=0; i <ncpu; i++)
    {
        r=*(uint32 *)mas_get_met( md, (metid_t)type, 0 );
        if ( !r)
        {
            (void)fprintf(stderr,"mas_get_met of %s failed\n", desc);
            mas_perror();
            quit(12);
        }
        rtot += r;      /* sum them for multi cpus */
    }
    return rtot /* /ncpu */ ;
}

static int
kmet_get_freemem()
{
    dl_t            *fm_p, fm, fmc, denom;
    time_t          td1;
    static time_t   td0;
    static dl_t     fm_old;
    

    td1 = time(NULL);
    if ((fm_p = (dl_t *)mas_get_met( md, FREEMEM, 0 )) == NULL )
    {
        (void)fprintf(stderr,"mas_get_met of freemem failed\n");
        mas_perror();
        quit(12);
    }
    fm = *fm_p; 
    
    denom.dl_hop = 0;
    denom.dl_lop = (long) (td1 - td0);
    td0 = td1;

    /* calculate the freemem difference divided by the time diff
     * giving the freemem in that time sample
     *  (new - old) / (time_between_samples)
     */
    fmc = lsub(fm, fm_old);
    fm_old = fm; 

    fmc = ldivide(fmc, denom);
    return  fmc.dl_lop;
}

/*
 * return # of processes currently executing on system
 */
static int
kmet_get_nproc()
{
    uint32 *p;
    if ((p = (uint32 *)mas_get_met( md, PROCUSE, 0 )) == NULL )
    {
        (void)fprintf(stderr,"mas_get_met of procuse failed\n");
        mas_perror();
        quit(11);
    }
    nproc = (int)*p;
}


/*
 * Function: 	kmet_verify
 * renamed from mas_usrtime example verify_met() fm Doug Souders
 *
 * Description:	Verify the registration data associated with this metric 
 *		match what are expected.  Cautious consumer applications 
 *		should do this sort of verification before using metrics.
 */
static void
kmet_verify( 
     uint32     md,         /* metric descriptor                */
     metid_t    id,         /* metric id number                 */
     units_t    units,      /* expected units of metric         */
     type_t     mettype,    /* expected type of metric          */
     uint32     metsz,      /* expected object size of metric   */
     uint32     nobj,       /* expected number of array elements */
     uint32     nlocs,      /* expected number of instances     */
     resource_t res_id,     /* expected resource id number      */
     uint32     ressz       /* expected resource object size    */
     )
{

    char		*name;		/* the name of the metric 	*/
    units_t		*units_p;	/* the units of the metric	*/
    type_t		*mettype_p;	/* type field of the metric	*/
    uint32 		*objsz_p;	/* size of each element in met 	*/
    uint32 		*nobj_p;	/* num of elements >1 then array*/
    uint32 		*nlocs_p;	/* total number of instances	*/
    uint32 		*status_p;	/* status word (update|avail)	*/
    resource_t	        *resource_p;	/* the resource list of the met	*/
    uint32		*resval_p;	/* pointer to resource		*/
    uint32 		*ressz_p;	/* size of the resource met	*/

    if (!(name = mas_get_met_name( md, id ))) 
    {
            (void)fprintf(stderr,"mas_get_met_name failed\n");
            mas_perror();
            quit(11);
    }
    
    if (!(status_p = mas_get_met_status( md, id ))) 
    {
            (void)fprintf(stderr,"mas_get_met_status of %s failed\n",
                name );
            mas_perror();
            quit(11);
    }
    if ( *status_p != MAS_AVAILABLE ) 
    {
        (void)fprintf(stderr,"unexpected status word for %s\n"
                                "- expected %u got %u\n",
                name, MAS_AVAILABLE, *status_p );
        quit(11);
    }
    if (!(units_p = mas_get_met_units( md, id ))) 
    {
            (void)fprintf(stderr,"mas_get_met_units of %s failed\n",
                name );
            mas_perror();
            quit(11);
    }
    if (units != *units_p ) 
    {
            (void)fprintf(stderr,"unexpected units for %s\n"
                                    "- expected %u got %u\n",
                name, units, *units_p );
            quit(11);
    }

    if (!(mettype_p = mas_get_met_type( md, id ))) 
    {
            (void)fprintf(stderr,"mas_get_met_type of %s failed\n",
                name );
            mas_perror();
            quit(11);
    }
    if (mettype != *mettype_p ) 
    {
            (void)fprintf(stderr,"unexpected metric type for %s\n"
                                    "- expected %u got %u\n",
                name, mettype , *mettype_p );
            quit(11);
    }

    if (!(objsz_p = mas_get_met_objsz( md, id ))) 
    {
            (void)fprintf(stderr,"mas_get_met_objsz of %s failed\n", name );
            mas_perror();
            quit(11);
    }
    if (*objsz_p != metsz ) 
    {
            (void)fprintf(stderr,"unexpected object size for %s\n"
                                    "- expected %u got %u\n",
                name, metsz, *objsz_p );
            quit(11);
    }

    if (!(nobj_p = mas_get_met_nobj( md, id ))) 
    {
            (void)fprintf(stderr,"mas_get_met_nobj of %s failed\n", name );
            mas_perror();
            quit(11);
    }
    if (nobj != *nobj_p ) 
    {
        (void)fprintf(stderr,"unexpected number of objects for %s\n"
                                    "- expected %u got %u\n",
                name, nobj, *nobj_p );
         quit(11);
    }

    /* get the number of instances that libmas thinks it knows about  */
    if (!(nlocs_p = mas_get_met_nlocs( md, id ))) 
    {
        (void)fprintf(stderr,"mas_get_met_nlocs of %s failed\n",  name );
        mas_perror();
        quit(11);
    }
    if (nlocs != *nlocs_p )
    {
        (void)fprintf(stderr,"unexpected number of instances for %s"
                        " - expected %u got %u\n",
                name, nlocs, *nlocs_p );
        quit(11);

    }
    /*	get the resource list for the metric */
    if (!(resource_p = mas_get_met_resources( md, id )))
    {
        (void)fprintf(stderr,"mas_get_met_resources of %s failed\n", name );
        mas_perror();
        quit(11);
    }
    if (*resource_p != res_id )
    {
        (void)fprintf(stderr,"unexpected resource id for %s\n"
                                    "- expected %u got %u\n",
                name, res_id, *resource_p);
        quit(11);
    }
    /*	get the size of the resource  */
    if (!(ressz_p = mas_get_met_objsz( md, (metid_t)(*resource_p) )))
    {
        (void)fprintf(stderr,"mas_get_met_objsz of resource failed\n");
        mas_perror();
        quit(11);
    }
    if (*ressz_p != ressz )
    {
        (void)fprintf(stderr,"unexpected resource size for %s\n"
                                    "- expected %u got %u\n",
                name, ressz, *ressz_p );
        quit(11);
    }
/*
 *	get the address of the resource
 */
    if (!(resval_p = (uint32 *)mas_get_met( md, *resource_p, 0 )))
    {
            (void)fprintf(stderr,"mas_get_met of resource failed\n");
            mas_perror();
            quit(11);
    }
    if (ressz == sizeof( short ) )
    {
        if( (uint32)(*(short *)resval_p) != nlocs )
        {
            (void)fprintf(stderr,"unexpected resource value for %s\n"
                                    "- expected %u got %u\n",
                        name, nlocs, (uint32)(*(short *)resval_p) );
            quit(11);
        }
    }
    else
    { /* assume size of uint32 */
        if (*resval_p != nlocs )
        {
            (void)fprintf(stderr,"unexpected resource value for %s\n"
                                    "- expected %u got %u\n",
                        name, nlocs, *resval_p );
            quit(11);
        }
    }
    return;
}

