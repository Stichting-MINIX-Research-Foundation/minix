#ifndef __VFS_FPROC_H__
#define __VFS_FPROC_H__

#include "threads.h"

#include <sys/select.h>
#include <minix/safecopies.h>

/* This is the per-process information.  A slot is reserved for each potential
 * process. Thus NR_PROCS must be the same as in the kernel. It is not
 * possible or even necessary to tell when a slot is free here.
 */
#define LOCK_DEBUG 0
EXTERN struct fproc {
  unsigned fp_flags;

  pid_t fp_pid;			/* process id */
  endpoint_t fp_endpoint;	/* kernel endpoint number of this process */

  struct vnode *fp_wd;		/* working directory; NULL during reboot */
  struct vnode *fp_rd;		/* root directory; NULL during reboot */

  struct filp *fp_filp[OPEN_MAX];/* the file descriptor table */
  fd_set fp_filp_inuse;		/* which fd's are in use? */
  fd_set fp_cloexec_set;	/* bit map for POSIX Table 6-2 FD_CLOEXEC */

  dev_t fp_tty;			/* major/minor of controlling tty */

  int fp_blocked_on;		/* what is it blocked on */
  int fp_block_callnr;		/* blocked call if rd/wr can't finish */
  int  fp_cum_io_partial;	/* partial byte count if rd/wr can't finish */
  endpoint_t fp_task;		/* which task is proc suspended on */
  endpoint_t fp_ioproc;		/* proc no. in suspended-on i/o message */

  cp_grant_id_t fp_grant;	/* revoke this grant on unsuspend if > -1 */

  uid_t fp_realuid;		/* real user id */
  uid_t fp_effuid;		/* effective user id */
  gid_t fp_realgid;		/* real group id */
  gid_t fp_effgid;		/* effective group id */
  int fp_ngroups;		/* number of supplemental groups */
  gid_t fp_sgroups[NGROUPS_MAX];/* supplemental groups */
  mode_t fp_umask;		/* mask set by umask system call */

  mutex_t fp_lock;		/* mutex to lock fproc object */
  struct job fp_job;		/* pending job */
  thread_t fp_wtid;		/* Thread ID of worker */
  char fp_name[PROC_NAME_LEN];	/* Last exec() */
#if LOCK_DEBUG
  int fp_vp_rdlocks;		/* number of read-only locks on vnodes */
  int fp_vmnt_rdlocks;		/* number of read-only locks on vmnts */
#endif
} fproc[NR_PROCS];

/* fp_flags */
#define FP_NOFLAGS	00
#define FP_SUSP_REOPEN	01	/* Process is suspended until the reopens are
				 * completed (after the restart of a driver).
				 */
#define FP_REVIVED	 0002	/* Indicates process is being revived */
#define FP_SESLDR	 0004	/* Set if process is session leader */
#define FP_PENDING	 0010	/* Set if process has pending work */
#define FP_EXITING	 0020	/* Set if process is exiting */
#define FP_PM_PENDING	 0040	/* Set if process has pending PM request */
#define FP_SRV_PROC	 0100	/* Set if process is a service */
#define FP_DROP_WORK	 0200	/* Set if process won't accept new work */

/* Field values. */
#define NOT_REVIVING       0xC0FFEEE	/* process is not being revived */
#define REVIVING           0xDEEAD	/* process is being revived from suspension */
#define PID_FREE	   0	/* process slot free */

#endif /* __VFS_FPROC_H__ */
