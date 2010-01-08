/* Global constants used in RS.
 */
#ifndef RS_CONST_H
#define RS_CONST_H

/* Space reserved for program and arguments. */
#define MAX_COMMAND_LEN     512         /* maximum argument string length */
#define MAX_SCRIPT_LEN      256         /* maximum restart script name length */
#define MAX_NR_ARGS           4         /* maximum number of arguments */

#define MAX_IPC_LIST        256         /* Max size of list for IPC target
                                         * process names
                                         */

/* Flag values. */
#define RS_IN_USE       0x001    /* set when process slot is in use */
#define RS_EXITING      0x004    /* set when exit is expected */
#define RS_REFRESHING   0x008    /* set when refresh must be done */
#define RS_NOPINGREPLY  0x010    /* service failed to reply to a ping request */
#define RS_KILLED       0x020    /* service is killed */
#define RS_CRASHED      0x040    /* service crashed */
#define RS_LATEREPLY    0x080    /* no reply sent to RS_DOWN caller yet */
#define RS_SIGNALED     0x100    /* service crashed */
#define RS_INITIALIZING 0x200    /* set when init is in progress */
#define RS_UPDATING     0x400    /* set when update is in progress */

/* Sys flag values. */
#define SF_CORE_SRV     0x001    /* set for core system services
                                  * XXX FIXME: This should trigger a system
                                  * panic when a CORE_SRV service cannot
                                  * be restarted. We need better error-handling
                                  * in RS to change this.
                                  */
#define SF_SYNCH_BOOT   0X002    /* set when process needs synch boot init */
#define SF_NEED_COPY    0x004    /* set when process needs copy to restart */
#define SF_USE_COPY     0x008    /* set when process has a copy in memory */

/* Constants determining RS period and binary exponential backoff. */
#define RS_INIT_T       600                     /* allow T ticks for init */
#define RS_DELTA_T       60                     /* check every T ticks */
#define BACKOFF_BITS    (sizeof(long)*8)        /* bits in backoff field */
#define MAX_BACKOFF      30                     /* max backoff in RS_DELTA_T */

/* Magic process table addresses. */
#define BEG_RPROC_ADDR  (&rproc[0])
#define END_RPROC_ADDR  (&rproc[NR_SYS_PROCS])
#define NIL_RPROC ((struct mproc *) 0)

/* Constants for live update. */
#define RS_DEFAULT_PREPARE_MAXTIME 2*RS_DELTA_T   /* default prepare max time */
#define RS_MAX_PREPARE_MAXTIME     20*RS_DELTA_T  /* max prepare max time */


/* Definitions for boot info tables. */
#define NULL_BOOT_NR    NR_BOOT_PROCS        /* marks a null boot entry */
#define DEFAULT_BOOT_NR NR_BOOT_PROCS        /* marks the default boot entry */
#define SYS_ALL_C       (-1)                 /* specifies all calls */
#define SYS_NULL_C      (-2)                 /* marks a null call entry */

/* Define privilege flags for the various process types. */
#define SRV_F  (SYS_PROC | PREEMPTIBLE)            /* system services */
#define DSRV_F (SRV_F | DYN_PRIV_ID | CHECK_IO_PORT | CHECK_IRQ)
                                                   /* dynamic system services */
#define VM_F   (SYS_PROC)                          /* vm  */
#define RUSR_F (BILLABLE | PREEMPTIBLE)            /* root user proc */

/* Define system call traps for the various process types. These call masks
 * determine what system call traps a process is allowed to make.
 */
#define SRV_T   (~0)                               /* system services */
#define DSRV_T  SRV_T                              /* dynamic system services */
#define RUSR_T  (1 << SENDREC)                     /* root user proc */

/* Send masks determine to whom processes can send messages or notifications. */
#define SRV_M   (~0)                               /* system services */
#define RUSR_M \
    ( spi_to(PM_PROC_NR) | spi_to(FS_PROC_NR) | spi_to(RS_PROC_NR) \
    | spi_to(VM_PROC_NR) )                         /* root user proc */

/* Define sys flags for the various process types. */
#define SRV_SF   (SF_CORE_SRV | SF_NEED_COPY)  /* system services */
#define SRVC_SF  (SRV_SF | SF_USE_COPY)        /* system services with a copy */
#define DSRV_SF  (0)                           /* dynamic system services */
#define VM_SF    (SRV_SF | SF_SYNCH_BOOT)      /* vm */

#endif /* RS_CONST_H */

