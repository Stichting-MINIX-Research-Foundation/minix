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
 * SYNOPSIS:  Linux 1.2.x, 1.3.x, 2.x, using the /proc filesystem
 *
 * DESCRIPTION:
 * This is the machine-dependent module for Linux 1.2.x, 1.3.x or 2.x.
 *
 * LIBS:
 *
 * CFLAGS: -DHAVE_GETOPT -DHAVE_STRERROR -DORDER
 *
 * TERMCAP: -lcurses
 *
 * AUTHOR: Richard Henderson <rth@tamu.edu>
 * Order support added by Alexey Klimkin <kad@klon.tme.mcst.ru>
 * Ported to 2.4 by William LeFebvre
 * Additions for 2.6 by William LeFebvre
 */

#include "config.h"

#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/vfs.h>

#include <sys/param.h>		/* for HZ */
#include <asm/page.h>		/* for PAGE_SHIFT */

#if 0
#include <linux/proc_fs.h>	/* for PROC_SUPER_MAGIC */
#else
#define PROC_SUPER_MAGIC 0x9fa0
#endif

#include "top.h"
#include "hash.h"
#include "machine.h"
#include "utils.h"
#include "username.h"

#define PROCFS "/proc"
extern char *myname;

/*=PROCESS INFORMATION==================================================*/

struct top_proc
{
    pid_t pid;
    uid_t uid;
    char *name;
    int pri, nice, threads;
    unsigned long size, rss, shared;	/* in k */
    int state;
    unsigned long time;
    unsigned long start_time;
    double pcpu;
    struct top_proc *next;
};
    

/*=STATE IDENT STRINGS==================================================*/

#define NPROCSTATES 7
static char *state_abbrev[NPROCSTATES+1] =
{
    "", "run", "sleep", "disk", "zomb", "stop", "swap",
    NULL
};

static char *procstatenames[NPROCSTATES+1] =
{
    "", " running, ", " sleeping, ", " uninterruptable, ",
    " zombie, ", " stopped, ", " swapping, ",
    NULL
};

#define NCPUSTATES 5
static char *cpustatenames[NCPUSTATES+1] =
{
    "user", "nice", "system", "idle", "iowait",
    NULL
};
static int show_iowait = 0;

#define KERNELCTXT    0
#define KERNELFLT     1
#define KERNELINTR    2
#define KERNELNEWPROC 3
#define NKERNELSTATS  4
static char *kernelnames[NKERNELSTATS+1] =
{
    " ctxsw, ", " flt, ", " intr, ", " newproc",
    NULL
};

#define MEMUSED    0
#define MEMFREE    1
#define MEMSHARED  2
#define MEMBUFFERS 3
#define MEMCACHED  4
#define NMEMSTATS  5
static char *memorynames[NMEMSTATS+1] =
{
    "K used, ", "K free, ", "K shared, ", "K buffers, ", "K cached",
    NULL
};

#define SWAPUSED   0
#define SWAPFREE   1
#define SWAPCACHED 2
#define NSWAPSTATS 3
static char *swapnames[NSWAPSTATS+1] =
{
    "K used, ", "K free, ", "K cached",
    NULL
};

static char fmt_header[] =
"  PID X         THR PRI NICE  SIZE   RES STATE   TIME    CPU COMMAND";

static char proc_header_thr[] =
"  PID %-9s THR PRI NICE  SIZE   RES   SHR STATE   TIME    CPU COMMAND";

static char proc_header_nothr[] =
"  PID %-9s PRI NICE  SIZE   RES   SHR STATE   TIME    CPU COMMAND";

/* these are names given to allowed sorting orders -- first is default */
char *ordernames[] = 
{"cpu", "size", "res", "time", "command", NULL};

/* forward definitions for comparison functions */
int compare_cpu();
int compare_size();
int compare_res();
int compare_time();
int compare_cmd();

int (*proc_compares[])() = {
    compare_cpu,
    compare_size,
    compare_res,
    compare_time,
    compare_cmd,
    NULL };
	
/*=SYSTEM STATE INFO====================================================*/

/* these are for calculating cpu state percentages */

static long cp_time[NCPUSTATES];
static long cp_old[NCPUSTATES];
static long cp_diff[NCPUSTATES];

/* for calculating the exponential average */

static struct timeval lasttime = { 0, 0 };
static struct timeval timediff = { 0, 0 };
static long elapsed_msecs;

/* these are for keeping track of processes and tasks */

#define HASH_SIZE	     (1003)
#define INITIAL_ACTIVE_SIZE  (256)
#define PROCBLOCK_SIZE       (32)
static hash_table *ptable;
static hash_table *tasktable;
static struct top_proc **pactive;
static struct top_proc **nextactive;
static unsigned int activesize = 0;
static time_t boottime = -1;
static int have_task = 0;

/* these are counters that need to be track */
static unsigned long last_ctxt = 0;
static unsigned long last_intr = 0;
static unsigned long last_newproc = 0;
static unsigned long last_flt = 0;

/* these are for passing data back to the machine independant portion */

static int cpu_states[NCPUSTATES];
static int process_states[NPROCSTATES];
static int kernel_stats[NKERNELSTATS];
static long memory_stats[NMEMSTATS];
static long swap_stats[NSWAPSTATS];

/* useful macros */
#define bytetok(x)	(((x) + 512) >> 10)
#define pagetok(x)	((x) << (PAGE_SHIFT - 10))
#define HASH(x)		(((x) * 1686629713U) % HASH_SIZE)

/* calculate a per-second rate using milliseconds */
#define per_second(n, msec)   (((n) * 1000) / (msec))

/*======================================================================*/

static inline char *
skip_ws(const char *p)
{
    while (isspace(*p)) p++;
    return (char *)p;
}
    
static inline char *
skip_token(const char *p)
{
    while (isspace(*p)) p++;
    while (*p && !isspace(*p)) p++;
    return (char *)p;
}

static void
xfrm_cmdline(char *p, int len)
{
    while (--len > 0)
    {
	if (*p == '\0')
	{
	    *p = ' ';
	}
	p++;
    }
}

static void
update_procname(struct top_proc *proc, char *cmd)

{
    printable(cmd);

    if (proc->name == NULL)
    {
	proc->name = strdup(cmd);
    }
    else if (strcmp(proc->name, cmd) != 0)
    {
	free(proc->name);
	proc->name = strdup(cmd);
    }
}

/*
 * Process structures are allocated and freed as needed.  Here we
 * keep big pools of them, adding more pool as needed.  When a
 * top_proc structure is freed, it is added to a freelist and reused.
 */

static struct top_proc *freelist = NULL;
static struct top_proc *procblock = NULL;
static struct top_proc *procmax = NULL;

static struct top_proc *
new_proc()
{
    struct top_proc *p;

    if (freelist)
    {
	p = freelist;
	freelist = freelist->next;
    }
    else if (procblock)
    {
	p = procblock;
	if (++procblock >= procmax)
	{
	    procblock = NULL;
	}
    }
    else
    {
	p = procblock = (struct top_proc *)calloc(PROCBLOCK_SIZE,
						  sizeof(struct top_proc));
	procmax = procblock++ + PROCBLOCK_SIZE;
    }

    /* initialization */
    if (p->name != NULL)
    {
	free(p->name);
	p->name = NULL;
    }

    return p;
}

static void
free_proc(struct top_proc *proc)
{
    proc->next = freelist;
    freelist = proc;
}

 
int
machine_init(struct statics *statics)

{
    /* make sure the proc filesystem is mounted */
    {
	struct statfs sb;
	if (statfs(PROCFS, &sb) < 0 || sb.f_type != PROC_SUPER_MAGIC)
	{
	    fprintf(stderr, "%s: proc filesystem not mounted on " PROCFS "\n",
		    myname);
	    return -1;
	}
    }

    /* chdir to the proc filesystem to make things easier */
    chdir(PROCFS);

    /* a few preliminary checks */
    {
	int fd;
	char buff[128];
	char *p;
	int cnt = 0;
	unsigned long uptime;
	struct timeval tv;
	struct stat st;

	/* get a boottime */
	if ((fd = open("uptime", 0)) != -1)
	{
	    if (read(fd, buff, sizeof(buff)) > 0)
	    {
		uptime = strtoul(buff, &p, 10);
		gettimeofday(&tv, 0);
		boottime = tv.tv_sec - uptime;
	    }
	    close(fd);
	}

	/* see how many states we get from stat */
	if ((fd = open("stat", 0)) != -1)
	{
	    if (read(fd, buff, sizeof(buff)) > 0)
	    {
		if ((p = strchr(buff, '\n')) != NULL)
		{
		    *p = '\0';
		    p = buff;
		    while (*p != '\0')
		    {
			if (*p++ == ' ')
			{
			    cnt++;
			}
		    }
		}
	    }

	    close(fd);
	}
	if (cnt > 5)
	{
	    /* we have iowait */
	    show_iowait = 1;
	}

	/* see if we have task subdirs */
	if (stat("self/task", &st) != -1 && S_ISDIR(st.st_mode))
	{
	    dprintf("we have task directories\n");
	    have_task = 1;
	}
    }

    /* if we aren't showing iowait, then we have to tweak cpustatenames */
    if (!show_iowait)
    {
	cpustatenames[4] = NULL;
    }

    /* fill in the statics information */
    statics->procstate_names = procstatenames;
    statics->cpustate_names = cpustatenames;
    statics->kernel_names = kernelnames;
    statics->memory_names = memorynames;
    statics->swap_names = swapnames;
    statics->order_names = ordernames;
    statics->boottime = boottime;
    statics->flags.fullcmds = 1;
    statics->flags.warmup = 1;
    statics->flags.threads = 1;

    /* allocate needed space */
    pactive = (struct top_proc **)malloc(sizeof(struct top_proc *) * INITIAL_ACTIVE_SIZE);
    activesize = INITIAL_ACTIVE_SIZE;

    /* create process and task hashes */
    ptable = hash_create(HASH_SIZE);
    tasktable = hash_create(HASH_SIZE);

    /* all done! */
    return 0;
}


void
get_system_info(struct system_info *info)

{
    char buffer[4096+1];
    int fd, len;
    char *p;
    struct timeval thistime;
    unsigned long intr = 0;
    unsigned long ctxt = 0;
    unsigned long newproc = 0;
    unsigned long flt = 0;

    /* timestamp and time difference */
    gettimeofday(&thistime, 0);
    timersub(&thistime, &lasttime, &timediff);
    elapsed_msecs = timediff.tv_sec * 1000 + timediff.tv_usec / 1000;
    lasttime = thistime;

    /* get load averages */
    if ((fd = open("loadavg", O_RDONLY)) != -1)
    {
	if ((len = read(fd, buffer, sizeof(buffer)-1)) > 0)
	{
	    buffer[len] = '\0';
	    info->load_avg[0] = strtod(buffer, &p);
	    info->load_avg[1] = strtod(p, &p);
	    info->load_avg[2] = strtod(p, &p);
	    p = skip_token(p);			/* skip running/tasks */
	    p = skip_ws(p);
	    if (*p)
	    {
		info->last_pid = atoi(p);
	    }
	    else
	    {
		info->last_pid = -1;
	    }
	}
	close(fd);
    }

    /* get the cpu time info */
    if ((fd = open("stat", O_RDONLY)) != -1)
    {
	if ((len = read(fd, buffer, sizeof(buffer)-1)) > 0)
	{
	    buffer[len] = '\0';
	    p = skip_token(buffer);			/* "cpu" */
	    cp_time[0] = strtoul(p, &p, 0);
	    cp_time[1] = strtoul(p, &p, 0);
	    cp_time[2] = strtoul(p, &p, 0);
	    cp_time[3] = strtoul(p, &p, 0);
	    if (show_iowait)
	    {
		cp_time[4] = strtoul(p, &p, 0);
	    }

	    /* convert cp_time counts to percentages */
	    percentages(NCPUSTATES, cpu_states, cp_time, cp_old, cp_diff);

	    /* get the rest of it */
	    p = strchr(p, '\n');
	    while (p != NULL)
	    {
		p++;
		if (strncmp(p, "intr ", 5) == 0)
		{
		    p = skip_token(p);
		    intr = strtoul(p, &p, 10);
		}
		else if (strncmp(p, "ctxt ", 5) == 0)
		{
		    p = skip_token(p);
		    ctxt = strtoul(p, &p, 10);
		}
		else if (strncmp(p, "processes ", 10) == 0)
		{
		    p = skip_token(p);
		    newproc = strtoul(p, &p, 10);
		}

		p = strchr(p, '\n');
	    }

	    kernel_stats[KERNELINTR] = per_second(intr - last_intr, elapsed_msecs);
	    kernel_stats[KERNELCTXT] = per_second(ctxt - last_ctxt, elapsed_msecs);
	    kernel_stats[KERNELNEWPROC] = per_second(newproc - last_newproc, elapsed_msecs);
	    last_intr = intr;
	    last_ctxt = ctxt;
	    last_newproc = newproc;
	}
	close(fd);
    }

    /* get system wide memory usage */
    if ((fd = open("meminfo", O_RDONLY)) != -1)
    {
	char *p;
	int mem = 0;
	int swap = 0;
	unsigned long memtotal = 0;
	unsigned long memfree = 0;
	unsigned long swaptotal = 0;

	if ((len = read(fd, buffer, sizeof(buffer)-1)) > 0)
	{
	    buffer[len] = '\0';
	    p = buffer-1;

	    /* iterate thru the lines */
	    while (p != NULL)
	    {
		p++;
		if (p[0] == ' ' || p[0] == '\t')
		{
		    /* skip */
		}
		else if (strncmp(p, "Mem:", 4) == 0)
		{
		    p = skip_token(p);			/* "Mem:" */
		    p = skip_token(p);			/* total memory */
		    memory_stats[MEMUSED] = strtoul(p, &p, 10);
		    memory_stats[MEMFREE] = strtoul(p, &p, 10);
		    memory_stats[MEMSHARED] = strtoul(p, &p, 10);
		    memory_stats[MEMBUFFERS] = strtoul(p, &p, 10);
		    memory_stats[MEMCACHED] = strtoul(p, &p, 10);
		    memory_stats[MEMUSED] = bytetok(memory_stats[MEMUSED]);
		    memory_stats[MEMFREE] = bytetok(memory_stats[MEMFREE]);
		    memory_stats[MEMSHARED] = bytetok(memory_stats[MEMSHARED]);
		    memory_stats[MEMBUFFERS] = bytetok(memory_stats[MEMBUFFERS]);
		    memory_stats[MEMCACHED] = bytetok(memory_stats[MEMCACHED]);
		    mem = 1;
		}
		else if (strncmp(p, "Swap:", 5) == 0)
		{
		    p = skip_token(p);			/* "Swap:" */
		    p = skip_token(p);			/* total swap */
		    swap_stats[SWAPUSED] = strtoul(p, &p, 10);
		    swap_stats[SWAPFREE] = strtoul(p, &p, 10);
		    swap_stats[SWAPUSED] = bytetok(swap_stats[SWAPUSED]);
		    swap_stats[SWAPFREE] = bytetok(swap_stats[SWAPFREE]);
		    swap = 1;
		}
		else if (!mem && strncmp(p, "MemTotal:", 9) == 0)
		{
		    p = skip_token(p);
		    memtotal = strtoul(p, &p, 10);
		}
		else if (!mem && memtotal > 0 && strncmp(p, "MemFree:", 8) == 0)
		{
		    p = skip_token(p);
		    memfree = strtoul(p, &p, 10);
		    memory_stats[MEMUSED] = memtotal - memfree;
		    memory_stats[MEMFREE] = memfree;
		}
		else if (!mem && strncmp(p, "MemShared:", 10) == 0)
		{
		    p = skip_token(p);
		    memory_stats[MEMSHARED] = strtoul(p, &p, 10);
		}
		else if (!mem && strncmp(p, "Buffers:", 8) == 0)
		{
		    p = skip_token(p);
		    memory_stats[MEMBUFFERS] = strtoul(p, &p, 10);
		}
		else if (!mem && strncmp(p, "Cached:", 7) == 0)
		{
		    p = skip_token(p);
		    memory_stats[MEMCACHED] = strtoul(p, &p, 10);
		}
		else if (!swap && strncmp(p, "SwapTotal:", 10) == 0)
		{
		    p = skip_token(p);
		    swaptotal = strtoul(p, &p, 10);
		}
		else if (!swap && swaptotal > 0 && strncmp(p, "SwapFree:", 9) == 0)
		{
		    p = skip_token(p);
		    memfree = strtoul(p, &p, 10);
		    swap_stats[SWAPUSED] = swaptotal - memfree;
		    swap_stats[SWAPFREE] = memfree;
		}
		else if (!mem && strncmp(p, "SwapCached:", 11) == 0)
		{
		    p = skip_token(p);
		    swap_stats[SWAPCACHED] = strtoul(p, &p, 10);
		}

		/* move to the next line */
		p = strchr(p, '\n');
	    }
	}
	close(fd);
    }

    /* get vm related stuff */
    if ((fd = open("vmstat", O_RDONLY)) != -1)
    {
	char *p;

	if ((len = read(fd, buffer, sizeof(buffer)-1)) > 0)
	{
	    buffer[len] = '\0';
	    p = buffer;

	    /* iterate thru the lines */
	    while (p != NULL)
	    {
		if (strncmp(p, "pgmajfault ", 11) == 0)
		{
		    p = skip_token(p);
		    flt = strtoul(p, &p, 10);
		    kernel_stats[KERNELFLT] = per_second(flt - last_flt, elapsed_msecs);
		    last_flt = flt;
		    break;
		}

		/* move to the next line */
		p = strchr(p, '\n');
		p++;
	    }
	}
	close(fd);
    }

    /* set arrays and strings */
    info->cpustates = cpu_states;
    info->memory = memory_stats;
    info->swap = swap_stats;
    info->kernel = kernel_stats;
}

static void
read_one_proc_stat(pid_t pid, pid_t taskpid, struct top_proc *proc, struct process_select *sel)
{
    char buffer[4096], *p, *q;
    int fd, len;
    int fullcmd;

    dprintf("reading proc %d - %d\n", pid, taskpid);

    /* if anything goes wrong, we return with proc->state == 0 */
    proc->state = 0;

    /* full cmd handling */
    fullcmd = sel->fullcmd;
    if (fullcmd)
    {
	if (taskpid == -1)
	{
	    sprintf(buffer, "%d/cmdline", pid);
	}
	else
	{
	    sprintf(buffer, "%d/task/%d/cmdline", pid, taskpid);
	}
	if ((fd = open(buffer, O_RDONLY)) != -1)
	{
	    /* read command line data */
	    /* (theres no sense in reading more than we can fit) */
	    if ((len = read(fd, buffer, MAX_COLS)) > 1)
	    {
		buffer[len] = '\0';
		xfrm_cmdline(buffer, len);
		update_procname(proc, buffer);
	    }
	    else
	    {
		fullcmd = 0;
	    }
	    close(fd);
	}
	else
	{
	    fullcmd = 0;
	}
    }

    /* grab the shared memory size */
    sprintf(buffer, "%d/statm", pid);
    fd = open(buffer, O_RDONLY);
    len = read(fd, buffer, sizeof(buffer)-1);
    close(fd);
    buffer[len] = '\0';
    p = buffer;
    p = skip_token(p);		/* skip size */
    p = skip_token(p);		/* skip resident */
    proc->shared = pagetok(strtoul(p, &p, 10));

    /* grab the proc stat info in one go */
    if (taskpid == -1)
    {
	sprintf(buffer, "%d/stat", pid);
    }
    else
    {
	sprintf(buffer, "%d/task/%d/stat", pid, taskpid);
    }

    fd = open(buffer, O_RDONLY);
    len = read(fd, buffer, sizeof(buffer)-1);
    close(fd);

    buffer[len] = '\0';

    proc->uid = (uid_t)proc_owner((int)pid);

    /* parse out the status */
    
    /* skip pid and locate command, which is in parentheses */
    if ((p = strchr(buffer, '(')) == NULL)
    {
	return;
    }
    if ((q = strrchr(++p, ')')) == NULL)
    {
	return;
    }

    /* set the procname */
    *q = '\0';
    if (!fullcmd)
    {
	update_procname(proc, p);
    }

    /* scan the rest of the line */
    p = q+1;
    p = skip_ws(p);
    switch (*p++)				/* state */
    {
    case 'R': proc->state = 1; break;
    case 'S': proc->state = 2; break;
    case 'D': proc->state = 3; break;
    case 'Z': proc->state = 4; break;
    case 'T': proc->state = 5; break;
    case 'W': proc->state = 6; break;
    case '\0': return;
    }
    
    p = skip_token(p);				/* skip ppid */
    p = skip_token(p);				/* skip pgrp */
    p = skip_token(p);				/* skip session */
    p = skip_token(p);				/* skip tty */
    p = skip_token(p);				/* skip tty pgrp */
    p = skip_token(p);				/* skip flags */
    p = skip_token(p);				/* skip min flt */
    p = skip_token(p);				/* skip cmin flt */
    p = skip_token(p);				/* skip maj flt */
    p = skip_token(p);				/* skip cmaj flt */
    
    proc->time = strtoul(p, &p, 10);		/* utime */
    proc->time += strtoul(p, &p, 10);		/* stime */

    p = skip_token(p);				/* skip cutime */
    p = skip_token(p);				/* skip cstime */

    proc->pri = strtol(p, &p, 10);		/* priority */
    proc->nice = strtol(p, &p, 10);		/* nice */
    proc->threads = strtol(p, &p, 10);		/* threads */

    p = skip_token(p);				/* skip it_real_val */
    proc->start_time = strtoul(p, &p, 10);	/* start_time */

    proc->size = bytetok(strtoul(p, &p, 10));	/* vsize */
    proc->rss = pagetok(strtoul(p, &p, 10));	/* rss */

#if 0
    /* for the record, here are the rest of the fields */
    p = skip_token(p);				/* skip rlim */
    p = skip_token(p);				/* skip start_code */
    p = skip_token(p);				/* skip end_code */
    p = skip_token(p);				/* skip start_stack */
    p = skip_token(p);				/* skip sp */
    p = skip_token(p);				/* skip pc */
    p = skip_token(p);				/* skip signal */
    p = skip_token(p);				/* skip sigblocked */
    p = skip_token(p);				/* skip sigignore */
    p = skip_token(p);				/* skip sigcatch */
    p = skip_token(p);				/* skip wchan */
#endif

}

static int show_usernames;
static int show_threads;


caddr_t
get_process_info(struct system_info *si,
		 struct process_select *sel,
		 int compare_index)
{
    struct top_proc *proc;
    struct top_proc *taskproc;
    pid_t pid;
    pid_t taskpid;
    unsigned long now;
    unsigned long elapsed;
    hash_item_pid *hi;
    hash_pos pos;

    /* round current time to a second */
    now = (unsigned long)lasttime.tv_sec;
    if (lasttime.tv_usec >= 500000)
    {
	now++;
    }

    /* calculate number of ticks since our last check */
    elapsed = timediff.tv_sec * HZ + (timediff.tv_usec * HZ) / 1000000;
    if (elapsed <= 0)
    {
	elapsed = 1;
    }
    dprintf("get_process_info: elapsed %d ticks\n", elapsed);

    /* mark all hash table entries as not seen */
    hi = hash_first_pid(ptable, &pos);
    while (hi != NULL)
    {
	((struct top_proc *)(hi->value))->state = 0;
	hi = hash_next_pid(&pos);
    }
    /* mark all hash table entries as not seen */
    hi = hash_first_pid(tasktable, &pos);
    while (hi != NULL)
    {
	((struct top_proc *)(hi->value))->state = 0;
	hi = hash_next_pid(&pos);
    }

    /* read the process information */
    {
	DIR *dir = opendir(".");
	DIR *taskdir;
	struct dirent *ent;
	struct dirent *taskent;
	int total_procs = 0;
	struct top_proc **active;
	hash_item_pid *hi;
	hash_pos pos;
	char buffer[64];

	int show_idle = sel->idle;
	int show_uid = sel->uid != -1;
	char *show_command = sel->command;

	show_usernames = sel->usernames;
	show_threads = sel->threads && have_task;

	memset(process_states, 0, sizeof(process_states));

	taskdir = NULL;
	taskent = NULL;
	taskpid = -1;

	while ((ent = readdir(dir)) != NULL)
	{
	    unsigned long otime;

	    if (!isdigit(ent->d_name[0]))
		continue;

	    pid = atoi(ent->d_name);

	    /* look up hash table entry */
	    proc = hash_lookup_pid(ptable, pid);

	    /* if we came up empty, create a new entry */
	    if (proc == NULL)
	    {
		proc = new_proc();
		proc->pid = pid;
		proc->time = 0;
		hash_add_pid(ptable, pid, (void *)proc);
	    }

	    /* remember the previous cpu time */
	    otime = proc->time;

	    /* get current data */
	    read_one_proc_stat(pid, -1, proc, sel);

	    /* continue on if this isn't really a process */
	    if (proc->state == 0)
		continue;

	    /* reset linked list (for threads) */
	    proc->next = NULL;

	    /* accumulate process state data */
	    total_procs++;
	    process_states[proc->state]++;

	    /* calculate pcpu */
	    if ((proc->pcpu = (proc->time - otime) / (double)elapsed) < 0.0001)
	    {
		proc->pcpu = 0;
	    }

	    /* if we have task subdirs and this process has more than
	       one thread, collect data on each thread */
	    if (have_task && proc->threads > 1)
	    {
		snprintf(buffer, sizeof(buffer), "%d/task", pid);
		if ((taskdir = opendir(buffer)) != NULL)
		{
		    while ((taskent = readdir(taskdir)) != NULL)
		    {
			if (!isdigit(taskent->d_name[0]))
			    continue;

			/* lookup entry in tasktable */
			taskpid = atoi(taskent->d_name);
			taskproc = hash_lookup_pid(tasktable, taskpid);

			/* if we came up empty, create a new entry */
			if (taskproc == NULL)
			{
			    taskproc = new_proc();
			    taskproc->pid = taskpid;
			    taskproc->time = 0;
			    hash_add_pid(tasktable, taskpid, (void *)taskproc);
			}

			/* remember the previous cpu time */
			otime = taskproc->time;

			/* get current data */
			read_one_proc_stat(pid, taskpid, taskproc, sel);

			/* ignore if it isnt real */
			if (taskproc->state == 0)
			    continue;

			/* when showing threads, add this to the accumulated
			   process state data, but remember that the first
			   thread is already accounted for */
			if (show_threads && pid != taskpid)
			{
			    total_procs++;
			    process_states[taskproc->state]++;
			}

			/* calculate pcpu */
			if ((taskproc->pcpu = (taskproc->time - otime) /
			     (double)elapsed) < 0.0)
			{
			    taskproc->pcpu = 0;
			}

			/* link this in to the proc's list */
			taskproc->next = proc->next;
			proc->next = taskproc;
		    }
		    closedir(taskdir);
		}
	    }
	}
	closedir(dir);

	/* make sure we have enough slots for the active procs */
	if (activesize < total_procs)
	{
	    pactive = (struct top_proc **)realloc(pactive,
				  sizeof(struct top_proc *) * total_procs);
	    activesize = total_procs;
	}

	/* set up the active procs and flush dead entries */
	active = pactive;
	hi = hash_first_pid(ptable, &pos);
	while (hi != NULL)
	{
	    proc = (struct top_proc *)(hi->value);
	    if (proc->state == 0)
	    {
		/* dead entry */
		hash_remove_pos_pid(&pos);
		free_proc(proc);
	    }
	    else
	    {
		/* check to see if it qualifies as active */
		if ((show_idle || proc->state == 1 || proc->pcpu) &&
		    (!show_uid || proc->uid == sel->uid) &&
		    (show_command == NULL ||
		     strstr(proc->name, show_command) != NULL))
		{
		    /* are we showing threads and does this proc have any? */
		    if (show_threads && proc->threads > 1 && proc->next != NULL)
		    {
			/* then add just the thread info -- the main process
			   info is included in the list */
			proc = proc->next;
			while (proc != NULL)
			{
			    *active++ = proc;
			    proc = proc->next;
			}
		    }
		    else
		    {
			/* add the process */
			*active++ = proc;
		    }
		}
	    }

	    hi = hash_next_pid(&pos);
	}

	si->p_active = active - pactive;
	si->p_total = total_procs;
	si->procstates = process_states;
    }

    /* if requested, sort the "active" procs */
    if (si->p_active)
	qsort(pactive, si->p_active, sizeof(struct top_proc *),
	      proc_compares[compare_index]);

    /* don't even pretend that the return value thing here isn't bogus */
    nextactive = pactive;
    return (caddr_t)0;
}


char *
format_header(char *uname_field)

{
    int uname_len = strlen(uname_field);
    if (uname_len > 8)
	uname_len = 8;

    memcpy(strchr(fmt_header, 'X'), uname_field, uname_len);

    return fmt_header;
}

static char p_header[MAX_COLS];

char *
format_process_header(struct process_select *sel, caddr_t handle, int count)

{
    char *h;

    h = sel->threads ? proc_header_nothr : proc_header_thr;

    snprintf(p_header, MAX_COLS, h, sel->usernames ? "USERNAME" : "UID");

    return p_header;
}


char *
format_next_process(caddr_t handle, char *(*get_userid)(int))

{
    static char fmt[MAX_COLS];	/* static area where result is built */
    struct top_proc *p = *nextactive++;
    char *userbuf;

    userbuf = show_usernames ? username(p->uid) : itoa_w(p->uid, 7);

    if (show_threads)
    {
	snprintf(fmt, sizeof(fmt),
		 "%5d %-8.8s  %3d %4d %5s %5s %5s %-5s %6s %5s%% %s",
		 p->pid,
		 userbuf,
		 p->pri < -99 ? -99 : p->pri,
		 p->nice,
		 format_k(p->size),
		 format_k(p->rss),
		 format_k(p->shared),
		 state_abbrev[p->state],
		 format_time(p->time / HZ),
		 format_percent(p->pcpu * 100.0),
		 p->name);
    }
    else
    {
	snprintf(fmt, sizeof(fmt),
		 "%5d %-8.8s %4d %3d %4d %5s %5s %5s %-5s %6s %5s%% %s",
		 p->pid,
		 userbuf,
		 p->threads <= 9999 ? p->threads : 9999,
		 p->pri < -99 ? -99 : p->pri,
		 p->nice,
		 format_k(p->size),
		 format_k(p->rss),
		 format_k(p->shared),
		 state_abbrev[p->state],
		 format_time(p->time / HZ),
		 format_percent(p->pcpu * 100.0),
		 p->name);
    }

    /* return the result */
    return (fmt);
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

#define ORDERKEY_PCTCPU  if (dresult = p2->pcpu - p1->pcpu,\
			 (result = dresult > 0.0 ? 1 : dresult < 0.0 ? -1 : 0) == 0)
#define ORDERKEY_CPTICKS if ((result = (long)p2->time - (long)p1->time) == 0)
#define ORDERKEY_STATE   if ((result = (sort_state[p2->state] - \
			 sort_state[p1->state])) == 0)
#define ORDERKEY_PRIO    if ((result = p2->pri - p1->pri) == 0)
#define ORDERKEY_RSSIZE  if ((result = p2->rss - p1->rss) == 0)
#define ORDERKEY_MEM     if ((result = p2->size - p1->size) == 0)
#define ORDERKEY_NAME    if ((result = strcmp(p1->name, p2->name)) == 0)

/* Now the array that maps process state to a weight */

unsigned char sort_state[] =
{
	0,	/* empty */
	6, 	/* run */
	3,	/* sleep */
	5,	/* disk wait */
	1,	/* zombie */
	2,	/* stop */
	4	/* swap */
};


/* compare_cpu - the comparison function for sorting by cpu percentage */

int
compare_cpu (
	       struct top_proc **pp1,
	       struct top_proc **pp2)
  {
    register struct top_proc *p1;
    register struct top_proc *p2;
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

    return result == 0 ? 0 : result < 0 ? -1 : 1;
  }

/* compare_size - the comparison function for sorting by total memory usage */

int
compare_size (
	       struct top_proc **pp1,
	       struct top_proc **pp2)
  {
    register struct top_proc *p1;
    register struct top_proc *p2;
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

    return result == 0 ? 0 : result < 0 ? -1 : 1;
  }

/* compare_res - the comparison function for sorting by resident set size */

int
compare_res (
	       struct top_proc **pp1,
	       struct top_proc **pp2)
  {
    register struct top_proc *p1;
    register struct top_proc *p2;
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

    return result == 0 ? 0 : result < 0 ? -1 : 1;
  }

/* compare_time - the comparison function for sorting by total cpu time */

int
compare_time (
	       struct top_proc **pp1,
	       struct top_proc **pp2)
  {
    register struct top_proc *p1;
    register struct top_proc *p2;
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

    return result == 0 ? 0 : result < 0 ? -1 : 1;
  }


/* compare_cmd - the comparison function for sorting by command name */

int
compare_cmd (
	       struct top_proc **pp1,
	       struct top_proc **pp2)
  {
    register struct top_proc *p1;
    register struct top_proc *p2;
    register long result;
    double dresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    ORDERKEY_NAME
    ORDERKEY_PCTCPU
    ORDERKEY_CPTICKS
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ;

    return result == 0 ? 0 : result < 0 ? -1 : 1;
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
proc_owner(int pid)

{
    struct stat sb;
    char buffer[32];
    sprintf(buffer, "%d", pid);

    if (stat(buffer, &sb) < 0)
	return -1;
    else
	return (int)sb.st_uid;
}
