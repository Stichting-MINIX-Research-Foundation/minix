#ifndef __VFS_FILE_H__
#define __VFS_FILE_H__

/* This is the filp table.  It is an intermediary between file descriptors and
 * inodes.  A slot is free if filp_count == 0.
 */

EXTERN struct filp {
  mode_t filp_mode;		/* RW bits, telling how file is opened */
  int filp_flags;		/* flags from open and fcntl */
  int filp_count;		/* how many file descriptors share this slot?*/
  struct vnode *filp_vno;	/* vnode belonging to this file */
  off_t filp_pos;		/* file position */
  mutex_t filp_lock;		/* lock to gain exclusive access */
  struct fproc *filp_softlock;	/* if not NULL; this filp didn't lock the
				 * vnode. Another filp already holds a lock
				 * for this thread */
  struct fproc *filp_ioctl_fp;	/* if not NULL, this filp is locked by the
				 * process for a currently ongoing IOCTL call
				 */

  /* the following fields are for select() and are owned by the generic
   * select() code (i.e., fd-type-specific select() code can't touch these).
   * These fields may be changed without holding the filp lock.
   */
  int filp_selectors;		/* select()ing processes blocking on this fd */
  int filp_select_ops;		/* interested in these SEL_* operations */
  int filp_select_flags;	/* Select flags for the filp */

  /* following are for fd-type-specific select() */
  int filp_pipe_select_ops;	/* used for pipes */
  dev_t filp_select_dev;	/* used for character and socket devices */
} filp[NR_FILPS];

#define FILP_CLOSED	0	/* filp_mode: associated device closed/gone */

#define FSF_UPDATE	001	/* The driver should be informed about new
				 * state.
				 */
#define FSF_BUSY	002	/* Select operation sent to driver but no
				 * reply yet.
				 */
#define FSF_RD_BLOCK	010	/* Read request is blocking, the driver should
				 * keep state.
				 */
#define FSF_WR_BLOCK	020	/* Write request is blocking */
#define FSF_ERR_BLOCK	040	/* Exception request is blocking */
#define FSF_BLOCKED	070
#endif
