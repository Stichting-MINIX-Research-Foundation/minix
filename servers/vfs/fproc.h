#include <sys/select.h>
#include <minix/safecopies.h>

/* This is the per-process information.  A slot is reserved for each potential
 * process. Thus NR_PROCS must be the same as in the kernel. It is not 
 * possible or even necessary to tell when a slot is free here.
 */
EXTERN struct fproc {
  unsigned fp_flags;

  mode_t fp_umask;		/* mask set by umask system call */
 
  struct vnode *fp_wd;		/* working directory */
  struct vnode *fp_rd;		/* root directory */
  
  struct filp *fp_filp[OPEN_MAX];/* the file descriptor table */

  fd_set fp_filp_inuse;		/* which fd's are in use? */
  uid_t fp_realuid;		/* real user id */
  uid_t fp_effuid;		/* effective user id */
  gid_t fp_realgid;		/* real group id */
  gid_t fp_effgid;		/* effective group id */
  dev_t fp_tty;			/* major/minor of controlling tty */
  int fp_fd;			/* place to save fd if rd/wr can't finish */
  char *fp_buffer;		/* place to save buffer if rd/wr can't finish*/
  int  fp_nbytes;		/* place to save bytes if rd/wr can't finish */
  int  fp_cum_io_partial;	/* partial byte count if rd/wr can't finish */
  char fp_suspended;		/* set to indicate process hanging */
  char fp_revived;		/* set to indicate process being revived */
  int fp_task;			/* which task is proc suspended on */
  
  endpoint_t fp_ioproc;		/* proc no. in suspended-on i/o message */
  cp_grant_id_t fp_grant;	/* revoke this grant on unsuspend if > -1 */
  
  char fp_sesldr;		/* true if proc is a session leader */
  char fp_execced;		/* true if proc has exec()ced after fork */
  pid_t fp_pid;			/* process id */
  
  fd_set fp_cloexec_set;	/* bit map for POSIX Table 6-2 FD_CLOEXEC */
  endpoint_t fp_endpoint;	/* kernel endpoint number of this process */
} fproc[NR_PROCS];

/* fp_flags */
#define NO_FLAGS	0
#define SUSP_REOPEN	1	/* Process is suspended until the reopens are
				 * completed (after the restart of a driver).
				 */

/* Field values. */
#define NOT_SUSPENDED      0	/* process is not suspended on pipe or task */
#define SUSPENDED          1	/* process is suspended on pipe or task */
#define NOT_REVIVING       0	/* process is not being revived */
#define REVIVING           1	/* process is being revived from suspension */
#define PID_FREE	   0	/* process slot free */

/* Check is process number is acceptable - includes system processes. */
#define isokprocnr(n)	((unsigned)((n)+NR_TASKS) < NR_PROCS + NR_TASKS)

