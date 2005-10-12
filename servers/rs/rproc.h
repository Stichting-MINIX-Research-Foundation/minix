/* This table has one slot per system process.  It contains information for
 * servers and driver needed by the reincarnation server to keep track of 
 * each process' status. 
 */

/* Space reserved for program and arguments. */
#define MAX_COMMAND_LEN     512		/* maximum argument string length */
#define MAX_NR_ARGS	      4		/* maximum number of arguments */

/* Definition of the system process table. This table only has entries for
 * the servers and drivers, and thus is not directly indexed by slot number.
 */
extern struct rproc {
  int r_proc_nr;		/* process slot number */
  pid_t r_pid;			/* process id */
  dev_t r_dev_nr;		/* major device number */
  int r_dev_style;		/* device style */

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
#define IN_USE          0x001	/* set when process slot is in use */
#define EXIT_PENDING    0x002	/* set when exit is expected */
#define STAT_PENDING    0x003   /* set when heartbeat is expected */

/* Magic process table addresses. */
#define BEG_RPROC_ADDR	(&rproc[0])
#define END_RPROC_ADDR	(&rproc[NR_SYS_PROCS])
#define NIL_RPROC ((struct mproc *) 0)

