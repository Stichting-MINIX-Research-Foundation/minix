#include "fs.h"


/*===========================================================================*
 *				fs_read					     *
 *===========================================================================*/
ssize_t fs_read(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t __unused pos, int call)
{
  int r;
  struct buf *bp;
  struct inode *rip;

  /* Find the inode referred */
  if ((rip = find_inode(ino_nr)) == NULL) return(EINVAL);

  if (!S_ISFIFO(rip->i_mode)) return(EIO);

  /* We can't read or write beyond the max file position */
  if (bytes > PIPE_BUF) return(EFBIG);

  if (bytes > rip->i_size) {
	/* There aren't that many bytes to read */
	bytes = rip->i_size;
  }

  /* Copy a chunk from the block buffer to user space. */
  if ((bp = get_block(rip->i_dev, rip->i_num)) == NULL) return(err_code);

  r = fsdriver_copyout(data, 0, bp->b_data, bytes);

  if (r == OK && rip->i_size > bytes) {
	/* Move any remaining data to the front of the buffer. */
	/* FIXME: see if this really is the optimal strategy. */
	memmove(bp->b_data, bp->b_data + bytes, rip->i_size - bytes);
  }

  put_block(rip->i_dev, rip->i_num);

  if (r != OK)
	return r;

  /* Update file size and access time. */
  rip->i_size -= bytes;
  rip->i_update |= ATIME;

  return(bytes);
}


/*===========================================================================*
 *				fs_write				     *
 *===========================================================================*/
ssize_t fs_write(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t __unused pos, int __unused call)
{
  int r;
  struct buf *bp;
  struct inode *rip;

  /* Find the inode referred */
  if ((rip = find_inode(ino_nr)) == NULL) return(EINVAL);

  if (!S_ISFIFO(rip->i_mode)) return(EIO);

  /* Check in advance to see if file will grow too big. */
  if (rip->i_size + bytes > PIPE_BUF)
	return(EFBIG);

  /* Copy the data from user space to the block buffer. */
  if ((bp = get_block(rip->i_dev, rip->i_num)) == NULL) return(err_code);

  r = fsdriver_copyin(data, 0, bp->b_data + rip->i_size, bytes);

  put_block(rip->i_dev, rip->i_num);

  if (r != OK)
	return r;

  /* Update file size and file times. */
  rip->i_size += bytes;
  rip->i_update |= CTIME | MTIME;

  return(bytes);
}
