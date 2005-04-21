/* Definition of the 'p_sendmask' bit mask used in the process table. The bit
 * mask of process is checked in mini_send() to see if the caller is allowed
 * to send to the destination. The bit masks accomodate bits for NR_TASKS + 
 * (LOW_USER+1) + 1. This means that there are bits for each task, driver, and
 * server process, INIT, and one bit to represent all ordinary user processes. 
 *
 * NOTE: the send masks definitions must be updated!!!
 *
 * Changes:
 *   May 01, 2004	created and sendmask definitions  (Jorrit N. Herder)
 */

#ifndef SENDMASK_H
#define SENDMASK_H

/* Define type for sendmask, if not already done. */
#include "type.h"

/* Constants to support the bitmask operations. */
#define BIT_0		(send_mask_t) 1
#define MASK_ENTRIES	NR_TASKS + (LOW_USER+1) + 1
#define USER_PROC_NR	LOW_USER+1 	/* used to set bit for user procs */
#define ALLOW_ALL_MASK	(send_mask_t) -1
#define DENY_ALL_MASK	(send_mask_t) 0

/* Check if given process number is in range. */
#define isvalid(n) ((unsigned) ((n)+NR_TASKS) <= MASK_ENTRIES -1)

/* Default masks and bit operations that easily allow to construct bit masks.
 * Note the one always must start with a default mask like allow_all_mask.
 * From that point, one can, for example, deny several processes.
 */
#define allow_all_mask		ALLOW_ALL_MASK
#define deny_all_mask		DENY_ALL_MASK
#define allow(enabled,n)	| (enabled << ((n) + NR_TASKS))
#define deny(enabled,n)		& ~(enabled << ((n) + NR_TASKS))
#define send_mask_allow(mask,n)	((mask) |= (1 << ((n) + NR_TASKS)))
#define send_mask_deny(mask,n)	((mask) &= ~(1 << ((n) + NR_TASKS)))

/* Check if the bit for the given process number is set. */
#define isallowed(mask,n) ((mask) & (BIT_0 << ((n) + NR_TASKS)))


/* The masks below match the processes (and order) in src/kernel/table.c.
 * Note that the masks are made effective the inclusion in the task table
 * which is used to set up the process table on start up.
 */

#define	TTY_SENDMASK \
    allow_all_mask 

#define DP8390_SENDMASK \
    allow_all_mask 
 
#define RTL8139_SENDMASK \
    deny_all_mask \
    allow(1, USER_PROC_NR) 	/* inet server starts as user process */ \
    allow(1, TTY) 		/* need to register function key */ \
    allow(1, SYSTASK) 		/* need system functionality */	 \
    allow(1, CLOCK)	  	/* need clock functionality */   

#define	IDLE_SENDMASK \
    deny_all_mask 

/* The tasktab in src/kernel/table.c supports up to 4 controllers 
 * it is possible to define separate masks for them here, but then 
 * a small update in table.c is required to make them effective 
 */
#define	CTRLR_SENDMASK \
    allow_all_mask 

#define SB16DSP_SENDMASK \
    allow_all_mask 

#define SB16MIX_SENDMASK \
    allow_all_mask 

#define	FLOPPY_SENDMASK \
    allow_all_mask 

#define	CLOCK_SENDMASK \
    allow_all_mask 

#define	SYSTEM_SENDMASK \
    allow_all_mask 

#define	HARDWARE_SENDMASK \
    allow_all_mask \
    deny(1, USER_PROC_NR) 

#define MM_SENDMASK \
    deny_all_mask \
    allow(1, IS_PROC_NR)	/* output diagnostics */ \
    allow(1, SYSTASK) \
    allow(1, TTY) \
    allow(1, CLOCK) \
    allow(1, INIT_PROC_NR) \
    allow(1, FS_PROC_NR) \
    allow(1, USER_PROC_NR)	/* reply to system calls */ 

#define AT_SENDMASK \
    allow_all_mask

#define FS_SENDMASK \
    allow_all_mask
#if 0
    deny_all_mask \
    allow(1, IS_PROC_NR)	/* output diagnostics */ \
    allow(1, SYSTASK) 		/* need system functionality */	 \
    allow(1, CLOCK)	  	/* need clock functionality */   \
    allow(1, IS_PROC_NR) 	/* output diagnostics */   \
    allow(1, TTY) 		/* a.o. observe function keys */ \
    allow(1, FLOPPY) \
    allow(ENABLE_SB16, SB16DSP ) \
    allow(ENABLE_SB16, SB16MIX ) \
    allow(ENABLE_PRINTER, PRINTER ) \
    allow(1, MEMORY ) \
    allow((NR_CTRLRS >= 1), CTRLR(0)) \
    allow((NR_CTRLRS >= 2), CTRLR(1)) \
    allow((NR_CTRLRS >= 3), CTRLR(2)) \
    allow((NR_CTRLRS >= 4), CTRLR(3)) \
    allow(1, INIT_PROC_NR) \
    allow(1, MM_PROC_NR) 	/* cooperates with memory manager */ \
    allow(1, USER_PROC_NR)	/* reply to system calls */ 
#endif

#define IS_SENDMASK \
    allow_all_mask 		/* IS handles all diagnostic messages */
#if 0
    deny_all_mask \
    allow(1, CLOCK)		/* clock delays and flag alarm needed */ \
    allow(1, FS_PROC_NR)	/* open /dev/mem to read CMOS clock */ \
    allow(1, SYSTASK) 		/* copy tables from kernel space */ \
    allow(1, TTY) 		/* request function key notifications */ \
    allow(1, USER_PROC_NR)	/* reply to system calls */ 
#endif

#define MEM_SENDMASK \
    deny_all_mask \
    allow(1, IS_PROC_NR)	/* output diagnostics */ \
    allow(1, SYSTASK) 		/* system functionality needed */ \
    allow(1, CLOCK) 		/* check clock alarms */ \
    allow(1, TTY) 		/* output diagnostics */ \
    allow(1, FS_PROC_NR)	/* FS is interface to the driver */ 

#define PRN_SENDMASK \
    deny_all_mask \
    allow(1, IS_PROC_NR)	/* output diagnostics */ \
    allow(1, SYSTASK) 		/* device port I/O needed */ \
    allow(1, TTY) 		/* output diagnostics */ \
    allow(1, CLOCK) 		/* need small delays */ \
    allow(1, FS_PROC_NR)	/* FS is interface to the driver */ 

#define INIT_SENDMASK \
    deny_all_mask \
    allow(1, FS_PROC_NR)	/* init makes system calls to FS and MM */ \
    allow(1, MM_PROC_NR)	

#define USER_PROC_SENDMASK \
    deny_all_mask \
    allow(1, FS_PROC_NR) 	/* users can only make system calls */ \
    allow(1, MM_PROC_NR) \
    allow(1, IS_PROC_NR) \
    allow(ENABLE_TASKSERVER, TS_PROC_NR) 


#endif  /* SENDMASK_H */

