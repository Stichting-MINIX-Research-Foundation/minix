/* This file contains file writing system call handlers.
 *
 * The entry points into this file are:
 *   do_write		perform the WRITE file system call
 *   do_ftrunc		perform the FTRUNC file system call
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

static int write_file(struct inode *ino, u64_t *posp, size_t *countp,
	cp_grant_id_t *grantp);

/*===========================================================================*
 *				write_file				     *
 *===========================================================================*/
static int write_file(struct inode *ino, u64_t *posp, size_t *countp,
	cp_grant_id_t *grantp)
{
/* Write data or zeroes to a file, depending on whether a valid pointer to
 * a data grant was provided.
 */
  u64_t pos;
  size_t count, size;
  vir_bytes off;
  char *ptr;
  int r, chunk;

  assert(!IS_DIR(ino));

  if ((r = get_handle(ino)) != OK)
	return r;

  pos = *posp;
  count = *countp;

  assert(count > 0);

  /* Use the buffer from below to eliminate extra copying. */
  size = sffs_table->t_writebuf(&ptr);
  off = 0;

  while (count > 0) {
	chunk = MIN(count, size);

	if (grantp != NULL) {
		r = sys_safecopyfrom(m_in.m_source, *grantp,
			off, (vir_bytes) ptr, chunk);

		if (r != OK)
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

  *posp = pos;
  *countp = off;

  return OK;
}

/*===========================================================================*
 *				do_write				     *
 *===========================================================================*/
int do_write(void)
{
/* Write data to a file.
 */
  struct inode *ino;
  off_t pos;
  size_t count;
  cp_grant_id_t grant;
  int r;

  if (state.s_read_only)
	return EROFS;

  if ((ino = find_inode(m_in.m_vfs_fs_readwrite.inode)) == NULL)
	return EINVAL;

  if (IS_DIR(ino)) return EISDIR;

  pos = m_in.m_vfs_fs_readwrite.seek_pos;
  count = m_in.m_vfs_fs_readwrite.nbytes;
  grant = m_in.m_vfs_fs_readwrite.grant;

  if (count == 0) return EINVAL;

  if ((r = write_file(ino, &pos, &count, &grant)) != OK)
	return r;

  m_out.m_fs_vfs_readwrite.seek_pos = pos;
  m_out.m_fs_vfs_readwrite.nbytes = count;

  return OK;
}

/*===========================================================================*
 *				do_ftrunc				     *
 *===========================================================================*/
int do_ftrunc(void)
{
/* Change file size or create file holes.
 */
  char path[PATH_MAX];
  struct inode *ino;
  struct sffs_attr attr;
  u64_t start, end, delta;
  size_t count;
  int r;

  if (state.s_read_only)
	return EROFS;

  if ((ino = find_inode(m_in.m_vfs_fs_ftrunc.inode)) == NULL)
	return EINVAL;

  if (IS_DIR(ino)) return EISDIR;

  start = m_in.m_vfs_fs_ftrunc.trc_start;
  end = m_in.m_vfs_fs_ftrunc.trc_end;

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

	delta = end - start;

	if (ex64hi(delta) != 0) return EINVAL;

	count = ex64lo(delta);

	r = write_file(ino, &start, &count, NULL);
  }

  return r;
}
