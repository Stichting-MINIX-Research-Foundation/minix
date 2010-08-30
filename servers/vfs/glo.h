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

EXTERN dev_t root_dev;		/* device number of the root device */
EXTERN int ROOT_FS_E;           /* kernel endpoint of the root FS proc */
EXTERN int last_login_fs_e;     /* endpoint of the FS proc that logged in
                                   before the corresponding mount request */
EXTERN u32_t system_hz;		/* system clock frequency. */

/* The parameters of the call are kept here. */
EXTERN message m_in;		/* the input message itself */
EXTERN message m_out;		/* the output message used for reply */
EXTERN int who_p, who_e;	/* caller's proc number, endpoint */
EXTERN int call_nr;		/* system call number */

EXTERN message mount_m_in;	/* the input message for a mount request */
EXTERN endpoint_t mount_fs_e;	/* endpoint of file system to mount */
EXTERN char mount_label[LABEL_MAX];	/* label of file system to mount */

EXTERN char user_fullpath[PATH_MAX+1];    /* storage for user path name */

/* The following variables are used for returning results to the caller. */
EXTERN int err_code;		/* temporary storage for error number */

/* Data initialized elsewhere. */
extern _PROTOTYPE (int (*call_vec[]), (void) ); /* sys call table */
extern _PROTOTYPE (int (*pfs_call_vec[]), (void) ); /* pfs callback table */
extern char dot1[2];   /* dot1 (&dot1[0]) and dot2 (&dot2[0]) have a special */
extern char dot2[3];   /* meaning to search_dir: no access permission check. */
