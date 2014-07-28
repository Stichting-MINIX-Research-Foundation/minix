#ifndef _MINIX_SYS_CONFIG_H
#define _MINIX_SYS_CONFIG_H 1

/*===========================================================================*
 *		This section contains user-settable parameters		     *
 *===========================================================================*/

#define _NR_PROCS	256
#define _NR_SYS_PROCS	64

/* Set the FP_FORMAT type based on the machine selected, either hw or sw    */
#define _FP_NONE		  0	/* no floating point support                */
#define _FP_IEEE		  1	/* conform IEEE floating point standard     */

#ifndef _MINIX_FP_FORMAT
#define _MINIX_FP_FORMAT   _FP_NONE
#endif

/* Kernel debug checks */
#define DEBUG_LOCK_CHECK 1	/* Interrupt Lock/unlock sanity checking. */

#define _KMESS_BUF_SIZE  10000

/* Default stack size (limit) */
#define DEFAULT_STACK_LIMIT (4 * 1024 * 1024)

#endif /* _MINIX_SYS_CONFIG_H */
