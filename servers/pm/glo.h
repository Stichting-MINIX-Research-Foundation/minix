/* EXTERN should be extern except in table.c */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

/* Global variables. */
EXTERN struct mproc *mp;	/* ptr to 'mproc' slot of current process */
EXTERN int procs_in_use;	/* how many processes are marked as IN_USE */
EXTERN char monitor_params[128*sizeof(char *)];	/* boot monitor parameters */
EXTERN struct kinfo kinfo;			/* kernel information */

/* The parameters of the call are kept here. */
EXTERN message m_in;		/* the incoming message itself is kept here. */
EXTERN int who;			/* caller's proc number */
EXTERN int call_nr;		/* system call number */

extern _PROTOTYPE (int (*call_vec[]), (void) );	/* system call handlers */
extern char core_name[];	/* file name where core images are produced */
EXTERN sigset_t core_sset;	/* which signals cause core images */
EXTERN sigset_t ign_sset;	/* which signals are by default ignored */

