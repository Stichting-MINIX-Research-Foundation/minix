#include "fs.h"

/*===========================================================================*
 *				fs_trunc				     *
 *===========================================================================*/
int fs_trunc(ino_t ino_nr, off_t start, off_t end)
{
  struct inode *rip;

  if( (rip = find_inode(ino_nr)) == NULL) return(EINVAL);

  if (end != 0) return(EINVAL); /* creating holes is not supported */

  return truncate_inode(rip, start);
}


/*===========================================================================*
 *				truncate_inode				     *
 *===========================================================================*/
int truncate_inode(rip, newsize)
register struct inode *rip;	/* pointer to inode to be truncated */
off_t newsize;			/* inode must become this size */
{
/* Set inode to a certain size, freeing any zones no longer referenced
 * and updating the size in the inode. If the inode is extended, the
 * extra space is a hole that reads as zeroes.
 *
 * Nothing special has to happen to file pointers if inode is opened in
 * O_APPEND mode, as this is different per fd and is checked when
 * writing is done.
 */

  /* Pipes can shrink, so adjust size to make sure all zones are removed. */
  if(newsize != 0) return(EINVAL);	/* Only truncate pipes to 0. */
  rip->i_size = newsize;

  /* Next correct the inode size. */
  wipe_inode(rip);	/* Pipes can only be truncated to 0. */

  return(OK);
}
