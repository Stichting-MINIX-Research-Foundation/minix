/* This is the filp table.  It is an intermediary between file descriptors and
 * inodes.  A slot is free if filp_count == 0.
 */

EXTERN struct filp {
  mode_t filp_mode;		/* RW bits, telling how file is opened */
  int filp_flags;		/* flags from open and fcntl */
  int filp_count;		/* how many file descriptors share this slot?*/
  struct inode *filp_ino;	/* pointer to the inode */
  off_t filp_pos;		/* file position */
} filp[NR_FILPS];

#define FILP_CLOSED	0	/* filp_mode: associated device closed */

#define NIL_FILP (struct filp *) 0	/* indicates absence of a filp slot */
