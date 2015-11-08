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
 * SYNOPSIS:  SCO UNIX OpenServer5
 *
 * DESCRIPTION:
 * This is the machine-dependent module for SCO OpenServer5.
 * Originally written for BSD4.3 system by Christos Zoulas.
 * Modified to m_sco.c (3.2v4.2)  by Gregory Shilin <shilin@onyx.co.il>
 * Modified to m_sco5.c (3.2v5.*) by Mike Hopkirk <hops@sco.com>
 * Works for:
 * SCO UNIX 3.2v5.*
 *
 * CFLAGS: -DHAVE_GETOPT -DORDER
 *
 * AUTHOR: Mike Hopkirk (hops@sco.com)
 * hops 10-Jul-98 - added sort fields
 *      17-Jul-98 - add philiph's chopped cmd string support
 *                    (define NO_COMMAND_ARGS to enable )
 *      09-Dec-98 - provide RSS calculation
 *      15-Mar-2000 - Fix broken lines and cleanup sysinfo access w macros
 */

#include <sys/types.h>
#include <sys/param.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <nlist.h>
#include <math.h>
#include <signal.h>
#include <string.h>

#include <sys/dir.h>
#include <sys/immu.h>
#include <sys/region.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/sysinfo.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/var.h>
#include <sys/sysi86.h>

#include "top.h"
#include "machine.h"
#include "utils.h"
#include "loadavg.h"

/*
typedef unsigned long  ulong;
typedef unsigned int   uint;
typedef unsigned short ushort;
*/
typedef unsigned char  uchar;

#define VMUNIX  "/unix"
#define KMEM    "/dev/kmem"
#define MEM     "/dev/mem"

#define SI_ACTIVE(p)   p->p_active
#define SI_TOTAL(p)    p->p_total

/* get_process_info passes back a handle. This is what it looks like: */
struct handle {
   struct proc **next_proc; /* points to next valid proc pointer */
   int           remaining; /* number of pointers remaining */
};

/* define what weighted cpu is */
#define weighted_cpu(pct, pp) ((pp)->p_time == 0 ? 0.0 : \
			 ((pct) / (1.0 - exp((pp)->p_time * logcpu))))

#define bytetok(bytes) ((bytes) >> 10)

/* what we consider to be process size: */
#define PROCSIZE(up) bytetok(ctob((up)->u_tsize + (up)->u_dsize+(up)->u_ssize))

/* definitions for indices in the nlist array */
#define X_V             0  /* System configuration information */
#define X_PROC          1  /* process tables */
#define X_FREEMEM       2  /* current free memory */
#define X_AVAILRMEM     3  /* available resident (not swappable) mem in pages
*/
#define X_AVAILSMEM     4  /* available swappable memory in pages */
#define X_MAXMEM        5  /* maximum available free memory in clicks */
#define X_PHYSMEM       6  /* physical memory in clicks */
#define X_NSWAP         7  /* size of swap space in blocks */
#define X_HZ            8  /* ticks/second of the clock */
#define X_MPID          9  /* last process id */
#define X_SYSINFO       10 /* system information (cpu states) */
#define X_CUR_CPU       11

static struct nlist nlst[] = {
   { "v" },             /* 0 */
   { "proc" },          /* 1 */
   { "freemem" },       /* 2 */
   { "availrmem" },     /* 3 */
   { "availsmem" },     /* 4 */
   { "maxmem" },        /* 5 */
   { "physmem" },       /* 6 */
   { "nswap" },         /* 7 */
   { "Hz" },            /* 8 */
   { "mpid" },          /* 9 */
   { "sysinfo" },       /* 10 */
   { "cur_cpu" },       /* 11 */
   { NULL }
};

/*
 *  These definitions control the format of the per-process area
 */

static char header[] =
  "  PID X        PRI NICE   SIZE   RES  STATE   TIME  COMMAND";
/* 0123456   -- field to fill in starts at header+6 */
#define UNAME_START 6

#define Proc_format \
	"%5d %-8.8s %3d %4d  %5s %5dK %-5s %6s  %.28s"

static int kmem, mem;

static double logcpu;

/* these are retrieved from the kernel in _init */
static int   Hz;
static struct var   v;
static ulong proca;
static load_avg  cur_cpu;

/* these are for detailing the process states */
int process_states[8];
char *procstatenames[] = {
    "", " sleeping, ", " running, ", " zombie, ", " stopped, ",
    " created, ", " onproc, ", " xswapped, ",
    NULL
};

/* process state names for the "STATE" column of the display */
char *state_abbrev[] = {
   "", "sleep", "run", "zomb", "stop", "create", "onpr", "swap"
};

/* these are for calculating cpu state percentages */
#define CPUSTATES       5    /* definition from struct sysinfo */
static time_t cp_time[CPUSTATES];
static time_t cp_old[CPUSTATES];
static time_t cp_diff[CPUSTATES];

/* these are for detailing the cpu states */
int cpu_states[CPUSTATES];
char *cpustatenames[] = {
    "idle", "user", "system", "wait", "sxbrk",
    NULL
};

/* these are for detailing the memory statistics */
unsigned long memory_stats[6];
char *memorynames[] = {
    "K phys, ", "K max, ", "K free, ", "K lck, ", "K unlck, ",
    "K swap,", NULL
};

/* these are for keeping track of the proc array */
static int bytes;
static int pref_len;
static struct proc *pbase;
static struct proc **pref;

/* forward definitions for comparison functions */
int proc_compare();
int compare_cpu();
int compare_size();
int compare_time();

int (*proc_compares[])() = {
    proc_compare,   /* state, pri, time, size */
    compare_cpu,    /* cpu, time, state, pri, size */
    compare_size,   /* size, cpu, time, state pri  */
    compare_time,   /* time, cpu, state, pri, size */
/* compare_res,     /* res,  cpu, time, state pri  */
    NULL };

/* these are names given to allowed sorting orders -- first is default */
char *ordernames[]={"state", "cpu", "size", "time", NULL}; /*hops*/

/* useful externals */
extern int errno;
extern char *sys_errlist[];

long time();
long percentages();

int
machine_init(struct statics *statics)

{
ulong ptr;

   if ((kmem = open(KMEM, O_RDONLY)) == -1) {
      perror(KMEM);
      return -1;
   }
   if ((mem = open(MEM, O_RDONLY)) == -1) {
      perror(MEM);
      return -1;
   }

   /* get the list of symbols we want to access in the kernel */
   if (nlist(VMUNIX, nlst) == -1) {
      fprintf(stderr, "top: nlist failed\n");
      return -1;
   }
   /* make sure they were all found */
   /*ZZ
   if (check_nlist(nlst) > 0)
      return -1;
   */

   proca = nlst[X_PROC].n_value;

   /* get the symbol values out of kmem */
   (void) getkval(nlst[X_CUR_CPU].n_value, (int *)(&cur_cpu), sizeof(cur_cpu),
		  nlst[X_CUR_CPU].n_name);
   (void) getkval(nlst[X_HZ].n_value,      (int *)(&Hz),      sizeof(Hz),
		  nlst[X_HZ].n_name);
   (void) getkval(nlst[X_V].n_value,       (int *)(&v),       sizeof(v),
		  nlst[X_V].n_name);

   /* this is used in calculating WCPU -- calculate it ahead of time */
   logcpu = log(fabs(loaddouble(cur_cpu)));

   /* allocate space for proc structure array and array of pointers */
   bytes = v.v_proc * sizeof(struct proc);
   pbase = (struct proc *)malloc(bytes);
   pref  = (struct proc **)malloc(v.v_proc * sizeof(struct proc *));
   if (pbase == (struct proc *)NULL || pref == (struct proc **)NULL) {
      fprintf(stderr, "top: cannot allocate sufficient memory\n");
      return -1;
   }

   /* fill in the statics information */
   statics->procstate_names = procstatenames;
   statics->cpustate_names = cpustatenames;
   statics->memory_names = memorynames;
   statics->order_names = ordernames ;  /* hops */

   return 0;
}

char *
format_header(register char *uname_field)

{
    register char *ptr;

    ptr = header + UNAME_START;
    while (*uname_field != '\0')
    {
	*ptr++ = *uname_field++;
    }

    return(header);
}


/* philiph - get run ave fm /dev/table info */
static int
tab_avenrun(double runave[])
{
   FILE *fp = fopen("/dev/table/avenrun", "r");
   int i;

   for (i=0; i<3; i++)
      runave[i] = -1.0;

   if (fp==NULL)
      return -1;
   else
   {
      short rawave[3];

	if (fread(rawave, sizeof(short), 3, fp) !=3 )
	{
	    fclose(fp);
	    return -1;
	}
	else
	{
	    int i;

	    for (i=0; i<3; i++)
		runave[i] = (double) (rawave[i] / 256.0);

	    fclose(fp);
	    return 0;
	}
    }
}

struct pregion *
get_pregion(void *ptr)
{
    static struct pregion preg;
    long addr = (long)ptr;

   (void) getkval(addr , (struct pregion *)(&preg),
		    sizeof(struct pregion), "pregion" );
    return &preg;
}

struct region *
get_region(void *ptr)
{
    static struct region reg;
    long addr = (long)ptr;

   (void) getkval( addr , (struct region *)(&reg),
		    sizeof(struct region), "region" );
    return &reg;
}

static unsigned char shareable[RT_VM86 + 1];     /* 1 if shareable */

/*
 * sum private referenced pages,
 * treat shared pages depending on value of TREAT_SHARABLE_PAGES macro
 *      undefined : ignore (don't account for - default)
 *      1:  divide among # of references
 *      2:  accumulate as if private
 */
/* #define TREAT_SHAREABLE_PAGES 1 */
static long
proc_residentsize(struct proc *pp)
{
    struct pregion *prp;
    struct region *rp;
    long rtot = 0;
    long stot = 0;
    long s1tot = 0;

    /* init shareable region array */
    if (shareable[RT_STEXT] == 0 )
	shareable[RT_STEXT] = shareable[RT_SHMEM] = shareable[RT_MAPFILE] = 1
	;

    prp = pp->p_region;

    if ( prp == 0)
	return 0;

    for( ; prp && (prp = get_pregion((void *)(prp))) &&
	   prp->p_reg && (rp = get_region((void*)(prp->p_reg)));
	   prp = prp->p_next)
    {
	if (shareable[rp->r_type] )     /* account for shared pgs separately
	*/
	{
	    stot  += (rp->r_nvalid / rp->r_refcnt);
	    s1tot += rp->r_nvalid;
	}
	else
	    rtot += rp->r_nvalid;

    }
#if defined(TREAT_SHAREABLE_PAGES) && TREAT_SHAREABLE_PAGES == 1
	rtot += stot;           /* accumulate and spread over users */
#endif

#if defined(TREAT_SHAREABLE_PAGES) && TREAT_SHAREABLE_PAGES == 1
	rtot += s1tot;          /* accumulate as if private */
#endif

    return rtot * NBPP/1024; ;
}

void
get_system_info(struct system_info *si)

{
long total;

   /* get process id of the last process */
   (void) getkval(nlst[X_MPID].n_value,  &(si->last_pid),
   sizeof(si->last_pid),
		  nlst[X_MPID].n_name);
   /* get the cp_time array */
   (void) getkval(nlst[X_SYSINFO].n_value, (int *)cp_time, sizeof(cp_time),
		  nlst[X_SYSINFO].n_name);

   /* convert cp_time counts to persentages */
   total = percentages(CPUSTATES, cpu_states, cp_time, cp_old, cp_diff);

   /* sum memory statistics */
   (void) getkval(nlst[X_PHYSMEM].n_value, &memory_stats[0],
		  sizeof(memory_stats[0]), nlst[X_PHYSMEM].n_name);
   (void) getkval(nlst[X_MAXMEM].n_value, &memory_stats[1],
		  sizeof(memory_stats[1]), nlst[X_MAXMEM].n_name);
   (void) getkval(nlst[X_FREEMEM].n_value, &memory_stats[2],
		  sizeof(memory_stats[2]), nlst[X_FREEMEM].n_name);
   (void) getkval(nlst[X_AVAILRMEM].n_value, &memory_stats[3],
		  sizeof(memory_stats[3]), nlst[X_AVAILRMEM].n_name);
   (void) getkval(nlst[X_AVAILSMEM].n_value, &memory_stats[4],
		  sizeof(memory_stats[4]), nlst[X_AVAILSMEM].n_name);
   (void) getkval(nlst[X_NSWAP].n_value, &memory_stats[5],
		  sizeof(memory_stats[5]), nlst[X_NSWAP].n_name);
   memory_stats[0] = bytetok(ctob(memory_stats[0]));    /* clicks -> bytes
   */
   memory_stats[1] = bytetok(ctob(memory_stats[1]));    /* clicks -> bytes
   */
   memory_stats[2] = bytetok(ctob(memory_stats[2]));    /* clicks -> bytes
   */
   memory_stats[3] = bytetok(memory_stats[3] * NBPP);   /* # bytes per page
   */
   memory_stats[4] = bytetok(memory_stats[4] * NBPP);   /* # bytes per page
   */
   memory_stats[5] = bytetok(memory_stats[5] * NBPSCTR);/* # bytes per sector
   */

   /* set arrays and strings */
   /* Note: we keep memory_stats as an unsigned long to avoid sign
      extension problems when shifting in bytetok. But the module
      interface requires an array of signed longs. So we just cast
      the pointer here and hope for the best.   --wnl
   */
   si->cpustates = cpu_states;
   si->memory = (long *)memory_stats;

   tab_avenrun(si->load_avg);   /* philiph */
}

static struct handle handle;

caddr_t
get_process_info(struct system_info *si,
		 struct process_select *sel,
		 int idx)

{
register int i;
register int total_procs;
register int active_procs;
register struct proc **prefp;
register struct proc *pp;

/* set up flags of what we are going to select */
/* these are copied out of sel for simplicity */
int show_idle = sel->idle;
int show_system = sel->system;
int show_uid = sel->uid != -1;
int show_command = sel->command != NULL;

   /* read all the proc structures in one fell swoop */
   (void) getkval(proca, (int *)pbase, bytes, "proc array");

   /* get a pointer to the states summary array */
   si->procstates = process_states;

   /* count up process states and get pointers to interesting procs */
   total_procs = active_procs = 0;
   memset((char *)process_states, 0, sizeof(process_states));
   prefp = pref;
   for (pp = pbase, i = 0; i < v.v_proc; pp++, i++) {
      /*
       * Place pointers to each valid proc structure in pref[].
       * Process slots that are actually in use have a non-zero
       * status field. Processes with SSYS set are system processes --
       * these are ignored unless show_system is set.
       */
      if (pp->p_stat && (show_system || ((pp->p_flag & SSYS) == 0))) {
	 total_procs++;
	 process_states[pp->p_stat]++;
	 if ((pp->p_stat != SZOMB) &&
	     (show_idle || (pp->p_stat == SRUN) || (pp->p_stat == SONPROC)) &&
	     (!show_uid || pp->p_uid == (ushort)sel->uid)) {
		*prefp++ = pp;
		active_procs++;
	 }
      }
   }

   /* if requested, sort the "interesting" processes */
   qsort((char *)pref, active_procs, sizeof(struct proc *), proc_compares[idx]);

   /* remember active and total counts */
   SI_TOTAL(si)  = total_procs;
   SI_ACTIVE(si) = pref_len = active_procs;

   /* pass back a handle */
   handle.next_proc = pref;
   handle.remaining = active_procs;
   return((caddr_t)&handle);
}

char fmt[128];          /* static area where result is built */

char *
format_next_process(caddr_t handle, char *(*get_userid)())

{
register struct proc *pp;
register time_t cputime;
register double pct;
int where;
struct user u;
struct handle *hp;
char command[29];
char * process;
char * process2;

   /* find and remember the next proc structure */
   hp = (struct handle *)handle;
   pp = *(hp->next_proc++);
   hp->remaining--;

   /* get the process's user struct and set cputime */
   if ((where = sysi86(RDUBLK, pp->p_pid, &u, sizeof(struct user))) != -1)
      where = (pp->p_flag & SLOAD) ? 0 : 1;
   if (where == -1) {
      strcpy(command, "<swapped>");
      cputime = 0;
   } else {
      /* set u_comm for system processes */
      if (u.u_comm[0] == '\0') {
	 if (pp->p_pid == 0)
	    strcpy(command, "Swapper");
	 else if (pp->p_pid == 2)
	    strcpy(command, "Pager");
	 else if (pp->p_pid == 3)
	    strcpy(command, "Sync'er");
      } else if (where == 1) {
	 /* print swapped processes as <pname> */
	 register char *s1;

	 u.u_psargs[28 - 3] = '\0';
	 strcpy(command, "<");
	 strcat(command, strtok(u.u_psargs, " "));
	 strcat(command, ">");
	 while (s1 = (char *)strtok(NULL, " "))
	    strcat(command, s1);
      } else {
	 sprintf(command, "%s", u.u_psargs);
      }
    cputime = u.u_utime + u.u_stime;
/*     cputime = pp->p_utime + pp->p_stime;  */
   }
   /* calculate the base for cpu percentages */
   pct = pctdouble(pp->p_cpu);

   /*
    * psargs gives the absolute path of the process... strip it to only the
    * command - [Changes by D. Currie & M. Muldner Aitt NS Canada]
    */
    process = printable(command);
#if NO_COMMAND_ARGS
    strtok(process," ");
#endif
    process2 = strrchr(process,'/');
    if(process2)
    {
	process = process2;
	process++;
    }


   /* format this entry */
   sprintf(fmt,
	   Proc_format,
	   pp->p_pid,
	   (*get_userid)(pp->p_uid),
	   pp->p_pri - PZERO,
	   pp->p_nice - NZERO,
	   format_k(PROCSIZE(&u)),  /* same as  pp->p_size * 4 */
	   proc_residentsize(pp),
	   state_abbrev[pp->p_stat],
	   format_time(cputime / Hz),
	   printable(process) );

   return(fmt);
}

/*
 * Checks the nlist to see if any symbols were not found.
 * For every symbol that was not found, a one-line message
 * is printed to stderr. The routine returns the number of
 * symbols NOT founded.
 */

int check_nlist(register struct nlist *nlst)

{
register int i = 0;

   while (nlst->n_name) {
      if (nlst->n_type == 0) {
	 fprintf(stderr, "kernel: no symbol named `%s'\n", nlst->n_name);
	 i++;
      }
      nlst++;
   }
   return i;
}

/*
 *  getkval(offset, ptr, size, refstr) - get a value out of the kernel.
 *      "offset" is the byte offset into the kernel for the desired value,
 *      "ptr" points to a buffer into which the value is retrieved,
 *      "size" is the size of the buffer (and the object to retrieve),
 *      "refstr" is a reference string used when printing error meessages,
 *          if "refstr" starts with a '!', then a failure on read will not
 *          be fatal (this may seem like a silly way to do things, but I
 *          really didn't want the overhead of another argument).
 *
 */

int
getkval(unsigned long offset, int *ptr, int size, char *refstr)

{
   if (lseek(kmem, (long)offset, SEEK_SET) == -1) {
      if (*refstr == '!')
	 refstr++;
      fprintf(stderr, "%s: lseek to %s: %s\n", KMEM,
	       refstr, errmsg(errno));
      quit(23);
   }
   if (read(kmem, (char *)ptr, size) == -1) {
      if (*refstr == '!')
	 return 0;
      fprintf(stderr, "%s: reading %s: %s\n", KMEM,
	       refstr, errmsg(errno));
      quit(23);
   }
   return(1);
}

/* comparison routine for qsort */
/* NOTE: this is specific to the BSD proc structure, but it should
   give you a good place to start. */

/*
 *  proc_compare - comparison function for "qsort"
 *      Compares the resource consumption of two processes using five
 *      distinct keys.  The keys (in descending order of importance) are:
 *      percent cpu, cpu ticks, state, resident set size, total virtual
 *      memory usage.  The process states are ordered as follows (from least
 *      to most important):  WAIT, zombie, sleep, stop, start, run.  The
 *      array declaration below maps a process state index into a number
 *      that reflects this ordering.
 */

static unsigned char sorted_state[] =
{
    0,  /* not used             */
    5,  /* sleep                */
    6,  /* run                  */
    2,  /* zombie               */
    4,  /* stop                 */
    1,  /* start                */
    7,  /* onpr                 */
    3,  /* swap                 */
};

int
proc_compare(struct proc **pp1, struct proc **pp2)

{
register struct proc *p1;
register struct proc *p2;
register int result;
register ulong lresult;

   /* remove one level of indirection */
   p1 = *pp1;
   p2 = *pp2;

   /* use process state to break the tie */
   if ((result = sorted_state[p2->p_stat] -
		 sorted_state[p1->p_stat])  == 0)
   {
      /* use priority to break the tie */
      if ((result = p2->p_pri - p1->p_pri) == 0)
      {
	 /* use time to break the tie */
	 if ((result = (p2->p_utime + p2->p_stime) -
		       (p1->p_utime + p1->p_stime)) == 0)
	 {
	    /* use resident set size (rssize) to break the tie */
	    if ((result = p2->p_size - p1->p_size) == 0)
	    {
	       result = 0;
	    }
	 }
      }
   }

    return(result);
}

/* returns uid of owner of process pid */
int
proc_owner(int pid)

{
register int cnt;
register struct proc **prefp;
register struct proc  *pp;

   prefp = pref;
   cnt = pref_len;
   while (--cnt >= 0) {
      if ((pp = *prefp++)->p_pid == (short)pid)
	 return ((int)pp->p_uid);
   }
   return(-1);
}

#if 0
int setpriority(int dummy, int who, int nicewal)
{
   errno = 1;
   return -1;
}
#endif

/* sigblock is not POSIX conformant */
sigset_t sigblock (sigset_t mask)
{
sigset_t oset;

   sigemptyset(&oset);
   sigprocmask(SIG_BLOCK, &mask, &oset);
   return oset;
}

/* sigsetmask is not POSIX conformant */
sigsetmask(sigset_t mask)
{
sigset_t oset;

   sigemptyset(&oset);
   sigprocmask(SIG_SETMASK, &mask, &oset);
   return oset;
}


/* ---------------- hops - comparison/ordering support ---------------- */

#define ORDERKEY_PCTCPU  if (dresult = pctdouble(p2->p_cpu) - pctdouble(p1->p_cpu),\
			     (result = dresult > 0.0 ? 1 : dresult < 0.0 ? -1 : 0) == 0)
#define ORDERKEY_MEMSIZE if ((result = (p2->p_size - p1->p_size)) == 0)
#define ORDERKEY_CPTIME  if ((result = (long)(p2->p_utime + p2->p_stime) -\
				       (long)(p1->p_utime + p1->p_stime)) == 0)

#define ORDERKEY_STATE   if ((result = (sorted_state[p2->p_stat] - \
			       sorted_state[p1->p_stat])) == 0)
#define ORDERKEY_PRIO    if ((result = p2->p_pri - p1->p_pri) == 0)


int
compare_cpu (   struct proc **pp1, struct proc **pp2)
{
    register struct proc *p1;
    register struct proc *p2;
    register int result;
    double dresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    ORDERKEY_PCTCPU
    ORDERKEY_CPTIME
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_MEMSIZE
    ;

    return (result);
}



/* compare_size - the comparison function for sorting by process size */
int
compare_size ( struct proc **pp1, struct proc **pp2)
{
    register struct proc *p1;
    register struct proc *p2;
    register int result;
    double dresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;


    ORDERKEY_MEMSIZE
    ORDERKEY_PCTCPU
    ORDERKEY_CPTIME
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ;

    return (result);
}

/* compare_res - the comparison function for sorting by resident set size */
/* TODO: add shadow proc struct updating usr + sys times and RSS for use
 * in comparison rtns, implement compare_res rtn as per compare_size()
 */

/* compare_time - the comparison function for sorting by total cpu time */
/* This is giving wrong results since its using the proc structure vals not
 * the u struct vals we display above
 * TODO: add shadow proc struct updating usr + sys times and RSS for use
 * in comparison rtns
 */
int
compare_time ( struct proc **pp1, struct proc **pp2)
{
    register struct proc *p1;
    register struct proc *p2;
    register int result;
    double dresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    ORDERKEY_CPTIME
    ORDERKEY_PCTCPU
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_MEMSIZE
    ;

    return (result);
}

