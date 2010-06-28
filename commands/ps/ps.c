/* ps - print status			Author: Peter Valkenburg */

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
 *
 * VERY IMPORTANT NOTE:
 *	To compile ps, the kernel/, fs/ and pm/ source directories must be in
 *	../../ relative to the directory where ps is compiled (normally the
 *	tools source directory).
 *
 *	If you want your ps to be useable by anyone, you can arrange the
 *	following access permissions (note the protected memory files and set
 *	*group* id on ps):
 *	-rwxr-sr-x  1 bin   kmem       11916 Jul  4 15:31 /bin/ps
 *	crw-r-----  1 bin   kmem      1,   1 Jan  1  1970 /dev/mem
 *	crw-r-----  1 bin   kmem      1,   2 Jan  1  1970 /dev/kmem
 */

/* Some technical comments on this implementation:
 *
 * Most fields are similar to V7 ps(1), except for CPU, NICE, PRI which are
 * absent, RECV which replaces WCHAN, and PGRP that is an extra.
 * The info is obtained from the following fields of proc, mproc and fproc:
 * F	- kernel status field, p_rts_flags
 * S	- kernel status field, p_rts_flags; mm status field, mp_flags (R if p_rts_flags
 *	  is 0; Z if mp_flags == ZOMBIE; T if mp_flags == STOPPED; else W).
 * UID	- mm eff uid field, mp_effuid
 * PID	- mm pid field, mp_pid
 * PPID	- mm parent process index field, mp_parent (used as index in proc).
 * PGRP - mm process group field, mp_procgrp
 * SZ	- kernel text size + physical stack address - physical data address
 *			   + stack size
 *	  p_memmap[T].mem_len + p_memmap[S].mem_phys - p_memmap[D].mem_phys
 *			   + p_memmap[S].mem_len
 * RECV	- kernel process index field for message receiving, p_getfrom
 *	  If sleeping, mm's mp_flags, or fs's fp_task are used for more info.
 * TTY	- fs controlling tty device field, fp_tty.
 * TIME	- kernel user + system times fields, user_time + sys_time
 * CMD	- system process index (converted to mnemonic name by using the p_name
 *	  field), or user process argument list (obtained by reading the stack
 *	  frame; the resulting address is used to get the argument vector from
 *	  user space and converted into a concatenated argument list).
 */

#include <minix/config.h>
#include <minix/com.h>
#include <minix/sysinfo.h>
#include <minix/endpoint.h>
#include <limits.h>
#include <timers.h>
#include <sys/types.h>

#include <minix/const.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <minix/com.h>
#include <fcntl.h>
#include <a.out.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdio.h>
#include <ttyent.h>

#include <machine/archtypes.h>
#include "kernel/const.h"
#include "kernel/type.h"
#include "kernel/proc.h"

#include "pm/mproc.h"
#include "pm/const.h"
#include "vfs/fproc.h"
#include "vfs/const.h"
#include "mfs/const.h"


/*----- ps's local stuff below this line ------*/


#define mindev(dev)	(((dev)>>MINOR) & 0377)	/* yield minor device */
#define majdev(dev)	(((dev)>>MAJOR) & 0377)	/* yield major device */

#define	TTY_MAJ		4	/* major device of console */

/* Structure for tty name info. */
typedef struct {
  char tty_name[NAME_MAX + 1];	/* file name in /dev */
  dev_t tty_dev;		/* major/minor pair */
} ttyinfo_t;

ttyinfo_t *ttyinfo;		/* ttyinfo holds actual tty info */
size_t n_ttyinfo;		/* Number of tty info slots */

/* Macro to convert memory offsets to rounded kilo-units */
#define	off_to_k(off)	((unsigned) (((off) + 512) / 1024))


/* Number of tasks and processes and addresses of the main process tables. */
int nr_tasks, nr_procs;		
extern int errno;

/* Process tables of the kernel, PM, and VFS. */
struct proc *ps_proc;
struct mproc *ps_mproc;
struct fproc *ps_fproc;

/* Where is INIT? */
int init_proc_nr;
#define low_user init_proc_nr

#define	KMEM_PATH	"/dev/kmem"	/* opened for kernel proc table */
#define	MEM_PATH	"/dev/mem"	/* opened for pm/fs + user processes */

int kmemfd, memfd;		/* file descriptors of [k]mem */

/* Short and long listing formats:
 *
 *   PID TTY  TIME CMD
 * ppppp tttmmm:ss cccccccccc...
 *
 *   F S UID   PID  PPID  PGRP   SZ       RECV TTY  TIME CMD
 * fff s uuu ppppp ppppp ppppp ssss rrrrrrrrrr tttmmm:ss cccccccc...
 */
#define S_HEADER "  PID TTY  TIME CMD\n"
#define S_FORMAT "%5s %3s %s %s\n"
#define L_HEADER "  F S UID   PID  PPID  PGRP     SZ         RECV TTY  TIME CMD\n"
#define L_FORMAT "%3o %c %3d %5s %5d %5d %6d %12s %3s %s %s\n"


struct pstat {			/* structure filled by pstat() */
  dev_t ps_dev;			/* major/minor of controlling tty */
  uid_t ps_ruid;		/* real uid */
  uid_t ps_euid;		/* effective uid */
  pid_t ps_pid;			/* process id */
  pid_t ps_ppid;		/* parent process id */
  int ps_pgrp;			/* process group id */
  int ps_flags;			/* kernel flags */
  int ps_mflags;		/* mm flags */
  int ps_ftask;			/* fs suspend task */
  int ps_blocked_on;		/* what is the process blocked on */
  char ps_state;		/* process state */
  vir_bytes ps_tsize;		/* text size (in bytes) */
  vir_bytes ps_dsize;		/* data size (in bytes) */
  vir_bytes ps_ssize;		/* stack size (in bytes) */
  phys_bytes ps_vtext;		/* virtual text offset */
  phys_bytes ps_vdata;		/* virtual data offset */
  phys_bytes ps_vstack;		/* virtual stack offset */
  phys_bytes ps_text;		/* physical text offset */
  phys_bytes ps_data;		/* physical data offset */
  phys_bytes ps_stack;		/* physical stack offset */
  int ps_recv;			/* process number to receive from */
  time_t ps_utime;		/* accumulated user time */
  time_t ps_stime;		/* accumulated system time */
  char *ps_args;		/* concatenated argument string */
  vir_bytes ps_procargs;	/* initial stack frame from PM */
};

/* Ps_state field values in pstat struct above */
#define	Z_STATE		'Z'	/* Zombie */
#define	W_STATE		'W'	/* Waiting */
#define	S_STATE		'S'	/* Sleeping */
#define	R_STATE		'R'	/* Runnable */
#define	T_STATE		'T'	/* stopped (Trace) */

_PROTOTYPE(void disaster, (int sig ));
_PROTOTYPE(int main, (int argc, char *argv []));
_PROTOTYPE(char *get_args, (struct pstat *bufp ));
_PROTOTYPE(int pstat, (int p_nr, struct pstat *bufp, int Eflag ));
_PROTOTYPE(int addrread, (int fd, phys_clicks base, vir_bytes addr, 
						    char *buf, int nbytes ));
_PROTOTYPE(void usage, (const char *pname ));
_PROTOTYPE(void err, (const char *s ));
_PROTOTYPE(int gettynames, (void));


/*
 * Tname returns mnemonic string for dev_nr. This is "?" for maj/min pairs that
 * are not found.  It uses the ttyinfo array (prepared by gettynames).
 * Tname assumes that the first three letters of the tty's name can be omitted
 * and returns the rest (except for the console, which yields "co").
 */
PRIVATE char *tname(dev_t dev_nr)
{
  int i;

  if (majdev(dev_nr) == TTY_MAJ && mindev(dev_nr) == 0) return "co";

  for (i = 0; i < n_ttyinfo && ttyinfo[i].tty_name[0] != '\0'; i++)
	if (ttyinfo[i].tty_dev == dev_nr)
		return ttyinfo[i].tty_name + 3;

  return "?";
}

/* Return canonical task name of task p_nr; overwritten on each call (yucch) */
PRIVATE char *taskname(int p_nr)
{
  int n;
  n = _ENDPOINT_P(p_nr) + nr_tasks;
  if(n < 0 || n >= nr_tasks + nr_procs) {
	return "OUTOFRANGE";
  }
  return ps_proc[n].p_name;
}

/* Prrecv prints the RECV field for process with pstat buffer pointer bufp.
 * This is either "ANY", "taskname", or "(blockreason) taskname".
 */
PRIVATE char *prrecv(struct pstat *bufp)
{
  char *blkstr, *task;		/* reason for blocking and task */
  static char recvstr[20];

  if (bufp->ps_recv == ANY) return "ANY";

  task = taskname(bufp->ps_recv);
  if (bufp->ps_state != S_STATE) return task;

  blkstr = "?";
  if (bufp->ps_recv == PM_PROC_NR) {
	if (bufp->ps_mflags & PAUSED)
		blkstr = "pause";
	else if (bufp->ps_mflags & WAITING)
		blkstr = "wait";
	else if (bufp->ps_mflags & SIGSUSPENDED)
		blkstr = "sigsusp";
  } else if (bufp->ps_recv == VFS_PROC_NR) {
	  switch(bufp->ps_blocked_on) {
		  case FP_BLOCKED_ON_PIPE:
			  blkstr = "pipe";
			  break;
		  case FP_BLOCKED_ON_POPEN:
			  blkstr = "popen";
			  break;
		  case FP_BLOCKED_ON_DOPEN:
			  blkstr = "dopen";
			  break;
		  case FP_BLOCKED_ON_LOCK:
			  blkstr = "flock";
			  break;
		  case FP_BLOCKED_ON_SELECT:
			  blkstr = "select";
			  break;
		  case FP_BLOCKED_ON_OTHER:
			  blkstr = taskname(bufp->ps_ftask);
			  break;
		  case FP_BLOCKED_ON_NONE:
			  blkstr = "??";
			  break;
	  }
  }
  (void) sprintf(recvstr, "(%s) %s", blkstr, task);
  return recvstr;
}

/* If disaster is called some of the system parameters imported into ps are
 * probably wrong.  This tends to result in memory faults.
 */
void disaster(sig)
int sig;
{
  fprintf(stderr, "Ooops, got signal %d\n", sig);
  fprintf(stderr, "Was ps recompiled since the last kernel change?\n");
  exit(3);
}

/* Main interprets arguments, gets system addresses, opens [k]mem, reads in
 * process tables from kernel/pm/fs and calls pstat() for relevant entries.
 */
int main(argc, argv)
int argc;
char *argv[];
{
  int i;
  struct pstat buf;
  int db_fd;
  int uid = getuid();		/* real uid of caller */
  char *opt;
  int opt_all = FALSE;		/* -a */
  int opt_long = FALSE;		/* -l */
  int opt_notty = FALSE;	/* -x */
  int opt_endpoint = FALSE;	/* -E */
  char *ke_path;		/* paths of kernel, */
  char *mm_path;		/* mm, */
  char *fs_path;		/* and fs used in ps -U */
  char pid[2 + sizeof(pid_t) * 3];
  unsigned long ustime;
  char cpu[sizeof(clock_t) * 3 + 1 + 2];
  struct kinfo kinfo;
  int s;
  u32_t system_hz;

  if(getsysinfo_up(PM_PROC_NR, SIU_SYSTEMHZ, sizeof(system_hz), &system_hz) < 0) {
	exit(1);
  }

  (void) signal(SIGSEGV, disaster);	/* catch a common crash */

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

  /* Open memory devices and get PS info from the kernel */
  if ((kmemfd = open(KMEM_PATH, O_RDONLY)) == -1) err(KMEM_PATH);
  if ((memfd = open(MEM_PATH, O_RDONLY)) == -1) err(MEM_PATH);
  if (gettynames() == -1) err("Can't get tty names");

  getsysinfo(PM_PROC_NR, SI_KINFO, &kinfo);

  nr_tasks = kinfo.nr_tasks;	
  nr_procs = kinfo.nr_procs;

  /* Allocate memory for process tables */
  ps_proc = (struct proc *) malloc((nr_tasks + nr_procs) * sizeof(ps_proc[0]));
  ps_mproc = (struct mproc *) malloc(nr_procs * sizeof(ps_mproc[0]));
  ps_fproc = (struct fproc *) malloc(nr_procs * sizeof(ps_fproc[0]));
  if (ps_proc == NULL || ps_mproc == NULL || ps_fproc == NULL)
	err("Out of memory");

	if(minix_getkproctab(ps_proc, nr_tasks + nr_procs, 1) < 0) {
		fprintf(stderr, "minix_getkproctab failed.\n");
		exit(1);
	}

	if(getsysinfo(PM_PROC_NR, SI_PROC_TAB, ps_mproc) < 0) {
		fprintf(stderr, "getsysinfo() for PM SI_PROC_TAB failed.\n");
		exit(1);
	}

	if(getsysinfo(VFS_PROC_NR, SI_PROC_TAB, ps_fproc) < 0) {
		fprintf(stderr, "getsysinfo() for VFS SI_PROC_TAB failed.\n");
		exit(1);
	}

  /* We need to know where INIT hangs out. */
  for (i = VFS_PROC_NR; i < nr_procs; i++) {
	if (strcmp(ps_proc[nr_tasks + i].p_name, "init") == 0) break;
  }
  init_proc_nr = i;

  /* Now loop through process table and handle each entry */
  printf("%s", opt_long ? L_HEADER : S_HEADER);
  for (i = -nr_tasks; i < nr_procs; i++) {
	if (pstat(i, &buf, opt_endpoint) != -1 &&
	    (opt_all || buf.ps_euid == uid || buf.ps_ruid == uid) &&
	    (opt_notty || majdev(buf.ps_dev) == TTY_MAJ)) {
		if (buf.ps_pid == 0 && i != PM_PROC_NR) {
			sprintf(pid, "(%d)", i);
		} else {
			sprintf(pid, "%d", buf.ps_pid);
		}

		ustime = (buf.ps_utime + buf.ps_stime) / system_hz;
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
			       buf.ps_flags, buf.ps_state,
			       buf.ps_euid, pid, buf.ps_ppid, 
			       buf.ps_pgrp,
#if 0
			       off_to_k((buf.ps_tsize
					 + buf.ps_stack - buf.ps_data
					 + buf.ps_ssize)),
#else
				0,
#endif
			       (buf.ps_flags & RTS_RECEIVING ?
				prrecv(&buf) :
				""),
			       tname((dev_t) buf.ps_dev),
			       cpu,
			       i <= init_proc_nr || buf.ps_args == NULL
				       ? taskname(i) : buf.ps_args);
		else
			printf(S_FORMAT,
			       pid, tname((dev_t) buf.ps_dev),
			       cpu,
			       i <= init_proc_nr || buf.ps_args == NULL
				       ? taskname(i) : buf.ps_args);
	}
  }
  return(0);
}

char *get_args(bufp)
struct pstat *bufp;
{
  int nargv;
  int cnt;			/* # of bytes read from stack frame */
  int neos;			/* # of '\0's seen in argv string space */
  phys_bytes iframe;
  long l;
  char *cp, *args;
  static union stack {
	vir_bytes stk_i;
	char *stk_cp;
	char stk_c;
  } stk[ARG_MAX / sizeof(char *)];
  union stack *sp;

  /* Phys address of the original stack frame. */
  iframe = bufp->ps_procargs - bufp->ps_vstack + bufp->ps_stack;

  /* Calculate the number of bytes to read from user stack */
  l = (phys_bytes) bufp->ps_ssize - (iframe - bufp->ps_stack);
  if (l > ARG_MAX) l = ARG_MAX;
  cnt = l;

  /* Get cnt bytes from user initial stack to local stack buffer */
  if (lseek(memfd, (off_t) iframe, 0) < 0)
	return NULL; 

  if ( read(memfd, (char *)stk, cnt) != cnt ) 
	return NULL;

  sp = stk;
  nargv = (int) sp[0].stk_i;  /* number of argv arguments */

  /* See if argv[0] is with the bytes we read in */
  l = (long) sp[1].stk_cp - (long) bufp->ps_procargs;

  if ( ( l < 0 ) || ( l > cnt ) )  
	return NULL;

  /* l is the offset of the argv[0] argument */
  /* change for concatenation the '\0' to space, for nargv elements */

  args = &((char *) stk)[(int)l]; 
  neos = 0;
  for (cp = args; cp < &((char *) stk)[cnt]; cp++)
	if (*cp == '\0')
		if (++neos >= nargv)
			break;
		else
			*cp = ' ';
  if (cp == args) return NULL;
  *cp = '\0';

  return args;

}

/* Pstat collects info on process number p_nr and returns it in buf.
 * It is assumed that tasks do not have entries in fproc/mproc.
 */
int pstat(int p_nr, struct pstat *bufp, int endpoints)
{
  int p_ki = p_nr + nr_tasks;	/* kernel proc index */

  if (p_nr < -nr_tasks || p_nr >= nr_procs) {
	fprintf(stderr, "pstat: %d out of range\n", p_nr);
	return -1;
  }

  if (isemptyp(&ps_proc[p_ki])
  				&& !(ps_mproc[p_nr].mp_flags & IN_USE)) {
	return -1;
  }

  bufp->ps_flags = ps_proc[p_ki].p_rts_flags;

  if (p_nr >= low_user) {
	bufp->ps_dev = ps_fproc[p_nr].fp_tty;
	bufp->ps_ftask = ps_fproc[p_nr].fp_task;
	bufp->ps_blocked_on = ps_fproc[p_nr].fp_blocked_on;
  } else {
	bufp->ps_dev = 0;
	bufp->ps_ftask = 0;
	bufp->ps_blocked_on = FP_BLOCKED_ON_NONE;
  }

  if (p_nr >= 0) {
	bufp->ps_ruid = ps_mproc[p_nr].mp_realuid;
	bufp->ps_euid = ps_mproc[p_nr].mp_effuid;
	if(endpoints) bufp->ps_pid = ps_proc[p_ki].p_endpoint;
	else bufp->ps_pid = ps_mproc[p_nr].mp_pid;
	bufp->ps_ppid = ps_mproc[ps_mproc[p_nr].mp_parent].mp_pid;
	/* Assume no parent when the parent and the child share the same pid.
	 * This is what PM currently assumes.
	 */
	if(bufp->ps_ppid == bufp->ps_pid) {
	    bufp->ps_ppid = NO_PID;
	}
	bufp->ps_pgrp = ps_mproc[p_nr].mp_procgrp;
	bufp->ps_mflags = ps_mproc[p_nr].mp_flags;
  } else {
	if(endpoints) bufp->ps_pid = ps_proc[p_ki].p_endpoint;
	else bufp->ps_pid = NO_PID;
	bufp->ps_ppid = NO_PID;
	bufp->ps_ruid = bufp->ps_euid = 0;
	bufp->ps_pgrp = 0;
	bufp->ps_mflags = 0;
  }

  /* State is interpretation of combined kernel/mm flags for non-tasks */
  if (p_nr >= low_user) {		/* non-tasks */
	if (ps_mproc[p_nr].mp_flags & ZOMBIE)
		bufp->ps_state = Z_STATE;	/* zombie */
	else if (ps_mproc[p_nr].mp_flags & STOPPED)
		bufp->ps_state = T_STATE;	/* stopped (traced) */
	else if (ps_proc[p_ki].p_rts_flags == 0)
		bufp->ps_state = R_STATE;	/* in run-queue */
	else if (ps_mproc[p_nr].mp_flags & (WAITING | PAUSED | SIGSUSPENDED) ||
			fp_is_blocked(&ps_fproc[p_nr]))
		bufp->ps_state = S_STATE;	/* sleeping */
	else
		bufp->ps_state = W_STATE;	/* a short wait */
  } else {			/* tasks are simple */
	if (ps_proc[p_ki].p_rts_flags == 0)
		bufp->ps_state = R_STATE;	/* in run-queue */
	else
		bufp->ps_state = W_STATE;	/* other i.e. waiting */
  }

  bufp->ps_tsize = (size_t) ps_proc[p_ki].p_memmap[T].mem_len << CLICK_SHIFT;
  bufp->ps_dsize = (size_t) ps_proc[p_ki].p_memmap[D].mem_len << CLICK_SHIFT;
  bufp->ps_ssize = (size_t) ps_proc[p_ki].p_memmap[S].mem_len << CLICK_SHIFT;
  bufp->ps_vtext = (off_t) ps_proc[p_ki].p_memmap[T].mem_vir << CLICK_SHIFT;
  bufp->ps_vdata = (off_t) ps_proc[p_ki].p_memmap[D].mem_vir << CLICK_SHIFT;
  bufp->ps_vstack = (off_t) ps_proc[p_ki].p_memmap[S].mem_vir << CLICK_SHIFT;
  bufp->ps_text = (off_t) ps_proc[p_ki].p_memmap[T].mem_phys << CLICK_SHIFT;
  bufp->ps_data = (off_t) ps_proc[p_ki].p_memmap[D].mem_phys << CLICK_SHIFT;
  bufp->ps_stack = (off_t) ps_proc[p_ki].p_memmap[S].mem_phys << CLICK_SHIFT;

  bufp->ps_recv = _ENDPOINT_P(ps_proc[p_ki].p_getfrom_e);

  bufp->ps_utime = ps_proc[p_ki].p_user_time;
  bufp->ps_stime = ps_proc[p_ki].p_sys_time;

  bufp->ps_procargs = ps_mproc[p_nr].mp_procargs;

  if (bufp->ps_state == Z_STATE)
	bufp->ps_args = "<defunct>";
  else if (p_nr > init_proc_nr)
	bufp->ps_args = get_args(bufp);

  return 0;
}

/* Addrread reads nbytes from offset addr to click base of fd into buf. */
int addrread(int fd, phys_clicks base, vir_bytes addr, char *buf, int nbytes)
{
  if (lseek(fd, ((off_t) base << CLICK_SHIFT) + addr, 0) < 0)
	return -1;

  return read(fd, buf, nbytes);
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
  int index;
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
