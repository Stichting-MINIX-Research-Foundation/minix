/* Type definitions used in RS.
 */
#ifndef RS_TYPE_H
#define RS_TYPE_H

/* Definition of an entry of the boot image priv table. */
struct boot_image_priv {
  endpoint_t endpoint;         /* process endpoint number */
  char label[RS_MAX_LABEL_LEN]; /* label to assign to this service */

  int flags;                   /* privilege flags */
};

/* Definition of an entry of the boot image sys table. */
struct boot_image_sys {
  endpoint_t endpoint;         /* process endpoint number */

  int flags;                   /* system flags */
};

/* Definition of an entry of the boot image dev table. */
struct boot_image_dev {
  endpoint_t endpoint;         /* process endpoint number */

  dev_t dev_nr;                /* major device number */
};

/* Definition of the update descriptors. */
struct rproc;
struct rprocupd {
  int lu_flags;		   /* user-specified live update flags */
  int init_flags;		   /* user-specified init flags */
  int prepare_state;       /* the state the process has to prepare for the update */
  endpoint_t state_endpoint; /* the custom process to transfer the state from (if any). */
  clock_t prepare_tm;      /* timestamp of when the update was scheduled */
  clock_t prepare_maxtime; /* max time to wait for the process to be ready */
  struct rproc *rp;        /* the process under update */
  struct rs_state_data prepare_state_data; /* state data for the update */
  cp_grant_id_t prepare_state_data_gid; /* state data gid */
  struct rprocupd *prev_rpupd;   /* the previous process under update */
  struct rprocupd *next_rpupd;   /* the next process under update */
};
struct rupdate {
  int flags;               /* flags to keep track of the status of the update */
  int num_rpupds;          /* number of descriptors scheduled for the update */
  int num_init_ready_pending;   /* number of pending init ready messages */
  struct rprocupd *curr_rpupd;  /* the current descriptor under update */
  struct rprocupd *first_rpupd; /* first descriptor scheduled for the update */
  struct rprocupd *last_rpupd;  /* last descriptor scheduled for the update */
  struct rprocupd *vm_rpupd;    /* VM descriptor scheduled for the update */
  struct rprocupd *rs_rpupd;    /* RS descriptor scheduled for the update */
};

/* Definition of an entry of the system process table. */
typedef struct priv ixfer_priv_s;
struct rproc {
  struct rprocpub *r_pub;       /* pointer to the corresponding public entry */
  struct rproc *r_old_rp;       /* pointer to the slot with the old version */
  struct rproc *r_new_rp;       /* pointer to the slot with the new version */
  struct rproc *r_prev_rp;      /* pointer to the slot with the prev replica */
  struct rproc *r_next_rp;      /* pointer to the slot with the next replica */
  struct rprocupd r_upd;        /* update descriptor */
  pid_t r_pid;			/* process id, -1 if the process is not there */

  int r_asr_count;		/* number of live updates with ASR */
  int r_restarts;		/* number of restarts (initially zero) */
  long r_backoff;		/* number of periods to wait before revive */
  unsigned r_flags; 		/* status and policy flags */
  int r_init_err;               /* error code at initialization time */

  long r_period;		/* heartbeat period (or zero) */
  clock_t r_check_tm;		/* timestamp of last check */
  clock_t r_alive_tm;		/* timestamp of last heartbeat */
  clock_t r_stop_tm;		/* timestamp of SIGTERM signal */
  endpoint_t r_caller;		/* RS_LATEREPLY caller */
  int r_caller_request;		/* RS_LATEREPLY caller request */

  char r_cmd[MAX_COMMAND_LEN];	/* raw command plus arguments */
  char r_args[MAX_COMMAND_LEN];	/* null-separated raw command plus arguments */
#define ARGV_ELEMENTS (MAX_NR_ARGS+2) /* path, args, null */
  char *r_argv[ARGV_ELEMENTS];
  int r_argc;  			/* number of arguments */
  char r_script[MAX_SCRIPT_LEN]; /* name of the restart script executable */

  char *r_exec;			/* Executable image */ 
  size_t r_exec_len;		/* Length of image */

  ixfer_priv_s r_priv;		/* Privilege structure to be passed to the
				 * kernel.
				 */
  uid_t r_uid;
  endpoint_t r_scheduler;	/* scheduler */
  int r_priority;		/* negative values are reserved for special meanings */
  int r_quantum;
  int r_cpu;
  vir_bytes r_map_prealloc_addr; /* preallocated mmap address */
  size_t r_map_prealloc_len;     /* preallocated mmap len */

  /* Backup values from the privilege structure. */
  struct io_range r_io_tab[NR_IO_RANGE];
  int r_nr_io_range;
  int r_irq_tab[NR_IRQ];
  int r_nr_irq;

  char r_ipc_list[MAX_IPC_LIST];
  int r_nr_control;
  char r_control[RS_NR_CONTROL][RS_MAX_LABEL_LEN];
};

#endif /* RS_TYPE_H */


