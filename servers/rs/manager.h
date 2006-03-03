/* This table has one slot per system process.  It contains information for
 * servers and driver needed by the reincarnation server to keep track of 
 * each process' status. 
 */

/* Space reserved for program and arguments. */
#define MAX_COMMAND_LEN     512		/* maximum argument string length */
#define MAX_NR_ARGS	      4		/* maximum number of arguments */
#define MAX_RESCUE_DIR_LEN   64		/* maximum rescue dir length */

/* Definition of the system process table. This table only has entries for
 * the servers and drivers, and thus is not directly indexed by slot number.
 */
extern struct rproc {
  int r_proc_nr_e;		/* process endpoint number */
  pid_t r_pid;			/* process id */
  dev_t r_dev_nr;		/* major device number */
  int r_dev_style;		/* device style */

  int r_restarts;		/* number of restarts (initially zero) */
  long r_backoff;		/* number of periods to wait before revive */
  unsigned r_flags; 		/* status and policy flags */

  long r_period;		/* heartbeat period (or zero) */
  clock_t r_check_tm;		/* timestamp of last check */
  clock_t r_alive_tm;		/* timestamp of last heartbeat */
  clock_t r_stop_tm;		/* timestamp of SIGTERM signal */

  char r_cmd[MAX_COMMAND_LEN];	/* raw command plus arguments */
  char *r_argv[MAX_NR_ARGS+2];  /* parsed arguments vector */
  int r_argc;  			/* number of arguments */
} rproc[NR_SYS_PROCS];

/* Mapping for fast access to the system process table. */ 
extern struct rproc *rproc_ptr[NR_PROCS];
extern int nr_in_use;

/* Flag values. */
#define RS_IN_USE       0x001	/* set when process slot is in use */
#define RS_EXITING      0x002	/* set when exit is expected */
#define RS_REFRESHING   0x004	/* set when refresh must be done */

/* Constants determining RS period and binary exponential backoff. */
#define RS_DELTA_T       60			/* check every T ticks */
#define BACKOFF_BITS 	(sizeof(long)*8)	/* bits in backoff field */
#define MAX_BACKOFF     30			/* max backoff in RS_DELTA_T */

/* Magic process table addresses. */
#define BEG_RPROC_ADDR	(&rproc[0])
#define END_RPROC_ADDR	(&rproc[NR_SYS_PROCS])
#define NIL_RPROC ((struct mproc *) 0)

