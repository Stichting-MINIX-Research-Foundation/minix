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
 * SYNOPSIS:  Intel based System V Release 4
 *
 * DESCRIPTION:
 *	System V release 4.0.x for i486
 *	System V release 4     for Okidata M88100
 *	System V release 4     for NCR 3000 series OS Rel 1.00 to 2.02
 *	System V release 4     for NCR 3000 series OS Rel 02.03.00 and above
 *	and probably other svr4 ports
 *
 * LIBS:  -lelf
 *
 * AUTHORS:  Andrew Herbert     <andrew@werple.apana.org.au>
 *           Robert Boucher     <boucher@sofkin.ca>
 * Ported to System 3000 Release 2.03 by:
 * 	     Jeff Janvrin       <jeff.janvrinColumbiaSC.NCR.COM>
 */

#include "top.h"
#include "machine.h"
#include "utils.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <nlist.h>
#include <string.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/procfs.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/vmmeter.h>
#include <vm/anon.h>
#include <sys/priocntl.h>
#include <sys/rtpriocntl.h>
#include <sys/tspriocntl.h>
#include <sys/procset.h>
#include <sys/var.h>

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

#define loaddouble(x) ((double)(x) / FSCALE)
#define percent_cpu(x) ((double)(x)->pr_cpu / FSCALE)
#define weighted_cpu(pct, pp) ( ((pp)->pr_time.tv_sec) == 0 ? 0.0 : \
        ((pp)->pr_cpu) / ((pp)->pr_time.tv_sec) )
#define pagetok(size) ctob(size) >> LOG1024

/* definitions for the index in the nlist array */
#define X_AVENRUN	0
#define X_MPID		1
#define X_V		2
#define X_NPROC		3
#define X_ANONINFO	4
#define X_TOTAL		5
#define X_SYSINFO	6

static struct nlist nlst[] =
{
{"avenrun"},			/* 0 */
{"mpid"},			/* 1 */
{"v"},			/* 2 */
{"nproc"},			/* 3 */
{"anoninfo"},			/* 4 */
{"total"},			/* 5 */
{"sysinfo"},			/* 6 */
{NULL}
};

static unsigned long avenrun_offset;
static unsigned long mpid_offset;
static unsigned long nproc_offset;
static unsigned long anoninfo_offset;
static unsigned long total_offset;
static unsigned long sysinfo_offset;

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
"  PID X        PRI NICE  SIZE   RES STATE   TIME   WCPU    CPU COMMAND";
/* 0123456   -- field to fill in starts at header+6 */
#define UNAME_START 6
#define Proc_format \
	"%5d %-8.8s %3d %4d %5s %5s %-5s %6s %3d.0%% %5.2f%% %.16s"

char *state_abbrev[] =
{"", "sleep", "run", "zombie", "stop", "start", "cpu", "swap"};

int process_states[8];
char *procstatenames[] =
{
  "", " sleeping, ", " running, ", " zombie, ", " stopped, ",
  " starting, ", " on cpu, ", " swapped, ",
  NULL
};

int cpu_states[CPUSTATES];
char *cpustatenames[] =
{"idle", "user", "kernel", "wait", "swap", NULL};

/* these are for detailing the memory statistics */

long memory_stats[5];
char *memorynames[] =
{"K real, ", "K active, ", "K free, ", "K swap, ", "K free swap", NULL};

/* forward reference for qsort comparison function */
int proc_compare();

static int kmem = -1;
static int nproc;
static int bytes;
static int use_stats = 0;
static struct prpsinfo *pbase;
static struct prpsinfo **pref;
static DIR *proc_dir;

/* useful externals */
extern int errno;
extern char *sys_errlist[];
extern char *myname;
extern int check_nlist ();
extern int getkval ();
extern void perror ();
extern void getptable ();
extern void quit ();
extern int nlist ();

int
machine_init (struct statics *statics)
  {
    static struct var v;

    /* fill in the statics information */
    statics->procstate_names = procstatenames;
    statics->cpustate_names = cpustatenames;
    statics->memory_names = memorynames;

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

    /* get the symbol values out of kmem */
    /* NPROC Tuning parameter for max number of processes */
    (void) getkval (nlst[X_V].n_value, &v, sizeof (struct var), nlst[X_V].n_name);
    nproc = v.v_proc;

    /* stash away certain offsets for later use */
    mpid_offset = nlst[X_MPID].n_value;
    nproc_offset = nlst[X_NPROC].n_value;
    avenrun_offset = nlst[X_AVENRUN].n_value;
    anoninfo_offset = nlst[X_ANONINFO].n_value;
    total_offset = nlst[X_TOTAL].n_value;
/* JJ this may need to be changed */
    sysinfo_offset = nlst[X_SYSINFO].n_value;

    /* allocate space for proc structure array and array of pointers */
    bytes = nproc * sizeof (struct prpsinfo);
    pbase = (struct prpsinfo *) malloc (bytes);
    pref = (struct prpsinfo **) malloc (nproc * sizeof (struct prpsinfo *));

    /* Just in case ... */
    if (pbase == (struct prpsinfo *) NULL || pref == (struct prpsinfo **) NULL)
      {
	(void) fprintf (stderr, "%s: can't allocate sufficient memory\n", myname);
	return (-1);
      }

    if (!(proc_dir = opendir (PROCFS)))
      {
	(void) fprintf (stderr, "Unable to open %s\n", PROCFS);
	return (-1);
      }

    if (chdir (PROCFS))
      {				/* handy for later on when we're reading it */
	(void) fprintf (stderr, "Unable to chdir to %s\n", PROCFS);
	return (-1);
      }

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
  struct sysinfo sysinfo;
  static struct sysinfo *mpinfo = NULL;	/* array, per-processor sysinfo structures. */
  struct vmtotal total;
  struct anoninfo anoninfo;
  static long cp_old[CPUSTATES];
  static long cp_diff[CPUSTATES];	/* for cpu state percentages */
  static int num_cpus;
  static int fd_cpu = 0;
  register int i;

  if ( use_stats == 1) {
    if ( fd_cpu == 0 ) {
      if ((fd_cpu = open("/stats/cpuinfo", O_RDONLY)) == -1) {
        (void) fprintf (stderr, "%s: Open of /stats/cpuinfo failed\n", myname);
	quit(2);
      }
      if (read(fd_cpu, &num_cpus, sizeof(int)) != sizeof(int)) {
        (void) fprintf (stderr, "%s: Read of /stats/cpuinfo failed\n", myname);
	quit(2);
      }
      close(fd_cpu);
    }
    if (mpinfo == NULL) {
      mpinfo = (struct sysinfo *)calloc(num_cpus, sizeof(mpinfo[0]));
      if (mpinfo == NULL) {
        (void) fprintf (stderr, "%s: can't allocate space for per-processor sysinfos\n", myname);
        quit(12);
      }
    }
    /* Read the per cpu sysinfo structures into mpinfo struct. */
    read_sysinfos(num_cpus, mpinfo);
    /* Add up all of the percpu sysinfos to get global sysinfo */
    sysinfo_data(num_cpus, &sysinfo, mpinfo);
  } else {
    (void) getkval (sysinfo_offset, &sysinfo, sizeof (struct sysinfo), "sysinfo");
  }

  /* convert cp_time counts to percentages */
  (void) percentages (CPUSTATES, cpu_states, sysinfo.cpu, cp_old, cp_diff);

  /* get mpid -- process id of last process */
  (void) getkval (mpid_offset, &(si->last_pid), sizeof (si->last_pid),
		  "mpid");

  /* get load average array */
  (void) getkval (avenrun_offset, (int *) avenrun, sizeof (avenrun), "avenrun");

  /* convert load averages to doubles */
  for (i = 0; i < 3; i++)
    si->load_avg[i] = loaddouble (avenrun[i]);

  /* get total -- systemwide main memory usage structure */
  (void) getkval (total_offset, (int *) (&total), sizeof (total), "total");
  /* convert memory stats to Kbytes */
  memory_stats[0] = pagetok (total.t_rm);
  memory_stats[1] = pagetok (total.t_arm);
  memory_stats[2] = pagetok (total.t_free);
  (void) getkval (anoninfo_offset, (int *) (&anoninfo), sizeof (anoninfo),
		  "anoninfo");
  memory_stats[3] = pagetok (anoninfo.ani_max - anoninfo.ani_free);
  memory_stats[4] = pagetok (anoninfo.ani_max - anoninfo.ani_resv);

  /* set arrays and strings */
  si->cpustates = cpu_states;
  si->memory = memory_stats;
}

static struct handle handle;

caddr_t
get_process_info (
		   struct system_info *si,
		   struct process_select *sel,
		   int x)
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
  (void) getkval (nproc_offset, (int *) (&nproc), sizeof (nproc), "nproc");

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

  for (pp = pbase, i = 0; i < nproc; pp++, i++)
    {
      /*
	 *  Place pointers to each valid proc structure in pref[].
	 *  Process slots that are actually in use have a non-zero
	 *  status field.  Processes with SSYS set are system
	 *  processes---these get ignored unless show_sysprocs is set.
	 */
      if (pp->pr_state != 0 &&
	  (show_system || ((pp->pr_flag & SSYS) == 0)))
	{
	  total_procs++;
	  process_states[pp->pr_state]++;
	  if ((!pp->pr_zomb) &&
	      (show_idle || (pp->pr_state == SRUN) || (pp->pr_state == SONPROC)) &&
	      (!show_uid || pp->pr_uid == (uid_t) sel->uid))
	    {
	      *prefp++ = pp;
	      active_procs++;
	    }
	}
    }

  /* if requested, sort the "interesting" processes */
  qsort ((char *) pref, active_procs, sizeof (struct prpsinfo *), proc_compare);

  /* remember active and total counts */
  si->p_total = total_procs;
  si->p_active = active_procs;

  /* pass back a handle */
  handle.next_proc = pref;
  handle.remaining = active_procs;
  return ((caddr_t) & handle);
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
  pctcpu = percent_cpu (pp);

  /* format this entry */
  (void) sprintf (fmt,
		  Proc_format,
		  pp->pr_pid,
		  (*get_userid) (pp->pr_uid),
		  pp->pr_pri - PZERO,
		  pp->pr_nice - NZERO,
		  format_k(pagetok (pp->pr_size)),
		  format_k(pagetok (pp->pr_rssize)),
		  state_abbrev[pp->pr_state],
		  format_time(cputime),
		  (pp->pr_cpu & 0377),
		  100.0 * pctcpu,
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
  struct stat stat_buf;

  /* check to see if we got ALL the symbols we requested */
  /* this will write one line to stderr for every symbol not found */

  i = 0;
  while (nlst->n_name != NULL)
    {
      if (nlst->n_type == 0)
	{
	  if (strcmp("sysinfo", nlst->n_name) == 0)
	    {
		/* check to see if /stats file system exists. If so, 	*/
		/* ignore error. 					*/
		if ( !((stat("/stats/sysinfo", &stat_buf) == 0) && 
		  (stat_buf.st_mode & S_IFREG)) )
		  {
		    (void) fprintf (stderr, "kernel: no symbol named `%s'\n", nlst->n_name);
		    i = 1;
		  } else {
		    use_stats = 1;
		  }
	    } else {
			
	      /* this one wasn't found */
	      (void) fprintf (stderr, "kernel: no symbol named `%s'\n", nlst->n_name);
	      i = 1;
	    }
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
#ifdef MIPS
  if (lseek (kmem, (long) (offset & 0x7fffffff), 0) == -1)
#else
  if (lseek (kmem, (long) offset, 0) == -1)
#endif
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


unsigned char sorted_state[] =
{
  0,				/* not used		*/
  3,				/* sleep		*/
  6,				/* run			*/
  2,				/* zombie		*/
  4,				/* stop			*/
  5,				/* start		*/
  7,				/* run on a processor   */
  1				/* being swapped (WAIT)	*/
};

int
proc_compare (
	       struct prpsinfo **pp1,
	       struct prpsinfo **pp2)
{
    register struct prpsinfo *p1;
    register struct prpsinfo *p2;
    register long result;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    /* compare percent cpu (pctcpu) */
    if ((result = (long) (p2->pr_cpu - p1->pr_cpu)) == 0)
      {
	/* use cpticks to break the tie */
	if ((result = p2->pr_time.tv_sec - p1->pr_time.tv_sec) == 0)
	  {
	    /* use process state to break the tie */
	    if ((result = (long) (sorted_state[p2->pr_state] -
				  sorted_state[p1->pr_state])) == 0)
	      {
		/* use priority to break the tie */
		if ((result = p2->pr_oldpri - p1->pr_oldpri) == 0)
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

/*
get process table
*/
void
getptable (struct prpsinfo *baseptr)
{
  struct prpsinfo *currproc;	/* pointer to current proc structure	*/
  int numprocs = 0;
  struct dirent *direntp;

  for (rewinddir (proc_dir); direntp = readdir (proc_dir);)
    {
      int fd;

      if ((fd = open (direntp->d_name, O_RDONLY)) < 0)
	continue;

      currproc = &baseptr[numprocs];
      if (ioctl (fd, PIOCPSINFO, currproc) < 0)
	{
	  (void) close (fd);
	  continue;
	}

      numprocs++;
      (void) close (fd);
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
      return (p->pr_uid);

  return (-1);
}

#ifndef HAVE_SETPRIORITY
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
#endif

/****************************************************************
 * read_sysinfos() -						*
 *	Read all of the CPU specific sysinfo sturctures in from	*
 *	the /stats file system.					*
 ****************************************************************/
read_sysinfos(num_cpus, buf)
	int num_cpus;
	struct sysinfo	*buf;
{

	static int	fd1=0;	/* file descriptor for /stats/sysinfo */
	int		read_sz;

	/* Open /stats/sysinfo one time only and leave it open */
	if (fd1==0) { 
		if ((fd1 = open("/stats/sysinfo", O_RDONLY)) == -1)
			(void) fprintf (stderr, "%s: Open of /stats/sysinfo failed\n", myname);
	}
	/* reset the read pointer to the beginning of the file */
	if (lseek(fd1, 0L, SEEK_SET) == -1)
		(void) fprintf (stderr, "%s: lseek to beginning of /stats/sysinfo failed\n", myname);
	read_sz = num_cpus * sizeof(buf[0]);
	if (read(fd1, buf, read_sz) != read_sz)
		(void) fprintf (stderr, "%s: Read of /stats/sysinfo failed\n", myname);
}
		
/****************************************************************
 * sysinfo_data() -						*
 *	Add up all of the CPU specific sysinfo sturctures to	*
 *	make the GLOBAL sysinfo.				*
 ****************************************************************/
sysinfo_data(num_cpus, global_si, percpu_si)
	int num_cpus;
	struct sysinfo	*global_si;
	struct sysinfo	*percpu_si;
{
	struct sysinfo	*percpu_p;
	int		cpu, i, *global, *src;

	/* null out the global statistics from last sample */
	memset(global_si, 0, sizeof(struct sysinfo));

	percpu_p = (struct sysinfo *)percpu_si;
	for(cpu = 0; cpu < num_cpus; cpu++) {
		global = (int *)global_si;
		src = (int *)percpu_p;

		/* assume sysinfo ends on an int boundary */
		/* Currently, all of the struct sysinfo members are the same
		 * size as an int. If that changes, we may not be able to
		 * do this. But this should be safe.
		 */
		for(i=0; i<sizeof(struct sysinfo)/sizeof(int); i++) {
			*global++ += *src++;
		}
		percpu_p++;
	}
}
