#ifndef __VFS_FILE_H__
#define __VFS_FILE_H__

/* This is the filp table.  It is an intermediary between file descriptors and
 * inodes.  A slot is free if filp_count == 0.
 */

EXTERN struct filp {
  mode_t filp_mode;		/* RW bits, telling how file is opened */
  int filp_flags;		/* flags from open and fcntl */
  int filp_state;		/* state for crash recovery */
  int filp_count;		/* how many file descriptors share this slot?*/
  struct vnode *filp_vno;	/* vnode belonging to this file */
  u64_t filp_pos;		/* file position */
  mutex_t filp_lock;		/* lock to gain exclusive access */
  struct fproc *filp_softlock;	/* if not NULL; this filp didn't lock the
				 * vnode. Another filp already holds a lock
				 * for this thread */

  /* the following fields are for select() and are owned by the generic
   * select() code (i.e., fd-type-specific select() code can't touch these).
   */
  int filp_selectors;		/* select()ing processes blocking on this fd */
  int filp_select_ops;		/* interested in these SEL_* operations */
  int filp_select_flags;	/* Select flags for the filp */

  /* following are for fd-type-specific select() */
  int filp_pipe_select_ops;
} filp[NR_FILPS];

#define FILP_CLOSED	0	/* filp_mode: associated device closed */

#define FS_NORMAL	000	/* file descriptor can be used normally */
#define FS_NEEDS_REOPEN	001	/* file descriptor needs to be re-opened */
#define FS_INVALIDATED	002	/* file was invalidated */

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
