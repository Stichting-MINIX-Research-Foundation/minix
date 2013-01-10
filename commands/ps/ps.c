/* ps - print status			Author: Peter Valkenburg */
/* Modified for ProcFS by Alen Stojanov and David van Moolenbroek */

/* Ps.c, Peter Valkenburg (valke@psy.vu.nl), january 1990.
 *
 * This is a V7 ps(1) look-alike for MINIX >= 1.5.0.
 * It does not support the 'k' option (i.e. cannot read memory from core file).
 * If you want to compile this for non-IBM PC architectures, the header files
 * require that you have your CHIP, MACHINE etc. defined.
 * Full syntax:
 *	ps [-][aeflx]
 * Option `a' gives all processes, `l' for detailed info, `x' includes even
 * processes without a terminal.
 * The `f' and `e' options were added by Kees Bot for the convenience of 
 * Solaris users accustomed to these options. The `e' option is equivalent to 
 * `a' and `f' is equivalent to  -l. These do not appear in the usage message.
 */

/* Some technical comments on this implementation:
 *
 * Most fields are similar to V7 ps(1), except for CPU, NICE, PRI which are
 * absent, RECV which replaces WCHAN, and PGRP that is an extra.
 * The info is obtained from the following fields of proc, mproc and fproc:
 * ST	- kernel status field, p_rts_flags; pm status field, mp_flags (R if
 *        p_rts_flags is 0; Z if mp_flags == ZOMBIE; T if mp_flags == STOPPED;
 *        else W).
 * UID	- pm eff uid field, mp_effuid
 * PID	- pm pid field, mp_pid
 * PPID	- pm parent process index field, mp_parent (used as index in proc).
 * PGRP - pm process group field, mp_procgrp
 * SZ	- memory size, including common and shared memory
 * RECV	- kernel process index field for message receiving, p_getfrom
 *	  If sleeping, pm's mp_flags, or fs's fp_task are used for more info.
 * TTY	- fs controlling tty device field, fp_tty.
 * TIME	- kernel user + system times fields, user_time + sys_time
 * CMD	- system process index (converted to mnemonic name by using the p_name
 *	  field), or user process argument list (obtained by reading the stack
 *	  frame; the resulting address is used to get the argument vector from
 *	  user space and converted into a concatenated argument list).
 */

#include <minix/config.h>
#include <minix/endpoint.h>
#include <paths.h>
#include <minix/procfs.h>
#include <limits.h>
#include <sys/types.h>

#include <minix/const.h>
#include <minix/type.h>
#include <minix/dmap.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <ttyent.h>


/*----- ps's local stuff below this line ------*/

/* Structure for tty name info. */
typedef struct {
  char tty_name[NAME_MAX + 1];	/* file name in /dev */
  dev_t tty_dev;		/* major/minor pair */
} ttyinfo_t;

ttyinfo_t *ttyinfo;		/* ttyinfo holds actual tty info */
size_t n_ttyinfo;		/* Number of tty info slots */

u32_t system_hz;		/* system clock frequency */
unsigned int nr_procs; 		/* maximum number of processes */
unsigned int nr_tasks;		/* maximum number of tasks */

struct pstat *ptable;		/* table with process information */

/* Macro to convert endpoints to slots into ptable */
#define SLOT_NR(e) (_ENDPOINT_P(e) + nr_tasks)

/* Macro to convert memory offsets to rounded kilo-units */
#define	off_to_k(off)	((unsigned) (((off) + 512) / 1024))


/* Short and long listing formats:
 *
 *   PID TTY  TIME CMD
 * ppppp tttmmm:ss cccccccccc...
 *
 * ST UID   PID  PPID  PGRP   SZ       RECV TTY  TIME CMD
 *  s uuu ppppp ppppp ppppp ssss rrrrrrrrrr tttmmm:ss cccccccc...
 */
#define S_HEADER "  PID TTY  TIME CMD\n"
#define S_FORMAT "%5s %3s %s %s\n"
#define L_HEADER "ST UID   PID  PPID  PGRP     SZ         RECV TTY  TIME CMD\n"
#define L_FORMAT " %c %3d %5s %5d %5d %6d %12s %3s %s %s\n"


struct pstat {			/* structure filled by pstat() */
  struct pstat *ps_next;	/* next in process list */
  int ps_task;			/* is this process a task or not? */
  int ps_endpt;			/* process endpoint (NONE means unused slot) */
  dev_t ps_dev;			/* major/minor of controlling tty */
  uid_t ps_ruid;		/* real uid */
  uid_t ps_euid;		/* effective uid */
  pid_t ps_pid;			/* process id */
  pid_t ps_ppid;		/* parent process id */
  int ps_pgrp;			/* process group id */
  char ps_state;		/* process state */
  char ps_pstate;		/* sleep state */
  char ps_fstate;		/* VFS block state */
  int ps_ftask;			/* VFS suspend task (endpoint) */
  vir_bytes ps_memory;		/* memory usage */
  int ps_recv;			/* process number to receive from (endpoint) */
  time_t ps_utime;		/* accumulated user time */
  time_t ps_stime;		/* accumulated system time */
  char ps_name[PROC_NAME_LEN+1];/* process name */
  char *ps_args;		/* concatenated argument string */
};

int main(int argc, char *argv []);
void plist(void);
int addrread(int fd, phys_clicks base, vir_bytes addr, char *buf, int
	nbytes );
void usage(const char *pname );
void err(const char *s );
int gettynames(void);


/*
 * Tname returns mnemonic string for dev_nr. This is "?" for maj/min pairs that
 * are not found.  It uses the ttyinfo array (prepared by gettynames).
 * Tname assumes that the first three letters of the tty's name can be omitted
 * and returns the rest (except for the console, which yields "co").
 */
static char *tname(dev_t dev_nr)
{
  unsigned int i;

  if (major(dev_nr) == TTY_MAJOR && minor(dev_nr) == 0) return "co";

  for (i = 0; i < n_ttyinfo && ttyinfo[i].tty_name[0] != '\0'; i++)
	if (ttyinfo[i].tty_dev == dev_nr)
		return ttyinfo[i].tty_name + 3;

  return "?";
}

/* Find a task by its endpoint. */
static struct pstat *findtask(endpoint_t endpt)
{
  struct pstat *ps;
  unsigned int slot;

  slot = SLOT_NR(endpt);

  if (slot >= nr_tasks + nr_procs)
	return NULL;

  ps = &ptable[slot];

  if (ps != NULL && ps->ps_endpt == (int) endpt)
	return ps;

  return NULL;
}

/* Return canonical task name of the given endpoint. */
static char *taskname(endpoint_t endpt)
{
  struct pstat *ps;

  ps = findtask(endpt);

  return ps ? ps->ps_name : "???";
}

/* Prrecv prints the RECV field for process with pstat buffer pointer ps.
 * This is either "ANY", "taskname", or "(blockreason) taskname".
 */
static char *prrecv(struct pstat *ps)
{
  char *blkstr, *task;		/* reason for blocking and task */
  static char recvstr[20];

  if (ps->ps_recv == ANY) return "ANY";

  task = taskname(ps->ps_recv);
  if (ps->ps_state != STATE_SLEEP) return task;

  blkstr = "?";
  if (ps->ps_recv == PM_PROC_NR) {
	switch (ps->ps_pstate) {
	case PSTATE_PAUSED: blkstr = "pause"; break;
	case PSTATE_WAITING: blkstr = "wait"; break;
	case PSTATE_SIGSUSP: blkstr = "sigsusp"; break;
	}
  } else if (ps->ps_recv == VFS_PROC_NR) {
	switch (ps->ps_fstate) {
	case FSTATE_PIPE: blkstr = "pipe"; break;
	case FSTATE_LOCK: blkstr = "flock"; break;
	case FSTATE_POPEN: blkstr = "popen"; break;
	case FSTATE_SELECT: blkstr = "select"; break;
	case FSTATE_DOPEN: blkstr = "dopen"; break;
	case FSTATE_TASK: blkstr = taskname(ps->ps_ftask); break;
	default: blkstr = "??"; break;
	}
  }
  (void) sprintf(recvstr, "(%s) %s", blkstr, task);
  return recvstr;
}

static void getkinfo(void)
{
	FILE *fp;

	if ((fp = fopen("kinfo", "r")) == NULL)
		err("Unable to open " _PATH_PROC "kinfo");

	if (fscanf(fp, "%u %u", &nr_procs, &nr_tasks) != 2)
		err("Unable to read from " _PATH_PROC "kinfo");

	fclose(fp);
}

/* Main interprets arguments, gathers information, and prints a process list.
 */
int main(argc, argv)
int argc;
char *argv[];
{
  int i;
  unsigned int n;
  struct pstat *ps;
  int uid = getuid();		/* real uid of caller */
  char *opt;
  int opt_all = FALSE;		/* -a */
  int opt_long = FALSE;		/* -l */
  int opt_notty = FALSE;	/* -x */
  int opt_endpoint = FALSE;	/* -E */
  char pid[2 + sizeof(pid_t) * 3];
  unsigned long ustime;
  char cpu[sizeof(clock_t) * 3 + 1 + 2];

  /* Parse arguments; a '-' need not be present (V7/BSD compatability) */
  for (i = 1; i < argc; i++) {
	opt = argv[i];
	if (opt[0] == '-') opt++;
	while (*opt != 0) switch (*opt++) {
		case 'a':	opt_all = TRUE;			break;
		case 'E':	opt_endpoint = TRUE;		break;
		case 'e':	opt_all = opt_notty = TRUE;	break;
		case 'f':
		case 'l':	opt_long = TRUE;		break;
		case 'x':	opt_notty = TRUE;		break;
		default:	usage(argv[0]);
	}
  }

  if (gettynames() == -1) err("Can't get tty names");

  if (chdir(_PATH_PROC) != 0) err("Can't chdir to /proc");

  /* Get information from the proc file system */
  system_hz = (u32_t) sysconf(_SC_CLK_TCK);

  getkinfo();

  plist();

  /* Now loop through process table and handle each entry */
  printf("%s", opt_long ? L_HEADER : S_HEADER);
  for (n = 0; n < nr_procs + nr_tasks; n++) {
	ps = &ptable[n];
	if (ps->ps_endpt == NONE)
		continue;

	if ((opt_all || ps->ps_euid == uid || ps->ps_ruid == uid) &&
	    (opt_notty || major(ps->ps_dev) == TTY_MAJOR)) {
		if (ps->ps_task) {
			sprintf(pid, "(%d)", ps->ps_pid);
		} else {
			sprintf(pid, "%d",
				opt_endpoint ? ps->ps_endpt : ps->ps_pid);
		}

		ustime = (ps->ps_utime + ps->ps_stime) / system_hz;
		if (ustime < 60 * 60) {
			sprintf(cpu, "%2lu:%02lu", ustime / 60, ustime % 60);
		} else
		if (ustime < 100L * 60 * 60) {
			ustime /= 60;
			sprintf(cpu, "%2luh%02lu", ustime / 60, ustime % 60);
		} else {
			sprintf(cpu, "%4luh", ustime / 3600);
		}

		if (opt_long) printf(L_FORMAT,
			       ps->ps_state,
			       ps->ps_euid, pid, ps->ps_ppid, 
			       ps->ps_pgrp,
			       off_to_k(ps->ps_memory),
			       (ps->ps_recv != NONE ? prrecv(ps) : ""),
			       tname((dev_t) ps->ps_dev),
			       cpu,
			       ps->ps_args != NULL ? ps->ps_args : ps->ps_name
			       );
		else
			printf(S_FORMAT,
			       pid, tname((dev_t) ps->ps_dev),
			       cpu,
			       ps->ps_args != NULL ? ps->ps_args : ps->ps_name
			       );
	}
  }
  return(0);
}

/* Get_args obtains the command line of a process. */
char *get_args(struct pstat *ps)
{
  char path[PATH_MAX], buf[4096];
  ssize_t i, n;
  int fd;

  /* Get a reasonable subset of the contents of the 'cmdline' file from procfs.
   * It contains all arguments, separated and terminated by null characters.
   */
  sprintf(path, "%d/cmdline", ps->ps_pid);

  fd = open(path, O_RDONLY);
  if (fd < 0) return NULL;

  n = read(fd, buf, sizeof(buf));
  if (n <= 0) {
	close(fd);

	return NULL;
  }

  close(fd);

  /* Replace all argument separating null characters with spaces. */
  for (i = 0; i < n-1; i++)
	if (buf[i] == '\0')
		buf[i] = ' ';

  /* The last character should already be null, except if it got cut off. */
  buf[n-1] = '\0';

  return strdup(buf);
}

/* Pstat obtains the actual information for the given process, and stores it
 * in the pstat structure. The outside world may change while we are doing
 * this, so nothing is reported in case any of the calls fail.
 */
int pstat(struct pstat *ps, pid_t pid)
{
  FILE *fp;
  int version, ruid, euid, dev;
  char type, path[PATH_MAX], name[256];

  ps->ps_pid = pid;
  ps->ps_next = NULL;

  sprintf(path, "%d/psinfo", pid);

  if ((fp = fopen(path, "r")) == NULL)
	return -1;

  if (fscanf(fp, "%d", &version) != 1) {
	fclose(fp);
	return -1;
  }

  /* The psinfo file's version must match what we expect. */
  if (version != PSINFO_VERSION) {
	fputs("procfs version mismatch!\n", stderr);
	exit(1);
  }

  if (fscanf(fp, " %c %d %255s %c %d %*d %u %u %*u %*u",
	&type, &ps->ps_endpt, name, &ps->ps_state,
	&ps->ps_recv, &ps->ps_utime, &ps->ps_stime) != 7) {

	fclose(fp);
	return -1;
  }

  strncpy(ps->ps_name, name, sizeof(ps->ps_name)-1);
  ps->ps_name[sizeof(ps->ps_name)-1] = 0;

  ps->ps_task = type == TYPE_TASK;

  if (!ps->ps_task) {
	if (fscanf(fp, " %lu %*u %*u %c %d %u %u %u %*d %c %d %u",
		&ps->ps_memory, &ps->ps_pstate, &ps->ps_ppid,
		&ruid, &euid, &ps->ps_pgrp, &ps->ps_fstate,
		&ps->ps_ftask, &dev) != 9) {

		fclose(fp);
		return -1;
	}

	ps->ps_ruid = ruid;
	ps->ps_euid = euid;
	ps->ps_dev = dev;
  } else {
	ps->ps_memory = 0L;
	ps->ps_pstate = PSTATE_NONE;
	ps->ps_ppid = 0;
	ps->ps_ruid = 0;
	ps->ps_euid = 0;
	ps->ps_pgrp = 0;
	ps->ps_fstate = FSTATE_NONE;
	ps->ps_ftask = NONE;
	ps->ps_dev = NO_DEV;
  }

  fclose(fp);

  if (ps->ps_state == STATE_ZOMBIE)
	ps->ps_args = "<defunct>";
  else if (!ps->ps_task)
	ps->ps_args = get_args(ps);
  else
	ps->ps_args = NULL;

  return 0;
}

/* Plist creates a list of processes with status information. */
void plist(void)
{
  DIR *p_dir;
  struct dirent *p_ent;
  struct pstat pbuf;
  pid_t pid;
  char *end;
  unsigned int slot;

  /* Allocate a table for process information. Initialize all slots' endpoints
   * to NONE, indicating those slots are not used.
   */
  if ((ptable = malloc((nr_tasks + nr_procs) * sizeof(struct pstat))) == NULL)
	err("Out of memory!");

  for (slot = 0; slot < nr_tasks + nr_procs; slot++)
	ptable[slot].ps_endpt = NONE;

  /* Fill in the table slots for all existing processes, by retrieving all PID
   * entries from the /proc directory.
   */
  p_dir = opendir(".");

  if (p_dir == NULL) err("Can't open " _PATH_PROC);

  p_ent = readdir(p_dir);
  while (p_ent != NULL) {
	pid = strtol(p_ent->d_name, &end, 10);

	if (!end[0] && pid != 0 && !pstat(&pbuf, pid)) {
		slot = SLOT_NR(pbuf.ps_endpt);

		if (slot < nr_tasks + nr_procs)
			memcpy(&ptable[slot], &pbuf, sizeof(pbuf));
	}

	p_ent = readdir(p_dir);
  }

  closedir(p_dir);
}

void usage(const char *pname)
{
  fprintf(stderr, "Usage: %s [-][aeflx]\n", pname);
  exit(1);
}

void err(const char *s)
{
  extern int errno;

  if (errno == 0)
	fprintf(stderr, "ps: %s\n", s);
  else
	fprintf(stderr, "ps: %s: %s\n", s, strerror(errno));

  exit(2);
}

/* Fill ttyinfo by fstatting character specials in /dev. */
int gettynames(void)
{
  static char dev_path[] = "/dev/";
  struct stat statbuf;
  static char path[sizeof(dev_path) + NAME_MAX];
  unsigned int index;
  struct ttyent *ttyp;

  index = 0;
  while ((ttyp = getttyent()) != NULL) {
	strcpy(path, dev_path);
	strcat(path, ttyp->ty_name);
	if (stat(path, &statbuf) == -1 || !S_ISCHR(statbuf.st_mode))
		continue;
	if (index >= n_ttyinfo) {
		n_ttyinfo= (index+16) * 2;
		ttyinfo = realloc(ttyinfo, n_ttyinfo * sizeof(ttyinfo[0]));
		if (ttyinfo == NULL) err("Out of memory");
	}
	ttyinfo[index].tty_dev = statbuf.st_rdev;
	strcpy(ttyinfo[index].tty_name, ttyp->ty_name);
	index++;
  }
  endttyent();
  while (index < n_ttyinfo) ttyinfo[index++].tty_dev= 0;

  return 0;
}
