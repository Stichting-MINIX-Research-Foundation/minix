/* This table has one slot per process.  It contains all the process management
 * information for each process.  Among other things, it defines the text, data
 * and stack segments, uids and gids, and various flags.  The kernel and file
 * systems have tables that are also indexed by process, with the contents
 * of corresponding slots referring to the same process in all three.
 */
#include <limits.h>
#include <timers.h>
#include <signal.h>

#include <sys/cdefs.h>

/* Needs to be included here, for 'ps' etc */
#include "const.h"

EXTERN struct mproc {
  char mp_exitstatus;		/* storage for status when process exits */
  char mp_sigstatus;		/* storage for signal # for killed procs */
  pid_t mp_pid;			/* process id */
  endpoint_t mp_endpoint;	/* kernel endpoint id */
  pid_t mp_procgrp;		/* pid of process group (used for signals) */
  pid_t mp_wpid;		/* pid this process is waiting for */
  int mp_parent;		/* index of parent process */
  int mp_tracer;		/* index of tracer process, or NO_TRACER */

  /* Child user and system times. Accounting done on child exit. */
  clock_t mp_child_utime;	/* cumulative user time of children */
  clock_t mp_child_stime;	/* cumulative sys time of children */

  /* Real and effective uids and gids. */
  uid_t mp_realuid;		/* process' real uid */
  uid_t mp_effuid;		/* process' effective uid */
  gid_t mp_realgid;		/* process' real gid */
  gid_t mp_effgid;		/* process' effective gid */

  /* Supplemental groups. */
  int mp_ngroups;		/* number of supplemental groups */
  gid_t mp_sgroups[NGROUPS_MAX];/* process' supplemental groups */

  /* Signal handling information. */
  sigset_t mp_ignore;		/* 1 means ignore the signal, 0 means don't */
  sigset_t mp_catch;		/* 1 means catch the signal, 0 means don't */
  sigset_t mp_sigmask;		/* signals to be blocked */
  sigset_t mp_sigmask2;		/* saved copy of mp_sigmask */
  sigset_t mp_sigpending;	/* pending signals to be handled */
  sigset_t mp_ksigpending;	/* bitmap for pending signals from the kernel */
  sigset_t mp_sigtrace;		/* signals to hand to tracer first */
  struct sigaction mp_sigact[_NSIG]; /* as in sigaction(2) */
#ifdef __ACK__
  char mp_padding[60];		/* align structure with new libc */
#endif
  vir_bytes mp_sigreturn; 	/* address of C library __sigreturn function */
  struct timer mp_timer;	/* watchdog timer for alarm(2), setitimer(2) */
  clock_t mp_interval[NR_ITIMERS];	/* setitimer(2) repetition intervals */

  unsigned mp_flags;		/* flag bits */
  unsigned mp_trace_flags;	/* trace options */
  message mp_reply;		/* reply message to be sent to one */

  /* Process execution frame. Both fields are used by procfs. */
  vir_bytes mp_frame_addr;	/* ptr to proc's initial stack arguments */
  size_t mp_frame_len;		/* size of proc's initial stack arguments */

  /* Scheduling priority. */
  signed int mp_nice;		/* nice is PRIO_MIN..PRIO_MAX, standard 0. */

  /* User space scheduling */
  endpoint_t mp_scheduler;	/* scheduler endpoint id */

  char mp_name[PROC_NAME_LEN];	/* process name */

  int mp_magic;			/* sanity check, MP_MAGIC */
} mproc[NR_PROCS];

/* Flag values */
#define IN_USE		0x00001	/* set when 'mproc' slot in use */
#define WAITING		0x00002	/* set by WAIT system call */
#define ZOMBIE		0x00004	/* waiting for parent to issue WAIT call */
#define PAUSED		0x00008	/* set by PAUSE system call */
#define ALARM_ON	0x00010	/* set when SIGALRM timer started */
#define EXITING		0x00020	/* set by EXIT, process is now exiting */
#define TOLD_PARENT	0x00040	/* parent wait() completed, ZOMBIE off */
#define STOPPED		0x00080	/* set if process stopped for tracing */
#define SIGSUSPENDED	0x00100	/* set by SIGSUSPEND system call */
#define REPLY		0x00200	/* set if a reply message is pending */
#define VFS_CALL       	0x00400	/* set if waiting for VFS (normal calls) */
#define PM_SIG_PENDING	0x00800	/* process got a signal while waiting for VFS */
#define UNPAUSED	0x01000	/* process is not in a blocking call */
#define PRIV_PROC	0x02000	/* system process, special privileges */
#define PARTIAL_EXEC	0x04000	/* process got a new map but no content */
#define TRACE_EXIT	0x08000	/* tracer is forcing this process to exit */
#define TRACE_ZOMBIE	0x10000	/* waiting for tracer to issue WAIT call */
#define DELAY_CALL	0x20000	/* waiting for call before sending signal */
#define TAINTED		0x40000 /* process is 'tainted' */

#define MP_MAGIC	0xC0FFEE0
