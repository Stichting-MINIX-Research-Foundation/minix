/*
 * top - a top users display for Unix
 *
 * SYNOPSIS:  Any Sun running SunOS 5.x (Solaris 2.x)
 *
 * DESCRIPTION:
 * This is the machine-dependent module for SunOS 5.x (Solaris 2).
 * There is some support for MP architectures.
 * This makes top work on all revisions of SunOS 5 from 5.0
 * through 5.9 (otherwise known as Solaris 9).  It has not been
 * tested on SunOS 5.10.
 *
 * AUTHORS:      Torsten Kasch 		<torsten@techfak.uni-bielefeld.de>
 *               Robert Boucher		<boucher@sofkin.ca>
 * CONTRIBUTORS: Marc Cohen 		<marc@aai.com>
 *               Charles Hedrick 	<hedrick@geneva.rutgers.edu>
 *	         William L. Jones 	<jones@chpc>
 *               Petri Kutvonen         <kutvonen@cs.helsinki.fi>
 *	         Casper Dik             <casper.dik@sun.com>
 *               Tim Pugh               <tpugh@oce.orst.edu>
 */

#define _KMEMUSER

#include "os.h"
#include "utils.h"
#include "username.h"
#include "display.h"

#if (OSREV == 551)
#undef OSREV
#define OSREV 55
#endif

/*
 * Starting with SunOS 5.6 the data in /proc changed along with the
 * means by which it is accessed.  In this case we define USE_NEW_PROC.
 * Note that with USE_NEW_PROC defined the structure named "prpsinfo"
 * is redefined to be "psinfo".  This will be confusing as you read
 * the code.
 */

#if OSREV >= 56
#define USE_NEW_PROC
#endif

#if defined(USE_NEW_PROC)
#define _STRUCTURED_PROC 1
#define prpsinfo psinfo
#include <sys/procfs.h>
#define pr_fill pr_nlwp
/* the "px" macros are used where the actual member could be in a substructure */
#define px_state pr_lwp.pr_state
#define px_nice pr_lwp.pr_nice
#define px_pri pr_lwp.pr_pri
#define px_onpro pr_lwp.pr_onpro
#define ZOMBIE(p)	((p)->pr_nlwp == 0)
#define SIZE_K(p)	(long)((p)->pr_size)
#define RSS_K(p)	(long)((p)->pr_rssize)
#else
#define px_state pr_state
#define px_oldpri pr_oldpri
#define px_nice pr_nice
#define px_pri pr_pri
#define px_onpro pr_filler[5]
#define ZOMBIE(p)	((p)->pr_zomb)
#define SIZE_K(p)	(long)((p)->pr_bysize/1024)
#define RSS_K(p)	(long)((p)->pr_byrssize/1024)
#endif

#include "top.h"
#include "machine.h"
#include <limits.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <nlist.h>
#include <string.h>
#include <kvm.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/vm.h>
#include <sys/var.h>
#include <sys/cpuvar.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/priocntl.h>
#include <sys/tspriocntl.h>
#include <sys/processor.h>
#include <sys/resource.h>
#include <sys/swap.h>
#include <sys/stat.h>
#include <vm/anon.h>
#include <math.h>
#include <utmpx.h>
#include "utils.h"
#include "hash.h"

#if OSREV >= 53
#define USE_KSTAT
#endif
#ifdef USE_KSTAT
#include <kstat.h>
/*
 * Some kstats are fixed at 32 bits, these will be specified as ui32; some
 * are "natural" size (32 bit on 32 bit Solaris, 64 on 64 bit Solaris
 * we'll make those unsigned long)
 * Older Solaris doesn't define KSTAT_DATA_UINT32, those are always 32 bit.
 */
# ifndef KSTAT_DATA_UINT32
#  define ui32 ul
# endif
#endif

#define UNIX "/dev/ksyms"
#define KMEM "/dev/kmem"
#define PROCFS "/proc"
#define CPUSTATES     5
#ifndef PRIO_MIN
#define PRIO_MIN	-20
#endif
#ifndef PRIO_MAX
#define PRIO_MAX	20
#endif

#ifndef FSCALE
#define FSHIFT  8		/* bits to right of fixed binary point */
#define FSCALE  (1<<FSHIFT)
#endif /* FSCALE */

#define loaddouble(la) ((double)(la) / FSCALE)
#define dbl_align(x)	(((unsigned long)(x)+(sizeof(double)-1)) & \
						~(sizeof(double)-1))

/*
 * SunOS 5.4 and above track pctcpu in the proc structure as pr_pctcpu.
 * These values are weighted over one minute whereas top output prefers
 * a near-instantaneous measure of cpu utilization.  So we choose to
 * ignore pr_pctcpu: we calculate our own cpu percentage and store it in
 * one of the spare slots in the prinfo structure.
 */

#define percent_cpu(pp) (*(double *)dbl_align(&pp->pr_filler[0]))

/* definitions for indices in the nlist array */
#define X_V			 0
#define X_MPID			 1
#define X_ANONINFO		 2
#define X_MAXMEM		 3
#define X_FREEMEM		 4
#define X_AVENRUN		 5
#define X_CPU			 6
#define X_NPROC			 7
#define X_NCPUS		   	 8

static struct nlist nlst[] =
{
  {"v"},			/* 0 */	/* replaced by dynamic allocation */
  {"mpid"},			/* 1 */
#if OSREV >= 56
  /* this structure really has some extra fields, but the first three match */
  {"k_anoninfo"},		/* 2 */
#else
  {"anoninfo"},			/* 2 */
#endif
  {"maxmem"},			/* 3 */ /* use sysconf */
  {"freemem"},			/* 4 */	/* available from kstat >= 2.5 */
  {"avenrun"},			/* 5 */ /* available from kstat */
  {"cpu"},			/* 6 */ /* available from kstat */
  {"nproc"},			/* 7 */ /* available from kstat */
  {"ncpus"},			/* 8 */ /* available from kstat */
  {0}
};

static unsigned long avenrun_offset;
static unsigned long mpid_offset;
#ifdef USE_KSTAT
static kstat_ctl_t *kc = NULL;
static kid_t kcid = 0;
#else
static unsigned long *cpu_offset;
#endif
static unsigned long nproc_offset;
static unsigned long freemem_offset;
static unsigned long maxmem_offset;
static unsigned long anoninfo_offset;
static int maxfiles = 256;
#define MAXFILES 2048
static int *display_fields;
static int show_threads = 0;
static int show_fullcmd;

/* get_process_info passes back a handle.  This is what it looks like: */
struct handle
{
    struct prpsinfo **next_proc;/* points to next valid proc pointer */
    int remaining;		/* number of pointers remaining */
};

/*
 * Structure for keeping track processes between updates.
 * We keep these things in a hash table, which is updated at every cycle.  
 */
struct oldproc
{
    pid_t pid;
    id_t lwpid;
    double oldtime;
    double oldpct;
    uid_t  owner_uid;
    int fd_psinfo;
    int fd_lpsinfo;
    int seen;
};

#define TIMESPEC_TO_DOUBLE(ts) ((ts).tv_sec * 1.0e9 + (ts).tv_nsec)

hash_table *prochash;
hash_table *threadhash;

/*
 * Structure for tracking per-cpu information
 */
struct cpustats
{
    unsigned int states[CPUSTATES];
    uint_t pswitch;
    uint_t trap;
    uint_t intr;
    uint_t syscall;
    uint_t sysfork;
    uint_t sysvfork;
    uint_t pfault;
    uint_t pgin;
    uint_t pgout;
};

/*
 * GCC assumes that all doubles are aligned.  Unfortunately it
 * doesn't round up the structure size to be a multiple of 8.
 * Thus we'll get a coredump when going through array.  The
 * following is a size rounded up to 8.
 */
#define PRPSINFOSIZE dbl_align(sizeof(struct prpsinfo))

/* this defines one field (or column) in the process display */

struct proc_field {
    char *name;
    int width;
    int rjust;
    int min_screenwidth;
    int (*format)(char *, int, struct prpsinfo *);
};

#define PROCSTATES 8
/* process state names for the "STATE" column of the display */
char *state_abbrev[] =
{"", "sleep", "run", "zombie", "stop", "start", "cpu", "swap"};

int process_states[PROCSTATES];
char *procstatenames[] =
{
  "", " sleeping, ", " running, ", " zombie, ", " stopped, ",
  " starting, ", " on cpu, ", " swapped, ",
  NULL
};

int cpu_states[CPUSTATES];
char *cpustatenames[] =
{"idle", "user", "kernel", "iowait", "swap", NULL};
#define CPUSTATE_IOWAIT 3
#define CPUSTATE_SWAP   4


/* these are for detailing the memory statistics */
long memory_stats[5];
char *memorynames[] =
{"K phys mem, ", "K free mem, ", "K total swap, ", "K free swap", NULL};
#define MEMORY_TOTALMEM  0
#define MEMORY_FREEMEM   1
#define MEMORY_TOTALSWAP 2
#define MEMORY_FREESWAP  3

/* these are for detailing kernel statistics */
int kernel_stats[8];
char *kernelnames[] =
{" ctxsw, ", " trap, ", " intr, ", " syscall, ", " fork, ",
 " flt, ", " pgin, ", " pgout, ", NULL};
#define KERNEL_CSWITCH 0
#define KERNEL_TRAP 1
#define KERNEL_INTR 2
#define KERNEL_SYSCALL 3
#define KERNEL_FORK 4
#define KERNEL_PFAULT 5
#define KERNEL_PGIN 6
#define KERNEL_PGOUT 7

/* these are names given to allowed sorting orders -- first is default */
char *ordernames[] = 
{"cpu", "size", "res", "time", "pid", NULL};

/* forward definitions for comparison functions */
int compare_cpu();
int compare_size();
int compare_res();
int compare_time();
int compare_pid();

int (*proc_compares[])() = {
    compare_cpu,
    compare_size,
    compare_res,
    compare_time,
    compare_pid,
    NULL };

kvm_t *kd;
static DIR *procdir;

/* "cpucount" is used to store the value for the kernel variable "ncpus".
   But since <sys/cpuvar.h> actually defines a variable "ncpus" we need
   to use a different name here.   --wnl */
static int cpucount;

/* pagetok function is really a pointer to an appropriate function */
static int pageshift;
static long (*p_pagetok) ();
#define pagetok(size) ((*p_pagetok)(size))

/* useful externals */
extern char *myname;
extern void perror ();
extern int getptable ();
extern void quit ();

/* process formatting functions and data */

int
fmt_pid(char *buf, int sz, struct prpsinfo *pp)

{
    return snprintf(buf, sz, "%6d", (int)pp->pr_pid);
}

int
fmt_username(char *buf, int sz, struct prpsinfo *pp)

{
    return snprintf(buf, sz, "%-8.8s", username(pp->pr_uid));
}

int
fmt_uid(char *buf, int sz, struct prpsinfo *pp)

{
    return snprintf(buf, sz, "%6d", (int)pp->pr_uid);
}

int
fmt_nlwp(char *buf, int sz, struct prpsinfo *pp)

{
    return snprintf(buf, sz, "%4d", pp->pr_fill < 999 ? pp->pr_fill: 999);
}

int
fmt_pri(char *buf, int sz, struct prpsinfo *pp)

{
    return snprintf(buf, sz, "%3d", pp->px_pri);
}

int
fmt_nice(char *buf, int sz, struct prpsinfo *pp)

{
    return snprintf(buf, sz, "%4d", pp->px_nice - NZERO);
}

int
fmt_size(char *buf, int sz, struct prpsinfo *pp)

{
    return snprintf(buf, sz, "%5s", format_k(SIZE_K(pp)));
}

int
fmt_res(char *buf, int sz, struct prpsinfo *pp)

{
    return snprintf(buf, sz, "%5s", format_k(RSS_K(pp)));
}

int
fmt_state(char *buf, int sz, struct prpsinfo *pp)

{
    if (pp->px_state == SONPROC && cpucount > 1)
    {
	/* large #s may overflow colums */
	if (pp->px_onpro < 100)
	{
	    return snprintf(buf, sz, "cpu/%-2d", pp->px_onpro);
	}
	return snprintf(buf, sz, "cpu/**");
    }

    return snprintf(buf, sz, "%-6s", state_abbrev[(int)pp->px_state]);
}

int
fmt_time(char *buf, int sz, struct prpsinfo *pp)

{
    return snprintf(buf, sz, "%6s", format_time(pp->pr_time.tv_sec));
}

int
fmt_cpu(char *buf, int sz, struct prpsinfo *pp)

{
    return snprintf(buf, sz, "%5s%%",
		    format_percent(percent_cpu(pp) / cpucount));
}

int
fmt_command(char *buf, int sz, struct prpsinfo *pp)

{
    return snprintf(buf, sz, "%s",
		    printable(show_fullcmd ? pp->pr_psargs : pp->pr_fname));
}

int
fmt_lwp(char *buf, int sz, struct prpsinfo *pp)

{
    return snprintf(buf, sz, "%4d", ((int)pp->pr_lwp.pr_lwpid < 10000 ?
		(int)pp->pr_lwp.pr_lwpid : 9999));
}

struct proc_field proc_field[] = {
    { "PID", 6, 1, 0, fmt_pid },
    { "USERNAME", 8, 0, 0, fmt_username },
#define FIELD_USERNAME 1
    { "UID", 6, 1, 0, fmt_uid },
#define FIELD_UID 2
    { "NLWP", 4, 1, 0, fmt_nlwp },
    { "PRI", 3, 1, 0, fmt_pri },
    { "NICE", 4, 1, 0, fmt_nice },
    { "SIZE", 5, 1, 0, fmt_size },
    { "RES", 5, 1, 0, fmt_res },
    { "STATE", 6, 0, 0, fmt_state },
    { "TIME", 6, 1, 0, fmt_time },
    { "CPU", 6, 1, 0, fmt_cpu },
    { "COMMAND", 7, 0, 0, fmt_command },
    { "LWP", 4, 1, 0, fmt_lwp },
};
#define MAX_FIELDS 13

static int proc_display[MAX_FIELDS];
static int thr_display[MAX_FIELDS];

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

/* p_pagetok points to one of the following, depending on which
   direction data has to be shifted: */

long pagetok_none(long size)

{
    return(size);
}

long pagetok_left(long size)

{
    return(size << pageshift);
}

long pagetok_right(long size)

{
    return(size >> pageshift);
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
getkval (unsigned long offset,
	 int *ptr,
	 int size,
	 char *refstr)
{
    dprintf("getkval(%08x, %08x, %d, %s)\n", offset, ptr, size, refstr);

    if (kvm_read (kd, offset, (char *) ptr, size) != size)
    {
	dprintf("getkval: read failed\n");
	if (*refstr == '!')
	{
	    return (0);
	}
	else
	{
	    fprintf (stderr, "top: kvm_read for %s: %s\n", refstr, strerror(errno));
	    quit (23);
	}
    }

    dprintf("getkval read %d (%08x)\n", *ptr);

    return (1);

}

/* procs structure memory management */

static struct prpsinfo **allprocs = NULL;
static struct prpsinfo **nextproc = NULL;
static int maxprocs = 0;
static int idxprocs = 0;

/*
 * void procs_prealloc(int cnt)
 *
 * Preallocate "cnt" procs structures.  If "cnt" is less than or equal
 * to procs_max() then this function has no effect.
 */

void
procs_prealloc(int max)

{
    int cnt;
    struct prpsinfo *new;
    struct prpsinfo **pp;

    cnt = max - maxprocs;
    if (cnt > 0)
    {
	dprintf("procs_prealloc: need %d, deficit %d\n", max, cnt);
	allprocs = (struct prpsinfo **)
	    realloc((void *)allprocs, max * sizeof(struct prpsinfo *));
	pp = nextproc = allprocs + idxprocs;
	new = (struct prpsinfo *)malloc(cnt * PRPSINFOSIZE);
	dprintf("procs_prealloc: idxprocs %d, allprocs %08x, nextproc %08x, new %08x\n",
		idxprocs, allprocs, nextproc, new);
	while (--cnt >= 0)
	{
	    *pp++ = new;
	    new = (struct prpsinfo *) ((char *)new + PRPSINFOSIZE);
	}
	dprintf("procs_prealloc: done filling at %08x\n", new);
	maxprocs = max;
    }
}

/*
 * struct prpsinfo *procs_next()
 *
 * Return the next available procs structure, allocating a new one
 * if needed.
 */

struct prpsinfo *
procs_next()

{
    if (idxprocs >= maxprocs)
    {
	/* allocate some more */
	procs_prealloc(maxprocs + 128);
    }
    idxprocs++;
    return *nextproc++;
}

struct prpsinfo *
procs_dup(struct prpsinfo *p)

{
    struct prpsinfo *n;

    n = procs_next();
    memcpy(n, p, PRPSINFOSIZE);
    return n;
}

/*
 * struct prpsinfo *procs_start()
 *
 * Return the first procs structure.
 */

struct prpsinfo *
procs_start()

{
    idxprocs = 0;
    nextproc = allprocs;
    return procs_next();
}

/*
 * int procs_max()
 *
 * Return the maximum number of procs structures currently allocated.
 */

int
procs_max()

{
    return maxprocs;
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
	if (nlst->n_type == 0)
	{
	    /* this one wasn't found */
	    fprintf (stderr, "kernel: no symbol named `%s'\n", nlst->n_name);
	    i = 1;
	}
	nlst++;
    }
    return (i);
}


char *
format_header (register char *uname_field)
{
  return ("");
}

#ifdef USE_KSTAT

long
kstat_data_value_l(kstat_named_t *kn)

{
#ifdef KSTAT_DATA_UINT32
    switch(kn->data_type)
    {
    case KSTAT_DATA_INT32:
	return ((long)(kn->value.i32));
    case KSTAT_DATA_UINT32:
	return ((long)(kn->value.ui32));
    case KSTAT_DATA_INT64:
	return ((long)(kn->value.i64));
    case KSTAT_DATA_UINT64:
	return ((long)(kn->value.ui64));
    }
    return 0;
#else
    return ((long)(kn->value.ui32));
#endif
}

int
kstat_safe_retrieve(kstat_t **ksp,
		    char *module, int instance, char *name, void *buf)

{
    kstat_t *ks;
    kid_t new_kcid;
    int changed;

    dprintf("kstat_safe_retrieve(%08x -> %08x, %s, %d, %s, %08x)\n",
	    ksp, *ksp, module, instance, name, buf);

    ks = *ksp;
    do {
	changed = 0;
	/* if we dont already have the kstat, retrieve it */
	if (ks == NULL)
	{
	    if ((ks = kstat_lookup(kc, module, instance, name)) == NULL)
	    {
		return (-1);
	    }
	    *ksp = ks;
	}

	/* attempt to read it */
	new_kcid = kstat_read(kc, ks, buf);
	/* chance for an infinite loop here if kstat_read keeps 
	   returning -1 */

	/* if the chain changed, update it */
	if (new_kcid != kcid)
	{
	    dprintf("kstat_safe_retrieve: chain changed to %d...updating\n",
		    new_kcid);
	    changed = 1;
	    kcid = kstat_chain_update(kc);
	}
    } while (changed);

    return (0);
}

/*
 * int kstat_safe_namematch(int num, kstat_t *ksp, char *name, void *buf)
 *
 * Safe scan of kstat chain for names starting with "name".  Matches
 * are copied in to "ksp", and kstat_read is called on each match using
 * "buf" as a buffer of length "size".  The actual number of records
 * found is returned.  Up to "num" kstats are copied in to "ksp", but
 * no more.  If any kstat_read indicates that the chain has changed, then
 * the whole process is restarted.
 */

int
kstat_safe_namematch(int num, kstat_t **ksparg, char *name, void *buf, int size)

{
    kstat_t *ks;
    kstat_t **ksp;
    kid_t new_kcid;
    int namelen;
    int count;
    int changed;
    char *cbuf;

    dprintf("kstat_safe_namematch(%d, %08x, %s, %08x, %d)\n",
	    num, ksparg, name, buf, size);

    namelen = strlen(name);

    do {
	/* initialize before the scan */
	cbuf = (char *)buf;
	ksp = ksparg;
	count = 0;
	changed = 0;

	/* scan the chain for matching kstats */
	for (ks = kc->kc_chain; ks != NULL; ks = ks->ks_next)
	{
	    if (strncmp(ks->ks_name, name, namelen) == 0)
	    {
		/* this kstat matches: save it if there is room */
		if (count++ < num)
		{
		    /* read the kstat */
		    new_kcid = kstat_read(kc, ks, cbuf);

		    /* if the chain changed, update it */
		    if (new_kcid != kcid)
		    {
			dprintf("kstat_safe_namematch: chain changed to %d...updating\n",
				new_kcid);
			changed = 1;
			kcid = kstat_chain_update(kc);

			/* there's no sense in continuing the scan */
			/* so break out of the for loop */
			break;
		    }

		    /* move to the next buffers */
		    cbuf += size;
		    *ksp++ = ks;
		}
	    }
	}
    } while(changed);

    dprintf("kstat_safe_namematch returns %d\n", count);

    return count;
}

static kstat_t *ks_system_misc = NULL;

#endif /* USE_KSTAT */


int
get_avenrun(int avenrun[3])

{
#ifdef USE_KSTAT
    int status;
    kstat_named_t *kn;

    dprintf("get_avenrun(%08x)\n", avenrun);

    if ((status = kstat_safe_retrieve(&ks_system_misc,
				      "unix", 0, "system_misc", NULL)) == 0)
    {
	if ((kn = kstat_data_lookup(ks_system_misc, "avenrun_1min")) != NULL)
	{
	    avenrun[0] = kn->value.ui32;
	}
	if ((kn = kstat_data_lookup(ks_system_misc, "avenrun_5min")) != NULL)
	{
	    avenrun[1] = kn->value.ui32;
	}
	if ((kn = kstat_data_lookup(ks_system_misc, "avenrun_15min")) != NULL)
	{
	    avenrun[2] = kn->value.ui32;
	}
    }
    dprintf("get_avenrun returns %d\n", status);
    return (status);

#else /* !USE_KSTAT */

    (void) getkval (avenrun_offset, (int *) avenrun, sizeof (int [3]), "avenrun");

    return 0;

#endif /* USE_KSTAT */
}

int
get_ncpus()

{
#ifdef USE_KSTAT
    kstat_named_t *kn;
    int ret = -1;

    if ((kn = kstat_data_lookup(ks_system_misc, "ncpus")) != NULL)
    {
	ret = (int)(kn->value.ui32);
    }

    return ret;
#else
    int ret;

    (void) getkval(nlst[X_NCPUS].n_value, (int *)(&ret), sizeof(ret), "ncpus");
    return ret;
#endif
}

int
get_nproc()

{
#ifdef USE_KSTAT
    kstat_named_t *kn;
    int ret = -1;

    if ((kn = kstat_data_lookup(ks_system_misc, "nproc")) != NULL)
    {
	ret = (int)(kn->value.ui32);
    }
#else
    int ret;

    (void) getkval (nproc_offset, (int *) (&ret), sizeof (ret), "nproc");
#endif

    dprintf("get_nproc returns %d\n", ret);
    return ret;
}

struct cpustats *
get_cpustats(int *cnt, struct cpustats *cpustats)

{
#ifdef USE_KSTAT
    static kstat_t **cpu_ks = NULL;
    static cpu_stat_t *cpu_stat = NULL;
    static unsigned int nelems = 0;
    cpu_stat_t *cpu_stat_p;
    int i, cpu_num;
    struct cpustats *cpustats_p;

    dprintf("get_cpustats(%d -> %d, %08x)\n", cnt, *cnt, cpustats);

    while (nelems > 0 ?
	   (cpu_num = kstat_safe_namematch(nelems,
					   cpu_ks,
					   "cpu_stat",
					   cpu_stat,
					   sizeof(cpu_stat_t))) > nelems :
	   (cpu_num = get_ncpus()) > 0)
    {
	/* reallocate the arrays */
	dprintf("realloc from %d to %d\n", nelems, cpu_num);
	nelems = cpu_num;
	if (cpu_ks != NULL)
	{
	    free(cpu_ks);
	}
	cpu_ks = (kstat_t **)calloc(nelems, sizeof(kstat_t *));
	if (cpu_stat != NULL)
	{
	    free(cpu_stat);
	}
	cpu_stat = (cpu_stat_t *)malloc(nelems * sizeof(cpu_stat_t));
    }

    /* do we have more cpus than our caller? */
    if (cpu_num > *cnt)
    {
	/* yes, so realloc their array, too */
	dprintf("realloc array from %d to %d\n", *cnt, cpu_num);
	*cnt = cpu_num;
	cpustats = (struct cpustats *)realloc(cpustats,
					      cpu_num * sizeof(struct cpustats));
    }

    cpu_stat_p = cpu_stat;
    cpustats_p = cpustats;
    for (i = 0; i < cpu_num; i++)
    {
	dprintf("cpu %d %08x: idle %u, user %u, syscall %u\n", i, cpu_stat_p,
		cpu_stat_p->cpu_sysinfo.cpu[0],
		cpu_stat_p->cpu_sysinfo.cpu[1],
		cpu_stat_p->cpu_sysinfo.syscall);

	cpustats_p->states[CPU_IDLE] = cpu_stat_p->cpu_sysinfo.cpu[CPU_IDLE];
	cpustats_p->states[CPU_USER] = cpu_stat_p->cpu_sysinfo.cpu[CPU_USER];
	cpustats_p->states[CPU_KERNEL] = cpu_stat_p->cpu_sysinfo.cpu[CPU_KERNEL];
	cpustats_p->states[CPUSTATE_IOWAIT] = cpu_stat_p->cpu_sysinfo.wait[W_IO] +
	    cpu_stat_p->cpu_sysinfo.wait[W_PIO];
	cpustats_p->states[CPUSTATE_SWAP] = cpu_stat_p->cpu_sysinfo.wait[W_SWAP];
	cpustats_p->pswitch = cpu_stat_p->cpu_sysinfo.pswitch;
	cpustats_p->trap = cpu_stat_p->cpu_sysinfo.trap;
	cpustats_p->intr = cpu_stat_p->cpu_sysinfo.intr;
	cpustats_p->syscall = cpu_stat_p->cpu_sysinfo.syscall;
	cpustats_p->sysfork = cpu_stat_p->cpu_sysinfo.sysfork;
	cpustats_p->sysvfork = cpu_stat_p->cpu_sysinfo.sysvfork;
	cpustats_p->pfault = cpu_stat_p->cpu_vminfo.hat_fault +
	    cpu_stat_p->cpu_vminfo.as_fault;
	cpustats_p->pgin = cpu_stat_p->cpu_vminfo.pgin;
	cpustats_p->pgout = cpu_stat_p->cpu_vminfo.pgout;
	cpustats_p++;
	cpu_stat_p++;
    }

    cpucount = cpu_num;

    dprintf("get_cpustats sees %d cpus and returns %08x\n", cpucount, cpustats);

    return (cpustats);
#else /* !USE_KSTAT */
    int i;
    struct cpu cpu;
    unsigned int (*cp_stats_p)[CPUSTATES];

    /* do we have more cpus than our caller? */
    if (cpucount > *cnt)
    {
	/* yes, so realloc their array, too */
	dprintf("realloc array from %d to %d\n", *cnt, cpucount);
	*cnt = cpucount;
	cp_stats = (unsigned int (*)[CPUSTATES])realloc(cp_stats,
			 cpucount * sizeof(unsigned int) * CPUSTATES);
    }

    cp_stats_p = cp_stats;
    for (i = 0; i < cpucount; i++)
    {
	if (cpu_offset[i] != 0)
	{
	    /* get struct cpu for this processor */
	    (void) getkval (cpu_offset[i], (int *)(&cpu), sizeof (struct cpu), "cpu");

	    (*cp_stats_p)[CPU_IDLE] = cpu.cpu_stat.cpu_sysinfo.cpu[CPU_IDLE];
	    (*cp_stats_p)[CPU_USER] = cpu.cpu_stat.cpu_sysinfo.cpu[CPU_USER];
	    (*cp_stats_p)[CPU_KERNEL] = cpu.cpu_stat.cpu_sysinfo.cpu[CPU_KERNEL];
	    (*cp_stats_p)[CPUSTATE_IOWAIT] = cpu.cpu_stat.cpu_sysinfo.wait[W_IO] +
		cpu.cpu_stat.cpu_sysinfo.wait[W_PIO];
	    (*cp_stats_p)[CPUSTATE_SWAP] = cpu.cpu_stat.cpu_sysinfo.wait[W_SWAP];
	    cp_stats_p++;
	}
    }

    return (cp_stats);
#endif /* USE_KSTAT */
}

/*
 * void get_meminfo(long *total, long *fr)
 *
 * Get information about the system's physical memory.  Pass back values
 * for total available and amount of memory that is free (in kilobytes).
 * It returns 0 on success and -1 on any kind of failure.
 */

int
get_meminfo(long *total, long *fr)

{
    long freemem;
    static kstat_t *ks = NULL;
    kstat_named_t *kn;

    /* total comes from sysconf */
    *total = pagetok(sysconf(_SC_PHYS_PAGES));

    /* free comes from the kernel's freemem or from kstat */
    /* prefer kmem for this because kstat unix:0:system_pages
       can be slow on systems with lots of memory */
    if (kd)
    {
	(void) getkval(freemem_offset, (int *)(&freemem), sizeof(freemem),
		       "freemem");
    }
    else
    {
#ifdef USE_KSTAT
	/* only need to grab kstat chain once */
	if (ks == NULL)
	{
	    ks = kstat_lookup(kc, "unix", 0, "system_pages");
	}

	if (ks != NULL &&
	    kstat_read(kc, ks, 0) != -1 &&
	    (kn = kstat_data_lookup(ks, "freemem")) != NULL)
	{
	    freemem = kstat_data_value_l(kn);
	}
	else
	{
	    freemem = -1;
	}
#else
	freemem = -1;
#endif
    }

    *fr = freemem == -1 ? -1 : pagetok(freemem);

    return (0);
}

/*
 * void get_swapinfo(long *total, long *fr)
 *
 * Get information about the system's swap.  Pass back values for
 * total swap available and amount of swap that is free (in kilobytes).
 * It returns 0 on success and -1 on any kind of failure.
 */

int
get_swapinfo(long *total, long *fr)

{
    register int cnt, i;
    register long t, f;
    struct swaptable *swt;
    struct swapent *ste;
    static char path[256];

    /* preset values to 0 just in case we have to return early */
    *total = 0;
    *fr = 0;

    /* get total number of swap entries */
    if ((cnt = swapctl(SC_GETNSWP, 0)) == -1)
    {
	return (-1);
    }

    /* allocate enough space to hold count + n swapents */
    swt = (struct swaptable *)malloc(sizeof(int) +
				     cnt * sizeof(struct swapent));
    if (swt == NULL)
    {
	return (-1);
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
    if (swapctl(SC_LIST, swt) == -1)
    {
	return (-1);
    }

    /* walk thru the structs and sum up the fields */
    t = f = 0;
    ste = &(swt->swt_ent[0]);
    i = cnt;
    while (--i >= 0)
    {
	/* dont count slots being deleted */
	if (!(ste->ste_flags & ST_INDEL) &&
	    !(ste->ste_flags & ST_DOINGDEL))
	{
	    t += ste->ste_pages;
	    f += ste->ste_free;
	}
	ste++;
    }

    /* fill in the results */
    *total = pagetok(t);
    *fr = pagetok(f);
    free(swt);

    /* good to go */
    return (0);
}

int
machine_init (struct statics *statics)
{
    struct utmpx ut;
    struct utmpx *up;
    struct rlimit rlim;
    int i;
    char *p;
    int *ip;
    int nproc;
#ifndef USE_KSTAT
    int offset;
#endif

    /* There's a buffer overflow bug in curses that can be exploited when
       we run as root.  By making sure that TERMINFO is set to something
       this bug is avoided.  This code thanks to Casper */
    if ((p = getenv("TERMINFO")) == NULL || *p == '\0')
    {
        putenv("TERMINFO=/usr/share/lib/terminfo/");
    }

    /* perform the kvm_open - suppress error here */
    if ((kd = kvm_open (NULL, NULL, NULL, O_RDONLY, NULL)) == NULL)
    {
	/* save the error message: we may need it later */
	p = strerror(errno);
    }
    dprintf("kvm_open: fd %d\n", kd);

    /*
     * turn off super group/user privs - but beware; we might
     * want the privs back later and we still have a fd to
     * /dev/kmem open so we can't use setgid()/setuid() as that
     * would allow a debugger to attach to this process. CD
     */
    setegid(getgid());
    seteuid(getuid()); /* super user not needed for NEW_PROC */

#ifdef USE_KSTAT
    /* open kstat */
    if ((kc = kstat_open()) == NULL)
    {
	fprintf(stderr, "Unable to open kstat.\n");
	return(-1);
    }
    kcid = kc->kc_chain_id;
    dprintf("kstat_open: chain %d\n", kcid);
#endif

    /* fill in the statics information */
    statics->procstate_names = procstatenames;
    statics->cpustate_names = cpustatenames;
    statics->memory_names = memorynames;
    statics->kernel_names = kernelnames;
    statics->order_names = ordernames;
    statics->flags.fullcmds = 1;
    statics->flags.warmup = 1;
    statics->flags.threads = 1;

    /* get boot time */
    ut.ut_type = BOOT_TIME;
    if ((up = getutxid(&ut)) != NULL)
    {
	statics->boottime = up->ut_tv.tv_sec;
    }
    endutxent();

    /* if the kvm_open succeeded, get the nlist */
    if (kd)
    {
	if (kvm_nlist (kd, nlst) < 0)
        {
	    perror ("kvm_nlist");
	    return (-1);
        }
	if (check_nlist (nlst) != 0)
	    return (-1);
    }
#ifndef USE_KSTAT
    /* if KSTAT is not available to us and we can't open /dev/kmem,
       this is a serious problem.
    */
    else
    {
	/* Print the error message here */
	(void) fprintf(stderr, "kvm_open: %s\n", p);
	return (-1);
    }
#endif

    /* stash away certain offsets for later use */
    mpid_offset = nlst[X_MPID].n_value;
    nproc_offset = nlst[X_NPROC].n_value;
    avenrun_offset = nlst[X_AVENRUN].n_value;
    anoninfo_offset = nlst[X_ANONINFO].n_value;
    freemem_offset = nlst[X_FREEMEM].n_value;
    maxmem_offset = nlst[X_MAXMEM].n_value;

#ifndef USE_KSTAT
    (void) getkval (nlst[X_NCPUS].n_value, (int *) (&cpucount),
		    sizeof (cpucount), "ncpus");

    cpu_offset = (unsigned long *) malloc (cpucount * sizeof (unsigned long));
    for (i = offset = 0; i < cpucount; offset += sizeof(unsigned long)) {
        (void) getkval (nlst[X_CPU].n_value + offset,
                        (int *)(&cpu_offset[i]), sizeof (unsigned long),
                        nlst[X_CPU].n_name );
        if (cpu_offset[i] != 0)
            i++;
    }
#endif

    /* we need to get the current nproc */
#ifdef USE_KSTAT
    /* get_nproc assumes that the chain has already been retrieved,
       so we need to do that here */
    kstat_safe_retrieve(&ks_system_misc, "unix", 0, "system_misc", NULL);
#endif
    nproc = get_nproc();
    dprintf("machine_init: nproc=%d\n", nproc);

    /* hash table for procs and threads sized based on current nproc*/
    prochash = hash_create(nproc > 100 ? nproc * 2 + 1 : 521);
    threadhash = hash_create(nproc > 100 ? nproc * 4 + 1 : 2053);

    /* calculate pageshift value */
    i = sysconf(_SC_PAGESIZE);
    pageshift = 0;
    while ((i >>= 1) > 0)
    {
	pageshift++;
    }

    /* calculate an amount to shift to K values */
    /* remember that log base 2 of 1024 is 10 (i.e.: 2^10 = 1024) */
    pageshift -= 10;

    /* now determine which pageshift function is appropriate for the 
       result (have to because x << y is undefined for y < 0) */
    if (pageshift > 0)
    {
	/* this is the most likely */
	p_pagetok = pagetok_left;
    }
    else if (pageshift == 0)
    {
	p_pagetok = pagetok_none;
    }
    else
    {
	p_pagetok = pagetok_right;
	pageshift = -pageshift;
    }

    /* we cache open files to improve performance, so we need to up
       the NOFILE limit */
    if (getrlimit(RLIMIT_NOFILE, &rlim) == 0)
    {
	/* set a new soft limit */
	maxfiles = (int)(rlim.rlim_max < MAXFILES ? rlim.rlim_max : MAXFILES);
	rlim.rlim_cur = (rlim_t)maxfiles;
	(void)setrlimit(RLIMIT_NOFILE, &rlim);

	/* now leave some wiggle room above the maximum */
	maxfiles -= 20;
    }

    /* set up the display indices */
    ip = proc_display;
    *ip++ = field_index("PID");
    *ip++ = field_index("USERNAME");
    *ip++ = field_index("NLWP");
    *ip++ = field_index("PRI");
    *ip++ = field_index("NICE");
    *ip++ = field_index("SIZE");
    *ip++ = field_index("RES");
    *ip++ = field_index("STATE");
    *ip++ = field_index("TIME");
    *ip++ = field_index("CPU");
    *ip++ = field_index("COMMAND");
    *ip = -1;
    ip = thr_display;
    *ip++ = field_index("PID");
    *ip++ = field_index("LWP");
    *ip++ = field_index("USERNAME");
    *ip++ = field_index("PRI");
    *ip++ = field_index("NICE");
    *ip++ = field_index("SIZE");
    *ip++ = field_index("RES");
    *ip++ = field_index("STATE");
    *ip++ = field_index("TIME");
    *ip++ = field_index("CPU");
    *ip++ = field_index("COMMAND");
    *ip = -1;

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

    /* all done! */
    return (0);
}

void
get_system_info (struct system_info *si)
{
    int avenrun[3];

    static long cp_time[CPUSTATES];
    static long cp_old[CPUSTATES];
    static long cp_diff[CPUSTATES];
    static struct cpustats *cpustats = NULL;
    static struct cpustats sum_current;
    static struct cpustats sum_old;
    static int cpus = 0;
    register int j, i;

    /* remember the old values and zero out the current */
    memcpy(&sum_old, &sum_current, sizeof(sum_current));
    memset(&sum_current, 0, sizeof(sum_current));

    /* get important information */
    get_avenrun(avenrun);

    /* get the cpu statistics arrays */
    cpustats = get_cpustats(&cpus, cpustats);

    /* zero the cp_time array */
    memset(cp_time, 0, sizeof(cp_time));

    /* sum stats in to a single array and a single structure */
    for (i = 0; i < cpus; i++)
    {
	for (j = 0; j < CPUSTATES; j++)
	{
	    cp_time[j] += cpustats[i].states[j];
	}
	sum_current.pswitch += cpustats[i].pswitch;
	sum_current.trap += cpustats[i].trap;
	sum_current.intr += cpustats[i].intr;
	sum_current.syscall += cpustats[i].syscall;
	sum_current.sysfork += cpustats[i].sysfork;
	sum_current.sysvfork += cpustats[i].sysvfork;
	sum_current.pfault += cpustats[i].pfault;
	sum_current.pgin += cpustats[i].pgin;
	sum_current.pgout += cpustats[i].pgout;
    }

    /* convert cp_time counts to percentages */
    (void) percentages (CPUSTATES, cpu_states, cp_time, cp_old, cp_diff);

    /* get mpid -- process id of last process */
    if (kd)
	(void) getkval(mpid_offset, &(si->last_pid), sizeof (si->last_pid), "mpid");
    else
	si->last_pid = -1;

    /* convert load averages to doubles */
    for (i = 0; i < 3; i++)
	si->load_avg[i] = loaddouble (avenrun[i]);

    /* get physical memory data */
    if (get_meminfo(&(memory_stats[MEMORY_TOTALMEM]),
		    &(memory_stats[MEMORY_FREEMEM])) == -1)
    {
	memory_stats[MEMORY_TOTALMEM] = memory_stats[MEMORY_FREEMEM] = -1;
    }

    /* get swap data */
    if (get_swapinfo(&(memory_stats[MEMORY_TOTALSWAP]),
		     &(memory_stats[MEMORY_FREESWAP])) == -1)
    {
	memory_stats[MEMORY_TOTALSWAP] = memory_stats[MEMORY_FREESWAP] = -1;
    }

    /* get kernel data */
    kernel_stats[KERNEL_CSWITCH] = diff_per_second(sum_current.pswitch, sum_old.pswitch);
    kernel_stats[KERNEL_TRAP] = diff_per_second(sum_current.trap, sum_old.trap);
    kernel_stats[KERNEL_INTR] = diff_per_second(sum_current.intr, sum_old.intr);
    kernel_stats[KERNEL_SYSCALL] = diff_per_second(sum_current.syscall, sum_old.syscall);
    kernel_stats[KERNEL_FORK] = diff_per_second(sum_current.sysfork + sum_current.sysvfork,
						sum_old.sysfork + sum_old.sysvfork);
    kernel_stats[KERNEL_PFAULT] = diff_per_second(sum_current.pfault, sum_old.pfault);
    kernel_stats[KERNEL_PGIN] = pagetok(diff_per_second(sum_current.pgin, sum_old.pgin));
    kernel_stats[KERNEL_PGOUT] = pagetok(diff_per_second(sum_current.pgout, sum_old.pgout));


    /* set arrays and strings */
    si->cpustates = cpu_states;
    si->memory = memory_stats;
    si->kernel = kernel_stats;

    dprintf("get_system_info returns\n");
}

static struct handle handle;

caddr_t
get_process_info (
		   struct system_info *si,
		   struct process_select *sel,
		   int compare_index)
{
    register int i;
    register int total_procs;
    register int active_procs;
    register struct prpsinfo **prefp;
    register struct prpsinfo *pp;
    int nproc;
    int state;

    /* these are copied out of sel for speed */
    int show_idle;
    int show_system;
    int show_uid;
    char *show_command;

    /* these persist across calls */
    static struct prpsinfo **pref = NULL;
    static int pref_size = 0;

    /* set up flags which define what we are going to select */
    show_idle = sel->idle;
    show_system = sel->system;
    show_uid = sel->uid != -1;
    show_fullcmd = sel->fullcmd;
    show_command = sel->command;
    show_threads = sel->threads;

    /* allocate enough space for twice our current needs */
    nproc = get_nproc();
    if (nproc > procs_max())
    {
	procs_prealloc(2 * nproc);
    }

    /* read all the proc structures */
    nproc = getptable();

    /* allocate pref[] */
    if (pref_size < nproc)
    {
	if (pref != NULL)
	{
	    free(pref);
	}
	pref = (struct prpsinfo **)malloc(nproc * sizeof(struct prpsinfo *));
	dprintf("get_process_info: allocated %d prinfo pointers at %08x\n",
		nproc, pref);
	pref_size = nproc;
    }

    /* get a pointer to the states summary array */
    si->procstates = process_states;

    /* count up process states and get pointers to interesting procs */
    total_procs = 0;
    active_procs = 0;
    (void) memset (process_states, 0, sizeof (process_states));
    prefp = pref;

    for (pp = procs_start(), i = 0; i < nproc;
	 i++, pp = procs_next())
    {
	dprintf("looking at #%d: %d.%d\n", i,
		pp->pr_pid, pp->pr_lwp.pr_lwpid);
	/*
	 *  Place pointers to each valid proc structure in pref[].
	 *  Process slots that are actually in use have a non-zero
	 *  status field.  Processes with SSYS set are system
	 *  processes---these get ignored unless show_sysprocs is set.
	 */
	if (pp->px_state != 0 &&
	    (show_system || ((pp->pr_flag & SSYS) == 0)))
	{
	    total_procs++;
	    state = (int)pp->px_state;
	    if (state > 0 && state < PROCSTATES)
	    {
		process_states[state]++;
	    }
	    else
	    {
		dprintf("process %d.%d: state out of bounds %d\n",
			pp->pr_pid, pp->pr_lwp.pr_lwpid, state);
	    }

	    if ((!ZOMBIE(pp)) &&
		(show_idle || percent_cpu (pp) || (pp->px_state == SRUN) || (pp->px_state == SONPROC)) &&
		(!show_uid || pp->pr_uid == (uid_t) sel->uid) &&
		(show_command == NULL ||
		 strstr(pp->pr_fname, show_command) != NULL))
	    {
		*prefp++ = pp;
		active_procs++;
	    }
	}
    }

    dprintf("total_procs %d, active_procs %d\n", total_procs, active_procs);

    /* if requested, sort the "interesting" processes */
    qsort ((char *) pref, active_procs, sizeof (struct prpsinfo *),
	   proc_compares[compare_index]);

    /* remember active and total counts */
    si->p_total = total_procs;
    si->p_active = active_procs;

    /* pass back a handle */
    handle.next_proc = pref;
    handle.remaining = active_procs;
    return ((caddr_t) & handle);
}

static char p_header[MAX_COLS];

char *
format_process_header(struct process_select *sel, caddr_t handle, int count)

{
    int cols;
    char *p;
    int *fi;
    struct proc_field *fp;

    /* check for null handle */
    if (handle == NULL)
    {
	return("");
    }

    /* remember how many columns there are on the display */
    cols = display_columns();

    /* mode & threads dictate format */
    fi = display_fields = sel->threads ? thr_display : proc_display;

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
    struct prpsinfo *pp;
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
    
    /* grab format descriptor */
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

#define ORDERKEY_PCTCPU  if (dresult = percent_cpu (p2) - percent_cpu (p1),\
			     (result = dresult > 0.0 ? 1 : dresult < 0.0 ? -1 : 0) == 0)
#define ORDERKEY_CPTICKS if ((result = p2->pr_time.tv_sec - p1->pr_time.tv_sec) == 0)
#define ORDERKEY_STATE   if ((result = (long) (sorted_state[(int)p2->px_state] - \
			       sorted_state[(int)p1->px_state])) == 0)
#define ORDERKEY_PRIO    if ((result = p2->px_pri - p1->px_pri) == 0)
#define ORDERKEY_RSSIZE  if ((result = p2->pr_rssize - p1->pr_rssize) == 0)
#define ORDERKEY_MEM     if ((result = (p2->pr_size - p1->pr_size)) == 0)
#define ORDERKEY_LWP     if ((result = (p1->pr_lwp.pr_lwpid - p2->pr_lwp.pr_lwpid)) == 0)
#define ORDERKEY_PID     if ((result = (p1->pr_pid - p2->pr_pid)) == 0)

/* Now the array that maps process state to a weight */

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


/* compare_cpu - the comparison function for sorting by cpu percentage */

int
compare_cpu (struct prpsinfo **pp1, struct prpsinfo **pp2)

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
    ORDERKEY_PID
    ORDERKEY_LWP
    ;

    return (result);
}

/* compare_size - the comparison function for sorting by total memory usage */

int
compare_size (struct prpsinfo **pp1, struct prpsinfo **pp2)

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
    ORDERKEY_PID
    ORDERKEY_LWP
    ;

    return (result);
}

/* compare_res - the comparison function for sorting by resident set size */

int
compare_res (struct prpsinfo **pp1, struct prpsinfo **pp2)

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
    ORDERKEY_PID
    ORDERKEY_LWP
    ;

    return (result);
}

/* compare_time - the comparison function for sorting by total cpu time */

int
compare_time (struct prpsinfo **pp1, struct prpsinfo **pp2)

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
    ORDERKEY_PID
    ORDERKEY_LWP
    ;

    return (result);
}

/* compare_pid - the comparison function for sorting by process id */

int
compare_pid (struct prpsinfo **pp1, struct prpsinfo **pp2)

{
    register struct prpsinfo *p1;
    register struct prpsinfo *p2;
    register long result;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    ORDERKEY_PID
    ORDERKEY_LWP
    ;

    return (result);
}

/* get process table */
int
getptable (struct prpsinfo *baseptr)
{
    struct prpsinfo *currproc;	/* pointer to current proc structure	*/
#ifndef USE_NEW_PROC
    struct prstatus prstatus;     /* for additional information */
#endif
    int numprocs = 0;
    struct dirent *direntp;
    struct oldproc *op;
    hash_pos pos;
    hash_item_pid *hi;
    hash_item_pidthr *hip;
    prheader_t *prp;
    lwpsinfo_t *lwpp;
    pidthr_t pidthr;
    static struct timeval lasttime =
	{0, 0};
    struct timeval thistime;
    struct stat st;
    double timediff;

    gettimeofday (&thistime, NULL);
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

    /* get our first procs pointer */
    currproc = procs_start();

    /* before reading /proc files, turn on root privs */
    /* (we don't care if this fails since it will be caught later) */
#ifndef USE_NEW_PROC
    seteuid(0);
#endif

    for (rewinddir (procdir); (direntp = readdir (procdir));)
    {
	int fd;
	int pid;
	char buf[40];

	/* skip dot files */
	if (direntp->d_name[0] == '.')
	    continue;

	/* convert pid to a number (and make sure its valid) */
	pid = atoi(direntp->d_name);
	if (pid <= 0)
	    continue;

	/* fetch the old proc data */
	op = (struct oldproc *)hash_lookup_pid(prochash, pid);
	if (op == NULL)
	{
	    /* new proc: create an entry for it */
	    op = (struct oldproc *)malloc(sizeof(struct oldproc));
	    hash_add_pid(prochash, pid, (void *)op);
	    op->pid = pid;
	    op->fd_psinfo = -1;
	    op->fd_lpsinfo = -1;
	    op->oldtime = 0.0;
	}

	/* do we have a cached file? */
	fd = op->fd_psinfo;
	if (fd == -1)
	{
	    /* no: open the psinfo file */
	    snprintf(buf, sizeof(buf), "%s/psinfo", direntp->d_name);
	    if ((fd = open(buf, O_RDONLY)) < 0)
	    {
		/* cleanup??? */
		continue;
	    }
	}

	/* read data from the file */
#ifdef USE_NEW_PROC
	if (pread(fd, currproc, sizeof(psinfo_t), 0) != sizeof(psinfo_t))
	{
	    (void) close (fd);
	    op->fd_psinfo = -1;
	    continue;
	}       
#else
	if (ioctl(fd, PIOCPSINFO, currproc) < 0)
	{
	    (void) close(fd);
	    op->fd_psinfo = -1;
	    continue;
	}

	if (ioctl(fd, PIOCSTATUS, &prstatus) < 0)
	{
	    /* not a show stopper -- just fill in the needed values */
	    currproc->pr_fill = 0;
	    currproc->px_onpro = 0;
	}
	else
	{
	    /* copy over the values we need from prstatus */
	    currproc->pr_fill = (short)prstatus.pr_nlwp;
	    currproc->px_onpro = prstatus.pr_processor;
	}
#endif

	/*
	 * We track our own cpu% usage.
	 * We compute it based on CPU since the last update by calculating
	 * the difference in cumulative cpu time and dividing by the amount
	 * of time we measured between updates (timediff).
	 * NOTE:  Solaris 2.4 and higher do maintain CPU% in psinfo,
	 * but it does not produce the kind of results we really want,
	 * so we don't use it even though its there.
	 */
	if (lasttime.tv_sec > 0)
	{
	    percent_cpu(currproc) =
		(TIMESPEC_TO_DOUBLE(currproc->pr_time) - op->oldtime) / timediff;
	}
	else
	{
	    /* first screen -- no difference is possible */
	    percent_cpu(currproc) = 0.0;
	}

	/* save data for next time */
	op->pid = currproc->pr_pid;
	op->oldtime = TIMESPEC_TO_DOUBLE(currproc->pr_time);
	op->owner_uid = currproc->pr_uid;
	op->seen = 1;

	/* cache the file descriptor if we can */
	if (fd < maxfiles)
	{
	    op->fd_psinfo = fd;
	}
	else
	{
	    (void) close(fd);
	}

#ifdef USE_NEW_PROC
	/* collect up the threads */
	/* use cached lps file if it's there */
	fd = op->fd_lpsinfo;
	if (fd == -1)
	{
	    snprintf(buf, sizeof(buf), "%s/lpsinfo", direntp->d_name);
	    fd = open(buf, O_RDONLY);
	}

	/* make sure we have a valid descriptor and the file's current size */
	if (fd >= 0 && fstat(fd, &st) != -1)
	{
	    char *p;
	    int i;

	    /* read the whole file */
	    p = malloc(st.st_size);
	    (void)pread(fd, p, st.st_size, 0);

	    /* cache the file descriptor if we can */
	    if (fd < maxfiles)
	    {
		op->fd_lpsinfo = fd;
	    }
	    else
	    {
		(void)close(fd);
	    }

	    /* the file starts with a struct prheader */
	    prp = (prheader_t *)p;
	    p += sizeof(prheader_t);

	    /* there are prp->pr_nent entries in the file */
	    for (i = 0; i < prp->pr_nent; i++)
	    {
		/* process this entry */
		lwpp = (lwpsinfo_t *)p;
		p += prp->pr_entsize;

		/* fetch the old thread data */
		/* this hash is indexed by both pid and lwpid */
		pidthr.k_pid = currproc->pr_pid;
		pidthr.k_thr = lwpp->pr_lwpid;
		dprintf("getptable: processing %d.%d\n",
			pidthr.k_pid, pidthr.k_thr);
		op = (struct oldproc *)hash_lookup_pidthr(threadhash, pidthr);
		if (op == NULL)
		{
		    /* new thread: create an entry for it */
		    op = (struct oldproc *)malloc(sizeof(struct oldproc));
		    hash_add_pidthr(threadhash, pidthr, (void *)op);
		    op->pid = pid;
		    op->lwpid = lwpp->pr_lwpid;
		    op->oldtime = 0.0;
		    dprintf("getptable: %d.%d: new thread\n",
			    pidthr.k_pid, pidthr.k_thr);
		}

		/* are we showing individual threads? */
		if (show_threads)
		{
		    /* yes: if this is the first thread we reuse the proc
		       entry we have, otherwise we create a new one by
		       duping the current one */
		    if (i > 0)
		    {
			currproc = procs_dup(currproc);
			numprocs++;
		    }

		    /* yes: copy over thread-specific data */
		    currproc->pr_time = lwpp->pr_time;
		    currproc->px_state = lwpp->pr_state;
		    currproc->px_pri = lwpp->pr_pri;
		    currproc->px_onpro = lwpp->pr_onpro;
		    currproc->pr_lwp.pr_lwpid = lwpp->pr_lwpid;

		    /* calculate percent cpu for just this thread */
		    if (lasttime.tv_sec > 0)
		    {
			percent_cpu(currproc) =
			    (TIMESPEC_TO_DOUBLE(lwpp->pr_time) - op->oldtime) /
			    timediff;
		    }
		    else
		    {
			/* first screen -- no difference is possible */
			percent_cpu(currproc) = 0.0;
		    }

		    dprintf("getptable: %d.%d: time %.0f, state %d, pctcpu %.2f\n",
			    currproc->pr_pid, lwpp->pr_lwpid,
			    TIMESPEC_TO_DOUBLE(currproc->pr_time),
			    currproc->px_state, percent_cpu(currproc));
		}

		/* save data for next time */
		op->oldtime = TIMESPEC_TO_DOUBLE(lwpp->pr_time);
		op->seen = 1;
	    }
	    free(p);
	}
#endif

	/* move to next */
	numprocs++;
	currproc = procs_next();
    }

#ifndef USE_NEW_PROC
    /* turn off root privs */
    seteuid(getuid());
#endif

    dprintf("getptable saw %d procs\n", numprocs);

    /* scan the hash tables and remove dead entries */
    hi = hash_first_pid(prochash, &pos);
    while (hi != NULL)
    {
	op = (struct oldproc *)(hi->value);
	if (op->seen)
	{
	    op->seen = 0;
	}
	else
	{
	    dprintf("removing %d from prochash\n", op->pid);
	    if (op->fd_psinfo >= 0)
	    {
		(void)close(op->fd_psinfo);
	    }
	    if (op->fd_lpsinfo >= 0)
	    {
		(void)close(op->fd_lpsinfo);
	    }
	    hash_remove_pos_pid(&pos);
	    free(op);
	}
	hi = hash_next_pid(&pos);
    }

    hip = hash_first_pidthr(threadhash, &pos);
    while (hip != NULL)
    {
	op = (struct oldproc *)(hip->value);
	if (op->seen)
	{
	    op->seen = 0;
	}
	else
	{
	    dprintf("removing %d from threadhash\n", op->pid);
	    hash_remove_pos_pidthr(&pos);
	    free(op);
	}
	hip = hash_next_pidthr(&pos);
    }

    lasttime = thistime;

    return numprocs;
}

/*
 * proc_owner(pid) - returns the uid that owns process "pid", or -1 if
 *              the process does not exist.
 *              It is EXTREMLY IMPORTANT that this function work correctly.
 *              If top runs setuid root (as in SVR4), then this function
 *              is the only thing that stands in the way of a serious
 *              security problem.  It validates requests for the "kill"
 *              and "renice" commands.
 */
int
proc_owner (int pid)
{
    struct oldproc *op;

    /* we keep this information in the hash table */
    op = (struct oldproc *)hash_lookup_pid(prochash, (pid_t)pid);
    if (op != NULL)
    {
	return((int)(op->owner_uid));
    }
    return(-1);
}

/* older revisions don't supply a setpriority */
#if (OSREV < 55)
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

