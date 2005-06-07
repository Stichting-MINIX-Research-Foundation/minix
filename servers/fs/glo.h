/* EXTERN should be extern except for the table file */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

/* File System global variables */
EXTERN struct fproc *fp;	/* pointer to caller's fproc struct */
EXTERN int super_user;		/* 1 if caller is super_user, else 0 */
EXTERN int susp_count;		/* number of procs suspended on pipe */
EXTERN int nr_locks;		/* number of locks currently in place */
EXTERN int reviving;		/* number of pipe processes to be revived */
EXTERN off_t rdahedpos;		/* position to read ahead */
EXTERN struct inode *rdahed_inode;	/* pointer to inode to read ahead */
EXTERN Dev_t root_dev;		/* device number of the root device */
EXTERN time_t boottime;		/* time in seconds at system boot */

/* The parameters of the call are kept here. */
EXTERN message m_in;		/* the input message itself */
EXTERN message m_out;		/* the output message used for reply */
EXTERN int who;			/* caller's proc number */
EXTERN int call_nr;		/* system call number */
EXTERN char user_path[PATH_MAX];/* storage for user path name */

/* The following variables are used for returning results to the caller. */
EXTERN int err_code;		/* temporary storage for error number */
EXTERN int rdwt_err;		/* status of last disk i/o request */

/* Data initialized elsewhere. */
extern _PROTOTYPE (int (*call_vec[]), (void) ); /* sys call table */
extern char dot1[2];   /* dot1 (&dot1[0]) and dot2 (&dot2[0]) have a special */
extern char dot2[3];   /* meaning to search_dir: no access permission check. */
