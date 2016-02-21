#ifndef __VFS_FPROC_H__
#define __VFS_FPROC_H__

#include "threads.h"

#include <sys/select.h>
#include <minix/safecopies.h>
#include <minix/sef.h>

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

  struct filp *fp_filp[OPEN_MAX];/* the file descriptor table (free if NULL) */
  fd_set fp_cloexec_set;	/* bit map for POSIX Table 6-2 FD_CLOEXEC */

  dev_t fp_tty;			/* major/minor of controlling tty */

  int fp_blocked_on;		/* what is it blocked on */
  union ixfer_fp_u {		/* state per blocking type */
	struct {			/* FP_BLOCKED_ON_PIPE */
		int callnr;		/* user call: VFS_READ or VFS_WRITE */
		int fd;			/* file descriptor for blocking call */
		vir_bytes buf;		/* user buffer address */
		size_t nbytes;		/* number of bytes left */
		size_t cum_io;		/* partial (write) result byte count */
	} u_pipe;
	struct {			/* FP_BLOCKED_ON_POPEN */
		int fd;			/* file descriptor for blocking call */
	} u_popen;
	struct {			/* FP_BLOCKED_ON_FLOCK */
		int fd;			/* file descriptor for blocking call */
		int cmd;		/* fcntl command, always F_SETLKW */
		vir_bytes arg;		/* user address of flock structure */
	} u_flock;
	/* nothing for FP_BLOCKED_ON_SELECT for now */
	struct {			/* FP_BLOCKED_ON_CDEV */
		dev_t dev;		/* device number for blocking call */
		endpoint_t endpt;	/* driver endpoint */
		cp_grant_id_t grant;	/* data grant */
	} u_cdev;
	struct {			/* FP_BLOCKED_ON_SDEV */
		dev_t dev;		/* socket number for blocking call */
		int callnr;		/* user call: a VFS_ socket call */
		cp_grant_id_t grant[3];	/* data grant(s) */
		union ixfer_u_aux {
			int fd;		/* listener file descr. (VFS_ACCEPT) */
			vir_bytes buf;	/* user buffer address (VFS_RECVMSG) */
		} aux;			/* call-specific auxiliary data */
	} u_sdev;
  } fp_u;

  uid_t fp_realuid;		/* real user id */
  uid_t fp_effuid;		/* effective user id */
  gid_t fp_realgid;		/* real group id */
  gid_t fp_effgid;		/* effective group id */
  int fp_ngroups;		/* number of supplemental groups */
  gid_t fp_sgroups[NGROUPS_MAX];/* supplemental groups */
  mode_t fp_umask;		/* mask set by umask system call */

  mutex_t fp_lock;		/* mutex to lock fproc object */
  struct worker_thread *fp_worker;/* active worker thread, or NULL */
  void (*fp_func)(void);		/* handler function for pending work */
  message fp_msg;		/* pending or active message from process */
  message fp_pm_msg;		/* pending/active postponed PM request */

  char fp_name[PROC_NAME_LEN];	/* Last exec() */
#if LOCK_DEBUG
  int fp_vp_rdlocks;		/* number of read-only locks on vnodes */
  int fp_vmnt_rdlocks;		/* number of read-only locks on vmnts */
#endif
} fproc[NR_PROCS];

/* Shortcuts for block state union substructures. */
#define fp_pipe		fp_u.u_pipe
#define fp_popen	fp_u.u_popen
#define fp_flock	fp_u.u_flock
#define fp_cdev		fp_u.u_cdev
#define fp_sdev		fp_u.u_sdev

/* fp_flags */
#define FP_NOFLAGS	 0000
#define FP_SRV_PROC	 0001	/* Set if process is a service */
#define FP_REVIVED	 0002	/* Indicates process is being revived */
#define FP_SESLDR	 0004	/* Set if process is session leader */
#define FP_PENDING	 0010	/* Set if process has pending work */
#define FP_EXITING	 0020	/* Set if process is exiting */
#define FP_PM_WORK	 0040	/* Set if process has a postponed PM request */

/* Field values. */
#define NOT_REVIVING       0xC0FFEEE	/* process is not being revived */
#define REVIVING           0xDEEAD	/* process is being revived from suspension */
#define PID_FREE	   0	/* process slot free */

/*
 * Upon request from the MIB service, this table is filled with a relatively
 * small subset of per-process fields, so that the MIB service can avoid
 * pulling in the entire fproc table.  Other fields may be added to this
 * structure as required by the MIB service.
 */
EXTERN struct fproc_light {
  dev_t fpl_tty;		/* copy of fproc.fp_tty */
  int fpl_blocked_on;		/* copy of fproc.fp_blocked_on */
  endpoint_t fpl_task;		/* copy of fproc.fp_task */
} fproc_light[NR_PROCS];

#endif /* __VFS_FPROC_H__ */
