
/* Author: Ben Gras <beng@few.vu.nl>  17 march 2006 */

#define _MINIX 1
#define _POSIX_SOURCE 1

#include <stdio.h>
#include <pwd.h>
#include <curses.h>
#include <timers.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <termcap.h>
#include <termios.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/ioc_tty.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>

#include <minix/ipc.h>
#include <minix/com.h>
#include <minix/sysinfo.h>
#include <minix/config.h>
#include <minix/type.h>
#include <minix/const.h>

#include <minix/u64.h>

#include "../../kernel/arch/i386/include/archtypes.h"
#include "../../servers/pm/mproc.h"
#include "../../kernel/const.h"
#include "../../kernel/proc.h"

u32_t system_hz;

#define  TC_BUFFER  1024        /* Size of termcap(3) buffer    */
#define  TC_STRINGS  200        /* Enough room for cm,cl,so,se  */

char *Tclr_all;

#if 0
int print_memory(struct pm_mem_info *pmi)
{
        int h;
        int largest_bytes = 0, total_bytes = 0; 
        for(h = 0; h < _NR_HOLES; h++) {
                if(pmi->pmi_holes[h].h_base && pmi->pmi_holes[h].h_len) {
                        int bytes;
                        bytes = pmi->pmi_holes[h].h_len << CLICK_SHIFT;
                        if(bytes > largest_bytes) largest_bytes = bytes;
                        total_bytes += bytes;
                }
        }

	printf("Mem: %dK Free, %dK Contiguous Free\n",
		total_bytes/1024, largest_bytes/1024);

	return 1;
}
#endif

int print_load(double *loads, int nloads)
{
	int i;
	printf("load averages: ");
	for(i = 0; i < nloads; i++)
		printf("%s %.2f", (i > 0) ? "," : "", loads[i]);
	printf("\n");
	return 1;
}

#define PROCS (NR_PROCS+NR_TASKS)

int print_proc_summary(struct proc *proc)
{
	int p, alive, running, sleeping;

	alive = running = sleeping = 0;

	for(p = 0; p < PROCS; p++) {
		if(p - NR_TASKS == IDLE)
			continue;
		if(isemptyp(&proc[p]))
			continue;
		alive++;
		if(!proc_is_runnable(&proc[p]))
			sleeping++;
		else
			running++;
	}
	printf("%d processes: %d running, %d sleeping\n",
		alive, running, sleeping);
	return 1;
}

static struct tp {
	struct proc *p;
	u64_t ticks;
} tick_procs[PROCS];

int cmp_ticks(const void *v1, const void *v2)
{
	struct tp *p1 = (struct tp *) v1, *p2 = (struct tp *) v2;

	if (p1->ticks.hi == p2->ticks.hi) {
		if(p1->ticks.lo < p2->ticks.lo)
			return 1;
		if(p1->ticks.lo > p2->ticks.lo)
			return -1;
	}
	if(p1->ticks.hi < p2->ticks.hi)
		return 1;
	if(p1->ticks.hi > p2->ticks.hi)
		return -1;
	if(p1->p->p_nr < p2->p->p_nr)
		return -1;
	if(p1->p->p_nr > p2->p->p_nr)
		return 1;
	return 0;
}

/*
 * since we don't have true div64(u64_t, u64_t) we scale the 64 bit counters to
 * 32. Since the samplig happens every ~1s and the counters count CPU cycles
 * during this period, unless we have extremely fast CPU, shifting the counters
 * by 12 make them 32bit counters which we can use for normal integer
 * arithmetics
 */
#define SCALE	(1 << 12)

void print_procs(int maxlines,
	struct proc *proc1, struct proc *proc2,
	struct mproc *mproc)
{
	int p, nprocs, tot=0;
	u64_t idleticks = cvu64(0);
	u64_t kernelticks = cvu64(0);
	u64_t systemticks = cvu64(0);
	u64_t userticks = cvu64(0);
	u64_t total_ticks = cvu64(0);
	unsigned long tcyc;
	unsigned long tmp;

	for(p = nprocs = 0; p < PROCS; p++) {
		if(isemptyp(&proc2[p]))
			continue;
		tick_procs[nprocs].p = proc2 + p;
		if(proc1[p].p_endpoint == proc2[p].p_endpoint) {
			tick_procs[nprocs].ticks =
				sub64(proc2[p].p_cycles, proc1[p].p_cycles);
		} else {
			tick_procs[nprocs].ticks =
				proc2[p].p_cycles;
		}
		total_ticks = add64(total_ticks, tick_procs[nprocs].ticks);
		if(p-NR_TASKS == IDLE) {
			idleticks = tick_procs[nprocs].ticks;
			continue;
		}
		if(p-NR_TASKS == KERNEL) {
			kernelticks = tick_procs[nprocs].ticks;
			continue;
		}
		if(mproc[proc2[p].p_nr].mp_procgrp == 0)
			systemticks = add64(systemticks, tick_procs[nprocs].ticks);
		else if (p > NR_TASKS)
			userticks = add64(userticks, tick_procs[nprocs].ticks);

		nprocs++;
	}

	if (!cmp64u(total_ticks, 0))
		return;

	qsort(tick_procs, nprocs, sizeof(tick_procs[0]), cmp_ticks);

	tcyc = div64u(total_ticks, SCALE);

	tmp = div64u(userticks, SCALE);
	printf("CPU states: %6.2f%% user, ", 100.0*(tmp)/tcyc);

	tmp = div64u(systemticks, SCALE);
	printf("%6.2f%% system, ", 100.0*tmp/tcyc);

	tmp = div64u(kernelticks, SCALE);
	printf("%6.2f%% kernel, ", 100.0*tmp/tcyc);

	tmp = div64u(idleticks, SCALE);
	printf("%6.2f%% idle", 100.0*tmp/tcyc);

	printf("\n\n");
	maxlines -= 2;

	printf("  PID USERNAME PRI NICE   SIZE STATE   TIME     CPU COMMAND\n");
	maxlines--;
	for(p = 0; p < nprocs; p++) {
		int euid = 0;
		struct proc *pr;
		int pnr, ticks;
		char *name = "";
		static struct passwd *who = NULL;
		static int last_who = -1;

		unsigned long pcyc;

		if(maxlines-- <= 0) break;

		pnr = tick_procs[p].p->p_nr;
		pr = tick_procs[p].p;
		ticks = pr->p_user_time;
		if(pnr >= 0) {
			printf("%5d ", mproc[pnr].mp_pid);
			euid = mproc[pnr].mp_effuid;
			name = mproc[pnr].mp_name;
		} else
			/* skip old kernel tasks as they don't run anymore */
			continue;
		if(last_who != euid || !who) {
			who = getpwuid(euid);
			last_who = euid;
		}

		if(who && who->pw_name) printf("%-8s ", who->pw_name);
		else if(pnr >= 0) printf("%8d ", mproc[pnr].mp_effuid);
		else printf("         ");

		printf(" %2d ", pr->p_priority);
		if(pnr >= 0) {
			printf(" %3d ", mproc[pnr].mp_nice);
		} else printf("     ");
		printf("%5dK",
			((pr->p_memmap[T].mem_len + 
			pr->p_memmap[D].mem_len) << CLICK_SHIFT)/1024);
		printf("%6s", pr->p_rts_flags ? "" : "RUN");
		printf(" %3d:%02d ", (ticks/system_hz/60), (ticks/system_hz)%60);

		pcyc = div64u(tick_procs[p].ticks, SCALE);

		printf("%6.2f%% %s\n",
			100.0*pcyc/tcyc, name);
	}
}

void showtop(int r)
{
#define NLOADS 3
	double loads[NLOADS];
	int nloads, i, p, lines = 0;
	static struct proc prev_proc[PROCS], proc[PROCS];
	static int preheated = 0;
	struct winsize winsize;
        /*
	static struct pm_mem_info pmi;
	*/
	static struct mproc mproc[NR_PROCS];
	int mem = 0;

	if(ioctl(STDIN_FILENO, TIOCGWINSZ, &winsize) != 0) {
		perror("TIOCGWINSZ");
		fprintf(stderr, "TIOCGWINSZ failed\n");
		exit(1);
	}

#if 0
        if(getsysinfo(PM_PROC_NR, SI_MEM_ALLOC, &pmi) < 0) {
		fprintf(stderr, "getsysinfo() for SI_MEM_ALLOC failed.\n");
		mem = 0;
		exit(1);;
	} else mem = 1;
#endif

retry:
	if(getsysinfo(PM_PROC_NR, SI_KPROC_TAB, proc) < 0) {
		fprintf(stderr, "getsysinfo() for SI_KPROC_TAB failed.\n");
		exit(1);
	}
	if (!preheated) {
		preheated = 1;
		memcpy(prev_proc, proc, sizeof(prev_proc));
		goto retry;;
	}

	if(getsysinfo(PM_PROC_NR, SI_PROC_TAB, mproc) < 0) {
		fprintf(stderr, "getsysinfo() for SI_PROC_TAB failed.\n");
		exit(1);
	}

	if((nloads = getloadavg(loads, NLOADS)) != NLOADS) {
		fprintf(stderr, "getloadavg() failed - %d loads\n", nloads);
		exit(1);
	}


	printf("%s", Tclr_all);

	lines += print_load(loads, NLOADS);
	lines += print_proc_summary(proc);
#if 0
	if(mem) { lines += print_memory(&pmi); }
#endif

	if(winsize.ws_row > 0) r = winsize.ws_row;

	print_procs(r - lines - 2, prev_proc,
		proc, mproc);

	memcpy(prev_proc, proc, sizeof(prev_proc));
}

void init(int *rows)
{
	char  *term;
	static char   buffer[TC_BUFFER], strings[TC_STRINGS];
	char *s = strings, *v;

	*rows = 0;

	if(!(term = getenv("TERM"))) {
		fprintf(stderr, "No TERM set\n");
		exit(1);
	}

	if ( tgetent( buffer, term ) != 1 ) {
		fprintf(stderr, "tgetent failed for term %s\n", term);
		exit(1);
	}

	if ( (Tclr_all = tgetstr( "cl", &s )) == NULL )
		Tclr_all = "\f";

	if((v = tgetstr ("li", &s)) != NULL)
		sscanf(v, "%d", rows);
	if(*rows < 1) *rows = 24;
	if(!initscr()) {
		fprintf(stderr, "initscr() failed\n");
		exit(1);
	}
	cbreak();
	nl();
}

void sigwinch(int sig) { }

int main(int argc, char *argv[])
{
	int r, c, s = 0, orig;

	getsysinfo_up(PM_PROC_NR, SIU_SYSTEMHZ, sizeof(system_hz), &system_hz);

	init(&r);

	while((c=getopt(argc, argv, "s:")) != EOF) {
		switch(c) {
			case 's':
				s = atoi(optarg);
				break;
			default:
				fprintf(stderr,
					"Usage: %s [-s<secdelay>]\n", argv[0]);
				return 1;
		}
	}

	if(s < 1) 
		s = 2;

	/* Catch window size changes so display is updated properly right away. */
	signal(SIGWINCH, sigwinch);

	while(1) {
		fd_set fds;
		int ns;
		struct timeval tv;
		showtop(r);
		tv.tv_sec = s;
		tv.tv_usec = 0;

		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		if((ns=select(STDIN_FILENO+1, &fds, NULL, NULL, &tv)) < 0
			&& errno != EINTR) {
			perror("select");
			sleep(1);
		}

		if(ns > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
			char c;
			if(read(STDIN_FILENO, &c, 1) == 1) {
				switch(c) {
					case 'q':
						return 0;
						break;
				}
			}
		}
	}

	return 0;
}

