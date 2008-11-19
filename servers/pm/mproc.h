/* This table has one slot per process.  It contains all the process management
 * information for each process.  Among other things, it defines the text, data
 * and stack segments, uids and gids, and various flags.  The kernel and file
 * systems have tables that are also indexed by process, with the contents
 * of corresponding slots referring to the same process in all three.
 */
#include <timers.h>
#include <signal.h>

EXTERN struct mproc {
  char mp_exitstatus;		/* storage for status when process exits */
  char mp_sigstatus;		/* storage for signal # for killed procs */
  pid_t mp_pid;			/* process id */
  endpoint_t mp_endpoint;	/* kernel endpoint id */
  pid_t mp_procgrp;		/* pid of process group (used for signals) */
  pid_t mp_wpid;		/* pid this process is waiting for */
  int mp_parent;		/* index of parent process */

  /* Child user and system times. Accounting done on child exit. */
  clock_t mp_child_utime;	/* cumulative user time of children */
  clock_t mp_child_stime;	/* cumulative sys time of children */

  /* Real and effective uids and gids. */
  uid_t mp_realuid;		/* process' real uid */
  uid_t mp_effuid;		/* process' effective uid */
  gid_t mp_realgid;		/* process' real gid */
  gid_t mp_effgid;		/* process' effective gid */

  /* Signal handling information. */
  sigset_t mp_ignore;		/* 1 means ignore the signal, 0 means don't */
  sigset_t mp_catch;		/* 1 means catch the signal, 0 means don't */
  sigset_t mp_sig2mess;		/* 1 means transform into notify message */
  sigset_t mp_sigmask;		/* signals to be blocked */
  sigset_t mp_sigmask2;		/* saved copy of mp_sigmask */
  sigset_t mp_sigpending;	/* pending signals to be handled */
  struct sigaction mp_sigact[_NSIG + 1]; /* as in sigaction(2) */
  vir_bytes mp_sigreturn; 	/* address of C library __sigreturn function */
  struct sigmsg mp_sigmsg;	/* Save the details of the signal until the
				 * PM_UNPAUSE request is delivered.
				 */
  struct timer mp_timer;	/* watchdog timer for alarm(2) */

  /* Backwards compatibility for signals. */
  sighandler_t mp_func;		/* all sigs vectored to a single user fcn */

  unsigned mp_flags;		/* flag bits */
  vir_bytes mp_procargs;        /* ptr to proc's initial stack arguments */
  struct mproc *mp_swapq;	/* queue of procs waiting to be swapped in */
  message mp_reply;		/* reply message to be sent to one */

  /* Communication with FS */
  int mp_fs_call;		/* Record the call for normal system calls */
  int mp_fs_call2;		/* Record the call for signals */
  char *mp_exec_path;		/* Path of executable */
  vir_bytes mp_exec_path_len;	/* Length of path (including nul) */
  char *mp_exec_frame;		/* Arguments */
  vir_bytes mp_exec_frame_len;	/* Length of arguments */

  /* Scheduling priority. */
  signed int mp_nice;		/* nice is PRIO_MIN..PRIO_MAX, standard 0. */

  char mp_name[PROC_NAME_LEN];	/* process name */
} mproc[NR_PROCS];

/* Flag values */
#define IN_USE          0x001	/* set when 'mproc' slot in use */
#define WAITING         0x002	/* set by WAIT system call */
#define ZOMBIE          0x004	/* set by EXIT, cleared by WAIT */
#define PAUSED          0x008	/* set by PAUSE system call */
#define ALARM_ON        0x010	/* set when SIGALRM timer started */
#define	TRACED		0x040	/* set if process is to be traced */
#define STOPPED		0x080	/* set if process stopped for tracing */
#define SIGSUSPENDED 	0x100	/* set by SIGSUSPEND system call */
#define REPLY	 	0x200	/* set if a reply message is pending */
#define PRIV_PROC      0x2000   /* system process, special privileges */
#define PM_SIG_PENDING 0x4000	/* process got a signal while waiting for FS */
#define PARTIAL_EXEC   0x8000	/* Process got a new map but no content */
#define TOLD_PARENT   0x10000	/* Parent wait() completed, ZOMBIE off */
#define HAS_DMA	      0x20000	/* Process directly or indirectly granted
				 * DMA buffers.
				 */

#define NIL_MPROC ((struct mproc *) 0)

