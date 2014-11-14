/* This file contains file writing system call handlers.
 *
 * The entry points into this file are:
 *   do_write		perform the WRITE file system call
 *   do_trunc		perform the TRUNC file system call
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

/*===========================================================================*
 *				write_file				     *
 *===========================================================================*/
static ssize_t write_file(struct inode *ino, off_t pos, size_t count,
	struct fsdriver_data *data)
{
/* Write data or zeroes to a file, depending on whether a valid pointer to
 * a data grant was provided.
 */
  size_t size, off, chunk;
  char *ptr;
  int r;

  if (pos < 0)
	return EINVAL;

  assert(!IS_DIR(ino));

  if ((r = get_handle(ino)) != OK)
	return r;

  assert(count > 0);

  /* Use the buffer from below to eliminate extra copying. */
  size = sffs_table->t_writebuf(&ptr);
  off = 0;

  while (count > 0) {
	chunk = MIN(count, size);

	if (data != NULL) {
		if ((r = fsdriver_copyin(data, off, ptr, chunk)) != OK)
			break;
	} else {
		/* Do this every time. We don't know what happens below. */
		memset(ptr, 0, chunk);
	}

	if ((r = sffs_table->t_write(ino->i_file, ptr, chunk, pos)) <= 0)
		break;

	count -= r;
	off += r;
	pos += r;
  }

  if (r < 0)
	return r;

  return off;
}

/*===========================================================================*
 *				do_write				     *
 *===========================================================================*/
ssize_t do_write(ino_t ino_nr, struct fsdriver_data *data, size_t count,
	off_t pos, int call)
{
/* Write data to a file.
 */
  struct inode *ino;

  if (read_only)
	return EROFS;

  if ((ino = find_inode(ino_nr)) == NULL)
	return EINVAL;

  if (IS_DIR(ino)) return EISDIR;

  if (count == 0) return 0;

  return write_file(ino, pos, count, data);
}

/*===========================================================================*
 *				do_trunc				     *
 *===========================================================================*/
int do_trunc(ino_t ino_nr, off_t start, off_t end)
{
/* Change file size or create file holes.
 */
  char path[PATH_MAX];
  struct inode *ino;
  struct sffs_attr attr;
  uint64_t delta;
  ssize_t r;

  if (read_only)
	return EROFS;

  if ((ino = find_inode(ino_nr)) == NULL)
	return EINVAL;

  if (IS_DIR(ino)) return EISDIR;

  if (end == 0) {
	/* Truncate or expand the file. */
	if ((r = verify_inode(ino, path, NULL)) != OK)
		return r;

	attr.a_mask = SFFS_ATTR_SIZE;
	attr.a_size = start;

	r = sffs_table->t_setattr(path, &attr);
  } else {
	/* Write zeroes to the file. We can't create holes. */
	if (end <= start) return EINVAL;

	delta = (uint64_t)end - (uint64_t)start;

	if (delta > SSIZE_MAX) return EINVAL;

	if ((r = write_file(ino, start, (size_t)delta, NULL)) >= 0)
		r = OK;
  }

  return r;
}
