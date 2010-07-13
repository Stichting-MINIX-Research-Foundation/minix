/* Privilege-related definitions. */

#ifndef _MINIX_PRIV_H
#define _MINIX_PRIV_H

#include <minix/com.h>
#include <minix/config.h>

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

#endif /* _MINIX_PRIV_H */
