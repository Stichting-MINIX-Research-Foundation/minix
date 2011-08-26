#ifndef __VFS_GLO_H__
#define __VFS_GLO_H__

/* EXTERN should be extern except for the table file */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

/* File System global variables */
EXTERN struct fproc *fp;	/* pointer to caller's fproc struct */
EXTERN int susp_count;		/* number of procs suspended on pipe */
EXTERN int nr_locks;		/* number of locks currently in place */
EXTERN int reviving;		/* number of pipe processes to be revived */
EXTERN int pending;
EXTERN int sending;

EXTERN dev_t ROOT_DEV;		/* device number of the root device */
EXTERN int ROOT_FS_E;           /* kernel endpoint of the root FS proc */
EXTERN u32_t system_hz;		/* system clock frequency. */

/* The parameters of the call are kept here. */
EXTERN message m_in;		/* the input message itself */
EXTERN message m_out;		/* the output message used for reply */
# define who_p		((int) (fp - fproc))
# define isokslot(p)	(p >= 0 && \
			 p < (int)(sizeof(fproc) / sizeof(struct fproc)))
#if 0
# define who_e		(isokslot(who_p) ? fp->fp_endpoint : m_in.m_source)
#else
# define who_e		(isokslot(who_p) && fp->fp_endpoint != NONE ? \
					fp->fp_endpoint : m_in.m_source)
#endif
# define call_nr	(m_in.m_type)
# define super_user	(fp->fp_effuid == SU_UID ? 1 : 0)
EXTERN struct worker_thread *self;
EXTERN endpoint_t receive_from;/* endpoint with pending reply */
EXTERN int force_sync;		/* toggle forced synchronous communication */
EXTERN int verbose;
EXTERN int deadlock_resolving;
EXTERN mutex_t exec_lock;
EXTERN mutex_t bsf_lock;/* Global lock for access to block special files */
EXTERN struct worker_thread workers[NR_WTHREADS];
EXTERN struct worker_thread sys_worker;
EXTERN struct worker_thread dl_worker;
EXTERN char mount_label[LABEL_MAX];	/* label of file system to mount */

/* The following variables are used for returning results to the caller. */
EXTERN int err_code;		/* temporary storage for error number */

/* Data initialized elsewhere. */
extern _PROTOTYPE (int (*call_vec[]), (void) ); /* sys call table */
extern _PROTOTYPE (int (*pfs_call_vec[]), (void) ); /* pfs callback table */
extern char dot1[2];   /* dot1 (&dot1[0]) and dot2 (&dot2[0]) have a special */
extern char dot2[3];   /* meaning to search_dir: no access permission check. */

#endif
