/* Definition of the 'p_sendmask' bit mask used in the process table. The bit
 * mask of process is checked in mini_send() to see if the caller is allowed
 * to send to the destination. 
 *
 * PLEASE NOTE: the send masks definitions are a mess and must be updated!!!
 *		this will be done when dynamic driver loading is implemented
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
#define MASK_ENTRIES	NR_TASKS + (INIT_PROC_NR+1) + 1
#define USER_PROC_NR	INIT_PROC_NR+1 	/* used to set bit for user procs */
#define ALLOW_ALL_MASK	(send_mask_t) -1
#define DENY_ALL_MASK	(send_mask_t) 0

/* Check if given process number is in range. */
#define isvalid(n) ((unsigned) ((n)+NR_TASKS) <= MASK_ENTRIES -1)

/* Default masks and bit operations that easily allow to construct bit masks.
 * Note the one always must start with a default mask like allow_all_mask.
 * From that point, one can, for example, deny several processes.
 */
#define allow(enabled,n)	| (enabled << ((n) + NR_TASKS))
#define deny(enabled,n)		& ~(enabled << ((n) + NR_TASKS))
#define send_mask_allow(mask,n)	((mask) |= (1 << ((n) + NR_TASKS)))
#define send_mask_deny(mask,n)	((mask) &= ~(1 << ((n) + NR_TASKS)))

/* Check if the bit for the given process number is set. */
#define isallowed(mask,n) ((mask) & (BIT_0 << ((n) + NR_TASKS)))

#define USER_PROC_SENDMASK \
    DENY_ALL_MASK allow(1, PM_PROC_NR) allow(1, FS_PROC_NR)

#endif  /* SENDMASK_H */

