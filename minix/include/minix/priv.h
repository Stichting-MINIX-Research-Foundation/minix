/* Privilege-related definitions. */

#ifndef _MINIX_PRIV_H
#define _MINIX_PRIV_H

#include <minix/com.h>
#include <minix/config.h>

/* Static privilege id definitions. */
#define NR_STATIC_PRIV_IDS         NR_BOOT_PROCS
#define is_static_priv_id(id)	   (id >= 0 && id < NR_STATIC_PRIV_IDS)
#define static_priv_id(n)          (NR_TASKS + (n))

/* Unprivileged user processes all share the privilege structure of the
 * user processesess.
 * This id must be fixed because it is used to check send mask entries.
 */
#define USER_PRIV_ID	static_priv_id(ROOT_USR_PROC_NR)
/* Specifies a null privilege id.
 */
#define NULL_PRIV_ID	(-1)

/* Allowed targets. */
#define NO_M      (-1)              /* no targets allowed */
#define ALL_M     (-2)              /* all targets allowed */

/* Allowed calls. */
#define NO_C      (-1)              /* no calls allowed */
#define ALL_C     (-2)              /* all calls allowed */
#define NULL_C    (-3)              /* null call entry */

/*
 * Default privilege settings used in the system
 */
/* privilege flags */
#define IDL_F     (SYS_PROC | BILLABLE) /* idle task is not preemptible as we
                                         * don't want it to interfere with the
                                         * timer tick interrupt handler code.
                                         * Unlike other processes idle task is
                                         * handled in a special way and is
                                         * preempted always if timer tick occurs
                                         * and there is another runnable process
                                         */
#define TSK_F     (SYS_PROC)                       /* other kernel tasks */
#define SRV_F     (SYS_PROC | PREEMPTIBLE)         /* system services */
#define DSRV_F    (SRV_F | DYN_PRIV_ID)            /* dynamic system services */
#define RSYS_F    (SRV_F | ROOT_SYS_PROC)          /* root sys proc */
#define VM_F      (SYS_PROC | VM_SYS_PROC)         /* vm */
#define USR_F     (BILLABLE | PREEMPTIBLE)         /* user processes */
#define IMM_F     (ROOT_SYS_PROC | VM_SYS_PROC | PREEMPTIBLE) /* immutable */

/* init flags */
#define TSK_I     0                               /* all kernel tasks */
#define SRV_I     0                               /* system services */
#define DSRV_I    0                               /* dynamic system services */
#define USR_I     0                               /* user processes */

/* allowed traps */
#define CSK_T     (1 << RECEIVE)                   /* clock and system */
#define TSK_T     0                                /* other kernel tasks */
#define SRV_T     (~0)                             /* system services */
#define DSRV_T    (~0)                             /* dynamic system services */
#define USR_T     (1 << SENDREC)                   /* user processes */

/* allowed targets */
#define TSK_M     NO_M                             /* all kernel tasks */
#define SRV_M     ALL_M                            /* system services */
#define DSRV_M    ALL_M                            /* dynamic system services */
#define USR_M     ALL_M                            /* user processes */

/* allowed kernel calls */
#define TSK_KC    NO_C                             /* all kernel tasks */
#define SRV_KC    ALL_C                            /* dynamic system services */
#define DSRV_KC   ALL_C                            /* default sys proc */
#define USR_KC    NO_C                             /* user processes */

/* allowed vm calls */
#define SRV_VC    ALL_C                            /* dynamic system services */
#define DSRV_VC   ALL_C                            /* default sys proc */
#define USR_VC    ALL_C                            /* user processes */

/* signal manager */
#define SRV_SM    ROOT_SYS_PROC_NR                 /* system services */
#define DSRV_SM   ROOT_SYS_PROC_NR                 /* dynamic system services */
#define USR_SM    PM_PROC_NR                       /* user processes */

/* scheduler */
#define SRV_SCH   KERNEL                           /* system services */
#define DSRV_SCH  SCHED_PROC_NR                    /* dynamic system services */
#define USR_SCH   NONE                             /* user processes */

/* scheduling priority queue. */
#define SRV_Q     USER_Q                           /* system services */
#define DSRV_Q    USER_Q                           /* dynamic system services */
#define USR_Q     USER_Q                           /* user processes */

/* scheduling quantum. */
#define SRV_QT    USER_QUANTUM                     /* system services */
#define DSRV_QT   USER_QUANTUM                     /* dynamic system services */
#define USR_QT    USER_QUANTUM                     /* user processes */

/* default CPU */
#define DSRV_CPU USER_DEFAULT_CPU

#endif /* _MINIX_PRIV_H */
