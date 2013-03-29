/* EXTERN should be extern except for the table file */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

/* The following variables are used for returning results to the caller. */

EXTERN int err_code;		/* temporary storage for error number */
EXTERN int rdwt_err;		/* status of last disk i/o request */

EXTERN int(*fs_call_vec[]) (void);

EXTERN message fs_m_in;		/* contains the input message of the request */
EXTERN message fs_m_out;	/* contains the output message of the 
				 * request */
EXTERN int FS_STATE;

EXTERN uid_t caller_uid;
EXTERN gid_t caller_gid;

EXTERN int req_nr;		/* request number to the server */

EXTERN int SELF_E;		/* process number */

EXTERN short path_processed;      /* number of characters processed */
EXTERN char user_path[PATH_MAX+1];  /* pathname to be processed */
EXTERN char *vfs_slink_storage;
EXTERN int symloop;

EXTERN int unmountdone;

EXTERN dev_t fs_dev;    /* the device that is handled by this FS proc */
EXTERN char fs_dev_label[16]; /* Name of the device driver that is handled */

