/* This is the filp table.  It is an intermediary between file descriptors and
 * inodes.  A slot is free if filp_count == 0.
 */

EXTERN struct filp {
  mode_t filp_mode;		/* RW bits, telling how file is opened */
  int filp_flags;		/* flags from open and fcntl */
  int filp_count;		/* how many file descriptors share this slot?*/
  struct inode *filp_ino;	/* pointer to the inode */
  off_t filp_pos;		/* file position */

  /* the following fields are for select() and are owned by the generic
   * select() code (i.e., fd-type-specific select() code can't touch these).
   */
  int filp_selectors;		/* select()ing processes blocking on this fd */
  int filp_select_ops;		/* interested in these SEL_* operations */

  /* following are for fd-type-specific select() */
  int filp_pipe_select_ops;
} filp[NR_FILPS];

#define FILP_CLOSED	0	/* filp_mode: associated device closed */

#define NIL_FILP (struct filp *) 0	/* indicates absence of a filp slot */
