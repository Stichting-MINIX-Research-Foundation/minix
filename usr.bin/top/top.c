
/* Author: Ben Gras <beng@few.vu.nl>  17 march 2006 */
/* Modified for ProcFS by Alen Stojanov and David van Moolenbroek */

#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <curses.h>
#include <timers.h>
#include <stdlib.h>
#include <limits.h>
#include <termcap.h>
#include <termios.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <assert.h>

#include <sys/ioc_tty.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>

#include <minix/com.h>
#include <minix/config.h>
#include <minix/type.h>
#include <minix/endpoint.h>
#include <minix/const.h>
#include <minix/u64.h>
#include <paths.h>
#include <minix/procfs.h>

#define TIMECYCLEKEY 't'
#define ORDERKEY 'o'

#define ORDER_CPU	0
#define ORDER_MEMORY	1
#define ORDER_HIGHEST	ORDER_MEMORY
int order = ORDER_CPU;

u32_t system_hz;

/* name of cpu cycle types, in the order they appear in /psinfo. */
const char *cputimenames[] = { "user", "ipc", "kernelcall" };

#define CPUTIMENAMES (sizeof(cputimenames)/sizeof(cputimenames[0]))

#define CPUTIME(m, i) (m & (1L << (i)))

unsigned int nr_procs, nr_tasks;
int nr_total;

#define  SLOT_NR(e) (_ENDPOINT_P(e) + nr_tasks)

#define  TC_BUFFER  1024        /* Size of termcap(3) buffer    */
#define  TC_STRINGS  200        /* Enough room for cm,cl,so,se  */

char *Tclr_all;

int blockedverbose = 0;

#define  USED		0x1
#define  IS_TASK	0x2
#define  IS_SYSTEM	0x4
#define  BLOCKED	0x8

struct proc {
	int p_flags;
	endpoint_t p_endpoint;
	pid_t p_pid;
	u64_t p_cpucycles[CPUTIMENAMES];
	int p_priority;
	endpoint_t p_blocked;
	time_t p_user_time;
	vir_bytes p_memory;
	uid_t p_effuid;
	int p_nice;
	char p_name[PROC_NAME_LEN+1];
};

struct proc *proc = NULL, *prev_proc = NULL;

static void parse_file(pid_t pid)
{
	char path[PATH_MAX], name[256], type, state;
	int version, endpt, effuid;
	unsigned long cycles_hi, cycles_lo;
	FILE *fp;
	struct proc *p;
	int slot;
	int i;

	sprintf(path, "%d/psinfo", pid);

	if ((fp = fopen(path, "r")) == NULL)
		return;

	if (fscanf(fp, "%d", &version) != 1) {
		fclose(fp);
		return;
	}

	if (version != PSINFO_VERSION) {
		fputs("procfs version mismatch!\n", stderr);
		exit(1);
	}

	if (fscanf(fp, " %c %d", &type, &endpt) != 2) {
		fclose(fp);
		return;
	}

	slot = SLOT_NR(endpt);

	if(slot < 0 || slot >= nr_total) {
		fprintf(stderr, "top: unreasonable endpoint number %d\n", endpt);
		fclose(fp);
		return;
	}

	p = &proc[slot];

	if (type == TYPE_TASK)
		p->p_flags |= IS_TASK;
	else if (type == TYPE_SYSTEM)
		p->p_flags |= IS_SYSTEM;

	p->p_endpoint = endpt;
	p->p_pid = pid;

	if (fscanf(fp, " %255s %c %d %d %u %*u %lu %lu",
		name, &state, &p->p_blocked, &p->p_priority,
		&p->p_user_time, &cycles_hi, &cycles_lo) != 7) {

		fclose(fp);
		return;
	}

	strncpy(p->p_name, name, sizeof(p->p_name)-1);
	p->p_name[sizeof(p->p_name)-1] = 0;

	if (state != STATE_RUN)
		p->p_flags |= BLOCKED;
	p->p_cpucycles[0] = make64(cycles_lo, cycles_hi);
	p->p_memory = 0L;

	if (!(p->p_flags & IS_TASK)) {
		int j;
		if ((j=fscanf(fp, " %lu %*u %*u %*c %*d %*u %u %*u %d %*c %*d %*u",
			&p->p_memory, &effuid, &p->p_nice)) != 3) {

			fclose(fp);
			return;
		}

		p->p_effuid = effuid;
	} else p->p_effuid = 0;

	for(i = 1; i < CPUTIMENAMES; i++) {
		if(fscanf(fp, " %lu %lu",
			&cycles_hi, &cycles_lo) == 2) {
			p->p_cpucycles[i] = make64(cycles_lo, cycles_hi);
		} else	{
			p->p_cpucycles[i] = 0;
		}
	}

	if ((p->p_flags & IS_TASK)) {
		if(fscanf(fp, " %lu", &p->p_memory) != 1) {
			p->p_memory = 0;
		}
	}

	p->p_flags |= USED;

	fclose(fp);
}

static void parse_dir(void)
{
	DIR *p_dir;
	struct dirent *p_ent;
	pid_t pid;
	char *end;

	if ((p_dir = opendir(".")) == NULL) {
		perror("opendir on " _PATH_PROC);
		exit(1);
	}

	for (p_ent = readdir(p_dir); p_ent != NULL; p_ent = readdir(p_dir)) {
		pid = strtol(p_ent->d_name, &end, 10);

		if (!end[0] && pid != 0)
			parse_file(pid);
	}

	closedir(p_dir);
}

static void get_procs(void)
{
	struct proc *p;
	int i;

	p = prev_proc;
	prev_proc = proc;
	proc = p;

	if (proc == NULL) {
		proc = malloc(nr_total * sizeof(proc[0]));

		if (proc == NULL) {
			fprintf(stderr, "Out of memory!\n");
			exit(1);
		}
	}

	for (i = 0; i < nr_total; i++)
		proc[i].p_flags = 0;

	parse_dir();
}

static int print_memory(void)
{
	FILE *fp;
	unsigned int pagesize;
	unsigned long total, free, largest, cached;

	if ((fp = fopen("meminfo", "r")) == NULL)
		return 0;

	if (fscanf(fp, "%u %lu %lu %lu %lu", &pagesize, &total, &free,
			&largest, &cached) != 5) {
		fclose(fp);
		return 0;
	}

	fclose(fp);

	printf("main memory: %ldK total, %ldK free, %ldK contig free, "
		"%ldK cached\n",
		(pagesize * total)/1024, (pagesize * free)/1024,
		(pagesize * largest)/1024, (pagesize * cached)/1024);

	return 1;
}

static int print_load(double *loads, int nloads)
{
	int i;
	printf("load averages: ");
	for(i = 0; i < nloads; i++)
		printf("%s %.2f", (i > 0) ? "," : "", loads[i]);
	printf("\n");
	return 1;
}

static int print_proc_summary(struct proc *proc)
{
	int p, alive, running, sleeping;

	alive = running = sleeping = 0;

	for(p = 0; p < nr_total; p++) {
		if (proc[p].p_endpoint == IDLE)
			continue;
		if(!(proc[p].p_flags & USED))
			continue;
		alive++;
		if(proc[p].p_flags & BLOCKED)
			sleeping++;
		else
			running++;
	}
	printf("%d processes: %d running, %d sleeping\n",
		alive, running, sleeping);
	return 1;
}

struct tp {
	struct proc *p;
	u64_t ticks;
};

static int cmp_procs(const void *v1, const void *v2)
{
	struct tp *p1 = (struct tp *) v1, *p2 = (struct tp *) v2;
	int p1blocked, p2blocked;

	if(order == ORDER_MEMORY) {
		if(p1->p->p_memory < p2->p->p_memory) return 1;
		if(p1->p->p_memory > p2->p->p_memory) return -1;
		return 0;
	}

	p1blocked = !!(p1->p->p_flags & BLOCKED);
	p2blocked = !!(p2->p->p_flags & BLOCKED);

	/* Primarily order by used number of cpu cycles.
	 *
	 * Exception: if in blockedverbose mode, a blocked
	 * process is always printed after an unblocked
	 * process, and used cpu cycles don't matter.
	 *
	 * In both cases, process slot number is a tie breaker.
	 */

	if(blockedverbose && (p1blocked || p2blocked)) {
		if(!p1blocked && p2blocked)
			return -1;
		if(!p2blocked && p1blocked)
			return 1;
	} else if(p1->ticks != p2->ticks) {
		return (p2->ticks - p1->ticks);
	}

	/* Process slot number is a tie breaker. */
	return (int) (p1->p - p2->p);
}

static struct tp *lookup(endpoint_t who, struct tp *tptab, int np)
{
	int t;

	for(t = 0; t < np; t++)
		if(who == tptab[t].p->p_endpoint)
			return &tptab[t];

	fprintf(stderr, "lookup: tp %d (0x%x) not found.\n", who, who);
	abort();

	return NULL;
}

double ktotal = 0;

static void print_proc(struct tp *tp, u64_t total_ticks)
{
	int euid = 0;
	static struct passwd *who = NULL;
	static int last_who = -1;
	char *name = "";
	int ticks;
	struct proc *pr = tp->p;

	printf("%5d ", pr->p_pid);
	euid = pr->p_effuid;
	name = pr->p_name;

	if(last_who != euid || !who) {
		who = getpwuid(euid);
		last_who = euid;
	}

	if(who && who->pw_name) printf("%-8s ", who->pw_name);
	else if(!(pr->p_flags & IS_TASK)) printf("%8d ", pr->p_effuid);
	else printf("         ");

	printf(" %2d ", pr->p_priority);
	if(!(pr->p_flags & IS_TASK)) {
		printf(" %3d ", pr->p_nice);
	} else printf("     ");
	printf("%6ldK", (pr->p_memory + 512) / 1024);
	printf("%6s", (pr->p_flags & BLOCKED) ? "" : "RUN");
	ticks = pr->p_user_time;
	printf(" %3u:%02u ", (ticks/system_hz/60), (ticks/system_hz)%60);

	printf("%6.2f%% %s", 100.0 * tp->ticks / total_ticks, name);
}

static char *cputimemodename(int cputimemode)
{
	static char name[100];
	int i;

	name[0] = '\0';

	for(i = 0; i < CPUTIMENAMES; i++) {
		if(CPUTIME(cputimemode, i)) {
			assert(strlen(name) +
				strlen(cputimenames[i]) < sizeof(name));
			strcat(name, cputimenames[i]);
			strcat(name, " ");
		}
	}

	return name;
}

static u64_t cputicks(struct proc *p1, struct proc *p2, int timemode)
{
	int i;
	u64_t t = 0;
	for(i = 0; i < CPUTIMENAMES; i++) {
		if(!CPUTIME(timemode, i))
			continue;
		if(p1->p_endpoint == p2->p_endpoint) {
			t = t + p2->p_cpucycles[i] - p1->p_cpucycles[i];
		} else {
			t = t + p2->p_cpucycles[i];
		}
	}

	return t;
}

static char *ordername(int orderno)
{
	switch(orderno) {
		case ORDER_CPU: return "cpu";
		case ORDER_MEMORY: return "memory";
	}
	return "invalid order";
}

static void print_procs(int maxlines,
	struct proc *proc1, struct proc *proc2, int cputimemode)
{
	int p, nprocs;
	u64_t idleticks = 0;
	u64_t kernelticks = 0;
	u64_t systemticks = 0;
	u64_t userticks = 0;
	u64_t total_ticks = 0;
	int blockedseen = 0;
	static struct tp *tick_procs = NULL;

	if (tick_procs == NULL) {
		tick_procs = malloc(nr_total * sizeof(tick_procs[0]));

		if (tick_procs == NULL) {
			fprintf(stderr, "Out of memory!\n");
			exit(1);
		}
	}

	for(p = nprocs = 0; p < nr_total; p++) {
		u64_t uticks;
		if(!(proc2[p].p_flags & USED))
			continue;
		tick_procs[nprocs].p = proc2 + p;
		tick_procs[nprocs].ticks = cputicks(&proc1[p], &proc2[p], cputimemode);
		uticks = cputicks(&proc1[p], &proc2[p], 1);
		total_ticks = total_ticks + uticks;
		if(p-NR_TASKS == IDLE) {
			idleticks = uticks;
			continue;
		}
		if(p-NR_TASKS == KERNEL) {
			kernelticks = uticks;
		}
		if(!(proc2[p].p_flags & IS_TASK)) {
			if(proc2[p].p_flags & IS_SYSTEM)
				systemticks = systemticks + tick_procs[nprocs].ticks;
			else
				userticks = userticks + tick_procs[nprocs].ticks;
		}

		nprocs++;
	}

	if (total_ticks == 0)
		return;

	qsort(tick_procs, nprocs, sizeof(tick_procs[0]), cmp_procs);

	printf("CPU states: %6.2f%% user, ", 100.0 * userticks / total_ticks);
	printf("%6.2f%% system, ", 100.0 * systemticks / total_ticks);
	printf("%6.2f%% kernel, ", 100.0 * kernelticks/ total_ticks);
	printf("%6.2f%% idle", 100.0 * idleticks / total_ticks);

#define NEWLINE do { printf("\n"); if(--maxlines <= 0) { return; } } while(0)
	NEWLINE;

	printf("CPU time displayed ('%c' to cycle): %s; ",
		TIMECYCLEKEY, cputimemodename(cputimemode));
	printf(" sort order ('%c' to cycle): %s", ORDERKEY, ordername(order));
	NEWLINE;

	NEWLINE;

	printf("  PID USERNAME PRI NICE    SIZE STATE   TIME     CPU COMMAND");
	NEWLINE;
	for(p = 0; p < nprocs; p++) {
		struct proc *pr;
		int level = 0;

		pr = tick_procs[p].p;

		if((pr->p_flags & IS_TASK) && pr->p_pid != KERNEL) {
			/* skip old kernel tasks as they don't run anymore */
			continue;
		}

		/* If we're in blocked verbose mode, indicate start of
		 * blocked processes.
		 */
		if(blockedverbose && (pr->p_flags & BLOCKED) && !blockedseen) {
			NEWLINE;
			printf("Blocked processes:");
			NEWLINE;
			blockedseen = 1;
		}

		print_proc(&tick_procs[p], total_ticks);
		NEWLINE;

		if(!blockedverbose)
			continue;

		/* Traverse dependency chain if blocked. */
		while(pr->p_flags & BLOCKED) {
			endpoint_t dep = NONE;
			struct tp *tpdep;
			level += 5;

			if((dep = pr->p_blocked) == NONE) {
				printf("not blocked on a process");
				NEWLINE;
				break;
			}

			if(dep == ANY)
				break;

			tpdep = lookup(dep, tick_procs, nprocs);
			pr = tpdep->p;
			printf("%*s> ", level, "");
			print_proc(tpdep, total_ticks);
			NEWLINE;
		}
	}
}

static void showtop(int cputimemode, int r)
{
#define NLOADS 3
	double loads[NLOADS];
	int nloads, lines = 0;
	struct winsize winsize;

	if(ioctl(STDIN_FILENO, TIOCGWINSZ, &winsize) != 0) {
		perror("TIOCGWINSZ");
		fprintf(stderr, "TIOCGWINSZ failed\n");
		exit(1);
	}

	get_procs();
	if (prev_proc == NULL)
		get_procs();

	if((nloads = getloadavg(loads, NLOADS)) != NLOADS) {
		fprintf(stderr, "getloadavg() failed - %d loads\n", nloads);
		exit(1);
	}


	printf("%s", Tclr_all);

	lines += print_load(loads, NLOADS);
	lines += print_proc_summary(proc);
	lines += print_memory();

	if(winsize.ws_row > 0) r = winsize.ws_row;

	print_procs(r - lines - 2, prev_proc, proc, cputimemode);
	fflush(NULL);
}

static void init(int *rows)
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

	initscr();
	cbreak();

	if ( (Tclr_all = tgetstr( "cl", &s )) == NULL )
		Tclr_all = "\f";

	if((v = tgetstr ("li", &s)) != NULL)
		sscanf(v, "%d", rows);
	if(*rows < 1) *rows = 24;
}

static void sigwinch(int sig) { }

static void getkinfo(void)
{
	FILE *fp;

	if ((fp = fopen("kinfo", "r")) == NULL) {
		fprintf(stderr, "opening " _PATH_PROC "kinfo failed\n");
		exit(1);
	}

	if (fscanf(fp, "%u %u", &nr_procs, &nr_tasks) != 2) {
		fprintf(stderr, "reading from " _PATH_PROC "kinfo failed\n");
		exit(1);
	}

	fclose(fp);

	nr_total = (int) (nr_procs + nr_tasks);
}

int main(int argc, char *argv[])
{
	int r, c, s = 0;
	int cputimemode = 1;	/* bitmap. */

	if (chdir(_PATH_PROC) != 0) {
		perror("chdir to " _PATH_PROC);
		return 1;
	}

	system_hz = (u32_t) sysconf(_SC_CLK_TCK);

	getkinfo();

	init(&r);

	while((c=getopt(argc, argv, "s:B")) != EOF) {
		switch(c) {
			case 's':
				s = atoi(optarg);
				break;
			case 'B':
				blockedverbose = 1;
				break;
			default:
				fprintf(stderr,
					"Usage: %s [-s<secdelay>] [-B]\n",
						argv[0]);
				return 1;
		}
	}

	if(s < 1)
		s = 2;

	/* Catch window size changes so display is updated properly
	 * right away.
	 */
	signal(SIGWINCH, sigwinch);

	while(1) {
		fd_set fds;
		int ns;
		struct timeval tv;
		showtop(cputimemode, r);
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
						putchar('\r');
						return 0;
						break;
					case 'o':
						order++;
						if(order > ORDER_HIGHEST)
							order = 0;
						break;
					case TIMECYCLEKEY:
						cputimemode++;
						if(cputimemode >= (1L << CPUTIMENAMES))
						cputimemode = 1;
						break;
				}
			}
		}
	}

	return 0;
}

