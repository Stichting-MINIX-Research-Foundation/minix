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
 * SYNOPSIS:  any uniprocessor, 32 bit SGI machine running IRIX 5.3
 *
 * DESCRIPTION:
 * This is the machine-dependent module for IRIX 5.3.
 * It has been tested on Indys running 5.3 and Indigos running 5.3XFS
 *
 * LIBS: -lmld
 * CFLAGS: -DHAVE_GETOPT
 *
 * AUTHOR: Sandeep Cariapa <cariapa@sgi.com>
 * This is not a supported product of Silicon Graphics, Inc.
 * Please do not call SGI for support.
 *
 */

#define _KMEMUSER

#include "config.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/swap.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/sysinfo.h>
#include <sys/sysmp.h>
#include <paths.h>
#include <dirent.h>
#include <stdio.h>
#include <nlist.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include "top.h"
#include "machine.h"

#ifdef IRIX64
#define nlist nlist64
#define lseek lseek64
#define off_t off64_t
#endif

#define UNIX	"/unix"
#define KMEM	"/dev/kmem"
#define CPUSTATES 6

#ifndef FSCALE
#define FSHIFT  8		/* bits to right of fixed binary point */
#define FSCALE  (1<<FSHIFT)
#endif /* FSCALE */

#ifdef FIXED_LOADAVG
  typedef long load_avg;
# define loaddouble(la) ((double)(la) / FIXED_LOADAVG)
# define intload(i) ((int)((i) * FIXED_LOADAVG))
#else
  typedef double load_avg;
# define loaddouble(la) (la)
# define intload(i) ((double)(i))
#endif

#define percent_cpu(pp) (*(double *)pp->pr_fill)
#define weighted_cpu(pp) (*(double *)&pp->pr_fill[2])

static int pagesize;
#define pagetok(size) ((size)*pagesize)

static int numcpus;

/*
 *  These definitions control the format of the per-process area
 */

static char header[] =
  "  PID X        PRI NICE  SIZE   RES STATE   TIME   WCPU    CPU COMMAND";
/* 0123456   -- field to fill in starts at header+6 */
#define UNAME_START 6

#define Proc_format \
	"%5d %-8.8s %3d %4d %5s %5s %-5s %6s %5.2f%% %5.2f%% %.16s"

/* these are for detailing the process states */
char *state_abbrev[] =
{"", "sleep", "run\0\0\0", "zombie", "stop", "idle", "", "swap"};

int process_states[8];
char *procstatenames[] = {
    "", " sleeping, ", " running, ", " zombie, ", " stopped, ",
    " idle, ", "", " swapped, ",
    NULL
};

/* these are for detailing the cpu states */
int cpu_states[CPUSTATES];
char *cpustatenames[] = {
    "idle", "usr", "ker", "wait", "swp", "intr",
    NULL
};

/* these are for detailing the memory statistics */

long memory_stats[5];
char *memorynames[] = {
    "K max, ", "K avail, ", "K free, ", "K swap, ", "K free swap", NULL
};

/* useful externals */
extern int errno;
extern char *myname;
extern char *sys_errlist[];
extern char *format_k();
extern char *format_time();
extern long percentages();

/* forward references */
int proc_compare (void *pp1, void *pp2);

#define X_AVENRUN	0
#define X_NPROC		1
#define X_FREEMEM	2
#define X_MAXMEM	3
#define X_AVAILRMEM     4
#define X_MPID		5

static struct nlist nlst[] = {
{ "avenrun" },		/* 0. Array containing the 3 load averages. */
{ "nproc" },		/* 1. Kernel parameter: Max number of processes. */
{ "freemem" },		/* 2. Amount of free memory in system. */
{ "maxmem" },		/* 3. Maximum amount of memory usable by system. */
{ "availrmem" },        /* 4. Available real memory. */
#ifndef IRIX64
{ "mpid" },		/* 5. PID of last process. */
#endif
{ 0 }
};
static unsigned long avenrun_offset;
static unsigned long nproc_offset;
static unsigned long freemem_offset;
static unsigned long maxmem_offset;
static unsigned long availrmem_offset;
static unsigned long mpid_offset;
double load[3];
char fmt[MAX_COLS];
static int kmem;
static int nproc;
static int bytes;
static struct prpsinfo *pbase;
static struct prpsinfo **pref;
static DIR *procdir;

/* get_process_info passes back a handle.  This is what it looks like: */
struct handle  {
  struct prpsinfo **next_proc;/* points to next valid proc pointer */
  int remaining;	      /* number of pointers remaining */
};

static struct handle handle;
void getptable();

/*
 * Structure for keeping track of CPU times from last time around
 * the program.  We keep these things in a hash table, which is
 * recreated at every cycle.
 */
struct oldproc
  {
    pid_t oldpid;
    double oldtime;
    double oldpct;
  };
static int oldprocs;			/* size of table */
static struct oldproc *oldbase;
#define HASH(x) ((x << 1) % oldprocs)
#define PRPSINFOSIZE (sizeof(struct prpsinfo))

int machine_init(statics)
     struct statics *statics;
{
  struct oldproc *op, *endbase;

  if ((kmem = open(KMEM, O_RDONLY)) == -1) {
    perror(KMEM);
    return(-1);
  }

  /* get the list of symbols we want to access in the kernel */
  (void) nlist(UNIX, nlst);
  if (nlst[0].n_type == 0) {
    fprintf(stderr, "%s: nlist failed\n", myname);
    return(-1);
  }

  /* Check if we got all of 'em. */
  if (check_nlist(nlst) > 0) {
      return(-1);
    }
  avenrun_offset = nlst[X_AVENRUN].n_value;
  nproc_offset = nlst[X_NPROC].n_value;
  freemem_offset = nlst[X_FREEMEM].n_value;
  maxmem_offset = nlst[X_MAXMEM].n_value;
  availrmem_offset = nlst[X_AVAILRMEM].n_value;
#ifndef IRIX64
   mpid_offset = nlst[X_MPID].n_value;
#endif

  /* Got to do this first so that we can map real estate for the
     process array. */
  (void) getkval(nproc_offset, (int *) (&nproc), sizeof(nproc), "nproc");

  /* allocate space for proc structure array and array of pointers */
  bytes = nproc * sizeof (struct prpsinfo);
  pbase = (struct prpsinfo *) malloc (bytes);
  pref = (struct prpsinfo **) malloc (nproc * sizeof (struct prpsinfo *));
  oldbase = (struct oldproc *) malloc (2 * nproc * sizeof (struct oldproc));

  /* Just in case ... */
  if (pbase == (struct prpsinfo *) NULL || pref == (struct prpsinfo **) NULL ||
      oldbase == (struct oldproc *)NULL) {
    (void) fprintf (stderr, "%s: can't allocate sufficient memory\n", myname);
    return (-1);
  }

  oldprocs = 2 * nproc;
  endbase = oldbase + oldprocs;
  for (op = oldbase; op < endbase; op++) {
    op->oldpid = -1;
  }

  if (!(procdir = opendir (_PATH_PROCFSPI))) {
    (void) fprintf (stderr, "Unable to open %s\n", _PATH_PROCFSPI);
    return (-1);
  }

  if (chdir (_PATH_PROCFSPI)) {
    /* handy for later on when we're reading it */
    (void) fprintf (stderr, "Unable to chdir to %s\n", _PATH_PROCFSPI);
    return (-1);
  }

  statics->procstate_names = procstatenames;
  statics->cpustate_names = cpustatenames;
  statics->memory_names = memorynames;

  pagesize = getpagesize()/1024;

  /* all done! */
  return(0);
}

char *format_header(uname_field)
     register char *uname_field;

{
  register char *ptr;

  ptr = header + UNAME_START;
  while (*uname_field != '\0') {
    *ptr++ = *uname_field++;
  }

  return(header);
}

void get_system_info(si)
     struct system_info *si;

{
  register int i;
  int avenrun[3];
  static int freemem;
  static int maxmem;
  static int availrmem;
  struct sysinfo sysinfo;
  static long cp_new[CPUSTATES];
  static long cp_old[CPUSTATES];
  static long cp_diff[CPUSTATES]; /* for cpu state percentages */
  off_t  fswap;          /* current free swap in blocks */
  off_t  tswap;          /* total swap in blocks */

  (void) getkval(avenrun_offset, (int *)avenrun, sizeof(avenrun), "avenrun");
  for (i = 0; i < 3; i++) {
    si->load_avg[i] = loaddouble (avenrun[i]);
    si->load_avg[i] = si->load_avg[i]/1024.0;
  }

  (void) getkval(freemem_offset, (int *) (&freemem), sizeof(freemem),
"freemem");
  (void) getkval(maxmem_offset, (int *) (&maxmem), sizeof(maxmem), "maxmem");
  (void) getkval(availrmem_offset, (int *) (&availrmem), sizeof(availrmem),
"availrmem");
#ifdef IRIX64
  si->last_pid = 0;
#else
  (void) getkval(mpid_offset, &(si->last_pid), sizeof (si->last_pid), "mpid");
#endif
  swapctl(SC_GETFREESWAP, &fswap);
  swapctl(SC_GETSWAPTOT, &tswap);
  memory_stats[0] = pagetok(maxmem);
  memory_stats[1] = pagetok(availrmem);
  memory_stats[2] = pagetok(freemem);
  memory_stats[3] = tswap / 2;
  memory_stats[4] = fswap / 2;

  /* use sysmp() to get current sysinfo usage. Can run into all kinds of
     problems if you try to nlist this kernel variable. */
  if (sysmp(MP_SAGET, MPSA_SINFO, &sysinfo, sizeof(struct sysinfo)) == -1) {
    perror("sysmp");
    return;
  }
  /* copy sysinfo.cpu to an array of longs, as expected by percentages() */
  for (i = 0; i < CPUSTATES; i++) {
    cp_new[i] = sysinfo.cpu[i];
  }
  (void) percentages (CPUSTATES, cpu_states, cp_new, cp_old, cp_diff);

  si->cpustates = cpu_states;
  si->memory = memory_stats;

  numcpus = sysmp(MP_NPROCS);

  /* add a slash to the "run" state abbreviation */
  if (numcpus > 1) {
    state_abbrev[SRUN][3] = '/';
  }

  return;
}

caddr_t get_process_info(si, sel, x)
     struct system_info *si;
     struct process_select *sel;
     int x;
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

  /* read all the proc structures */
  getptable (pbase);

  /* get a pointer to the states summary array */
  si->procstates = process_states;

  /* set up flags which define what we are going to select */
  show_idle = sel->idle;
  show_system = sel->system;
  show_uid = sel->uid != -1;

  /* count up process states and get pointers to interesting procs */
  total_procs = 0;
  active_procs = 0;
  (void) memset (process_states, 0, sizeof (process_states));
  prefp = pref;

  for (pp = pbase, i = 0; i < nproc; pp++, i++)    {
    /*
     *  Place pointers to each valid proc structure in pref[].
     *  Process slots that are actually in use have a non-zero
     *  status field.  Processes with SSYS set are system
     *  processes---these get ignored unless show_system is set.
     */
    if (pp->pr_state != 0 &&
	(show_system || ((pp->pr_flag & SSYS) == 0))) {
      total_procs++;
      process_states[pp->pr_state]++;
      if ((!pp->pr_zomb) &&
	  (show_idle || (pp->pr_state == SRUN)) &&
	  (!show_uid || pp->pr_uid == (uid_t) sel->uid))  {
	*prefp++ = pp;
	active_procs++;
      }
    }
  }

  /* if requested, sort the "interesting" processes */
  if (compare != NULL)
    qsort ((char *) pref, active_procs, sizeof (struct prpsinfo *), proc_compare);

  /* remember active and total counts */
  si->p_total = total_procs;
  si->p_active = active_procs;

  /* pass back a handle */
  handle.next_proc = pref;
  handle.remaining = active_procs;
  return((caddr_t)&handle);
}

char *format_next_process(handle, get_userid)
     caddr_t handle;
     char *(*get_userid)();

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
  pctcpu = percent_cpu (pp);

  if (numcpus > 1) {
	if (pp->pr_sonproc < 0)
		state_abbrev[SRUN][4] = '*';
	else
		state_abbrev[SRUN][4] = pp->pr_sonproc + '0';
  }

  /* format this entry */
  sprintf (fmt,
	   Proc_format,
	   pp->pr_pid,
	   (*get_userid) (pp->pr_uid),
	   pp->pr_pri - PZERO,
	   pp->pr_nice - NZERO,
	   format_k(pagetok(pp->pr_size)),
	   format_k(pagetok(pp->pr_rssize)),
	   state_abbrev[pp->pr_state],
	   format_time(cputime),
	   weighted_cpu (pp),
	   pctcpu,
	   pp->pr_fname);

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

int getkval(offset, ptr, size, refstr)
     off_t offset;
     int *ptr;
     int size;
     char *refstr;

{
  if (lseek(kmem, offset, SEEK_SET) == -1) {
    if (*refstr == '!')
      refstr++;
    (void) fprintf(stderr, "%s: lseek to %s: %s\n", KMEM,
		   refstr, strerror(errno));
    quit(0);
  }
  if (read(kmem, (char *) ptr, size) == -1) {
    if (*refstr == '!')
      return(0);
    else {
      (void) fprintf(stderr, "%s: reading %s: %s\n", KMEM,
		     refstr, strerror(errno));
      quit(0);
    }
  }
  return(1);
}

/*
 *  proc_compare - comparison function for "qsort"
 *	Compares the resource consumption of two processes using five
 *  	distinct keys.  The keys (in descending order of importance) are:
 *  	percent cpu, cpu ticks, state, resident set size, total virtual
 *  	memory usage.  The process states are ordered as follows (from least
 *  	to most important):  WAIT, zombie, sleep, stop, idle, run.  The
 *  	array declaration below maps a process state index into a number
 *  	that reflects this ordering.
 */


unsigned char sorted_state[] =
{
  0,				/* not used		*/
  3,				/* sleep		*/
  6,				/* run			*/
  2,				/* zombie		*/
  4,				/* stop			*/
  5,				/* idle 		*/
  0,				/* not used             */
  1				/* being swapped (WAIT)	*/
};

int proc_compare (pp1, pp2)
     void *pp1;
     void *pp2;
{
  register struct prpsinfo *p1;
  register struct prpsinfo *p2;
  register long result;

  /* remove one level of indirection */
  p1 = *(struct prpsinfo **)pp1;
  p2 = *(struct prpsinfo **)pp2;

  /* compare percent cpu (pctcpu) */
  if ((result = (long) (p2->pr_cpu - p1->pr_cpu)) == 0) {
    /* use cpticks to break the tie */
    if ((result = p2->pr_time.tv_sec - p1->pr_time.tv_sec) == 0) {
      /* use process state to break the tie */
      if ((result = (long) (sorted_state[p2->pr_state] -
			    sorted_state[p1->pr_state])) == 0) {
	/* use priority to break the tie */
	if ((result = p2->pr_oldpri - p1->pr_oldpri) == 0)  {
	  /* use resident set size (rssize) to break the tie */
	  if ((result = p2->pr_rssize - p1->pr_rssize) == 0)  {
	    /* use total memory to break the tie */
	    result = (p2->pr_size - p1->pr_size);
	  }
	}
      }
    }
  }
  return (result);
}

/* return the owner of the specified process. */
int proc_owner (pid)
     int pid;
{
  register struct prpsinfo *p;
  int i;

  for (i = 0, p = pbase; i < nproc; i++, p++)
    if (p->pr_pid == (oid_t)pid)
      return ((int)p->pr_uid);

  return (-1);
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
  while (nlst->n_name != NULL)   {
      if (nlst->n_type == 0) {
	  /* this one wasn't found */
	  fprintf(stderr, "kernel: no symbol named `%s'\n", nlst->n_name);
	  i = 1;
	}
      nlst++;
    }

  return(i);
}

/* get process table */
void getptable (baseptr)
     struct prpsinfo *baseptr;
{
  struct prpsinfo *currproc;	/* pointer to current proc structure	*/
  int numprocs = 0;
  int i;
  struct dirent *directp;
  struct oldproc *op;
  static struct timeval lasttime =
  {0L, 0L};
  struct timeval thistime;
  struct timezone thiszone;
  double timediff;
  double alpha, beta;
  struct oldproc *endbase;

  gettimeofday (&thistime, &thiszone);

  /*
   * To avoid divides, we keep times in nanoseconds.  This is
   * scaled by 1e7 rather than 1e9 so that when we divide we
   * get percent.
   */
  if (lasttime.tv_sec)
    timediff = ((double) thistime.tv_sec * 1.0e7 +
		((double) thistime.tv_usec * 10.0)) -
      ((double) lasttime.tv_sec * 1.0e7 +
       ((double) lasttime.tv_usec * 10.0));
  else
    timediff = 1.0e7;

  /*
     * constants for exponential average.  avg = alpha * new + beta * avg
     * The goal is 50% decay in 30 sec.  However if the sample period
     * is greater than 30 sec, there's not a lot we can do.
     */
  if (timediff < 30.0e7)
    {
      alpha = 0.5 * (timediff / 30.0e7);
      beta = 1.0 - alpha;
    }
  else
    {
      alpha = 0.5;
      beta = 0.5;
    }

  endbase = oldbase + oldprocs;
  currproc = baseptr;


  for (rewinddir (procdir); directp = readdir (procdir);)
    {
      int fd;

      if ((fd = open (directp->d_name, O_RDONLY)) < 0)
	continue;

      currproc = &baseptr[numprocs];
      if (ioctl (fd, PIOCPSINFO, currproc) < 0)
	{
	  (void) close (fd);
	  continue;
	}

      /*
       * SVr4 doesn't keep track of CPU% in the kernel, so we have
       * to do our own.  See if we've heard of this process before.
       * If so, compute % based on CPU since last time.
       */
      op = oldbase + HASH (currproc->pr_pid);
      while (1)
	{
	  if (op->oldpid == -1)	/* not there */
	    break;
	  if (op->oldpid == currproc->pr_pid)
	    {			/* found old data */
	      percent_cpu (currproc) =
		((currproc->pr_time.tv_sec * 1.0e9 +
		  currproc->pr_time.tv_nsec)
		 - op->oldtime) / timediff;
	      weighted_cpu (currproc) =
		op->oldpct * beta + percent_cpu (currproc) * alpha;

	      break;
	    }
	  op++;			/* try next entry in hash table */
	  if (op == endbase)	/* table wrapped around */
	    op = oldbase;
	}

      /* Otherwise, it's new, so use all of its CPU time */
      if (op->oldpid == -1)
	{
	  if (lasttime.tv_sec)
	    {
	      percent_cpu (currproc) =
		(currproc->pr_time.tv_sec * 1.0e9 +
		 currproc->pr_time.tv_nsec) / timediff;
	      weighted_cpu (currproc) =
		percent_cpu (currproc);
	    }
	  else
	    {			/* first screen -- no difference is possible */
	      percent_cpu (currproc) = 0.0;
	      weighted_cpu (currproc) = 0.0;
	    }
	}

      numprocs++;
      (void) close (fd);
    }

  if (nproc != numprocs)
    nproc = numprocs;

  /*
   * Save current CPU time for next time around
   * For the moment recreate the hash table each time, as the code
   * is easier that way.
   */
  oldprocs = 2 * nproc;
  endbase = oldbase + oldprocs;
  for (op = oldbase; op < endbase; op++)
    op->oldpid = -1;
  for (i = 0, currproc = baseptr;
       i < nproc;
     i++, currproc = (struct prpsinfo *) ((char *) currproc + PRPSINFOSIZE))
    {
      /* find an empty spot */
      op = oldbase + HASH (currproc->pr_pid);
      while (1)
	{
	  if (op->oldpid == -1)
	    break;
	  op++;
	  if (op == endbase)
	    op = oldbase;
	}
      op->oldpid = currproc->pr_pid;
      op->oldtime = (currproc->pr_time.tv_sec * 1.0e9 +
		     currproc->pr_time.tv_nsec);
      op->oldpct = weighted_cpu (currproc);
    }
  lasttime = thistime;

}

