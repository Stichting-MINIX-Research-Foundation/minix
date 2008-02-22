/* This is the filp table.  It is an intermediary between file descriptors and
 * inodes.  A slot is free if filp_count == 0.
 */

EXTERN struct filp {
  mode_t filp_mode;		/* RW bits, telling how file is opened */
  int filp_flags;		/* flags from open and fcntl */
  int filp_state;		/* state for crash recovery */
  int filp_count;		/* how many file descriptors share this slot?*/
/*  struct inode *filp_ino;*/	/* pointer to the inode */

  struct vnode *filp_vno;
  
  u64_t filp_pos;		/* file position */

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

#define FS_NORMAL	0	/* file descriptor can be used normally */
#define FS_NEEDS_REOPEN	1	/* file descriptor needs to be re-opened */

#define FSF_UPDATE	1	/* The driver should be informed about new
				 * state.
				 */
#define FSF_BUSY	2	/* Select operation sent to driver but no 
				 * reply yet.
				 */
#define FSF_BLOCK	4	/* Request is blocking, the driver should 
				 * keep state.
				 */

#define NIL_FILP (struct filp *) 0	/* indicates absence of a filp slot */
