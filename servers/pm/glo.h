/* EXTERN should be extern except in table.c */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

/* Global variables. */
EXTERN struct mproc *mp;	/* ptr to 'mproc' slot of current process */
EXTERN int procs_in_use;	/* how many processes are marked as IN_USE */
EXTERN char monitor_params[128*sizeof(char *)];	/* boot monitor parameters */
EXTERN struct kinfo kinfo;	/* kernel information */

/* Misc.c */
extern struct utsname uts_val;	/* uname info */

/* The parameters of the call are kept here. */
EXTERN message m_in;		/* the incoming message itself is kept here. */
EXTERN int who_p, who_e;	/* caller's proc number, endpoint */
EXTERN int call_nr;		/* system call number */

extern _PROTOTYPE (int (*call_vec[]), (void) );	/* system call handlers */
EXTERN sigset_t core_sset;	/* which signals cause core images */
EXTERN sigset_t ign_sset;	/* which signals are by default ignored */
EXTERN sigset_t noign_sset;	/* which signals cannot be ignored */

EXTERN u32_t system_hz;		/* System clock frequency. */
EXTERN int abort_flag;
EXTERN char monitor_code[256];		
