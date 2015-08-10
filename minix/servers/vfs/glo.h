#ifndef __VFS_GLO_H__
#define __VFS_GLO_H__

/* EXTERN should be extern except for the table file */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

#include <minix/param.h>

/* File System global variables */
EXTERN struct fproc *fp;	/* pointer to caller's fproc struct */
EXTERN int susp_count;		/* number of procs suspended on pipe */
EXTERN int nr_locks;		/* number of locks currently in place */
EXTERN int reviving;		/* number of pipe processes to be revived */
EXTERN int sending;
EXTERN int verbose;

EXTERN dev_t ROOT_DEV;		/* device number of the root device */
EXTERN int ROOT_FS_E;           /* kernel endpoint of the root FS proc */
EXTERN u32_t system_hz;		/* system clock frequency. */

/* The parameters of the call are kept here. */
EXTERN message m_in;		/* the input message itself */
# define who_p		((int) (fp - fproc))
# define fproc_addr(e)	(&fproc[_ENDPOINT_P(e)])
# define who_e		(self != NULL ? fp->fp_endpoint : m_in.m_source)
# define call_nr	(m_in.m_type)
# define job_m_in	(self->w_m_in)
# define job_m_out	(self->w_m_out)
# define job_call_nr	(job_m_in.m_type)
# define super_user	(fp->fp_effuid == SU_UID ? 1 : 0)
EXTERN struct worker_thread *self;
EXTERN int deadlock_resolving;
EXTERN mutex_t bsf_lock;/* Global lock for access to block special files */
EXTERN struct worker_thread workers[NR_WTHREADS];
EXTERN char mount_label[LABEL_MAX];	/* label of file system to mount */

/* The following variables are used for returning results to the caller. */
EXTERN int err_code;		/* temporary storage for error number */

/* Data initialized elsewhere. */
extern int (* const call_vec[])(void);

#endif
