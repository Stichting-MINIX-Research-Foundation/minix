/* This table has one slot per system process.  It contains information for
 * servers and driver needed by the reincarnation server to keep track of 
 * each process' status. 
 */

/* Space reserved for program and arguments. */
#define MAX_COMMAND_LEN     512		/* maximum argument string length */
#define MAX_LABEL_LEN	     16		/* Unique name of (this instance of)
					 * the driver
					 */
#define MAX_SCRIPT_LEN      256		/* maximum restart script name length */
#define MAX_NR_ARGS	      4		/* maximum number of arguments */
#define MAX_RESCUE_DIR_LEN   64		/* maximum rescue dir length */

#define MAX_NR_PCI_ID	      4		/* maximum number of PCI device IDs */
#define MAX_NR_PCI_CLASS      4		/* maximum number of PCI class IDs */
#define MAX_NR_SYSTEM	      2		/* should match RSS_NR_SYSTEM */
#define MAX_IPC_LIST	    256		/* Max size of list for IPC target
					 * process names
					 */

/* Definition of the system process table. This table only has entries for
 * the servers and drivers, and thus is not directly indexed by slot number.
 */
extern struct rproc {
  int r_proc_nr_e;		/* process endpoint number */
  pid_t r_pid;			/* process id, -1 if the process is not there */
  dev_t r_dev_nr;		/* major device number */
  int r_dev_style;		/* device style */

  int r_restarts;		/* number of restarts (initially zero) */
  long r_backoff;		/* number of periods to wait before revive */
  unsigned r_flags; 		/* status and policy flags */

  long r_period;		/* heartbeat period (or zero) */
  clock_t r_check_tm;		/* timestamp of last check */
  clock_t r_alive_tm;		/* timestamp of last heartbeat */
  clock_t r_stop_tm;		/* timestamp of SIGTERM signal */
  endpoint_t r_caller;		/* RS_LATEREPLY caller */

  char *r_exec;			/* Executable image */ 
  size_t r_exec_len;		/* Length of image */

  char r_label[MAX_LABEL_LEN];	/* unique name of this driver */
  char r_cmd[MAX_COMMAND_LEN];	/* raw command plus arguments */
  char r_script[MAX_SCRIPT_LEN]; /* name of the restart script executable */
  char *r_argv[MAX_NR_ARGS+2];  /* parsed arguments vector */
  int r_argc;  			/* number of arguments */

  /* Resources */
  int r_set_resources;
  struct priv r_priv;		/* Privilege structure to be passed to the
				 * kernel.
				 */
  uid_t r_uid;
  int r_nice;
  int r_nr_pci_id;		/* Number of PCI devices IDs */
  struct { u16_t vid; u16_t did; } r_pci_id[MAX_NR_PCI_ID];
  int r_nr_pci_class;		/* Number of PCI class IDs */
  struct { u32_t class; u32_t mask; } r_pci_class[MAX_NR_PCI_CLASS];

  u32_t r_call_mask[MAX_NR_SYSTEM];
  char r_ipc_list[MAX_IPC_LIST];
} rproc[NR_SYS_PROCS];

/* Mapping for fast access to the system process table. */ 
extern struct rproc *rproc_ptr[NR_PROCS];

/* Pipe for detection of exec failures. The pipe is close-on-exec, and
 * no data will be written to the pipe if the exec succeeds. After an 
 * exec failure, the slot number is written to the pipe. After each exit,
 * a non-blocking read retrieves the slot number from the pipe.
 */
int exec_pipe[2];

/* Flag values. */
#define RS_IN_USE       0x001	/* set when process slot is in use */
#define RS_EXITING      0x004	/* set when exit is expected */
#define RS_REFRESHING   0x008	/* set when refresh must be done */
#define RS_NOPINGREPLY 	0x010	/* driver failed to reply to a ping request */
#define RS_KILLED 	0x020	/* driver is killed */
#define RS_CRASHED 	0x040	/* driver crashed */
#define RS_LATEREPLY	0x080	/* no reply sent to RS_DOWN caller yet */
#define RS_SIGNALED 	0x100	/* driver crashed */
#define RS_EXECFAILED 	0x200	/* exec failed */

/* Constants determining RS period and binary exponential backoff. */
#define RS_DELTA_T       60			/* check every T ticks */
#define BACKOFF_BITS 	(sizeof(long)*8)	/* bits in backoff field */
#define MAX_BACKOFF     30			/* max backoff in RS_DELTA_T */

/* Magic process table addresses. */
#define BEG_RPROC_ADDR	(&rproc[0])
#define END_RPROC_ADDR	(&rproc[NR_SYS_PROCS])
#define NIL_RPROC ((struct mproc *) 0)

