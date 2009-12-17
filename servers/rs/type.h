/* Type definitions used in RS.
 */
#ifndef RS_TYPE_H
#define RS_TYPE_H

/* Definition of an entry of the boot image priv table. */
struct boot_image_priv {
  endpoint_t endpoint;         /* process endpoint number */

  int flags;                   /* privilege flags */
  short trap_mask;             /* allowed system call traps */
  int ipc_to;                  /* send mask protection */
  int *k_calls;                /* kernel call protection */
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
  int dev_style;               /* device style */
  long period;                 /* heartbeat period (or zero) */
};

/* Definition of an entry of the system process table. */
struct rproc {
  endpoint_t r_proc_nr_e;	/* process endpoint number */
  pid_t r_pid;			/* process id, -1 if the process is not there */
  dev_t r_dev_nr;		/* major device number */
  int r_dev_style;		/* device style */

  int r_restarts;		/* number of restarts (initially zero) */
  long r_backoff;		/* number of periods to wait before revive */
  unsigned r_flags; 		/* status and policy flags */
  unsigned r_sys_flags; 	/* sys flags */

  long r_period;		/* heartbeat period (or zero) */
  clock_t r_check_tm;		/* timestamp of last check */
  clock_t r_alive_tm;		/* timestamp of last heartbeat */
  clock_t r_stop_tm;		/* timestamp of SIGTERM signal */
  endpoint_t r_caller;		/* RS_LATEREPLY caller */

  char *r_exec;			/* Executable image */ 
  size_t r_exec_len;		/* Length of image */

  char r_label[MAX_LABEL_LEN];	/* unique name of this service */
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
  struct { u16_t vid; u16_t did; } r_pci_id[RSS_NR_PCI_ID];
  int r_nr_pci_class;		/* Number of PCI class IDs */
  struct { u32_t class; u32_t mask; } r_pci_class[RSS_NR_PCI_CLASS];

  u32_t r_call_mask[RSS_NR_SYSTEM];
  char r_ipc_list[MAX_IPC_LIST];
  bitchunk_t r_vm[RSS_VM_CALL_SIZE];
  int r_nr_control;
  char r_control[RSS_NR_CONTROL][MAX_LABEL_LEN];
};

#endif /* RS_TYPE_H */
