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
 * m_macosx.c
 *
 * AUTHOR:	Andrew S. Townley
 *		based on m_bsd44.c and m_next32.c
 *		by Christos Zoulas and Tim Pugh
 * CREATED:	Tue Aug 11 01:51:35 CDT 1998
 * SYNOPSIS:  MacOS X Server (Rhapsody Developer Release 2)
 * DESCRIPTION:
 *	MacOS X Server (Rhapsody Developer Release 2)
 *
 * CFLAGS: -DHAVE_STRERROR
 * TERMCAP: none
 * MATH: none
 */

/*
 * normal stuff
 */

#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "os.h"
#include "top.h"
#include "machine.h"
#include "utils.h"

/*
 * MacOS kernel stuff
 */

#include <kvm.h>
#include <fcntl.h>
#include <sys/dkstat.h>
#include <sys/sysctl.h>
#include <mach/message.h>
#include <mach/vm_statistics.h>
#include <mach/mach.h>
#include <mach/host_info.h>

#define VMUNIX		"/mach_kernel"
#define MEM		"/dev/mem"
#define SWAP		NULL

#define NUM_AVERAGES	3
#define LOG1024		10

#define PP(pp, field)	((pp)->kp_proc . field)
#define EP(pp, field)	((pp)->kp_eproc . field)
#define VP(pp, field)	((pp)->kp_eproc.e_vm . field)
#define MPP(mp, field)	(PP((mp)->kproc, field))
#define MEP(mp, field)	(EP((mp)->kproc, field))
#define MVP(mp, field)	(VP((mp)->kproc, field))
#define TP(mp, field)	((mp)->task_info . field)
#define RP(mp, field)	((mp)->thread_summary . field)

/* define what weighted cpu is */
#define weighted_cpu(pct, s) (s == 0 ? 0.0 : \
                         ((pct) / (1.0 - exp(s * logcpu)))) 

/* what we consider to be process size: */
#ifdef notdef
#define PROCSIZE(pp) (VP((pp), vm_tsize) + VP((pp), vm_dsize) + VP((pp), vm_ssize))
#endif
#define PROCSIZE(pp) (EP(pp, e_xsize))
#define TASKSIZE(t) (TP(t, virtual_size) + TP(t, resident_size))

/* what we consider to be resident set size: */
#ifdef notdef
#define RSSIZE(pp) (MVP((pp), vm_rssize))
#endif
#define RSSIZE(pp) (MEP((pp), e_xrssize))

#define pctdouble(p) ((double)(p) / FSCALE)

/*
 * globals
 */

static kvm_t		*kd = NULL;
static int		nproc;
static int		onproc = -1;
static int		pref_len;
static int		maxmem;
static char		fmt[MAX_COLS];
static double		logcpu = 1.0;

/* process array stuff */

static struct kinfo_proc	*kproc_list = NULL;
static struct macos_proc	*proc_list = NULL;
static struct macos_proc	**proc_ref = NULL;
static int			process_states[7];
static struct handle		handle;

/*
 * The mach information hopefully will not be necessary
 * when the kvm_* interfaces are supported completely.
 *
 * Since we're only concerned with task and thread info
 * for 'interesting' processes, we're going to only allocate
 * as many task and thread structures as needed.
 */

static struct task_basic_info	*task_list = NULL;

/* memory statistics */

static int 		pageshift 	= 0;
static int		pagesize	= 0;
#define pagetok(size)	((size) << pageshift)

static int		swappgsin	= -1;
static int		swappgsout	= -1;
static vm_statistics_data_t	vm_stats;
static long		memory_stats[7];

/* CPU state percentages */

host_cpu_load_info_data_t cpuload;

static long	cp_time[CPU_STATE_MAX];
static long	cp_old[CPU_STATE_MAX];
static long	cp_diff[CPU_STATE_MAX];
static int		cpu_states[CPU_STATE_MAX];

/*
 * types
 */

typedef long 		pctcpu;

//struct statics
//{
//	char	**procstate_names;
//	char	**cpustate_names;
//	char	**memory_names;
//	char	**order_names;
//};
//
//struct system_info
//{
//	int	last_pid;
//	double	load_avg[NUM_AVERAGES];
//	int	p_total;	/* total # of processes */
//	int	p_active;	/* number processes considered active */
//	int	*procstates;
//	int	*cpustates;
//	int	*memory;
//};
//
//struct process_select
//{
//	int	idle;		/* show idle processes */
//	int	system;		/* show system processes */
//	int	uid;		/* show only this uid (unless -1) */
//	char	*command;	/* only this command (unless NULL) */
//};

/*
 * We need to declare a hybrid structure which will store all
 * of the stuff we care about for each process.
 */

struct macos_proc
{
	struct kinfo_proc		*kproc;
	task_t				the_task;
	struct task_basic_info		task_info;
	unsigned int			thread_count;
	struct thread_basic_info	thread_summary;
};

struct handle
{
	struct macos_proc		**next_proc;
	int				remaining;
};

static char header[] =
  "  PID X        PRI THRD  SIZE   RES STATE   TIME    MEM    CPU COMMAND";
/* 0123456   -- field to fill in starts at header+6 */
#define UNAME_START 6
     
#define Proc_format \
        "%5d %-8.8s %3d %4d %5s %5s %-5s %6s %5.2f%% %5.2f%% %.16s"


int proc_compare(const void *, const void *);


/*
 * puke()
 *
 * This function is used to report errors to stderr.
 */

static void puke(const char* fmt, ...)
{
	va_list	args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fputc('\n', stderr);
	fflush(stderr);
}

/*
 * kread()
 *
 * This function is a wrapper for the kvm_read() function
 * with the addition of a message parameter per kvm_open().
 *
 * All other behavior is per kvm_read except the error reporting.
 */

static ssize_t kread(u_long addr, void *buf, 
	size_t nbytes, const char *errstr)
{
	ssize_t	s = 0;

	s = kvm_read(kd, addr, buf, nbytes);
	if(s == -1)
		{
		puke("error:  kvm_read() failed for '%s' (%s)\n",
			errstr, strerror(errno));
		}

	return s;
}

/*
 * prototypes for functions which top needs
 */

char *printable();

/*
 * definitions for offsets
 */

#define X_NPROC		0
#define X_HZ		1
#define X_MAXMEM	2

#define NLIST_LAST	3

static struct nlist	nlst[] =
{
	{ "_maxproc" },		/* 0 *** maximum processes */
	{ "_hz" },		/* 1 */
	{ "_mem_size" },	/* 2 */
	{ 0 }
};

static char *procstates[] =
{
	"",
	" starting, ",
	" running, ",
	" sleeping, ",
	" stopped, ",
	" zombie, ",
	" swapped ",
	NULL
};

static char *cpustates[] =
{
	"user",
	"system",
	"idle",
	"nice",
	NULL
};

static char *state_abbrev[] =
{
	"",
	"start",
	"run\0\0\0",
	"sleep",
	"stop",
	"zomb"
};

static char *mach_state[] =
{
	"",
	"R",
	"T",
	"S",
	"U",
	"H"
};

static char *thread_state[] =
{
	"",
	"run\0\0\0",
	"stop",
	"wait",
	"uwait",
	"halted",
};

static char *flags_state[] =
{
	"",
	"W",
	"I"
};

static char *memnames[] =
{
	"K Tot, ",
	"K Free, ",
	"K Act, ",
	"K Inact, ",
	"K Wired, ",
	"K in, ",
	"K out ",
	NULL
};

/*
 * format_header()
 *
 * This function is used to add the username into the
 * header information.
 */

char *format_header(register char *uname_field)
{
	register char *ptr;

	ptr = header + UNAME_START;
	while(*uname_field != '\0')
		*ptr++ = *uname_field++;

	return(header);
}

/*
 * format_next_process()
 *
 * This function actuall is responsible for the formatting of
 * each row which is displayed.
 */

char *format_next_process(caddr_t handle, char *(*getuserid)())
{
	register struct macos_proc	*pp;
	register long			cputime;
	register double			pct;
	register int			vsize;
	register int			rsize;
	struct handle			*hp;

	/*
	 * we need to keep track of the next proc structure.
	 */

	hp = (struct handle*)handle;
	pp = *(hp->next_proc++);
	hp->remaining--;

	/*
	 * get the process structure and take care of the cputime
	 */

	if((MPP(pp, p_flag) & P_INMEM) == 0)
		{
		/* we want to print swapped processes as <pname> */
		char	*comm = MPP(pp, p_comm);
#define COMSIZ	sizeof(MPP(pp, p_comm))
		char	buf[COMSIZ];
		strncpy(buf, comm, COMSIZ);
		comm[0] = '<';
		strncpy(&comm[1], buf, COMSIZ - 2);
		comm[COMSIZ - 2] = '\0';
		strncat(comm, ">", COMSIZ - 1);
		comm[COMSIZ - 1] = '\0';
		}

	/*
	 * count the cpu time, but ignore the interrupts
	 *
	 * At the present time (DR2 8/1998), MacOS X doesn't
	 * correctly report this information through the
	 * kinfo_proc structure.  We need to get it from the
	 * task threads.
	 *
	 * cputime = PP(pp, p_rtime).tv_sec;
	 */
	
	cputime = RP(pp, user_time).seconds + RP(pp, system_time).seconds;

	/*
	 * calculate the base cpu percentages
	 *
	 * Again, at the present time, MacOS X doesn't report
	 * this information through the kinfo_proc.  We need
	 * to talk to the threads.
	 */

//	pct = pctdouble(PP(pp, p_pctcpu));
	pct = (double)(RP(pp, cpu_usage))/TH_USAGE_SCALE;

	/*
	 * format the entry
	 */

	/*
	 * In the final version, I would expect this to work correctly,
	 * but it seems that not all of the fields in the proc
	 * structure are being used.
	 *
	 * For now, we'll attempt to get some of the things we need
	 * from the mach task info.
	 */

	sprintf(fmt,
		Proc_format,
		MPP(pp, p_pid),
		(*getuserid)(MEP(pp, e_pcred.p_ruid)),
//		TP(pp, base_priority),
		0,
		pp->thread_count,
		format_k(TASKSIZE(pp) / 1024),
		format_k(pagetok(RSSIZE(pp))),
		state_abbrev[(u_char)MPP(pp, p_stat)],
		format_time(cputime),
		100.0 * TP(pp, resident_size) / maxmem,
//		100.0 * weighted_cpu(pct, (RP(pp, user_time).seconds + RP(pp, system_time).seconds)),
		100.0 * pct,
		printable(MPP(pp, p_comm)));

	return(fmt);
}

/*
 * get_process_info()
 *
 * This function returns information about the processes
 * on the system.
 */

caddr_t get_process_info(struct system_info *si,
		struct process_select *sel, int x)

{
	register int 				i;
	register int 				total_procs;
	register int 				active_procs;
	register struct macos_proc 		**prefp;
	register struct macos_proc 		*pp;
	register struct kinfo_proc		*pp2;
	register struct kinfo_proc		**prefp2;
	register struct thread_basic_info 	*thread;

	/*
	 * these are copied out of sel for speed
	 */

	int show_idle;
	int show_system;
	int show_uid;
	int show_command;

	kproc_list = kvm_getprocs(kd, KERN_PROC_ALL, 0, &nproc);

	if(nproc > onproc)
		{
		proc_list = (struct macos_proc*)realloc(proc_list, sizeof(struct macos_proc) * nproc);
		proc_ref = (struct macos_proc **)realloc(proc_ref, sizeof(struct macos_proc *) * (onproc = nproc));
		}

	if(proc_ref == NULL || proc_list == NULL || kproc_list == NULL)
		{
		puke("error:  out of memory (%s)", strerror(errno));
		return(NULL);
		}

	/*
	 * now, our task is to build the array of information we
	 * need to function correctly.  This involves setting a pointer
	 * to each real kinfo_proc structure returned by kvm_getprocs()
	 * in addition to getting the mach information for each of
	 * those processes.
	 */

	for(pp2 = kproc_list, i = 0; i < nproc; pp2++, i++)
		{
		kern_return_t	rc;
		u_int		info_count = TASK_BASIC_INFO_COUNT;

		/*
		 * first, we set the pointer to the reference in
		 * the kproc list.
		 */
		
		proc_list[i].kproc = pp2;

		/*
		 * then, we load all of the task info for the process
		 */

		if(PP(pp2, p_stat) != SZOMB)
			{
			rc = task_for_pid(mach_task_self(), 
				PP(pp2, p_pid), 
				&(proc_list[i].the_task));

			if(rc != KERN_SUCCESS)
				{
				puke("error:  get task info for pid %d failed with rc = %d", PP(pp2, p_pid), rc);
				}

			/*
			 * load the task information
			 */

			rc = task_info(proc_list[i].the_task, TASK_BASIC_INFO, 
				(task_info_t)&(proc_list[i].task_info),
				&info_count);

			if(rc != KERN_SUCCESS)
				{
				puke("error:  couldn't get task info (%s); rc = %d", strerror(errno), rc);
				}

			/*
			 * load the thread summary information
			 */

			load_thread_info(&proc_list[i]);
			}
		}

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
	memset((char *)process_states, 0, sizeof(process_states));
	prefp = proc_ref;
	for(pp = proc_list, i = 0; i < nproc; pp++, i++)
		{
		/*
		 *  Place pointers to each valid proc structure in 
		 * proc_ref[].  Process slots that are actually in use 
		 * have a non-zero status field.  Processes with
		 * P_SYSTEM set are system processes---these get 
		 * ignored unless show_sysprocs is set.
		 */
		if(MPP(pp, p_stat) != 0 && 
				(show_system || ((MPP(pp, p_flag) & P_SYSTEM) == 0)))
			{
			total_procs++;
			process_states[(unsigned char) MPP(pp, p_stat)]++;
			if((MPP(pp, p_stat) != SZOMB) &&
					(show_idle || (MPP(pp, p_pctcpu) != 0) || 
			 		(MPP(pp, p_stat) == SRUN)) &&
					(!show_uid || MEP(pp, e_pcred.p_ruid) == (uid_t)sel->uid))
				{
				*prefp++ = pp;
				active_procs++;
				}
			}
		}
	
	/* 
	 * if requested, sort the "interesting" processes
	 */

	qsort((char *)proc_ref, active_procs, sizeof(struct macos_proc *), proc_compare);

	/* remember active and total counts */
	si->p_total = total_procs;
	si->p_active = pref_len = active_procs;

	/* pass back a handle */
	handle.next_proc = proc_ref;
	handle.remaining = active_procs;
	return((caddr_t)&handle);
}

/*
 * get_system_info()
 *
 * This function is responsible for geting the periodic
 * system information snapshot.
 */

void get_system_info(struct system_info *si)
{
	register long	total;
	register int	i;
	unsigned int count = HOST_CPU_LOAD_INFO_COUNT;

	if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
			    (host_info_t)&cpuload, &count) == KERN_SUCCESS)
	{
	    for (i = 0; i < CPU_STATE_MAX; i++)
	    {
		cp_time[i] = cpuload.cpu_ticks[i];
	    }
	}

#ifdef MAX_VERBOSE

	/*
	 * print out the entries
	 */

	for(i = 0; i < CPU_STATE_MAX; i++)
		printf("cp_time[%d] = %d\n", i, cp_time[i]);
	fflush(stdout);

#endif /* MAX_VERBOSE */

	/*
	 * get the load averages
	 */

	if(kvm_getloadavg(kd, si->load_avg, NUM_AVERAGES) == -1)
		{
		puke("error:  kvm_getloadavg() failed (%s)", strerror(errno));
		return;
		}

#ifdef MAX_VERBOSE
	printf("%-30s%03.2f, %03.2f, %03.2f\n", 
			"load averages:", 
			si->load_avg[0],
			si->load_avg[1],
			si->load_avg[2]);
#endif /* MAX_VERBOSE */

	total = percentages(CPU_STATE_MAX, cpu_states, cp_time, cp_old, cp_diff);
	/*
	 * get the memory statistics
	 */

	{
		kern_return_t	status;

		count = HOST_VM_INFO_COUNT;
		status = host_statistics(mach_host_self(), HOST_VM_INFO,
					 (host_info_t)&vm_stats, &count);

		if(status != KERN_SUCCESS)
			{
			puke("error:  vm_statistics() failed (%s)", strerror(errno));
			return;
			}

		/*
		 * we already have the total memory, we just need
		 * to get it in the right format.
		 */

		memory_stats[0] = pagetok(maxmem / pagesize);
		memory_stats[1] = pagetok(vm_stats.free_count);
		memory_stats[2] = pagetok(vm_stats.active_count);
		memory_stats[3] = pagetok(vm_stats.inactive_count);
		memory_stats[4] = pagetok(vm_stats.wire_count);

		if(swappgsin < 0)
			{
			memory_stats[5] = 1;
			memory_stats[6] = 1;
			}
		else
			{
			memory_stats[5] = pagetok(((vm_stats.pageins - swappgsin)));
			memory_stats[6] = pagetok(((vm_stats.pageouts - swappgsout)));
			}
		swappgsin = vm_stats.pageins;
		swappgsout = vm_stats.pageouts;
	}
	
	si->cpustates = cpu_states;
	si->memory = memory_stats;
	si->last_pid = -1;

	return;
}

/*
 * machine_init()
 *
 * This function is responsible for filling in the values of the
 * statics structure.
 */

int machine_init(struct statics *stat)
{
	register int rc = 0;
	register int i = 0;
	size_t size;

	size = sizeof(maxmem);
	sysctlbyname("hw.physmem", &maxmem, &size, NULL, 0);

	size = sizeof(nproc);
	sysctlbyname("kern.maxproc", &nproc, &size, NULL, 0);

#ifdef MAX_VERBOSE
	printf("%-30s%10d\n", "total system memory:", maxmem);
#endif /* MAX_VERBOSE */

	/*
	 * calculate the pageshift from the system page size
	 */

	pagesize = getpagesize();
	pageshift = 0;
	while((pagesize >>= 1) > 0)
		pageshift++;

	pageshift -= LOG1024;

	/*
	 * fill in the statics information
	 */

	stat->procstate_names = procstates;
	stat->cpustate_names = cpustates;
	stat->memory_names = memnames;

	if ((kd = kvm_open(NULL, NULL, NULL, O_RDONLY, "kvm_open")) == NULL)
	  return -1;

	return(0);
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
 
int proc_compare(const void *pp1, const void *pp2)
{
    register struct macos_proc *p1;
    register struct macos_proc *p2;
    register int result;
    register pctcpu lresult;

    /* remove one level of indirection */
    p1 = *(struct macos_proc **) pp1;
    p2 = *(struct macos_proc **) pp2;

    /* compare percent cpu (pctcpu) */
    if ((lresult = RP(p2, cpu_usage) - RP(p1, cpu_usage)) == 0)
    {
	/* use cpticks to break the tie */
	if ((result = MPP(p2, p_cpticks) - MPP(p1, p_cpticks)) == 0)
	{
	    /* use process state to break the tie */
	    if ((result = sorted_state[(unsigned char) MPP(p2, p_stat)] -
			  sorted_state[(unsigned char) MPP(p1, p_stat)])  == 0)
	    {
		/* use priority to break the tie */
		if ((result = MPP(p2, p_priority) - MPP(p1, p_priority)) == 0)
		{
		    /* use resident set size (rssize) to break the tie */
		    if ((result = RSSIZE(p2) - RSSIZE(p1)) == 0)
		    {
			/* use total memory to break the tie */
			result = PROCSIZE(p2->kproc) - PROCSIZE(p1->kproc);
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
    register struct macos_proc **prefp;
    register struct macos_proc *pp;

    prefp = proc_ref;
    cnt = pref_len;
    while (--cnt >= 0)
    {
	pp = *prefp++;	
	if (MPP(pp, p_pid) == (pid_t)pid)
	{
	    return((int)MEP(pp, e_pcred.p_ruid));
	}
    }
    return(-1);
}

/*
 * load_thread_info()
 *
 * This function will attempt to load the thread summary info
 * for a Mach task.  The task is located as part of the macos_proc
 * structure.
 *
 * returns the kern_return_t value of any failed call or KERN_SUCCESS
 * if everything works.
 */

int load_thread_info(struct macos_proc *mp)
{
	register kern_return_t		rc = 0;
	register int			i = 0;
	register int			t_utime = 0;
	register int			t_stime = 0;
	register int			t_cpu = 0;
	register int			t_state = 0;
	register task_t			the_task = mp->the_task;

	thread_array_t			thread_list = NULL;

	/*
	 * We need to load all of the threads for the 
	 * given task so we can get the performance 
	 * data from them.
	 */

	mp->thread_count = 0;
	rc = task_threads(the_task, &thread_list, &(mp->thread_count));

	if(rc != KERN_SUCCESS)
		{
//		puke("error:  unable to load threads for task (%s); rc = %d", strerror(errno), rc);
		return(rc);
		}

	/*
	 * now, for each of the threads, we need to sum the stats
	 * so we can present the whole thing to the caller.
	 */

	for(i = 0; i < mp->thread_count; i++)
		{
		struct thread_basic_info	t_info;
		unsigned int			icount = THREAD_BASIC_INFO_COUNT;
		kern_return_t			rc = 0;

		rc = thread_info(thread_list[i], THREAD_BASIC_INFO, 
				(thread_info_t)&t_info, &icount);

		if(rc != KERN_SUCCESS)
			{
			puke("error:  unable to load thread info for task (%s); rc = %d", strerror(errno), rc);
			return(rc);
			}

		t_utime += t_info.user_time.seconds;
		t_stime += t_info.system_time.seconds;
		t_cpu += t_info.cpu_usage;
		}

	vm_deallocate(mach_task_self(), (vm_address_t)thread_list, sizeof(thread_array_t)*(mp->thread_count));

	/*
	 * Now, we load the values in the structure above.
	 */

	RP(mp, user_time).seconds = t_utime;
	RP(mp, system_time).seconds = t_stime;
	RP(mp, cpu_usage) = t_cpu;

	return(KERN_SUCCESS);
}

