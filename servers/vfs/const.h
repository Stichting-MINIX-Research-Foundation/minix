#ifndef __VFS_CONST_H__
#define __VFS_CONST_H__

/* Tables sizes */
#define NR_FILPS        1024	/* # slots in filp table */
#define NR_LOCKS           8	/* # slots in the file locking table */
#define NR_MNTS           16 	/* # slots in mount table */
#define NR_VNODES       1024	/* # slots in vnode table */
#define NR_WTHREADS	   9	/* # slots in worker thread table */

#define NR_NONEDEVS	NR_MNTS	/* # slots in nonedev bitmap */

/* Miscellaneous constants */
#define SU_UID 	 ((uid_t) 0)	/* super_user's uid_t */
#define SYS_UID  ((uid_t) 0)	/* uid_t for system processes and INIT */
#define SYS_GID  ((gid_t) 0)	/* gid_t for system processes and INIT */

#define FP_BLOCKED_ON_NONE	0 /* not blocked */
#define FP_BLOCKED_ON_PIPE	1 /* susp'd on pipe */
#define FP_BLOCKED_ON_LOCK	2 /* susp'd on lock */
#define FP_BLOCKED_ON_POPEN	3 /* susp'd on pipe open */
#define FP_BLOCKED_ON_SELECT	4 /* susp'd on select */
#define FP_BLOCKED_ON_OTHER	5 /* blocked on other process, check
				     fp_task to find out */

/* test if the process is blocked on something */
#define fp_is_blocked(fp)	((fp)->fp_blocked_on != FP_BLOCKED_ON_NONE)

#define INVALID_THREAD	((thread_t) -1) 	/* known-invalid thread ID */

#define SYMLOOP		16

#define LABEL_MAX	16	/* maximum label size (including '\0'). Should
				 * not be smaller than 16 or bigger than
				 * M_PATH_STRING_MAX.
				 */
#define FSTYPE_MAX	VFS_NAMELEN	/* maximum file system type size */

/* possible select() operation types; read, write, errors */
#define SEL_RD		CDEV_OP_RD
#define SEL_WR		CDEV_OP_WR
#define SEL_ERR		CDEV_OP_ERR
#define SEL_NOTIFY	CDEV_NOTIFY /* not a real select operation */

/* special driver endpoint for CTTY_MAJOR; must be able to pass isokendpt() */
#define CTTY_ENDPT	VFS_PROC_NR

#endif
