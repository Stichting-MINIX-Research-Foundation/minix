#ifndef __VFS_CONST_H__
#define __VFS_CONST_H__

/* Tables sizes */
#define NR_FILPS         512	/* # slots in filp table */
#define NR_LOCKS           8	/* # slots in the file locking table */
#define NR_MNTS           16 	/* # slots in mount table */
#define NR_VNODES        512	/* # slots in vnode table */
#define NR_WTHREADS	   8	/* # slots in worker thread table */

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
#define FP_BLOCKED_ON_DOPEN	5 /* susp'd on device open */
#define FP_BLOCKED_ON_OTHER	6 /* blocked on other process, check
				     fp_task to find out */

/* test if the process is blocked on something */
#define fp_is_blocked(fp)	((fp)->fp_blocked_on != FP_BLOCKED_ON_NONE)

/* test if reply is a driver reply */
#define IS_DRV_REPLY(x)	(IS_DEV_RS(x) || IS_BDEV_RS(x) || (x) == TASK_REPLY)
#define DUP_MASK        0100	/* mask to distinguish dup2 from dup */

#define LOOK_UP            0 /* tells search_dir to lookup string */
#define ENTER              1 /* tells search_dir to make dir entry */
#define DELETE             2 /* tells search_dir to delete entry */
#define IS_EMPTY           3 /* tells search_dir to ret. OK or ENOTEMPTY */

#define SYMLOOP		16

#define LABEL_MAX	16	/* maximum label size (including '\0'). Should
				 * not be smaller than 16 or bigger than
				 * M3_LONG_STRING.
				 */

/* Args to dev_io */
#define VFS_DEV_READ	2001
#define	VFS_DEV_WRITE	2002
#define VFS_DEV_IOCTL	2005
#define VFS_DEV_SELECT	2006

#endif
